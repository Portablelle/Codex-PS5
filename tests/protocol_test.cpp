#include "../src/protocol.hpp"
#include "../src/read_only_command.hpp"
#include "../src/command_summary.hpp"
#include <cassert>
#include "../src/language.hpp"
#include <iostream>
#ifndef EXPECTED_CLIENT_VERSION
#error "tools/test.sh passes the contents of VERSION as EXPECTED_CLIENT_VERSION"
#endif
using namespace assistant;
int main(){
    const std::string runtimeShell="/data/codex-ps5/runtime/tools-0195b42eb983440b31ca75b19881e9ee/sh";
    // Approval summaries describe the command, including the one wrapped by the runtime shell.
    assert(commandSummary("mv /data/notes.txt /data/archives/notes.txt")=="Moving a file");
    assert(commandSummary("mv /data/a.txt /data/b.txt")=="Renaming a file");
    assert(commandSummary(runtimeShell+" -lc 'mv /data/notes.txt /data/archives/'")=="Moving a file");
    assert(commandSummary("/bin/bash -c \"mv a b c\"")=="Moving files");
    assert(commandSummary("rm /data/a.txt")=="Deleting a file" && commandSummary("rm a b")=="Deleting files");
    assert(commandSummary("rm -rf /data/old")=="Deleting folders and their contents");
    assert(commandSummary("cp -R a b")=="Copying a folder" && commandSummary("mkdir -p /data/x")=="Creating a folder");
    assert(commandSummary("sed -i s/a/b/ f")=="Editing a file" && commandSummary("sed s/a/b/ f")=="Transforming text");
    assert(commandSummary("tar -czf out.tgz dir")=="Creating an archive" && commandSummary("tar xf a.tar")=="Extracting an archive");
    assert(commandSummary("chmod +x run.sh")=="Making a file executable" && commandSummary("chown -R me dir")=="Changing ownership of a folder and its contents");
    assert(commandSummary("cd /data && rm old.txt")=="Deleting a file" && commandSummary("cd /data")=="Changing directory");
    assert(commandSummary("env LANG=C nice -n 5 rm x")=="Deleting a file" && commandSummary("FOO=1 python3 x.py")=="Running a script");
    assert(commandSummary("find . -name '*.tmp' -delete")=="Finding and deleting files");
    assert(commandSummary("find . -exec rm {} +")=="Finding files and running a command on them");
    assert(commandSummary("sysctl kern.x=1")=="Changing system settings" && commandSummary("sysctl -n kern.ostype")=="Reading system information");
    assert(commandSummary("date")=="Reading the date" && commandSummary("date 0101120026")=="Changing the system date");
    assert(commandSummary("curl -O https://example.com/x")=="Downloading data from the network");
    assert(commandSummary("grep -r x . 2>/dev/null")=="Searching text in files" && commandSummary("ls 2>&1")=="Listing files");
    assert(commandSummary("./script.sh")=="Running a program" && commandSummary("sh install.sh")=="Running a shell script");
    // A tool name is trusted only from system or bundled runtime directories.
    assert(commandSummary("/bin/rm a")=="Deleting a file" && commandSummary("/data/codex-ps5/runtime/tools-0195b42eb983440b31ca75b19881e9ee/ls")=="Listing files");
    for(auto command:{"/data/codex-ps5/workspace/ls","./ls","bin/ls","/data/codex-ps5/runtime/tools-x/ls","/data/codex-ps5/workspace/sh -c 'ls'"})
        assert(commandSummary(command)=="Running a program");
    // A harmless-looking first command must not hide what follows it.
    for(auto command:{"ls; rm -rf /data","ls && rm a","ls || rm a","cat a | sh","ls $(rm a)","ls `rm a`","ls &","cd /data; ls; rm a"})
        assert(commandSummary(command)=="Running several commands");
    assert(commandSummary(runtimeShell+" -lc 'ls; rm -rf /data'")=="Running several commands");
    assert(commandSummary(runtimeShell+" -lc \"ls $(rm a)\"")=="Running several commands");
    assert(commandSummary("echo hi > /data/a.txt")=="Writing to a file" && commandSummary("echo hi >>/data/a.txt")=="Writing to a file");
    assert(commandSummary("ls ';' 'a|b'")=="Listing files" && commandSummary("ls '$(x)'")=="Running several commands");
    // Escapes, comments and $'...' quoting are not modelled: a separator anywhere then counts.
    for(auto command:{"grep \"a\\\"b\" f; rm -rf /data","ls #'\nrm -rf /data","ls $'a\\'b'; rm -rf /data","cat <(rm a)"})
        assert(commandSummary(command)=="Running several commands");
    assert(commandSummary(runtimeShell+" -lc 'grep \"a\\\"b\" f; rm -rf /data'")=="Running several commands");
    // Globs, long options and combined flags never shrink what a command does.
    assert(commandSummary("rm /data/*")=="Deleting files" && commandSummary("mv *.txt d/")=="Moving files");
    assert(commandSummary("rm --recursive /data")=="Deleting folders and their contents" && commandSummary("cp -a src dst")=="Copying a folder");
    assert(commandSummary("sed -Ei s/a/b/ f")=="Editing a file" && commandSummary("sed --in-place s/a/b/ f")=="Editing a file");
    assert(commandSummary("").empty() && commandSummary("   ").empty() && commandSummary("ls 'open").empty());
    assert(fileChangeSummary(Json::parse(R"([{"path":"/data/a","kind":{"type":"add"}}])"))=="Creating a file");
    assert(fileChangeSummary(Json::parse(R"([{"path":"/data/a","kind":"delete"},{"path":"/data/b","kind":"delete"}])"))=="Deleting files");
    assert(fileChangeSummary(Json::parse(R"([{"path":"/data/a","kind":{"type":"update","move_path":null}}])"))=="Editing a file");
    assert(fileChangeSummary(Json::parse(R"([{"path":"/data/a","kind":{"type":"update","move_path":"/data/b"}}])"))=="Renaming a file");
    assert(fileChangeSummary(Json::parse(R"([{"path":"/data/a","kind":{"type":"update","move_path":"/data/old/a"}}])"))=="Moving a file");
    assert(fileChangeSummary(Json::parse(R"([{"path":"/data/a","kind":{"type":"add"}},{"path":"/data/b","diff":"x"}])"))=="Changing several files");
    assert(fileChangeSummary(Json::array()).empty() && fileChangeSummary(Json()).empty());
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
    assert(Json::parse(p.outgoing.front())["params"]["clientInfo"]["version"]==EXPECTED_CLIENT_VERSION);
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
    // Each kind of request gets its own summary line next to the command's.
    const std::pair<const char*,const char*> contexts[]={
        {R"({"kind":"stdin"})","Sending input to a running command"},
        {R"({"additionalPermissions":{"fileSystem":{"write":["/data"]}}})","Checking storage space\nRequesting additional permissions"},
        {R"({"networkApprovalContext":{"host":"example.com"}})","Checking storage space\nRequesting network access"},
        {R"({"grantRoot":"/data"})","Checking storage space\nRequesting write access to a folder"}};
    for(const auto& [context,summary]:contexts){
        auto params=Json::parse(context);params["command"]=runtimeShell+" -lc 'df -h'";
        assert(p.ingest(Json({{"method","item/commandExecution/requestApproval"},{"id","context"},{"params",params}}).dump()));
        assert(p.state.approvals.size()==1 && p.state.approvals.front().summary==summary);p.decide(false);
    }
    assert(p.ingest(Json({{"method","item/commandExecution/requestApproval"},{"id","wrapped-write"},
                         {"params",{{"command",runtimeShell+" -lc 'rm /data/a'"}}}}).dump()));
    assert(p.state.approvals.size()==1);p.decide(false);
    assert(p.ingest(R"({"method":"item/started","params":{"item":{"type":"commandExecution","id":"read-parent","command":"ls /data"}}})"));
    assert(p.ingest(R"({"method":"item/commandExecution/requestApproval","id":"cached","params":{"itemId":"read-parent"}})"));
    assert(p.state.approvals.size()==1);p.decide(false);
    assert(p.ingest(R"({"method":"item/started","params":{"item":{"type":"commandExecution","id":"parent-command","command":"parent-script"}}})"));
    assert(p.ingest(R"({"method":"item/commandExecution/requestApproval","id":"approval-1","params":{"itemId":"parent-command","command":"mv a b"}})"));
    assert(p.state.approvals.front().description=="mv a b" && p.state.approvals.front().summary=="Renaming a file");
    assert(p.state.approvals.size()==1);
    auto before=p.outgoing.size();p.decide(false);assert(p.outgoing.size()==before+1);
    auto reply=Json::parse(p.outgoing.back());assert(reply["id"]=="approval-1");assert(reply["result"]["decision"]=="decline");
    assert(p.ingest(R"({"method":"item/started","params":{"item":{"type":"fileChange","id":"patch1","changes":[{"path":"/data/note.txt","kind":{"type":"update","move_path":null},"diff":"-old\n+new"}]}}})"));
    assert(p.ingest(R"({"method":"item/fileChange/requestApproval","id":77,"params":{"itemId":"patch1","reason":"edit"}})"));
    assert(p.state.approvals.front().summary=="Editing a file");
    p.decide(true);reply=Json::parse(p.outgoing.back());assert(reply["id"]==77 && reply["result"]["decision"]=="accept");
    assert(p.ingest(R"({"method":"item/fileChange/requestApproval","id":78,"params":{"itemId":"unknown"}})"));
    assert(p.state.approvals.front().summary.empty());
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
    // Quit sequence (Client::stop): decline every pending approval, then interrupt the active turn.
    p.submit("Run a long task.");
    assert(p.ingest(R"({"method":"turn/started","params":{"turn":{"id":"quit-turn"}}})"));
    assert(p.ingest(R"({"method":"item/commandExecution/requestApproval","id":"quit-1","params":{"command":"mv a b"}})"));
    assert(p.ingest(R"({"method":"item/commandExecution/requestApproval","id":"quit-2","params":{"command":"mv c d"}})"));
    assert(p.state.busy && p.state.approvals.size()==2);
    before=p.outgoing.size();
    while(!p.state.approvals.empty())p.decide(false);
    p.interrupt();
    assert(p.outgoing.size()==before+3);
    reply=Json::parse(p.outgoing[before]);assert(reply["id"]=="quit-1" && reply["result"]["decision"]=="decline");
    reply=Json::parse(p.outgoing[before+1]);assert(reply["id"]=="quit-2" && reply["result"]["decision"]=="decline");
    reply=Json::parse(p.outgoing[before+2]);
    assert(reply["method"]=="turn/interrupt" && reply["params"]["turnId"]=="quit-turn");
    // Once the turn has ended there is nothing to interrupt, so quitting queues nothing.
    assert(p.ingest(R"({"method":"turn/completed","params":{"turn":{"status":"interrupted"}}})"));
    before=p.outgoing.size();p.interrupt();assert(p.outgoing.size()==before);
    // An interrupt asked before the turn id is known is deferred until it arrives.
    p.submit("Another task.");before=p.outgoing.size();
    p.interrupt();
    assert(p.interruptPending() && p.outgoing.size()==before);
    assert(p.ingest(R"({"method":"turn/started","params":{"turn":{"id":"late-turn"}}})"));
    assert(!p.interruptPending() && p.outgoing.size()==before+1);
    reply=Json::parse(p.outgoing[before]);
    assert(reply["method"]=="turn/interrupt" && reply["params"]["turnId"]=="late-turn");
    assert(p.ingest(R"({"method":"turn/completed","params":{"turn":{"status":"interrupted"}}})"));
    // A deferred interrupt must not leak into a later turn.
    p.submit("Third task.");p.interrupt();assert(p.interruptPending());
    assert(p.ingest(R"({"method":"turn/completed","params":{"turn":{"status":"failed"}}})"));
    assert(!p.interruptPending());
    p.submit("Fourth task.");before=p.outgoing.size();
    assert(p.ingest(R"({"method":"turn/started","params":{"turn":{"id":"turn-4"}}})"));
    assert(p.outgoing.size()==before);
    assert(p.ingest(R"({"method":"turn/completed","params":{"turn":{"status":"completed"}}})"));
    assert(!p.ingest("[1,2]"));assert(!p.ingest("not json"));
    assert(!p.ingest(R"({"id":9223372036854775807,"result":{}})"));
    assert(p.state.authenticated);
    p.disconnect();assert(p.outgoing.empty() && p.state.thread.empty() && !p.state.authenticated);
    std::cout<<"Protocol: handshake, UTF-8, approvals, errors and disconnect passed\n";
}
