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
struct Approval {Json id;std::string method,description;bool reviewable=true;std::string summary;};
// A decision keeps the English summary lines in text; decisionLine() translates them when drawn.
enum class Decision:unsigned char {none,allowed,declined};
struct Message {bool user=false;std::string text,itemId;Decision decision=Decision::none;};
std::string decisionLine(const Message& message);
struct State {
    bool connected=false,ready=false,busy=false,authenticated=false,threadFailed=false;
    std::string status="Starting engine...",thread,turn,response,code,url;
    // Thread the app remembers across launches: set once it holds a turn, cleared for a new conversation.
    std::string conversation;
    std::deque<Approval> approvals;
    std::deque<Message> messages;
    std::uint64_t chatRevision=1;
    std::uint64_t revision=1;
    std::string turnStatus;
};
class Protocol {
public:
    explicit Protocol(bool gameplay=false):gameplay_(gameplay){}
    State state;
    std::deque<std::string> outgoing;
    void begin(std::string_view resume={});
    void disconnect();
    bool ingest(std::string_view line);
    void login();
    bool submit(std::string_view text,std::string_view imagePath={});
    bool retryThread();
    bool newThread();
    void interrupt();
    bool interruptPending() const {return interruptPending_;}
    void decide(bool accept);
private:
    bool gameplay_=false;
    int next_=1;
    std::map<int,std::string> pending_;
    // Details of items still running, so a later approval can show them; dropped on item/completed.
    struct Item {std::string id,details,change;};
    std::deque<Item> items_;
    std::uint64_t turnSequence_=0;
    bool interruptPending_=false; // interrupt asked before the turn id was known
    std::string resume_; // saved thread to resume instead of starting one
    void flushInterrupt();
    void agentMessage(std::string id,std::string text,bool append);
    void request(const char* method,Json params);
    void emit(Json message);
    void startThread();
    bool threadPending() const;
    void restore(const Json& thread);
};
}
