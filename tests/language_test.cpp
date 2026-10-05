// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/language.hpp"
#include "../src/protocol.hpp"
#include <cassert>
using namespace assistant;
int main(){
    for(int id=-1;id<48;++id){
        language::initialize(id);
        const bool french=id==2 || id==22;
        assert(language::selectable==french && language::isFrench()==french);
        assert(language::toggle()==french);
        assert(!language::isFrench());
        if(french){assert(language::toggle());assert(language::isFrench());}
    }
    language::initialize(1);
    assert(language::tr("Codex réfléchit...")=="Codex is thinking...");
    assert(language::tr("Erreur: example")=="Error: example");
    assert(std::string(language::code())=="en");
    Protocol p;p.begin();p.ingest(R"({"id":1,"result":{}})");
    p.ingest(R"({"id":2,"result":{"account":{}}})");
    assert(Json::parse(p.outgoing.back())["params"]["developerInstructions"].get<std::string>().starts_with("Follow the response language requested in the current user message."));
    language::initialize(2);
    auto size=p.outgoing.size();p.changeLanguage();
    assert(language::isFrench() && size==p.outgoing.size()); // thread/start still pending
    p.ingest(R"({"id":3,"result":{"thread":{"id":"existing"}}})");
    language::initialize(1);
    size=p.outgoing.size();p.changeLanguage();assert(size==p.outgoing.size());
    language::initialize(2);p.state.messages.push_back({true,"Keep this message",{}});
    p.state.busy=true;p.changeLanguage();assert(language::isFrench());p.state.busy=false;
    // A fresh thread has no rollout yet: toggling must not send thread/resume.
    size=p.outgoing.size();
    for(int i=0;i<5;++i)p.changeLanguage();
    assert(!language::isFrench() && p.state.ready && size==p.outgoing.size());
    assert(p.state.thread=="existing" && p.state.messages.front().text=="Keep this message");
    for(int i=0;i<4;++i){
        const bool french=language::isFrench();
        p.submit("Continue");
        auto turn=Json::parse(p.outgoing.back());
        assert(turn["method"]=="turn/start" && turn["params"]["threadId"]=="existing");
        assert(turn["params"]["input"][0]["text"]==std::string(french?"Answer in French.\n\n":"Answer in English.\n\n")+"Continue");
        assert(p.state.messages.back().text=="Continue");
        size=p.outgoing.size();p.changeLanguage();
        assert(language::isFrench()==french && size==p.outgoing.size());
        p.ingest(Json({{"id",turn["id"]},{"result",{{"turn",{{"id","turn"}}}}}}).dump());
        p.ingest(R"({"method":"turn/completed","params":{"turn":{"status":"completed"}}})");
        p.changeLanguage();
        assert(language::isFrench()!=french && p.state.ready && size==p.outgoing.size());
        assert(p.state.thread=="existing" && p.state.messages.front().text=="Keep this message");
    }
    p.state.approvals.push_back({"approval","command","rm file",true});
    p.changeLanguage();assert(!language::isFrench());p.state.approvals.clear();
    p.disconnect();p.changeLanguage();assert(!language::isFrench());
}
