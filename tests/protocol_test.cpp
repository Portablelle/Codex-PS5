#include "../src/protocol.hpp"
#include "../src/read_only_command.hpp"
#include <cassert>
#include <iostream>
using namespace assistant;
int main(){
    for(auto command:{"ls -la /data", "cat '/data/my notes.txt'", "head -n 20 /data/log.txt",
                      "tail -n 10 /data/log.txt", "wc -l /data/log.txt", "grep error /data/log.txt",
                      "du -s /data", "df -h", "ps -A", "pwd", "uname -a", "whoami",
                      "readlink /data/link", "ps5-tools", "sysctl -n kern.hostname", "sysctl -b kern.proc"})
        assert(readOnlyCommand(command));
    for(auto command:{"", "rm /data/a", "mv a b", "chmod 777 a", "find /data -delete",
                      "sed -i s/a/b/ a", "sort -o a a", "tee a", "sh -c 'ls'", "env ls",
                      "/data/cat a", "./ls", "PATH=/data ls", "ls; rm a", "ls && rm a",
                      "ls | tee a", "cat a > b", "cat a 2>>b", "cat <a", "ls\nrm a",
                      "cat $(touch a)", "cat `touch a`", "ls $HOME", "ls *", "ls \\\nrm a",
                      "cat 'unfinished", "sysctl kern.hostname=x", "sysctl -w kern.hostname",
                      "sysctl", "sysctl -n", "cat /data/a\r"})
        assert(!readOnlyCommand(command));
    Protocol p;p.begin();assert(p.outgoing.size()==1);
    assert(p.ingest(R"({"id":1,"result":{}})"));assert(p.state.ready);
    assert(p.outgoing.size()==3);
    assert(p.ingest(R"({"id":2,"result":{"account":{"type":"chatgpt"}}})"));
    assert(p.outgoing.size()==4);
    auto threadRequest=Json::parse(p.outgoing.back())["params"];
    assert(threadRequest["model"]=="gpt-6.1-sol" && threadRequest["config"]["model_reasoning_effort"]=="low");
    assert(threadRequest["allowProviderModelFallback"]==false);
    assert(p.ingest(R"({"id":3,"result":{"thread":{"id":"test-thread"}}})"));
    p.submit("Déplace le fichier");assert(p.state.busy);
    auto turnRequest=Json::parse(p.outgoing.back())["params"];
    assert(turnRequest["model"]=="gpt-6.1-sol" && turnRequest["effort"]=="low");
    assert(p.state.messages.size()==1 && p.state.messages.front().user && p.state.messages.front().text=="Déplace le fichier");
    auto autoBefore=p.outgoing.size();
    assert(p.ingest(R"({"method":"item/commandExecution/requestApproval","id":"read-1","params":{"command":"df -h"}})"));
    assert(p.state.approvals.empty() && p.outgoing.size()==autoBefore+1);
    auto autoReply=Json::parse(p.outgoing.back());
    assert(autoReply["id"]=="read-1" && autoReply["result"]["decision"]=="accept");
    for(auto context:{R"({"kind":"stdin"})", R"({"additionalPermissions":{"fileSystem":{"write":["/data"]}}})",
                      R"({"networkApprovalContext":{"host":"example.com"}})", R"({"grantRoot":"/data"})"}){
        auto params=Json::parse(context);params["command"]="cat /data/note.txt";
        assert(p.ingest(Json({{"method","item/commandExecution/requestApproval"},{"id","context"},{"params",params}}).dump()));
        assert(p.state.approvals.size()==1);p.decide(false);
    }
    assert(p.ingest(R"({"method":"item/started","params":{"item":{"type":"commandExecution","id":"read-parent","command":"ls /data"}}})"));
    assert(p.ingest(R"({"method":"item/commandExecution/requestApproval","id":"cached","params":{"itemId":"read-parent"}})"));
    assert(p.state.approvals.size()==1);p.decide(false);
    assert(p.ingest(R"({"method":"item/started","params":{"item":{"type":"commandExecution","id":"parent-command","command":"parent-script"}}})"));
    assert(p.ingest(R"({"method":"item/commandExecution/requestApproval","id":"approval-1","params":{"itemId":"parent-command","command":"mv a b"}})"));
    assert(p.state.approvals.front().description=="mv a b");
    assert(p.state.approvals.size()==1);
    auto before=p.outgoing.size();p.decide(false);assert(p.outgoing.size()==before+1);
    auto reply=Json::parse(p.outgoing.back());assert(reply["id"]=="approval-1");assert(reply["result"]["decision"]=="decline");
    assert(p.ingest(R"({"method":"item/started","params":{"item":{"type":"fileChange","id":"patch1","changes":[{"path":"/data/note.txt","diff":"-old\n+new"}]}}})"));
    assert(p.ingest(R"({"method":"item/fileChange/requestApproval","id":77,"params":{"itemId":"patch1","reason":"edit"}})"));
    p.decide(true);reply=Json::parse(p.outgoing.back());assert(reply["id"]==77 && reply["result"]["decision"]=="accept");
    assert(p.ingest(R"({"method":"item/fileChange/requestApproval","id":78,"params":{"itemId":"unknown"}})"));
    p.decide(true);reply=Json::parse(p.outgoing.back());assert(reply["result"]["decision"]=="decline");
    assert(p.ingest(R"({"method":"item/agentMessage/delta","params":{"delta":"Créé."}})"));assert(p.state.response=="Créé.");
    assert(p.ingest(R"({"method":"turn/completed","params":{"turn":{"status":"failed","error":{"message":"permission denied"}}}})"));
    assert(!p.state.busy && p.state.status.find("permission denied")!=std::string::npos);
    assert(p.state.messages.size()==2 && p.state.messages.back().text=="Créé.");
    p.submit("Et le disque externe ?");
    assert(p.state.messages.size()==3 && p.state.messages[1].text=="Créé.");
    assert(p.ingest(R"({"method":"item/agentMessage/delta","params":{"itemId":"reply2","delta":"Il reste "}})"));
    assert(p.ingest(R"({"method":"item/agentMessage/delta","params":{"itemId":"reply2","delta":"575 Gio."}})"));
    assert(p.state.messages.size()==4 && p.state.messages.back().text=="Il reste 575 Gio.");
    assert(p.ingest(R"({"method":"item/completed","params":{"item":{"id":"reply2","type":"agentMessage","text":"Il reste 575,5 Gio."}}})"));
    assert(p.state.messages.size()==4 && p.state.messages.back().text=="Il reste 575,5 Gio.");
    assert(p.ingest(R"({"method":"turn/completed","params":{"turn":{"status":"completed"}}})"));
    assert(!p.ingest("[1,2]"));assert(!p.ingest("not json"));
    assert(!p.ingest(R"({"id":9223372036854775807,"result":{}})"));
    p.disconnect();assert(p.outgoing.empty() && p.state.thread.empty());
    std::cout<<"Protocol: handshake, UTF-8, approvals, errors and disconnect passed\n";
}
