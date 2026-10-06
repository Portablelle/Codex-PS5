#include "../src/ingame.hpp"
#include <cassert>
#include <iostream>
using namespace assistant;
static void ready(InGameSession& s){
    s.capabilities({true,true,true,true});s.engine.begin();
    assert(s.engineEvent(R"({"id":1,"result":{}})",0));
    assert(s.engineEvent(R"({"id":2,"result":{"account":{"type":"chatgpt"}}})",0));
    const auto request=Json::parse(s.engine.outgoing.back());
    assert(request["params"]["sandbox"]=="read-only");
    assert(request["params"]["approvalPolicy"]=="never");
    assert(request["params"]["model"]=="gpt-6.1-sol");
    const auto response=Json({{"id",request["id"]},{"result",{{"thread",{{"id","game-thread"}}}}}});
    assert(s.engineEvent(response.dump(),0));s.engine.outgoing.clear();s.actions.clear();
}
static Json event(const InGameSession& s,const char* name){return {{"request",s.request()},{"event",name}};}
static Json pop(InGameSession& s,const char* expected){
    assert(!s.actions.empty());auto result=s.actions.front();s.actions.pop_front();
    assert(result["action"]==expected);return result;
}
static void record(InGameSession& s){
    s.trigger(100);assert(s.phase()==InGamePhase::capturing);
    const auto capture=pop(s,"capture");assert(capture["path"]=="/data/codex-ps5/captures/ingame-1.png");
    assert(s.actions.empty()); // Must not draw anything before capture completes.
    assert(s.adapterEvent(event(s,"captured"),150));
    assert(pop(s,"overlay")["visible"]==true);
    assert(pop(s,"record")["maxSeconds"]==30);
    assert(s.phase()==InGamePhase::listening);
}
static void ask(InGameSession& s){
    record(s);s.trigger(200);
    pop(s,"stopRecording");assert(pop(s,"overlay")["visible"]==false);
    auto text=event(s,"transcribed");text["text"]="How do I beat this monster?";
    assert(s.adapterEvent(text,250));assert(s.phase()==InGamePhase::thinking);
    auto request=Json::parse(s.engine.outgoing.back());
    assert(request["method"]=="turn/start");
    assert(request["params"]["input"].size()==2);
    assert(request["params"]["input"][1]["type"]=="localImage");
    assert(request["params"]["input"][1]["path"]=="/data/codex-ps5/captures/ingame-1.png");
}
int main(){
    InGameShortcut unbound;assert(!unbound.sample(0xffffffff));
    InGameShortcut shortcut(3);
    assert(!shortcut.sample(1));assert(shortcut.sample(3));assert(!shortcut.sample(3));
    assert(!shortcut.sample(2));assert(shortcut.sample(3));

    InGameSession unavailable;unavailable.trigger(0);assert(unavailable.actions.empty());
    assert(unavailable.status()=="In-game platform adapter unavailable");
    unavailable.capabilities({true,true,true,true});unavailable.trigger(0);
    assert(unavailable.status()=="Codex is not ready");
    {
        // The in-game shortcut also retries a failed thread/start.
        InGameSession failed;failed.capabilities({true,true,true,true});failed.engine.begin();
        failed.engineEvent(R"({"id":1,"result":{}})",0);failed.engineEvent(R"({"id":2,"result":{"account":{"type":"chatgpt"}}})",0);
        assert(failed.engineEvent(R"({"id":3,"error":{"message":"unavailable"}})",0));
        const auto count=failed.engine.outgoing.size();failed.trigger(0);
        assert(failed.status()=="Codex is not ready" && failed.actions.empty() && failed.engine.outgoing.size()==count+1);
        const auto retry=Json::parse(failed.engine.outgoing.back());assert(retry["method"]=="thread/start");
        // Once the retry succeeds, the status no longer claims Codex is not ready.
        assert(failed.engineEvent(Json({{"id",retry["id"]},{"result",{{"thread",{{"id","retried"}}}}}}).dump(),0));
        assert(failed.status()=="Idle" && failed.actions.empty());
    }
    for(const auto path:{"/", "relative", "/data/../system", "/data//captures", "/data/captures/"}){
        InGameSession invalid(path);ready(invalid);invalid.trigger(0);assert(invalid.actions.empty());
    }
    InGameSession s;ready(s);ask(s);
    const auto queued=s.engine.outgoing.size();s.trigger(300);assert(s.engine.outgoing.size()==queued);
    assert(!s.adapterEvent({{"request",0},{"event","transcribed"},{"text","old"}},300));
    assert(!s.adapterEvent({{"request",-1},{"event","captured"}},300));
    assert(!s.adapterEvent(event(s,"speechFinished"),300));
    assert(s.engineEvent(R"({"method":"item/agentMessage/delta","params":{"delta":"intermediate text"}})",300));
    assert(s.actions.empty()); // No speech of partial/streaming responses.
    assert(s.engineEvent(R"({"method":"item/completed","params":{"item":{"type":"agentMessage","id":"final","text":"Dodge left, then attack."}}})",350));
    assert(s.engineEvent(R"({"method":"turn/completed","params":{"turn":{"status":"completed"}}})",400));
    pop(s,"releaseCapture");assert(pop(s,"speak")["text"]=="Dodge left, then attack.");
    assert(s.phase()==InGamePhase::speaking);
    s.trigger(500);pop(s,"stopSpeech");pop(s,"overlay");assert(s.actions.empty());
    assert(s.phase()==InGamePhase::idle);
    s.trigger(600);pop(s,"capture");
    assert(!s.adapterEvent({{"request",1},{"event","captured"}},650));
    s.cancel(700);pop(s,"cancelMedia");pop(s,"overlay");pop(s,"releaseCapture");

    InGameSession autoStop;ready(autoStop);record(autoStop);autoStop.tick(30150);
    pop(autoStop,"stopRecording");pop(autoStop,"overlay");
    assert(autoStop.phase()==InGamePhase::transcribing);
    autoStop.tick(150150);assert(autoStop.phase()==InGamePhase::idle);
    pop(autoStop,"cancelMedia");pop(autoStop,"overlay");pop(autoStop,"releaseCapture");

    InGameSession empty;ready(empty);record(empty);empty.trigger(200);empty.actions.clear();
    auto text=event(empty,"transcribed");text["text"]="";
    assert(empty.adapterEvent(text,250));assert(empty.phase()==InGamePhase::idle);
    assert(empty.engine.outgoing.empty());

    InGameSession captureFailure;ready(captureFailure);captureFailure.trigger(100);pop(captureFailure,"capture");
    assert(captureFailure.adapterEvent(event(captureFailure,"failed"),150));
    pop(captureFailure,"cancelMedia");pop(captureFailure,"overlay");pop(captureFailure,"releaseCapture");
    assert(captureFailure.actions.empty());assert(captureFailure.phase()==InGamePhase::idle);
    assert(!captureFailure.adapterEvent(event(captureFailure,"captured"),200));

    InGameSession captureTimeout;ready(captureTimeout);captureTimeout.trigger(100);pop(captureTimeout,"capture");
    captureTimeout.tick(5100);pop(captureTimeout,"cancelMedia");pop(captureTimeout,"overlay");pop(captureTimeout,"releaseCapture");
    assert(captureTimeout.actions.empty());assert(captureTimeout.phase()==InGamePhase::idle);

    InGameSession cancelled;ready(cancelled);ask(cancelled);cancelled.engine.outgoing.clear();
    cancelled.cancel(300);assert(cancelled.phase()==InGamePhase::cancelling);
    assert(cancelled.engine.outgoing.empty());pop(cancelled,"overlay");
    // Cancellation requested before the engine has assigned a turn ID.
    assert(cancelled.engineEvent(R"({"method":"turn/started","params":{"turn":{"id":"turn-1"}}})",350));
    assert(Json::parse(cancelled.engine.outgoing.back())["method"]=="turn/interrupt");
    auto count=cancelled.engine.outgoing.size();
    assert(cancelled.engineEvent(R"({"method":"item/agentMessage/delta","params":{"delta":"too late"}})",400));
    assert(cancelled.engine.outgoing.size()==count);
    assert(cancelled.engineEvent(R"({"method":"turn/completed","params":{"turn":{"status":"completed"}}})",450));
    pop(cancelled,"overlay");pop(cancelled,"releaseCapture");assert(cancelled.actions.empty());
    assert(cancelled.phase()==InGamePhase::idle);

    InGameSession timeout;ready(timeout);ask(timeout);timeout.actions.clear();
    timeout.tick(180250);assert(timeout.phase()==InGamePhase::cancelling);pop(timeout,"overlay");
    timeout.tick(190250);pop(timeout,"disconnectEngine");pop(timeout,"cancelMedia");
    pop(timeout,"overlay");pop(timeout,"releaseCapture");assert(!timeout.engine.state.connected);
    assert(timeout.engine.outgoing.empty());

    InGameSession failed;ready(failed);ask(failed);
    assert(failed.engineEvent(R"({"method":"item/agentMessage/delta","params":{"delta":"Do not speak this"}})",300));
    assert(failed.engineEvent(R"({"method":"turn/completed","params":{"turn":{"status":"failed","error":{"message":"offline"}}}})",350));
    pop(failed,"overlay");pop(failed,"releaseCapture");assert(failed.actions.empty());
    assert(failed.phase()==InGamePhase::idle);

    for(const auto& answer:{std::string(),std::string(16385,'a')}){
        InGameSession unusable;ready(unusable);ask(unusable);
        if(!answer.empty())assert(unusable.engineEvent(Json({{"method","item/completed"},{"params",{{"item",{{"type","agentMessage"},{"id","x"},{"text",answer}}}}}}).dump(),300));
        assert(unusable.engineEvent(R"({"method":"turn/completed","params":{"turn":{"status":"completed"}}})",350));
        pop(unusable,"overlay");pop(unusable,"releaseCapture");assert(unusable.actions.empty());
        assert(unusable.phase()==InGamePhase::idle);assert(unusable.status()=="No usable answer");
    }

    InGameSession playback;ready(playback);ask(playback);
    assert(playback.engineEvent(R"({"method":"item/completed","params":{"item":{"type":"agentMessage","id":"reply","text":"Use the shield."}}})",300));
    assert(playback.engineEvent(R"({"method":"turn/completed","params":{"turn":{"status":"completed"}}})",350));
    pop(playback,"releaseCapture");pop(playback,"speak");
    assert(playback.adapterEvent(event(playback,"speechFinished"),400));pop(playback,"overlay");
    assert(playback.phase()==InGamePhase::idle);

    InGameSession disconnect;ready(disconnect);ask(disconnect);disconnect.engine.outgoing.clear();
    disconnect.disconnected();pop(disconnect,"cancelMedia");pop(disconnect,"overlay");pop(disconnect,"releaseCapture");
    disconnect.trigger(400);assert(disconnect.actions.empty());assert(disconnect.engine.outgoing.empty());
    assert(!disconnect.engineEvent("not JSON",500));
    pop(disconnect,"disconnectEngine");

    InGameSession rejected;ready(rejected);ask(rejected);
    auto start=Json::parse(rejected.engine.outgoing.back());
    assert(rejected.engineEvent(Json({{"id",start["id"]},{"error",{{"code",-1},{"message","Image could not be read"}}}}).dump(),300));
    pop(rejected,"overlay");pop(rejected,"releaseCapture");assert(rejected.actions.empty());
    assert(rejected.phase()==InGamePhase::idle);

    InGameSession approval;ready(approval);
    assert(approval.engineEvent(R"({"method":"item/commandExecution/requestApproval","id":"read","params":{"command":"ls /data"}})",0));
    assert(approval.engine.state.approvals.empty());
    assert(Json::parse(approval.engine.outgoing.back())["result"]["decision"]=="decline");
    assert(!approval.engine.submit("question","relative.png"));
    assert(!approval.engine.submit("question",std::string("/a\0.png",7)));
    assert(approval.engine.submit("question"));
    assert(Json::parse(approval.engine.outgoing.back())["params"]["input"].size()==1);
    std::cout<<"in-game session tests passed\n";
}
