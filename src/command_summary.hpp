// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../vendor/json.hpp"
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
namespace assistant {
// Plain-language summaries shown above an action awaiting approval. They only
// help the user read the request: they never decide anything. Shell operators
// give a generic summary so a harmless-looking command cannot hide what follows.
std::string_view commandSummary(std::string_view command,unsigned depth=0);
namespace summary_detail {
inline std::string_view simple(std::vector<std::string> w,unsigned depth){
    // Skip variable assignments and wrappers that only change how a command runs.
    // A PATH change means a bare name no longer resolves to the system tool.
    while(!w.empty() && w.front().find('=')!=std::string::npos && w.front().front()!='=' && w.front().front()!='-'){
        if(w.front().starts_with("PATH="))return "Running a program";
        w.erase(w.begin());
    }
    if(w.empty())return "Setting a variable";
    // Only system and bundled runtime directories are trusted to hold the tool a
    // name suggests; any other path (./ls, /data/x/rm) is just some program.
    auto name=w.front();
    if(const auto slash=name.rfind('/');slash!=std::string::npos){
        const auto folder=std::string_view(name).substr(0,slash+1);
        constexpr std::string_view runtime="/data/codex-ps5/runtime/tools-";
        const bool bundled=folder.size()==runtime.size()+33 && folder.starts_with(runtime) &&
            folder.substr(runtime.size(),32).find_first_not_of("0123456789abcdef")==std::string_view::npos;
        if(!bundled && folder!="/bin/" && folder!="/usr/bin/" && folder!="/sbin/" && folder!="/usr/sbin/" && folder!="/usr/local/bin/")
            return "Running a program";
        name.erase(0,slash+1);
    }
    if(name=="env" || name=="nohup" || name=="time" || name=="setsid" || name=="sudo" || name=="nice" || name=="command" || name=="exec"){
        // Only known options are skipped; any other one may take an argument
        // that would otherwise be mistaken for the wrapped command.
        std::size_t i=1;
        for(;i<w.size();++i){
            const auto& o=w[i];
            if(o=="--"){++i;break;}
            if(name=="env" && o.find('=')!=std::string::npos && o.front()!='-'){if(o.starts_with("PATH="))return "Running a program";continue;}
            if(!o.starts_with("-"))break;
            if(name=="env" && o=="-i")continue;
            if((name=="env" && o=="-u") || (name=="nice" && o=="-n")){++i;continue;}
            if(name=="time" && o=="-p")continue;
            return "Running a program";
        }
        if(i>=w.size())return name=="env"?"Reading environment variables":"Running a program";
        return depth<4?simple(std::vector<std::string>(w.begin()+static_cast<std::ptrdiff_t>(i),w.end()),depth+1):"Running a program";
    }
    if(name=="sh" || name=="bash" || name=="dash" || name=="zsh" || name=="ash" || name=="ksh"){
        // -c applies to the first operand after a leading run of plain short options.
        bool command=false;std::size_t i=1;
        for(;i<w.size() && w[i].size()>1 && w[i][0]=='-';++i){
            if(w[i][1]=='-' || w[i].find_first_not_of("abcefhilmnuvx",1)!=std::string::npos)return "Running a program";
            if(w[i].find('c')!=std::string::npos)command=true;
        }
        if(command)return i<w.size() && depth<4?commandSummary(w[i],depth+1):"Running a program";
        return i<w.size()?"Running a shell script":"Opening a shell";
    }
    // Err towards the larger reading: a glob may expand to many files.
    std::size_t operands=0;bool recursive=false,inPlace=false,glob=false;
    for(std::size_t i=1;i<w.size();++i){
        if(w[i].starts_with("--")){
            if(w[i]=="--recursive" || w[i]=="--archive")recursive=true;
            if(w[i].starts_with("--in-place"))inPlace=true;
        } else if(w[i].size()>1 && w[i][0]=='-'){
            if(w[i].find_first_of(name=="cp"?"rRa":"rR")!=std::string::npos)recursive=true;
            if(w[i].find('i')!=std::string::npos)inPlace=true;
        } else {
            ++operands;
            if(w[i].find_first_of("*?[{")!=std::string::npos)glob=true;
        }
    }
    const auto has=[&](std::string_view prefix){for(std::size_t i=1;i<w.size();++i)if(w[i].starts_with(prefix))return true;return false;};
    const bool many=operands>2 || glob;
    if(name=="mv"){
        if(many)return "Moving files";
        // Same folder on both sides is a rename; otherwise the file moves.
        std::vector<std::string_view> paths;for(std::size_t i=1;i<w.size();++i)if(!w[i].starts_with("-"))paths.push_back(w[i]);
        const auto folder=[](std::string_view p){const auto s=p.rfind('/');return s==std::string_view::npos?std::string_view{}:p.substr(0,s);};
        return paths.size()==2 && !paths[1].ends_with("/") && folder(paths[0])==folder(paths[1])?"Renaming a file":"Moving a file";
    }
    if(name=="cp")return recursive?"Copying a folder":many?"Copying files":"Copying a file";
    if(name=="rm")return recursive?"Deleting folders and their contents":operands>1 || glob?"Deleting files":"Deleting a file";
    if(name=="unlink")return "Deleting a file";
    if(name=="rmdir")return "Deleting an empty folder";
    if(name=="mkdir")return "Creating a folder";
    if(name=="touch")return "Creating or updating a file";
    if(name=="ln" || name=="link")return "Creating a link";
    if(name=="mkfifo")return "Creating a named pipe";
    if(name=="mktemp")return "Creating a temporary file";
    if(name=="chmod"){
        if(has("+x") || has("u+x") || has("a+x"))return "Making a file executable";
        return recursive?"Changing permissions of a folder and its contents":"Changing file permissions";
    }
    if(name=="chown" || name=="chgrp")return recursive?"Changing ownership of a folder and its contents":"Changing file ownership";
    if(name=="tar" && w.size()>1){
        if(w[1].find('c')!=std::string::npos)return "Creating an archive";
        if(w[1].find('x')!=std::string::npos)return "Extracting an archive";
        if(w[1].find('t')!=std::string::npos)return "Listing an archive's contents";
    }
    if(name=="tar" || name=="zip" || name=="unzip" || name=="gzip" || name=="gunzip")return "Working with an archive";
    if(name=="dd")return "Copying raw data";
    if(name=="split")return "Splitting a file";
    if(name=="sed"){
        // Scripts can write files (w, W) or run commands (e): look at the flags of a
        // single s command, otherwise at the whole script.
        std::vector<std::string_view> scripts;bool explicitScript=false;
        for(std::size_t i=1;i<w.size();++i){
            if(w[i]=="-f" || w[i].starts_with("--file"))return "Running a script";
            if(w[i]=="-e" && i+1<w.size()){scripts.push_back(w[++i]);explicitScript=true;}
        }
        if(!explicitScript)for(std::size_t i=1;i<w.size();++i)if(!w[i].starts_with("-")){scripts.push_back(w[i]);break;}
        bool write=false,run=false;
        for(auto script:scripts){
            std::string_view tail=script;
            if(script.size()>2 && script[0]=='s' && script.find_first_of(";\n}")==std::string_view::npos){
                const char d=script[1];std::size_t p=2;
                for(int seen=0;p<script.size() && seen<2;++p)if(script[p]==d)++seen;
                tail=script.substr(p);
            }
            if(tail.find_first_of("wW")!=std::string_view::npos)write=true;
            if(tail.find('e')!=std::string_view::npos)run=true;
        }
        if(run)return "Running a program";
        return write?"Writing to a file":inPlace?"Editing a file":"Transforming text";
    }
    if(name=="ed" || name=="apply_patch" || name=="patch")return "Editing a file";
    if(name=="sponge" || name=="tee")return "Writing to a file";
    if(name=="uudecode")return "Decoding a file";
    if(name=="uuencode")return "Encoding a file";
    if(name=="kill" || name=="pkill" || name=="killall")return "Stopping a process";
    if(name=="renice")return "Changing a process priority";
    if(name=="sync")return "Flushing data to disk";
    if(name=="sleep")return "Waiting";
    if(name=="xargs")return "Running a command on each input";
    if(name=="ls" || name=="tree")return "Listing files";
    if(name=="find"){
        if(has("-delete"))return "Finding and deleting files";
        if(has("-exec") || has("-ok"))return "Finding files and running a command on them";
        return "Searching for files";
    }
    if(name=="grep" || name=="egrep" || name=="fgrep" || name=="rg")return "Searching text in files";
    if(name=="cat" || name=="head" || name=="tail" || name=="strings" || name=="od" || name=="nl" || name=="less" || name=="more" || name=="hexdump" || name=="xxd")return "Reading a file";
    if(name=="wc")return "Counting lines and words";
    if(name=="cmp" || name=="comm" || name=="diff")return "Comparing files";
    if(name=="awk" || name=="gawk" || name=="mawk")return "Running a script";
    if(name=="sort" && (has("-o") || has("--output")))return "Writing to a file";
    if(name=="sort" || name=="uniq" || name=="cut" || name=="tr" || name=="rev" || name=="fold" || name=="expand" ||
       name=="unexpand" || name=="paste" || name=="join" || name=="tsort")return "Transforming text";
    if(name=="du" || name=="df")return "Checking storage space";
    if(name=="ps" || name=="top")return "Listing processes";
    if(name=="cksum" || name=="md5sum" || name.starts_with("sha"))return "Computing checksums";
    if(name=="echo" || name=="printf" || name=="seq" || name=="yes")return "Printing text";
    if(name=="test" || name=="[" || name=="true" || name=="false")return "Checking a condition";
    if(name=="expr")return "Computing a value";
    if(name=="basename" || name=="dirname" || name=="pathchk" || name=="readlink" || name=="realpath" || name=="pwd" ||
       name=="which" || name=="stat" || name=="file")return "Inspecting paths";
    if(name=="printenv")return "Reading environment variables";
    if(name=="date")return operands>0 && !w.back().starts_with("+")?"Changing the system date":"Reading the date";
    if(name=="hostname")return operands>0?"Changing the host name":"Reading system information";
    if(name=="sysctl")return has("-w") || std::any_of(w.begin()+1,w.end(),[](const std::string& x){return x.find('=')!=std::string::npos;})?
        "Changing system settings":"Reading system information";
    if(name=="uname" || name=="whoami" || name=="id" || name=="ps5-tools")return "Reading system information";
    if(name=="curl" || name=="wget")return "Downloading data from the network";
    if(name=="ping" || name=="nc" || name=="ssh" || name=="scp" || name=="rsync")return "Connecting over the network";
    if(name=="git")return "Running Git";
    if(name=="python" || name=="python3" || name=="node" || name=="perl" || name=="ruby" || name=="lua")return "Running a script";
    if(name=="make" || name=="cmake" || name=="cc" || name=="gcc" || name=="clang" || name=="c++" || name=="g++" || name=="clang++")return "Building code";
    if(name=="mount" || name=="umount")return "Mounting or unmounting a drive";
    if(name=="reboot" || name=="shutdown" || name=="halt" || name=="poweroff")return "Restarting or shutting down the console";
    return "Running a program";
}
}
inline std::string_view commandSummary(std::string_view command,unsigned depth) {
    // Without escapes, comments or substitutions, plain '…' and "…" quoting is
    // modelled exactly. Otherwise any separator, even one that looks quoted, may
    // start another command: do not try to second-guess the shell.
    const auto contains=[&](std::string_view set){return command.find_first_of(set)!=std::string_view::npos;};
    if(command.find("$(")!=std::string_view::npos || command.find("<(")!=std::string_view::npos ||
       command.find(">(")!=std::string_view::npos || contains("`") || (contains(";|&\n") && contains("\\#$")))
        return "Running several commands";
    // Split into simple commands.
    std::vector<std::vector<std::string>> segments(1);
    std::string word;char quote=0;bool started=false,writes=false,background=false;
    const auto flush=[&]{if(started){segments.back().push_back(word);word.clear();started=false;}};
    for(std::size_t i=0;i<command.size();++i){
        const char c=command[i];
        if(quote){if(c==quote)quote=0;else word+=c;continue;}
        if(c=='\'' || c=='"'){quote=c;started=true;continue;}
        if(c=='\\' && i+1<command.size()){word+=command[++i];started=true;continue;}
        if(c=='>' || c=='<'){
            // A redirection to a real file counts as writing; 2>&1 or /dev/null does not.
            if(started && word.find_first_not_of("0123456789&")==std::string::npos){word.clear();started=false;}
            flush();
            while(i+1<command.size() && (command[i+1]=='>' || command[i+1]=='&'))++i;
            while(i+1<command.size() && (command[i+1]==' ' || command[i+1]=='\t'))++i;
            std::string target;
            while(i+1<command.size() && std::string_view(" \t\n;|&<>").find(command[i+1])==std::string_view::npos)target+=command[++i];
            // The target scan does not model quotes: never guess past one.
            if(target.find_first_of("'\"")!=std::string::npos)return "Running several commands";
            if(c=='>' && target!="/dev/null" && target.find_first_not_of("0123456789-")!=std::string::npos)writes=true;
            continue;
        }
        if(c==';' || c=='|' || c=='&' || c=='\n'){
            flush();
            if(c=='&'){if(i+1<command.size() && command[i+1]=='&')++i;else background=true;}
            else if(c=='|' && i+1<command.size() && command[i+1]=='|')++i;
            if(!segments.back().empty())segments.emplace_back();
            continue;
        }
        if(c==' ' || c=='\t'){flush();continue;}
        word+=c;started=true;
    }
    flush();
    if(quote || std::all_of(segments.begin(),segments.end(),[](const auto& s){return s.empty();}))return {};
    // "cd dir && command" is how Codex usually picks a directory: describe the command.
    std::vector<std::vector<std::string>> work;
    for(auto& s:segments)if(!s.empty() && !(s.front()=="cd" && s.size()<=2))work.push_back(std::move(s));
    if(background || work.size()>1)return "Running several commands";
    if(work.empty())return "Changing directory";
    if(writes)return "Writing to a file";
    return summary_detail::simple(std::move(work.front()),depth);
}
inline std::string_view fileChangeSummary(const nlohmann::json& changes) {
    if(!changes.is_array())return {};
    unsigned added=0,deleted=0,updated=0,moved=0;
    for(const auto& change:changes){
        if(!change.is_object())continue;
        const auto kind=change.find("kind");std::string type;
        if(kind!=change.end() && kind->is_string())type=kind->get<std::string>();
        else if(kind!=change.end() && kind->is_object()){
            if(const auto t=kind->find("type");t!=kind->end() && t->is_string())type=t->get<std::string>();
            for(const char* key:{"move_path","movePath"})if(const auto m=kind->find(key);m!=kind->end() && m->is_string())type="move";
            if(type=="move" && moved==0 && changes.size()==1){
                // A single move within the same folder is a rename.
                const auto folder=[](const std::string& p){const auto s=p.rfind('/');return s==std::string::npos?std::string{}:p.substr(0,s);};
                std::string from,to;
                if(const auto p=change.find("path");p!=change.end() && p->is_string())from=p->get<std::string>();
                for(const char* key:{"move_path","movePath"})if(const auto m=kind->find(key);m!=kind->end() && m->is_string())to=m->get<std::string>();
                if(!from.empty() && folder(from)==folder(to))return "Renaming a file";
            }
        }
        if(type=="add")++added;else if(type=="delete")++deleted;else if(type=="move")++moved;else ++updated;
    }
    const unsigned total=added+deleted+updated+moved;
    if(total==0)return {};
    if(added==total)return total==1?"Creating a file":"Creating files";
    if(deleted==total)return total==1?"Deleting a file":"Deleting files";
    if(moved==total)return total==1?"Moving a file":"Moving files";
    if(updated==total)return total==1?"Editing a file":"Editing files";
    return "Changing several files";
}
}
