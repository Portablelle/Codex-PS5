// SPDX-License-Identifier: GPL-3.0-or-later
#include "renderer.hpp"
#include "client.hpp"
#include "voice.hpp"
#include "platform.hpp"
#include "markdown.hpp"
#include "language.hpp"
#include <array>
#include <algorithm>
#include <vector>
extern "C" {
#include "../vendor/ps5-pad.h"
int sceUserServiceGetInitialUser(int*);
int scePadRead(int,PS5_PadData*,int);
}
namespace {
using ps5::demo::Canvas;using ps5::demo::Color;
assistant::Client client;assistant::Voice voice;
std::string composer,composerNotice;int pad=-1;unsigned previous=0,key=0,scroll=0,approvalScroll=0;
bool keyboard=false,approve=false;std::string approvalIdentity;
std::uint64_t uiRevision=0,lastProtocol=0;std::string lastMic;
std::uint64_t nextPadRetry=0;
bool followConversation=true;
unsigned chatScrollMaximum=0;
constexpr Color rgb(unsigned r,unsigned g,unsigned b){return static_cast<Color>(0xff000000|r|(g<<8)|(b<<16));}
constexpr auto bg=rgb(10,15,23),card=rgb(19,29,41),ink=rgb(233,241,250),muted=rgb(152,171,193),accent=rgb(84,231,167);
const std::array<std::string_view,50> keys={"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z","0","1","2","3","4","5","6","7","8","9","/",".","_","-","espace","effacer","é","è","à","ç","?",":","maj","fermer"};
void eraseCodepoint(std::string& s){if(s.empty())return;auto n=s.size()-1;while(n && (static_cast<unsigned char>(s[n])&0xc0)==0x80)--n;s.resize(n);}
void lines(Canvas& c,unsigned x,unsigned y,std::string_view value,unsigned width,unsigned count,unsigned offset=0){
    unsigned row=0,seen=0;
    while(!value.empty() && row<count){
        std::size_t n=0,fit=0;
        while(n<value.size() && value[n]!='\n'){
            ++n;while(n<value.size() && (static_cast<unsigned char>(value[n])&0xc0)==0x80)++n;
            if(c.text_width(value.substr(0,n),24)>width)break;fit=n;
        }
        if(n<value.size() && value[n]=='\n')fit=n;
        if(!fit){if(value.front()=='\n'){value.remove_prefix(1);if(seen++>=offset)++row;continue;}fit=n;}
        if(seen++>=offset){c.label(x,y+row*34,value.substr(0,fit),24,ink);++row;}
        value.remove_prefix(fit);if(!value.empty() && value.front()=='\n')value.remove_prefix(1);
    }
}
struct ChatLine {std::string text;bool user=false,heading=false;std::vector<unsigned char> styles;};
std::vector<ChatLine> chatLines(Canvas& c,const assistant::State& state){
    std::vector<ChatLine> rows;
    const auto add=[&](bool user,std::string_view text){
        rows.push_back({user?assistant::language::tr("Toi"):"Codex",user,true,{}});
        const auto formatted=user?assistant::MarkdownText{}:assistant::markdown(text);
        if(!user)text=formatted.text;
        std::size_t consumed=0;
        const unsigned width=user?1350:1700;
        while(!text.empty()){
            if(text.front()=='\n'){rows.push_back({{},user,false,{}});text.remove_prefix(1);++consumed;continue;}
            std::size_t n=0,fit=0;
            while(n<text.size() && text[n]!='\n'){
                ++n;while(n<text.size() && (static_cast<unsigned char>(text[n])&0xc0)==0x80)++n;
                if(c.text_width(text.substr(0,n),24)>width)break;fit=n;
            }
            if(!fit)fit=n;
            ChatLine row{std::string(text.substr(0,fit)),user,false,{}};
            if(!user)row.styles.assign(formatted.styles.begin()+consumed,formatted.styles.begin()+consumed+fit);
            rows.push_back(std::move(row));text.remove_prefix(fit);consumed+=fit;
            if(!text.empty() && text.front()=='\n'){text.remove_prefix(1);++consumed;}
        }
        rows.push_back({});
    };
    for(const auto& message:state.messages)add(message.user,message.text);
    if(state.messages.empty() && !state.response.empty())add(false,state.response);
    if(state.busy){
        if(state.messages.empty() || state.messages.back().user)add(false,state.status);
        else rows.push_back({state.status,false,true,{}});
    }
    if(rows.empty())add(false,assistant::language::tr("Dicte ou saisis une demande pour agir sur les fichiers de la PS5."));
    return rows;
}
bool draw(Canvas& c) noexcept {
    if(pad<0 && botty::platform::now()>=nextPadRetry){
        int user=-1;
        if(sceUserServiceGetInitialUser(&user)==0)
            pad=scePadOpen(user,PS5_PAD_PORT_TYPE_STANDARD,0,nullptr);
        botty::platform::log(pad>=0?"Controller handle opened":"Controller unavailable - retrying");
        nextPadRetry=botty::platform::now()+2000000;
    }
    std::array<PS5_PadData,64> records{};unsigned pressed=0;
    if(pad>=0){int n=scePadRead(pad,records.data(),64);
        if(n<0){scePadClose(pad);pad=-1;previous=0;}
        for(int i=0;i<n && i<static_cast<int>(records.size());++i){unsigned buttons=records[i].connected?records[i].buttons:0;pressed|=buttons&~previous;previous=buttons;}
    }
    if(c.take_resumed()){pressed=0;approve=false;++uiRevision;}
    auto state=client.snapshot();auto mic=voice.snapshot();
    const auto text=voice.takeTranscript();if(!text.empty()){
        if(composer.size()+text.size()+1<=16384){if(!composer.empty())composer+=' ';composer+=text;composerNotice.clear();}
        else composerNotice=assistant::language::tr("Prompt trop long : la nouvelle dictee n'a pas ete ajoutee");
        keyboard=false;
    }
    if(!state.approvals.empty()){
        const auto identity=state.approvals.front().id.dump();
        if(identity!=approvalIdentity){approvalIdentity=identity;approve=false;pressed=0;approvalScroll=0;}
        if(pressed&PS5_PAD_BUTTON_UP && approvalScroll)--approvalScroll;
        if(pressed&PS5_PAD_BUTTON_DOWN && approvalScroll<8192)++approvalScroll;
        if((pressed&(PS5_PAD_BUTTON_LEFT|PS5_PAD_BUTTON_RIGHT)) && state.approvals.front().reviewable)approve=!approve;
        if(pressed&PS5_PAD_BUTTON_CROSS){client.decide(approve);approvalIdentity.clear();}
        if(pressed&PS5_PAD_BUTTON_CIRCLE){client.decide(false);approvalIdentity.clear();}
    } else {
        approvalIdentity.clear();
        if((pressed&PS5_PAD_BUTTON_R1) && !state.busy && !mic.recording && !mic.working)client.changeLanguage();
        if(pressed&PS5_PAD_BUTTON_L1)client.login();
        if(pressed&PS5_PAD_BUTTON_TRIANGLE)voice.toggle();
        if(pressed&PS5_PAD_BUTTON_CIRCLE){if(mic.recording||mic.working)voice.cancel();else if(state.busy)client.interrupt();else keyboard=false;}
        if(pressed&PS5_PAD_BUTTON_SQUARE)keyboard=!keyboard;
        if((pressed&PS5_PAD_BUTTON_OPTIONS) && !mic.recording && !mic.working && !state.busy && state.ready && !state.thread.empty() && !composer.empty()){
            client.submit(composer);composer.clear();scroll=0;followConversation=true;
        }
        if(keyboard){
            if(pressed&PS5_PAD_BUTTON_LEFT)key=(key+49)%50;
            if(pressed&PS5_PAD_BUTTON_RIGHT)key=(key+1)%50;
            if(pressed&PS5_PAD_BUTTON_UP)key=(key+40)%50;
            if(pressed&PS5_PAD_BUTTON_DOWN)key=(key+10)%50;
            if(pressed&PS5_PAD_BUTTON_CROSS){
                if(key==41)eraseCodepoint(composer);else if(key==49)keyboard=false;
                else if(key==48){if(!composer.empty() && composer.back()>='a'&&composer.back()<='z')composer.back()-=32;}
                else if(composer.size()+keys[key].size()<16384)composer+=(key==40?" ":keys[key]);
            }
        } else {
            if(pressed&PS5_PAD_BUTTON_UP && scroll){--scroll;followConversation=false;}
            if(pressed&PS5_PAD_BUTTON_DOWN && scroll<chatScrollMaximum){++scroll;followConversation=scroll==chatScrollMaximum;}
        }
    }
    if(pressed || !text.empty() || state.revision!=lastProtocol || mic.status!=lastMic){++uiRevision;lastProtocol=state.revision;lastMic=mic.status;}
    if(!c.needs_update(uiRevision))return true;
    c.clear(bg);c.label(70,45,"Codex PS5",44,ink);c.label(70,105,assistant::language::tr(state.status),24,accent);
    if(assistant::language::selectable)c.label(1030,105,assistant::language::tr("R1 : langue (francais / anglais)"),20,muted);
    c.label(1300,60,"GPT-6.1 SOL · LOW",24,muted);
    c.rounded(70,160,1780,keyboard?430:600,20,card);
    if(!state.code.empty()){
        c.label(105,180,assistant::language::tr("Connexion ChatGPT sur ton telephone"),28,ink);
        c.label(105,225,state.url,24,accent);c.label(105,270,state.code,60,ink);
    } else {
        const auto rows=chatLines(c,state);const unsigned visible=keyboard?11:16;
        chatScrollMaximum=rows.size()>visible?static_cast<unsigned>(rows.size()-visible):0;
        if(followConversation)scroll=chatScrollMaximum;else scroll=std::min(scroll,chatScrollMaximum);
        for(unsigned i=0;i<visible && scroll+i<rows.size();++i){
            const auto& row=rows[scroll+i];const unsigned x=row.user?440:105,y=180+i*34;
            if(row.user)c.rounded(x-15,y-2,1380,34,4,rgb(30,66,53));
            if(row.styles.empty())c.label(x,y,row.text,row.heading?20:24,row.heading?accent:ink);
            else {
                unsigned pen=x;
                for(std::size_t start=0;start<row.text.size();){
                    auto end=start+1;while(end<row.text.size() && row.styles[end]==row.styles[start])++end;
                    const auto segment=std::string_view(row.text).substr(start,end-start);
                    const auto width=c.text_width(segment,24);const auto style=row.styles[start];
                    if(style&assistant::code)c.rounded(pen,y-1,width,32,3,rgb(31,44,58));
                    c.label(pen,y,segment,24,ink);
                    if(style&assistant::bold)c.label(pen+1,y,segment,24,ink);
                    pen+=width;start=end;
                }
            }
        }
    }
    const unsigned promptY=keyboard?610:790;
    c.rounded(70,promptY,1780,155,20,card);lines(c,105,promptY+18,composer.empty()?assistant::language::tr("Ton prompt..."):composer,1710,3);
    c.label(75,980,assistant::language::tr(composerNotice.empty()?mic.status:composerNotice),24,accent);
    c.label(75,1020,assistant::language::tr("Triangle : dicter   Carre : clavier   Options : envoyer   L1 : connexion   Rond : annuler"),20,muted);
    if(keyboard){
        for(unsigned i=0;i<keys.size();++i){unsigned x=75+(i%10)*177,y=780+(i/10)*35;
            if(i==key)c.rounded(x-3,y-1,167,32,6,rgb(39,91,71));c.label(x+5,y,assistant::language::tr(keys[i]),20,ink);}
    }
    if(!state.approvals.empty()){
        c.shade(190);c.rounded(190,200,1540,660,24,card);
        c.label(225,225,assistant::language::tr("Codex demande ton autorisation"),32,accent);
        lines(c,225,295,state.approvals.front().description,1470,11,approvalScroll);
        c.label(225,730,approve?assistant::language::tr("  Refuser          [ Autoriser ]"):assistant::language::tr("[ Refuser ]          Autoriser"),32,ink);
        c.label(225,800,assistant::language::tr("Haut/bas : lire. Gauche/droite : choisir. Croix : confirmer. Rond : refuser."),24,muted);
    }
    return true;
}
}
int main(){
    const int userResult=sceUserServiceInitialize(nullptr);
    botty::platform::log(userResult==0?"User service initialized":"User service initialization returned nonzero");
    const int padResult=scePadInit();
    botty::platform::log(padResult==0?"Pad initialized":"Pad initialization returned nonzero");
    assistant::language::initialize(botty::platform::systemLanguage());
    ps5::demo::load_font();client.start();
    ps5::demo::run(draw,"Codex PS5 native frontend");
    voice.shutdown();client.stop();if(pad>=0)scePadClose(pad);
    for(;;)botty::platform::sleep(1000000);
}
