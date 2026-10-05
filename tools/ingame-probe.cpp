// SPDX-License-Identifier: GPL-3.0-or-later
// Host-only JSONL probe. No PS5 hooks, microphone, TTS, or account access.
#include "../src/ingame.hpp"
#include <chrono>
#include <iostream>
#include <string>
using namespace assistant;
static std::uint64_t now(){
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
int main(){
    InGameSession session;
    session.engine.begin();
    const auto flush=[&]{
        while(!session.engine.outgoing.empty()){
            std::cout<<Json({{"channel","codex"},{"message",Json::parse(session.engine.outgoing.front())}}).dump()<<'\n';
            session.engine.outgoing.pop_front();
        }
        while(!session.actions.empty()){
            std::cout<<Json({{"channel","platform"},{"message",session.actions.front()}}).dump()<<'\n';
            session.actions.pop_front();
        }
        std::cout<<Json({{"channel","status"},{"message",session.status()},{"request",session.request()}}).dump()<<std::endl;
    };
    flush();
    std::string line;
    // Bounded reader: never allocate a whole untrusted line before checking its size.
    char byte;
    while(std::cin.get(byte)){
        if(byte!='\n'){
            if(line.size()>=1048576){std::cerr<<"Input line too large\n";return 1;}
            line+=byte;continue;
        }
        auto input=Json::parse(line,nullptr,false);line.clear();
        if(input.is_discarded() || !input.is_object()){std::cerr<<"Invalid JSON object\n";return 1;}
        const auto command=field(input,"command");const auto time=now();
        if(command=="capabilities"){
            const auto enabled=[&](const char* key){auto it=input.find(key);return it!=input.end() && it->is_boolean() && it->get<bool>();};
            session.capabilities({enabled("overlay"),enabled("capture"),enabled("microphone"),enabled("speech")});
        } else if(command=="trigger")session.trigger(time);
        else if(command=="cancel")session.cancel(time);
        else if(command=="tick")session.tick(time);
        else if(command=="disconnected")session.disconnected();
        else if(command=="engine" && input.contains("message") && input["message"].is_object()){
            session.engineEvent(input["message"].dump(),time);
        } else if(command=="adapter" && input.contains("message")){
            if(!session.adapterEvent(input["message"],time))std::cerr<<"Ignored invalid or stale adapter event\n";
        } else {std::cerr<<"Unknown command\n";return 1;}
        flush();
    }
    if(!line.empty()){std::cerr<<"Incomplete JSONL command\n";return 1;}
    session.cancel(now());flush();return 0;
}
