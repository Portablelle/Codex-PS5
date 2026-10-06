// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <string_view>
namespace assistant::keyboard {
using Layout=std::array<std::string_view,50>;
constexpr Layout english={
    "q","w","e","r","t","y","u","i","o","p",
    "a","s","d","f","g","h","j","k","l","delete",
    "z","x","c","v","b","n","m",".","_","-",
    "0","1","2","3","4","5","6","7","8","9",
    "é","è","à","ç","?",":","shift","close","space","/"
};
constexpr Layout french={
    "a","z","e","r","t","y","u","i","o","p",
    "q","s","d","f","g","h","j","k","l","m",
    "w","x","c","v","b","n",".","_","-","delete",
    "0","1","2","3","4","5","6","7","8","9",
    "é","è","à","ç","?",":","shift","close","space","/"
};
constexpr const Layout& layout(bool isFrench){return isFrench?french:english;}
// Index of the key labelled like `key` in the layout selected by `toFrench`.
constexpr unsigned follow(unsigned key,bool toFrench){
    const auto& from=layout(!toFrench);const auto& to=layout(toFrench);
    for(unsigned i=0;i<to.size();++i)if(key<from.size() && to[i]==from[key])return i;
    return 0;
}
}
