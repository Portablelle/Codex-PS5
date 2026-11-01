// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#define JSON_NOEXCEPTION 1
#include "../vendor/json.hpp"
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>
namespace assistant {
using Json=nlohmann::json;
std::string field(const Json& value,const char* key);
struct Approval {Json id;std::string method,description;bool reviewable=true;std::string summary;};
// A decision keeps the English summary lines in text; decisionLine() translates them when drawn.
enum class Decision:unsigned char {none,allowed,declined};
struct Message {bool user=false;std::string text,itemId;Decision decision=Decision::none;};
std::string decisionLine(const Message& message);
// A model offered by model/list, with the reasoning efforts it accepts.
struct ModelOption {std::string model,name;std::vector<std::string> efforts;std::string defaultEffort;};
struct State {
    bool connected=false,ready=false,busy=false,authenticated=false,threadFailed=false;
    std::string status="Starting engine...",thread,turn,response,code,url;
    // Thread the app remembers across launches: set when a prompt is sent in it, cleared for a new conversation.
    std::string conversation;
    std::deque<Approval> approvals;
    std::deque<Message> messages;
    std::uint64_t chatRevision=1;
    std::uint64_t revision=1;
    std::string turnStatus;
    // Model and effort sent with thread/start and each turn/start; the list is fetched on demand.
    std::string model="gpt-6.1-sol",effort="low",modelName="GPT-6.1 Sol";
    std::vector<ModelOption> models;
    bool modelsLoading=false;
    std::uint64_t modelLists=0; // model/list answers received, successful or not
};
class Protocol {
public:
    explicit Protocol(bool gameplay=false):gameplay_(gameplay){}
    State state;
    std::deque<std::string> outgoing;
    // skipResume keeps the saved conversation but starts a new one (its last resume cut the connection).
    void begin(std::string_view resume={},bool skipResume=false);
    void disconnect();
    bool ingest(std::string_view line);
    void login();
    bool submit(std::string_view text,std::string_view imagePath={});
    bool retryThread();
    bool newThread();
    void interrupt();
    bool interruptPending() const {return interruptPending_;}
    bool resuming() const;
    void decide(bool accept);
    void listModels();
    // Only a listed model and one of its efforts, between turns; never in gameplay mode.
    bool selectModel(std::string_view model,std::string_view effort);
    // Restores a saved choice before the list is known; a malformed one is ignored.
    void preferModel(std::string_view model,std::string_view effort,std::string_view name);
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
    bool resumeSkipped_=false;
    std::string history_; // resumed thread whose older turns are still being fetched
    std::size_t historyTurns_=0;
    void flushInterrupt();
    void agentMessage(std::string id,std::string text,bool append);
    void request(const char* method,Json params);
    void emit(Json message);
    void startThread();
    bool threadPending() const;
    void prependTurns(const Json& turns);
    void olderHistory(const std::string& cursor);
};
}
