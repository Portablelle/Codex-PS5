// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace assistant {
enum MarkdownStyle : unsigned char {plain=0,bold=1,code=2};
struct MarkdownText {
    std::string text;
    std::vector<unsigned char> styles;
    void append(std::string_view value,unsigned char style=plain){
        text.append(value);styles.insert(styles.end(),value.size(),style);
    }
};
inline void markdownInline(MarkdownText& out,std::string_view input,unsigned char style=plain,unsigned depth=0){
    for(std::size_t i=0;i<input.size();){
        if(input[i]=='\\' && i+1<input.size() && std::string_view("\\`*_{}[]()#+-.!>").find(input[i+1])!=std::string_view::npos){
            out.append(input.substr(i+1,1),style);i+=2;continue;
        }
        if(input[i]=='`'){
            std::size_t count=1;while(i+count<input.size() && input[i+count]=='`')++count;
            auto end=input.find(input.substr(i,count),i+count);
            if(end!=std::string_view::npos){out.append(input.substr(i+count,end-i-count),code);i=end+count;continue;}
        }
        if(depth<8 && input[i]=='['){
            auto end=input.find("](",i+1);
            auto close=end==std::string_view::npos?end:input.find(')',end+2);
            if(close!=std::string_view::npos){
                markdownInline(out,input.substr(i+1,end-i-1),style,depth+1);
                out.append(" (");out.append(input.substr(end+2,close-end-2));out.append(")");i=close+1;continue;
            }
        }
        // Do not interpret underscores inside paths or words as emphasis.
        if(depth<8 && (input[i]=='*' || (input[i]=='_' && (i==0 || input[i-1]==' ')))){
            const auto count=i+1<input.size() && input[i+1]==input[i]?2U:1U;
            auto end=input.find(input.substr(i,count),i+count);
            if(end!=std::string_view::npos && end>i+count && input[i+count]!=' ' && input[end-1]!=' '){
                markdownInline(out,input.substr(i+count,end-i-count),style|(count==2?bold:plain),depth+1);
                i=end+count;continue;
            }
        }
        out.append(input.substr(i,1),style);++i;
    }
}
inline MarkdownText markdown(std::string_view input){
    MarkdownText out;bool fenced=false;std::string fence;
    while(!input.empty()){
        const auto end=input.find('\n');auto line=input.substr(0,end);
        if(!line.empty() && line.back()=='\r')line.remove_suffix(1);
        auto content=line;const auto indent=content.find_first_not_of(' ');
        if(indent!=std::string_view::npos && indent<=3)content.remove_prefix(indent);
        if(content.starts_with("```") || content.starts_with("~~~")){
            if(!fenced){fenced=true;fence=std::string(content.substr(0,3));}
            else if(content.starts_with(fence)){fenced=false;fence.clear();}
            else out.append(line,code);
        } else if(fenced){out.append(line,code);out.append("\n",code);}
        else {
            unsigned char style=plain;std::size_t hashes=0;
            while(hashes<content.size() && content[hashes]=='#')++hashes;
            if(hashes && hashes<=6 && hashes<content.size() && content[hashes]==' '){content.remove_prefix(hashes+1);style=bold;}
            else content=line;
            if(content.starts_with("- ") || content.starts_with("* ") || content.starts_with("+ ")){out.append("– ");content.remove_prefix(2);}
            else if(content.starts_with("> ")){out.append("│ ");content.remove_prefix(2);}
            markdownInline(out,content,style);out.append("\n");
        }
        if(end==std::string_view::npos)break;input.remove_prefix(end+1);
    }
    if(!out.text.empty() && out.text.back()=='\n'){out.text.pop_back();out.styles.pop_back();}
    return out;
}
}
