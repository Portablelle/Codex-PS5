// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace assistant {
// Deliberately accept only a single literal command. This is not a shell parser:
// expansions, escapes and operators must go through manual approval. The
// app-server's bundled shell wrapper is inspected as one additional layer.
inline bool readOnlyCommand(std::string_view command, bool allowRuntimeShell=true) {
    if(command.empty() || command.size()>8192)return false;
    std::vector<std::string> words;
    std::string word;char quote=0;bool started=false;
    for(unsigned char c:command){
        if(c=='\'' || c=='"'){
            if(!quote)quote=c;
            else if(quote==c)quote=0;
            else word+=c;
            started=true;continue;
        }
        if((c==' ' || c=='\t') && !quote){
            if(started){words.push_back(word);word.clear();started=false;}
            continue;
        }
        // Even inside quotes reject shell syntax, control bytes and globbing.
        if(c<32 || c==127 || std::string_view("\\$`;|&<>(){}[]*?!~=").find(c)!=std::string_view::npos)return false;
        word+=c;started=true;
    }
    if(quote)return false;
    if(started)words.push_back(word);
    if(words.empty())return false;
    const auto& name=words.front();
    constexpr std::string_view runtimePrefix="/data/codex-ps5/runtime/tools-";
    if(allowRuntimeShell && name.starts_with(runtimePrefix) && name.ends_with("/sh") &&
       name.size()==runtimePrefix.size()+32+3 && words.size()==3 &&
       (words[1]=="-c" || words[1]=="-lc")){
        const auto identity=std::string_view(name).substr(runtimePrefix.size(),32);
        if(identity.find_first_not_of("0123456789abcdef")!=std::string_view::npos)return false;
        // Reparse the decoded shell argument: quoting must not turn a script or
        // another wrapper into an automatically accepted inspection command.
        return readOnlyCommand(words[2],false);
    }
    // Bare names resolve through the bundled runtime PATH. Arbitrary executable
    // paths (including a different binary named cat) are never trusted here.
    constexpr std::string_view readers[]={"ls","cat","head","tail","wc","grep","du","df","ps",
        "pwd","uname","whoami","readlink","ps5-tools"};
    for(auto reader:readers)if(name==reader)return true;
    if(name!="sysctl" || words.size()<2)return false;
    bool property=false;
    for(std::size_t i=1;i<words.size();++i){
        if(words[i]=="-n" || words[i]=="-b")continue;
        if(words[i].empty() || words[i][0]=='-')return false;
        for(unsigned char c:words[i])
            if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='.' || c=='_'))return false;
        property=true;
    }
    return property;
}
}
