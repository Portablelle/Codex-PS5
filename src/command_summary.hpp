// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace assistant {
// Plain-language summary shown above a command awaiting approval. It only
// helps the user read the command: it never decides anything, and an empty
// result means the command is not recognized. Shell operators make it a
// generic summary so a harmless-looking first word cannot hide what follows.
inline std::string_view commandSummary(std::string_view command,unsigned depth=0) {
    std::vector<std::string> words;
    std::string word;char quote=0;bool started=false,compound=false,writes=false;
    for(std::size_t i=0;i<command.size();++i){
        const char c=command[i];
        if(quote!='\'' && (c=='`' || (c=='$' && i+1<command.size() && command[i+1]=='(')))compound=true;
        if(quote){if(c==quote)quote=0;else word+=c;continue;}
        if(c=='\'' || c=='"'){quote=c;started=true;continue;}
        if(c=='\\' && i+1<command.size()){word+=command[++i];started=true;continue;}
        if(c==';' || c=='|' || c=='&' || c=='\n')compound=true;
        if(c=='>')writes=true;
        if(c==' ' || c=='\t' || c=='\n' || c==';' || c=='|' || c=='&' || c=='>' || c=='<'){
            if(started){words.push_back(word);word.clear();started=false;}
            continue;
        }
        word+=c;started=true;
    }
    if(started)words.push_back(word);
    if(words.empty())return {};
    auto name=std::string_view(words.front());
    if(const auto slash=name.rfind('/');slash!=std::string_view::npos)name.remove_prefix(slash+1);
    // The app-server wraps commands in a shell: describe the wrapped command.
    if((name=="sh" || name=="bash" || name=="dash" || name=="zsh") && words.size()==3 &&
       (words[1]=="-c" || words[1]=="-lc") && !compound && !writes && depth<2)
        return commandSummary(words[2],depth+1);
    if(compound)return "Running several commands";
    if(writes)return "Writing to a file";
    std::size_t operands=0;bool recursive=false,inPlace=false;
    for(std::size_t i=1;i<words.size();++i){
        if(words[i].size()>1 && words[i][0]=='-'){
            if(words[i][1]!='-' && words[i].find_first_of("rR")!=std::string::npos)recursive=true;
            if(words[i].starts_with("-i"))inPlace=true;
        } else ++operands;
    }
    const bool many=operands>2;
    if(name=="mv")return many?"Moving files":"Moving a file";
    if(name=="cp")return recursive?"Copying a folder":many?"Copying files":"Copying a file";
    if(name=="rm")return recursive?"Deleting folders and their contents":operands>1?"Deleting files":"Deleting a file";
    if(name=="rmdir")return "Deleting an empty folder";
    if(name=="mkdir")return "Creating a folder";
    if(name=="touch")return "Creating or updating a file";
    if(name=="ln")return "Creating a link";
    if(name=="chmod")return "Changing file permissions";
    if(name=="chown" || name=="chgrp")return "Changing file ownership";
    if(name=="tar")return "Working with an archive";
    if(name=="dd")return "Copying raw data";
    if(name=="sed")return inPlace?"Editing a file":"Transforming text";
    if(name=="ed")return "Editing a file";
    if(name=="sponge" || name=="tee")return "Writing to a file";
    if(name=="kill")return "Stopping a process";
    if(name=="renice")return "Changing a process priority";
    if(name=="ls")return "Listing files";
    if(name=="find"){
        for(const auto& w:words)if(w=="-delete" || w.starts_with("-exec") || w.starts_with("-ok"))return {};
        return "Searching for files";
    }
    if(name=="grep")return "Searching text in files";
    if(name=="cat" || name=="head" || name=="tail" || name=="wc" || name=="strings")return "Reading a file";
    if(name=="du" || name=="df")return "Checking storage space";
    if(name=="ps")return "Listing processes";
    if(name=="cksum" || name.starts_with("sha"))return "Computing checksums";
    if(name=="sysctl" && command.find('=')!=std::string_view::npos)return {};
    if(name=="sysctl" || name=="uname" || name=="whoami" || name=="ps5-tools")return "Reading system information";
    return {};
}
}
