// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/keyboard.hpp"
#include <algorithm>
#include <cassert>
using namespace assistant::keyboard;
static int count(const Layout& l,std::string_view label){return static_cast<int>(std::count(l.begin(),l.end(),label));}
int main(){
    for(bool isFrench:{false,true}){
        const auto& l=layout(isFrench);
        for(auto label:l)assert(count(l,label)==1);
        for(auto action:{"delete","space","shift","close"})assert(count(l,action)==1);
    }
    auto a=english,b=french;
    std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());
    assert(a==b);
    assert(&layout(false)==&english && &layout(true)==&french);
    assert(english[0]=="q" && french[0]=="a" && french[1]=="z" && french[10]=="q");
    for(unsigned i=0;i<english.size();++i){
        assert(french[follow(i,true)]==english[i]);
        assert(english[follow(i,false)]==french[i]);
    }
    assert(follow(50,true)==0);
}
