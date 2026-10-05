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
    assert(Json::parse(p.outgoing.back())["params"]["developerInstructions"].get<std::string>().starts_with("Answer in English."));
    p.ingest(R"({"id":3,"result":{"thread":{"id":"existing"}}})");
    auto size=p.outgoing.size();p.changeLanguage();assert(size==p.outgoing.size());
    language::initialize(2);p.state.messages.push_back({true,"Keep this message",{}});
    p.state.busy=true;p.changeLanguage();assert(language::isFrench());p.state.busy=false;
    p.changeLanguage();assert(!language::isFrench() && !p.state.ready);
    auto resume=Json::parse(p.outgoing.back());assert(resume["method"]=="thread/resume");
    assert(resume["params"]["threadId"]=="existing");
    assert(resume["params"]["developerInstructions"].get<std::string>().starts_with("Answer in English."));
    size=p.outgoing.size();p.submit("Wait");p.changeLanguage();assert(size==p.outgoing.size());
    p.ingest(Json({{"id",resume["id"]},{"result",Json::object()}}).dump());
    assert(p.state.ready && p.state.messages.front().text=="Keep this message");
    p.changeLanguage();resume=Json::parse(p.outgoing.back());assert(language::isFrench());
    p.ingest(Json({{"id",resume["id"]},{"error",{{"message","Failed"}}}}).dump());
    assert(!language::isFrench() && p.state.ready);
}
