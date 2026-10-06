// SPDX-License-Identifier: GPL-3.0-or-later
#include "ingame.hpp"
#include <limits>

namespace assistant {
InGameSession::InGameSession(std::string directory):directory_(std::move(directory)){
    validDirectory_=directory_.size()>1 && directory_.size()<900 && directory_.front()=='/' &&
        directory_.back()!='/' && directory_.find('\0')==std::string::npos;
    // Paths are adapter-owned snapshots, never arbitrary paths supplied by an event.
    for(std::size_t pos=1;validDirectory_ && pos<directory_.size();){
        auto end=directory_.find('/',pos);if(end==std::string::npos)end=directory_.size();
        const auto part=directory_.substr(pos,end-pos);
        if(part.empty() || part=="." || part=="..")validDirectory_=false;
        pos=end+1;
    }
}
void InGameSession::action(const char* name,Json fields){
    fields["action"]=name;fields["request"]=request_;actions.push_back(std::move(fields));
}
void InGameSession::transition(InGamePhase phase,const char* status,std::uint64_t deadline){
    phase_=phase;status_=status;deadline_=deadline;
}
void InGameSession::finish(std::string status){
    action("overlay",{{"visible",false}});
    if(!image_.empty()){action("releaseCapture",{{"path",image_}});image_.clear();}
    phase_=InGamePhase::idle;deadline_=0;status_=std::move(status);
}
void InGameSession::trigger(std::uint64_t now){
    if(phase_==InGamePhase::listening){
        action("stopRecording");action("overlay",{{"visible",false}});
        transition(InGamePhase::transcribing,"Transcribing",now+120000);return;
    }
    if(phase_==InGamePhase::speaking){cancel(now);return;}
    if(phase_!=InGamePhase::idle)return;
    if(!validDirectory_){status_="Invalid capture directory";return;}
    if(!capabilities_.overlay || !capabilities_.capture || !capabilities_.microphone || !capabilities_.speech){
        status_="In-game platform adapter unavailable";return;
    }
    if(!engine.state.ready || !engine.state.authenticated || engine.state.thread.empty() || engine.state.busy){
        if(!engine.state.busy)engine.retryThread();
        status_="Codex is not ready";return;
    }
    if(request_==std::numeric_limits<std::uint64_t>::max()){status_="Request limit reached";return;}
    ++request_;interruptSent_=false;image_=directory_+"/ingame-"+std::to_string(request_)+".png";
    transition(InGamePhase::capturing,"Capturing",now+5000);
    action("capture",{{"path",image_}});
}
void InGameSession::cancel(std::uint64_t now){
    if(phase_==InGamePhase::idle || phase_==InGamePhase::cancelling)return;
    if(phase_==InGamePhase::thinking){
        if(!engine.state.turn.empty()){engine.interrupt();interruptSent_=true;}
        action("overlay",{{"visible",false}});
        transition(InGamePhase::cancelling,"Cancelling",now+10000);return;
    }
    if(phase_==InGamePhase::speaking)action("stopSpeech");
    else action("cancelMedia"); // abort capture/recording/transcription and join before release
    finish("Cancelled");
}
bool InGameSession::adapterEvent(const Json& event,std::uint64_t now){
    if(!event.is_object())return false;
    const auto id=event.find("request");
    if(id==event.end() || !id->is_number_integer() ||
       (!id->is_number_unsigned() && id->get<std::int64_t>()<0))return false;
    if(id->get<std::uint64_t>()!=request_ || phase_==InGamePhase::idle)return false;
    const auto type=field(event,"event");
    if(type=="failed"){
        if(phase_==InGamePhase::thinking || phase_==InGamePhase::cancelling)return false;
        if(phase_==InGamePhase::speaking)action("stopSpeech");else action("cancelMedia");
        finish("Platform operation failed");return true;
    }
    if(type=="captured" && phase_==InGamePhase::capturing){
        // Completion acknowledges the exact requested path; it cannot replace it.
        action("overlay",{{"visible",true},{"status","listening"}});
        action("record",{{"maxSeconds",30}});
        transition(InGamePhase::listening,"Listening",now+30000);return true;
    }
    if(type=="recordingStopped" && phase_==InGamePhase::listening){
        action("overlay",{{"visible",false}});
        transition(InGamePhase::transcribing,"Transcribing",now+120000);return true;
    }
    if(type=="transcribed" && phase_==InGamePhase::transcribing){
        const auto text=field(event,"text");
        if(text.find_first_not_of(" \t\r\n")==std::string::npos || !engine.submit(text,image_)){
            finish("Question empty, too long, or Codex unavailable");return true;
        }
        transition(InGamePhase::thinking,"Thinking",now+180000);return true;
    }
    if(type=="speechFinished" && phase_==InGamePhase::speaking){finish("Completed");return true;}
    return false;
}
void InGameSession::syncEngine(std::uint64_t now){
    if(phase_!=InGamePhase::thinking && phase_!=InGamePhase::cancelling)return;
    if(engine.state.busy){
        if(phase_==InGamePhase::cancelling && !interruptSent_ && !engine.state.turn.empty()){
            engine.interrupt();interruptSent_=true;
        }
        return;
    }
    if(phase_==InGamePhase::cancelling){finish("Cancelled");return;}
    if(engine.state.turnStatus!="completed"){finish(engine.state.status);return;}
    if(engine.state.response.empty() || engine.state.response.size()>16384){finish("No usable answer");return;}
    // Codex has consumed the snapshot; it may now be removed before playback.
    action("releaseCapture",{{"path",image_}});image_.clear();
    action("speak",{{"text",engine.state.response}});
    transition(InGamePhase::speaking,"Speaking",now+120000);
}
bool InGameSession::engineEvent(std::string_view line,std::uint64_t now){
    if(!engine.ingest(line)){action("disconnectEngine");disconnected();return false;}
    syncEngine(now);return true;
}
void InGameSession::disconnected(){
    engine.disconnect();
    if(phase_!=InGamePhase::idle){
        if(phase_==InGamePhase::speaking)action("stopSpeech");else action("cancelMedia");
        finish("Engine disconnected; question will not be resent");
    }
}
void InGameSession::tick(std::uint64_t now){
    if(phase_==InGamePhase::idle || now<deadline_)return;
    if(phase_==InGamePhase::listening){trigger(now);return;}
    if(phase_==InGamePhase::thinking){cancel(now);status_="Codex timed out; cancelling";return;}
    if(phase_==InGamePhase::cancelling){
        // Transport must close before applying releaseCapture; no automatic replay.
        action("disconnectEngine");disconnected();return;
    }
    cancel(now);status_="Platform operation timed out";
}
}
