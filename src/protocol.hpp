// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#define JSON_NOEXCEPTION 1
#include "../vendor/json.hpp"
#include <deque>
#include <map>
#include <mutex>
#include <string>
namespace assistant {
using Json=nlohmann::json;
std::string field(const Json& value,const char* key);
struct Approval {Json id;std::string method,description;bool reviewable=true;};
struct Message {bool user=false;std::string text,itemId;};
struct State {
    bool connected=false,ready=false,busy=false,authenticated=false;
    std::string status="Demarrage du moteur...",thread,turn,response,code,url;
    std::deque<Approval> approvals;
    std::deque<Message> messages;
    std::uint64_t chatRevision=1;
    std::uint64_t revision=1;
};
class Protocol {
public:
    State state;
    std::deque<std::string> outgoing;
    void begin();
    void disconnect();
    bool ingest(std::string_view line);
    void login();
    void submit(std::string_view text);
    void interrupt();
    void decide(bool accept);
private:
    int next_=1;
    std::map<int,std::string> pending_;
    std::map<std::string,std::string> items_;
    std::uint64_t turnSequence_=0;
    void agentMessage(std::string id,std::string text,bool append);
    void request(const char* method,Json params);
    void emit(Json message);
    void startThread();
};
}
