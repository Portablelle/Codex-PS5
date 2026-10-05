#include "../src/protocol.hpp"
#include "../src/read_only_command.hpp"
#include <cassert>
#include "../src/language.hpp"
#include <iostream>
using namespace assistant;
int main(){
    const std::string runtimeShell="/data/codex-ps5/runtime/tools-0195b42eb983440b31ca75b19881e9ee/sh";
    for(auto command:{"df", "'df -h'", "\"df -h /data\"", "\"cat '/data/my notes.txt'\""})
        assert(readOnlyCommand(runtimeShell+" -lc "+command));
    assert(readOnlyCommand(runtimeShell+" -c 'df -h'"));
    for(auto command:{"'rm /data/a'", "'df; rm /data/a'", "'df && rm /data/a'",
                      "'df | tee /data/a'", "'df > /data/a'", "'df $(touch /data/a)'",
                      "'sh -c df'", "'df' extra", "\"cat 'unfinished\""})
        assert(!readOnlyCommand(runtimeShell+" -lc "+command));
    for(auto shell:{"sh", "/bin/sh", "/data/sh",
                    "/data/codex-ps5/runtime/tools-invalid/sh",
                    "/data/codex-ps5/runtime/tools-0195b42eb983440b31ca75b19881e9eg/sh",
                    "/data/codex-ps5/runtime/tools-0195b42eb983440b31ca75b19881e9ee/../sh"})
        assert(!readOnlyCommand(std::string(shell)+" -lc 'df -h'"));
    assert(!readOnlyCommand(runtimeShell+" -lc '"+runtimeShell+" -lc df'"));
    assert(!readOnlyCommand(runtimeShell+" -x 'df'"));
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
    language::initialize(2);
    Protocol p;p.begin();assert(p.outgoing.size()==1);
    assert(p.ingest(R"({"id":1,"result":{}})"));assert(p.state.ready);
    assert(p.outgoing.size()==3);
    assert(p.ingest(R"({"id":2,"result":{"account":{"type":"chatgpt"}}})"));
    assert(p.outgoing.size()==4);
    auto threadRequest=Json::parse(p.outgoing.back())["params"];
    assert(threadRequest["model"]=="gpt-6.1-sol" && threadRequest["config"]["model_reasoning_effort"]=="low");
    assert(threadRequest["allowProviderModelFallback"]==false);
    assert(p.ingest(R"({"id":3,"result":{"thread":{"id":"test-thread"}}})"));
    p.submit("Move the file");assert(p.state.busy);
    auto turnRequest=Json::parse(p.outgoing.back())["params"];
    assert(turnRequest["model"]=="gpt-6.1-sol" && turnRequest["effort"]=="low");
    assert(p.state.messages.size()==1 && p.state.messages.front().user && p.state.messages.front().text=="Move the file");
    auto autoBefore=p.outgoing.size();
    assert(p.ingest(R"({"method":"item/commandExecution/requestApproval","id":"read-1","params":{"command":"df -h"}})"));
    assert(p.state.approvals.empty() && p.outgoing.size()==autoBefore+1);
    auto autoReply=Json::parse(p.outgoing.back());
    assert(autoReply["id"]=="read-1" && autoReply["result"]["decision"]=="accept");
    for(auto command:{"df", "'df -h'"}){
        const auto count=p.outgoing.size();
        assert(p.ingest(Json({{"method","item/commandExecution/requestApproval"},{"id","wrapped-read"},
                             {"params",{{"command",runtimeShell+" -lc "+command},
                                        {"cwd","/data/codex-ps5/workspace"}}}}).dump()));
        assert(p.state.approvals.empty() && p.outgoing.size()==count+1);
        const auto result=Json::parse(p.outgoing.back());
        assert(result["id"]=="wrapped-read" && result["result"]["decision"]=="accept");
    }
    for(auto context:{R"({"kind":"stdin"})", R"({"additionalPermissions":{"fileSystem":{"write":["/data"]}}})",
                      R"({"networkApprovalContext":{"host":"example.com"}})", R"({"grantRoot":"/data"})"}){
        auto params=Json::parse(context);params["command"]=runtimeShell+" -lc 'df -h'";
        assert(p.ingest(Json({{"method","item/commandExecution/requestApproval"},{"id","context"},{"params",params}}).dump()));
        assert(p.state.approvals.size()==1);p.decide(false);
    }
    assert(p.ingest(Json({{"method","item/commandExecution/requestApproval"},{"id","wrapped-write"},
                         {"params",{{"command",runtimeShell+" -lc 'rm /data/a'"}}}}).dump()));
    assert(p.state.approvals.size()==1);p.decide(false);
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
    assert(p.ingest(R"({"method":"item/agentMessage/delta","params":{"delta":"Created — done."}})"));assert(p.state.response=="Created — done.");
    assert(p.ingest(R"({"method":"turn/completed","params":{"turn":{"status":"failed","error":{"message":"permission denied"}}}})"));
    assert(!p.state.busy && p.state.status.find("permission denied")!=std::string::npos);
    assert(p.state.messages.size()==2 && p.state.messages.back().text=="Created — done.");
    p.submit("What about the external drive?");
    assert(p.state.messages.size()==3 && p.state.messages[1].text=="Created — done.");
    assert(p.ingest(R"({"method":"item/agentMessage/delta","params":{"itemId":"reply2","delta":"There is "}})"));
    assert(p.ingest(R"({"method":"item/agentMessage/delta","params":{"itemId":"reply2","delta":"575 GiB left."}})"));
    assert(p.state.messages.size()==4 && p.state.messages.back().text=="There is 575 GiB left.");
    assert(p.ingest(R"({"method":"item/completed","params":{"item":{"id":"reply2","type":"agentMessage","text":"There is 575.5 GiB left."}}})"));
    assert(p.state.messages.size()==4 && p.state.messages.back().text=="There is 575.5 GiB left.");
    assert(p.ingest(R"({"method":"turn/completed","params":{"turn":{"status":"completed"}}})"));
    assert(!p.ingest("[1,2]"));assert(!p.ingest("not json"));
    assert(!p.ingest(R"({"id":9223372036854775807,"result":{}})"));
    p.disconnect();assert(p.outgoing.empty() && p.state.thread.empty());
    std::cout<<"Protocol: handshake, UTF-8, approvals, errors and disconnect passed\n";
}
