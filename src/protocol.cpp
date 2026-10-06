// SPDX-License-Identifier: GPL-3.0-or-later
#include "protocol.hpp"
#include "read_only_command.hpp"
#include "command_summary.hpp"
#include "language.hpp"
#include <algorithm>
namespace assistant {
std::string field(const Json& j,const char* key) {
    if(!j.is_object())return {};
    auto it=j.find(key);return it!=j.end() && it->is_string()?it->get<std::string>():std::string{};
}
std::string decisionLine(const Message& m){
    std::string line=language::tr(m.decision==Decision::allowed?"Allowed: ":"Declined: ");
    for(std::size_t start=0;start<m.text.size();){
        const auto end=std::min(m.text.find('\n',start),m.text.size());
        if(start)line+=" · ";
        line+=language::tr(std::string_view(m.text).substr(start,end-start));start=end+1;
    }
    return line;
}
static const Json& object(const Json& j,const char* key) {
    static const Json empty=Json::object();
    if(!j.is_object())return empty;auto it=j.find(key);
    return it!=j.end() && it->is_object()?*it:empty;
}
void Protocol::emit(Json j){outgoing.push_back(j.dump(-1,' ',false,Json::error_handler_t::replace)+"\n");}
void Protocol::request(const char* method,Json params){
    const int id=next_++;pending_[id]=method;
    emit({{"id",id},{"method",method},{"params",std::move(params)}});
}
static std::string instructions(){
    return "Follow the user's requested response language; otherwise keep the language of the conversation. "
    "You are an assistant running locally on a jailbroken PS5. "
                    "Inspect before modifying. Work on user files in /data or /mnt/usb*. "
                    "Perform read-only inspection without asking the user for confirmation. "
                    "Prefer single direct commands from the bundled toolbox for inspection. "
                    "Do not change firmware, /system, exploit settings, running services, or delete files "
                    "unless the user explicitly asks. Avoid displaying tokens or credentials. "
                    "Discover the installed command capabilities with ps5-tools when needed. "
                    "Choose your own commands and inspect their results. Base claims about running services "
                    "on live observations; installed files alone do not establish that a process is running. "
                    "Report unsupported capabilities honestly instead of guessing. "
                    "Do not mention implementation details or sandboxing unless asked.";
}
bool Protocol::threadPending() const {
    for(const auto& item:pending_)if(item.second=="thread/start" || item.second=="thread/resume")return true;
    return false;
}
void Protocol::startThread(){
    if(!state.authenticated || !state.thread.empty() || threadPending())return;
    Json params={{"model","gpt-6.1-sol"},{"config",{{"model_reasoning_effort","low"}}},
                    {"cwd","/data/codex-ps5/workspace"},{"approvalPolicy",gameplay_?"never":"untrusted"},
                    {"sandbox",gameplay_?"read-only":"danger-full-access"},{"developerInstructions",
                    gameplay_?"You are a voice companion helping the user play a game on PS5. "
                    "Use the supplied screenshot and question. Answer in the user's language, "
                    "briefly and naturally for spoken playback, without Markdown. "
                    "Give actionable help, avoid unsolicited spoilers, and ask a short question "
                    "if the game or situation cannot be identified. Never invent what the image shows. "
                    "Do not modify files, run console commands, or control the game.":instructions()}};
    if(!resume_.empty()){params["threadId"]=resume_;request("thread/resume",std::move(params));return;}
    params["allowProviderModelFallback"]=false;
    request("thread/start",std::move(params));
}
// After a failed thread/start, a new attempt starts only when the user asks for one.
bool Protocol::retryThread(){
    if(!state.ready || !state.authenticated || !state.thread.empty() || threadPending())return false;
    startThread();state.threadFailed=false;state.status="Starting a new conversation...";++state.revision;
    return true;
}
// Leaves the current conversation (kept by Codex) for a new one, only between turns.
bool Protocol::newThread(){
    if(gameplay_ || !state.ready || !state.authenticated || state.busy || !state.approvals.empty() || threadPending())return false;
    state.thread.clear();state.conversation.clear();resume_.clear();state.messages.clear();state.response.clear();
    state.turnStatus.clear();state.threadFailed=false;items_.clear();++state.chatRevision;
    startThread();state.status="Starting a new conversation...";++state.revision;
    return true;
}
// Shows the stored messages of a resumed thread; its approvals and running items are not restored.
void Protocol::restore(const Json& thread){
    state.messages.clear();
    const auto turns=thread.find("turns");
    if(turns!=thread.end() && turns->is_array())for(const auto& turn:*turns){
        const auto items=turn.find("items");
        if(items==turn.end() || !items->is_array())continue;
        for(const auto& item:*items){
            const auto type=field(item,"type");std::string text;
            if(type=="agentMessage")text=field(item,"text");
            else if(type=="userMessage" && item.contains("content") && item["content"].is_array())
                for(const auto& part:item["content"])if(field(part,"type")=="text"){if(!text.empty())text+='\n';text+=field(part,"text");}
            if(text.empty() || (type!="agentMessage" && type!="userMessage"))continue;
            if(text.size()>262144){ // cut on a UTF-8 boundary
                std::size_t end=262144;while(end && (static_cast<unsigned char>(text[end])&0xc0)==0x80)--end;text.resize(end);
            }
            state.messages.push_back({type=="userMessage",std::move(text),field(item,"id")});
        }
    }
    std::size_t bytes=0;for(const auto& m:state.messages)bytes+=m.text.size();
    while(!state.messages.empty() && (state.messages.size()>64 || bytes>1048576)){
        bytes-=state.messages.front().text.size();state.messages.pop_front();
    }
    ++state.chatRevision;
}
static bool threadId(std::string_view id){
    return !id.empty() && id.size()<=128 && std::all_of(id.begin(),id.end(),[](char c){
        return (c>='0' && c<='9') || (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='-' || c=='_';});
}
void Protocol::begin(std::string_view resume){
    resume_=!gameplay_ && threadId(resume)?std::string(resume):std::string(); // may view state.conversation
    pending_.clear();items_.clear();outgoing.clear();state=State{};state.connected=true;interruptPending_=false;
    state.conversation=resume_;
    request("initialize",{{"clientInfo",{{"name","codex_ps5"},{"title","Codex PS5"},{"version","0.0.3"}}},
                          {"capabilities",{{"experimentalApi",true}}}});
}
void Protocol::disconnect(){
    pending_.clear();items_.clear();outgoing.clear();state.connected=state.ready=state.busy=false;
    state.approvals.clear();state.thread.clear();state.turn.clear();state.authenticated=false;interruptPending_=false;
    state.status="Engine disconnected. The request will not be resent.";++state.revision;
}
void Protocol::login(){
    if(!state.ready || state.authenticated || !state.code.empty())return;
    for(const auto& item:pending_)if(item.second=="account/login/start")return;
    request("account/login/start",{{"type","chatgptDeviceCode"}});
}
bool Protocol::submit(std::string_view text,std::string_view imagePath){
    if(state.thread.empty()){retryThread();return false;}
    if(!state.ready || state.busy || state.thread.empty() || text.empty() || text.size()>16384)return false;
    if(!imagePath.empty() && (imagePath.front()!='/' || imagePath.size()>1024 || imagePath.find('\0')!=std::string_view::npos))return false;
    state.busy=true;interruptPending_=false;state.response.clear();state.status="Codex is thinking...";
    state.turnStatus.clear();
    ++turnSequence_;
    if(!gameplay_)state.conversation=state.thread; // remembered before the turn reaches Codex
    state.messages.push_back({true,std::string(text),{}});++state.chatRevision;
    while(state.messages.size()>64)state.messages.pop_front();
    auto input=Json::array({{{"type","text"},{"text",std::string(text)},{"text_elements",Json::array()}}});
    if(!imagePath.empty())input.push_back({{"type","localImage"},{"path",std::string(imagePath)}});
    request("turn/start",{{"threadId",state.thread},{"model","gpt-6.1-sol"},{"effort","low"},{"input",std::move(input)}});
    ++state.revision;
    return true;
}
void Protocol::agentMessage(std::string id,std::string text,bool append){
    if(id.empty())id="local-turn-"+std::to_string(turnSequence_);
    auto found=std::find_if(state.messages.begin(),state.messages.end(),[&](const Message& m){return !m.user && m.itemId==id;});
    if(found==state.messages.end())state.messages.push_back({false,std::move(text),std::move(id)});
    else if(append)found->text+=text;
    else found->text=std::move(text);
    std::size_t bytes=0;for(const auto& m:state.messages)bytes+=m.text.size();
    // Keep recent conversation text bounded without truncating the current response.
    while(state.messages.size()>2 && (state.messages.size()>64 || bytes>1048576)){
        bytes-=state.messages.front().text.size();state.messages.pop_front();
    }
    ++state.chatRevision;
}
void Protocol::interrupt(){
    if(!state.busy)return;
    if(state.turn.empty()){interruptPending_=true;return;} // sent as soon as the turn id is known
    interruptPending_=false;
    request("turn/interrupt",{{"threadId",state.thread},{"turnId",state.turn}});
}
void Protocol::flushInterrupt(){if(interruptPending_ && state.busy && !state.turn.empty())interrupt();}
void Protocol::decide(bool accept){
    if(state.approvals.empty())return;
    auto a=state.approvals.front();state.approvals.pop_front();
    const bool accepted=accept && a.reviewable;
    emit({{"id",a.id},{"result",{{"decision",accepted?"accept":"decline"}}}});
    // Keep a trace of the choice in the conversation.
    auto what=a.summary;
    if(what.empty())what=a.method=="item/fileChange/requestApproval"?"A file change":"A command";
    state.messages.push_back({false,std::move(what),{},accepted?Decision::allowed:Decision::declined});
    while(state.messages.size()>64)state.messages.pop_front();
    ++state.chatRevision;
    state.status=state.approvals.empty()?"Codex is resuming...":"Next action needs approval";
    ++state.revision;
}
bool Protocol::ingest(std::string_view line){
    if(line.size()>1048576)return false;
    auto j=Json::parse(line,nullptr,false);if(j.is_discarded() || !j.is_object())return false;
    const auto method=field(j,"method");const auto& p=object(j,"params");
    auto id=j.find("id");
    if(!method.empty() && id!=j.end()) {
        if(!(id->is_string() || id->is_number_integer()))return false;
        if(method=="item/commandExecution/requestApproval" || method=="item/fileChange/requestApproval"){
            if(gameplay_){emit({{"id",*id},{"result",{{"decision","decline"}}}});return true;}
            // Approve only the exact command supplied with this request, never a
            // cached parent command or a request for broader/network permissions.
            const auto hasContext=[&](const char* key){return p.contains(key) && !p[key].is_null();};
            if(method=="item/commandExecution/requestApproval" &&
               (!hasContext("kind") || field(p,"kind")=="command") &&
               !hasContext("additionalPermissions") && !hasContext("networkApprovalContext") &&
               !hasContext("grantRoot") && readOnlyCommand(field(p,"command"))){
                emit({{"id",*id},{"result",{{"decision","accept"}}}});
                ++state.revision;return true;
            }
            if(state.approvals.size()>=16)return false;
            const auto itemId=field(p,"itemId");
            const auto cached=std::find_if(items_.begin(),items_.end(),[&](const Item& i){return !itemId.empty() && i.id==itemId;});
            std::string description=field(p,"command");
            if(description.empty() && cached!=items_.end())description=cached->details;
            const bool detailAvailable=!description.empty() && field(p,"kind")!="stdin";
            // One plain-language line per aspect of the request, translated when drawn.
            std::string summary;
            const auto line=[&](std::string_view text){if(!text.empty()){if(!summary.empty())summary+='\n';summary+=text;}};
            if(field(p,"kind")=="stdin")line("Sending input to a running command");
            else if(method=="item/commandExecution/requestApproval")line(commandSummary(description));
            else if(cached!=items_.end())line(cached->change);
            if(hasContext("grantRoot"))line("Requesting write access to a folder");
            if(hasContext("networkApprovalContext"))line("Requesting network access");
            if(hasContext("additionalPermissions"))line("Requesting additional permissions");
            const auto cwd=field(p,"cwd"),reason=field(p,"reason"),root=field(p,"grantRoot");
            if(!cwd.empty())description+=language::tr("\nDirectory: ")+cwd;
            if(!root.empty())description+=language::tr("\nRequested root: ")+root;
            if(!reason.empty())description+=language::tr("\nReason: ")+reason;
            if(p.contains("additionalPermissions") && !p["additionalPermissions"].is_null())description+=language::tr("\nPermissions: ")+p["additionalPermissions"].dump();
            const bool complete=detailAvailable && description.size()<=8192;
            if(!complete)description=language::tr("Details incomplete or too long: decline only.\n")+description;
            state.approvals.push_back({*id,method,description.substr(0,8192),complete,summary});
            state.status="Action awaiting approval";
        } else emit({{"id",*id},{"error",{{"code",-32601},{"message",language::tr("Interaction unsupported by the PS5 prototype")}}}});
    } else if(method.empty() && id!=j.end() && id->is_number_integer()) {
        if(!id->is_number_unsigned() && id->get<std::int64_t>()<0)return false;
        const auto wide=id->get<std::uint64_t>();if(wide>2147483647)return false;
        auto it=pending_.find(static_cast<int>(wide));if(it==pending_.end())return true;
        const auto requestMethod=it->second;pending_.erase(it);
        if(j.contains("error")) {
            state.status="Error: "+field(object(j,"error"),"message");
            if(requestMethod=="turn/start"){state.busy=false;interruptPending_=false;}
            if(requestMethod=="thread/start")state.threadFailed=true;
            // A new conversation starts, without any prompt. The saved one is replaced only once
            // a turn starts in the new thread, so a transient failure does not lose it.
            if(requestMethod=="thread/resume"){
                resume_.clear();startThread();
                state.status="The previous conversation could not be resumed. A new one has started.";
            }
        } else {
            const auto& r=object(j,"result");
            if(requestMethod=="initialize") {
                emit({{"method","initialized"}});state.ready=true;
                request("account/read",{{"refreshToken",false}});

            } else if(requestMethod=="thread/start"){
                state.thread=field(object(r,"thread"),"id");
                if(state.status=="Starting a new conversation...")state.status="Ready";
            } else if(requestMethod=="thread/resume"){
                const auto& thread=object(r,"thread");state.thread=field(thread,"id");
                if(!threadId(state.thread)){
                    state.thread.clear();resume_.clear();startThread();
                    state.status="The previous conversation could not be resumed. A new one has started.";
                } else {state.conversation=state.thread;restore(thread);state.status="Conversation resumed";}
            }
            else if(requestMethod=="turn/start"){
                state.turn=field(object(r,"turn"),"id");flushInterrupt();
            }
            else if(requestMethod=="account/login/start") {
                state.code=field(r,"userCode");state.url=field(r,"verificationUrl");
                state.status="Sign in on your phone using the displayed code.";
            } else if(requestMethod=="account/read") {
                state.authenticated=r.contains("account") && r["account"].is_object();
                state.status=state.authenticated?"Ready":"\uE000: sign in to ChatGPT";
                startThread();
            }
        }
    } else if(method=="item/started" || method=="item/completed") {
        const auto& item=object(p,"item");const auto type=field(item,"type");
        const auto itemId=field(item,"id");std::string details;
        if(state.busy && state.approvals.empty()){
            if(method=="item/started" && type=="commandExecution")state.status="Codex is running a command...";
            else if(method=="item/started" && type=="agentMessage")state.status="Codex is replying...";
            else if(type=="reasoning" || method=="item/completed")state.status="Codex is thinking...";
        }
        if(type=="agentMessage" && method=="item/completed"){
            const auto text=field(item,"text");if(text.size()>262144)return false;
            if(!text.empty())agentMessage(itemId,text,false);
            if(gameplay_ && !text.empty())state.response=text;
        }
        if(type=="commandExecution")details=field(item,"command");
        else if(type=="fileChange" && item.contains("changes") && item["changes"].is_array()) {
            for(const auto& change:item["changes"]){
                details+=language::tr("File: ")+field(change,"path")+"\n"+field(change,"diff")+"\n";
                if(details.size()>8192)break;
            }
        }
        // A finished item can no longer ask for approval: forget it, so a long turn
        // never pushes out the details of an item still waiting for one.
        auto known=std::find_if(items_.begin(),items_.end(),[&](const Item& i){return i.id==itemId;});
        if(method=="item/completed"){if(known!=items_.end())items_.erase(known);}
        else if(!itemId.empty() && (!details.empty() || type=="fileChange")){
            if(known==items_.end()){if(items_.size()>=64)items_.pop_front();items_.push_back({itemId,{},{}});known=std::prev(items_.end());}
            known->details=details.substr(0,8193);
            if(type=="fileChange")known->change=std::string(fileChangeSummary(item.contains("changes")?item["changes"]:Json()));
        }
    } else if(method=="item/agentMessage/delta") {
        const auto delta=field(p,"delta");
        if(state.response.size()+delta.size()>262144)return false;
        state.response+=delta;
        agentMessage(field(p,"itemId"),delta,true);
        if(state.approvals.empty())state.status="Codex is replying...";
    } else if(method=="item/reasoning/textDelta" || method=="item/reasoning/summaryTextDelta" || method=="item/reasoning/summaryPartAdded"){
        if(state.busy && state.approvals.empty())state.status="Codex is thinking...";
    } else if(method=="turn/started") {state.turn=field(object(p,"turn"),"id");state.busy=true;flushInterrupt();}
    else if(method=="turn/completed") {
        state.busy=false;interruptPending_=false;state.turn.clear();state.approvals.clear();items_.clear();
        const auto& turn=object(p,"turn");const auto status=field(turn,"status");
        state.turnStatus=status;
        state.status=status=="completed"?"Completed":status=="interrupted"?"Interrupted":"Failed: "+field(object(turn,"error"),"message");
    } else if(method=="account/login/completed") {
        if(p.contains("success") && p["success"]==true) {
            state.code.clear();state.url.clear();request("account/read",{{"refreshToken",false}});
        } else {state.code.clear();state.url.clear();state.status="Sign-in failed: "+field(p,"error");}
    } else if(method=="error")state.status="Error: "+field(object(p,"error"),"message");
    ++state.revision;return true;
}
}
