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
bool quitting=false,confirmQuit=false;
bool keyboard=false,approve=false;std::string approvalIdentity;
std::uint64_t uiRevision=0,lastProtocol=0;std::string lastMic;
std::uint64_t nextPadRetry=0;
bool followConversation=true;
unsigned chatScrollMaximum=0;
constexpr Color rgb(unsigned r,unsigned g,unsigned b){return static_cast<Color>(0xff000000|r|(g<<8)|(b<<16));}
// Palette from the Paper redesign: instrument black surfaces with one amber accent.
constexpr auto bg=rgb(11,11,12),surface=rgb(21,21,23),raised=rgb(28,28,31),well=rgb(35,35,38),edge=rgb(42,42,46);
constexpr auto ink=rgb(242,239,233),soft=rgb(185,181,174),muted=rgb(142,139,133),dim=rgb(94,91,87);
constexpr auto accent=rgb(255,176,32),codeInk=rgb(255,207,112),danger=rgb(229,72,77);
constexpr unsigned left=96,right=1824,span=right-left;
const std::array<std::string_view,50> keys={"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z","0","1","2","3","4","5","6","7","8","9","/",".","_","-","space","delete","é","è","à","ç","?",":","shift","close"};
void eraseCodepoint(std::string& s){if(s.empty())return;auto n=s.size()-1;while(n && (static_cast<unsigned char>(s[n])&0xc0)==0x80)--n;s.resize(n);}
std::size_t codepoint(std::string_view s){std::size_t n=1;while(n<s.size() && (static_cast<unsigned char>(s[n])&0xc0)==0x80)++n;return n;}
// Letter-spaced small caps labels.
unsigned trackedWidth(Canvas& c,std::string_view s,unsigned size,unsigned gap){
    unsigned width=0;while(!s.empty()){const auto n=codepoint(s);width+=c.text_width(s.substr(0,n),size)+gap;s.remove_prefix(n);}
    return width?width-gap:0;
}
void tracked(Canvas& c,unsigned x,unsigned y,std::string_view s,unsigned size,Color color,unsigned gap){
    while(!s.empty()){const auto n=codepoint(s);c.label(x,y,s.substr(0,n),size,color);x+=c.text_width(s.substr(0,n),size)+gap;s.remove_prefix(n);}
}
// Word-wrapped rows; paragraph counts the newlines before the row.
struct Wrapped {std::size_t start=0,length=0;unsigned paragraph=0;bool first=true;};
std::vector<Wrapped> wrap(Canvas& c,std::string_view text,unsigned width,unsigned size){
    std::vector<Wrapped> rows;std::size_t at=0;unsigned paragraph=0;bool first=true;
    while(at<text.size()){
        if(text[at]=='\n'){if(first)rows.push_back({at,0,paragraph,true});++at;++paragraph;first=true;continue;}
        std::size_t n=at,fit=at,space=at;
        while(n<text.size() && text[n]!='\n'){
            if(text[n]==' ')space=n;
            n+=codepoint(text.substr(n));
            if(c.text_width(text.substr(at,n-at),size)>width){if(space>at)fit=space+1;break;}
            fit=n;
        }
        if(fit==at)fit=n;
        rows.push_back({at,fit-at,paragraph,first});first=false;at=fit;
    }
    return rows;
}
struct ChatRow {
    enum Kind:unsigned char {gap,pad,label,body} kind=gap;
    bool user=false,latest=false,status=false,bullet=false,indent=false;
    unsigned message=0,height=0;std::string text;std::vector<unsigned char> styles;
};
constexpr unsigned userWidth=1000,codexWidth=1400;
std::vector<ChatRow> chatRows(Canvas& c,const assistant::State& state){
    std::vector<ChatRow> rows;unsigned message=0;
    std::size_t latest=state.messages.size();
    for(std::size_t i=0;i<state.messages.size();++i)if(!state.messages[i].user)latest=i;
    const bool trailingStatus=state.busy && (state.messages.empty() || state.messages.back().user);
    const auto add=[&](bool user,std::string_view text,bool isLatest,bool status){
        ++message;
        if(!rows.empty())rows.push_back({ChatRow::gap,false,false,false,false,false,message,48,{},{}});
        if(user)rows.push_back({ChatRow::pad,true,false,false,false,false,message,20,{},{}});
        else rows.push_back({ChatRow::label,false,isLatest,false,false,false,message,40,{},{}});
        assistant::MarkdownText formatted;
        if(user || status)formatted.append(text);else formatted=assistant::markdown(text);
        const std::string_view all=formatted.text;std::size_t offset=0;
        while(true){
            auto end=all.find('\n',offset);if(end==std::string_view::npos)end=all.size();
            auto paragraph=all.substr(offset,end-offset);std::size_t base=offset;
            const bool bullet=!user && !status && paragraph.starts_with("– ") && formatted.styles[base]==assistant::plain;
            if(bullet){paragraph.remove_prefix(4);base+=4;}
            if(paragraph.empty())rows.push_back({ChatRow::body,user,isLatest,status,false,false,message,16,{},{}});
            for(const auto& piece:wrap(c,paragraph,(user?userWidth:codexWidth)-(bullet?28:0),28)){
                ChatRow row{ChatRow::body,user,isLatest,status,bullet && piece.first,bullet,message,42,std::string(paragraph.substr(piece.start,piece.length)),{}};
                if(!user)row.styles.assign(formatted.styles.begin()+base+piece.start,formatted.styles.begin()+base+piece.start+piece.length);
                rows.push_back(std::move(row));
            }
            if(end==all.size())break;offset=end+1;
        }
        if(user)rows.push_back({ChatRow::pad,true,false,false,false,false,message,22,{},{}});
    };
    for(std::size_t i=0;i<state.messages.size();++i)
        add(state.messages[i].user,state.messages[i].text,i==latest && !trailingStatus,false);
    if(state.messages.empty() && !state.response.empty() && !state.busy)add(false,state.response,true,false);
    if(trailingStatus)add(false,assistant::language::tr(state.status),true,true);
    else if(state.busy){
        rows.push_back({ChatRow::body,false,true,true,false,false,message,16,{},{}});
        rows.push_back({ChatRow::body,false,true,true,false,false,message,42,assistant::language::tr(state.status),{}});
    }
    if(rows.empty())add(false,assistant::language::tr("Dictate or type a request to work on PS5 files."),true,false);
    return rows;
}
bool failing(std::string_view status){
    return status.starts_with("Error") || status.starts_with("Failed") || status.starts_with("Sign-in failed") || status.starts_with("Engine");
}
void header(Canvas& c,const assistant::State& state){
    c.rounded(left,58,44,44,12,accent);c.label(left+(44-c.text_width("C",24))/2,62,"C",24,bg);
    c.label(160,56,"Codex",32,ink);tracked(c,172+c.text_width("Codex",32),68,"PS5",20,muted,3);
    constexpr std::string_view model="GPT-6.1 SOL · LOW";
    const unsigned modelX=right-trackedWidth(c,model,20,2);tracked(c,modelX,68,model,20,muted,2);
    c.rectangle(modelX-33,66,1,28,edge);
    const auto status=assistant::language::tr(state.status);
    const unsigned statusX=modelX-65-std::min(c.text_width(status,24),1100U);
    c.circle(statusX-17,81,5,failing(state.status)?danger:accent);c.label(statusX,64,status,24,ink);
}
void hint(Canvas& c,unsigned& x,unsigned y,std::string_view glyph,std::string_view text,Color color){
    if(!glyph.empty()){c.label(x,y,glyph,24,color);x+=c.text_width(glyph,24)+12;}
    c.label(x,y,text,24,color);x+=c.text_width(text,24)+36;
}
unsigned hintWidth(Canvas& c,std::string_view glyph,std::string_view text){
    return (glyph.empty()?0:c.text_width(glyph,24)+12)+c.text_width(text,24);
}
void touchpad(Canvas& c,unsigned x,unsigned y,Color color){c.rounded(x,y,38,24,7,color);c.rounded(x+2,y+2,34,20,5,bg);}
// A filled pill button; focused buttons invert to the ink colour.
void button(Canvas& c,unsigned x,unsigned y,std::string_view text,bool focused,Color idleInk=soft,Color focusFill=ink,Color focusInk=bg){
    c.rounded(x,y,240,72,20,focused?focusFill:well);
    c.label(x+(240-c.text_width(text,28))/2,y+17,text,28,focused?focusInk:idleInk);
}
void conversation(Canvas& c,const assistant::State& state,unsigned top,unsigned bottom){
    const auto rows=chatRows(c,state);
    unsigned fitted=0;std::size_t first=rows.size();
    while(first>0 && fitted+rows[first-1].height<=bottom-top)fitted+=rows[--first].height;
    chatScrollMaximum=static_cast<unsigned>(first);
    if(followConversation)scroll=chatScrollMaximum;else scroll=std::min(scroll,chatScrollMaximum);
    std::vector<unsigned> ys;unsigned y=top;
    for(std::size_t i=scroll;i<rows.size() && y+rows[i].height<=bottom;++i){ys.push_back(y);y+=rows[i].height;}
    // Backgrounds first: user bubbles and the accent rule beside the latest reply.
    std::vector<unsigned> widths(rows.empty()?1:rows.back().message+1,0);
    for(const auto& row:rows)if(row.user && row.kind==ChatRow::body)widths[row.message]=std::max(widths[row.message],c.text_width(row.text,28));
    for(std::size_t i=0;i<ys.size();){
        const auto& row=rows[scroll+i];
        if(row.kind==ChatRow::gap || !(row.user || row.latest)){++i;continue;}
        std::size_t j=i;unsigned end=ys[i];
        while(j<ys.size() && rows[scroll+j].message==row.message && rows[scroll+j].kind!=ChatRow::gap){end=ys[j]+rows[scroll+j].height;++j;}
        if(row.user){const unsigned width=widths[row.message]+64;c.rounded(right-width,ys[i],width,end-ys[i],24,raised);}
        else c.rounded(left,ys[i],4,end-ys[i],2,accent);
        i=j;
    }
    for(std::size_t i=0;i<ys.size();++i){
        const auto& row=rows[scroll+i];const unsigned rowY=ys[i];
        if(row.kind==ChatRow::label){tracked(c,128,rowY,"CODEX",20,row.latest?accent:muted,2);continue;}
        if(row.kind!=ChatRow::body || row.text.empty())continue;
        const unsigned x=row.user?right-widths[row.message]-32:128+(row.indent?28:0);
        const Color color=row.status?muted:row.user||row.latest?ink:soft;
        if(row.bullet)c.circle(x-20,rowY+22,4,muted);
        if(row.styles.empty()){c.label(x,rowY+4,row.text,28,color);continue;}
        unsigned pen=x;
        for(std::size_t start=0;start<row.text.size();){
            auto end=start+1;while(end<row.text.size() && row.styles[end]==row.styles[start])++end;
            const auto segment=std::string_view(row.text).substr(start,end-start);
            const auto width=c.text_width(segment,28);const auto style=row.styles[start];
            if(style&assistant::code)c.rounded(pen-2,rowY+3,width+4,36,8,well);
            const Color segmentColor=style&assistant::code?codeInk:color;
            c.label(pen,rowY+4,segment,28,segmentColor);
            if(style&assistant::bold)c.label(pen+1,rowY+4,segment,28,segmentColor);
            pen+=width;start=end;
        }
    }
}
void signIn(Canvas& c,const assistant::State& state,unsigned top,unsigned bottom){
    const unsigned y=top+(bottom-top-440)/2;
    tracked(c,left,y,assistant::language::tr("SIGN IN TO CHATGPT"),20,accent,2);
    c.label(left,y+36,assistant::language::tr("Finish signing in on your phone."),60,ink);
    c.label(left,y+164,assistant::language::tr("1 · Open"),24,muted);
    c.label(left,y+198,state.url,32,codeInk);
    c.label(left,y+284,assistant::language::tr("2 · Enter the code"),24,muted);
    tracked(c,left,y+318,state.code,76,ink,6);
}
void composerBox(Canvas& c,const assistant::State& state,const assistant::VoiceState& mic,unsigned bottom){
    constexpr unsigned textWidth=1460;
    const bool empty=composer.empty();
    const std::string_view text=empty?std::string_view{}:std::string_view(composer);
    auto rows=wrap(c,text,textWidth,28);
    const unsigned count=std::clamp<unsigned>(static_cast<unsigned>(rows.size()),1,3);
    const unsigned height=78+count*42,top=bottom-height;
    c.rounded(left,top,span,height,28,keyboard?accent:edge);c.rounded(left+2,top+2,span-4,height-4,26,surface);
    // Show the end of a long prompt so the text being typed stays visible.
    const std::size_t from=rows.size()>count?rows.size()-count:0;
    if(empty)c.label(left+36,top+43,assistant::language::tr("Your prompt..."),28,dim);
    for(std::size_t i=from;i<rows.size();++i)c.label(left+36,top+43+static_cast<unsigned>(i-from)*42,text.substr(rows[i].start,rows[i].length),28,ink);
    if(keyboard){
        unsigned caretX=left+36,caretY=top+39;
        if(!rows.empty()){caretX+=c.text_width(text.substr(rows.back().start,rows.back().length),28)+4;caretY+=static_cast<unsigned>(rows.size()-1-from)*42;}
        if(!empty && text.back()=='\n'){caretX=left+36;caretY+=42;}
        if(caretY+34<bottom)c.rectangle(caretX,caretY+4,3,34,accent);
    }
    const bool ready=!mic.recording && !mic.working && !state.busy && state.ready && !state.thread.empty() && !empty;
    const auto label=assistant::language::tr("Send");
    const unsigned width=c.text_width("☰",24)+14+c.text_width(label,24)+48,x=right-30-width,y=top+(height-60)/2;
    c.rounded(x,y,width,60,18,ready?accent:well);
    c.label(x+20,y+15,"☰",24,ready?bg:dim);c.label(x+34+c.text_width("☰",24),y+14,label,24,ready?bg:dim);
}
void keyboardPanel(Canvas& c,unsigned top){
    constexpr unsigned gap=10;
    for(unsigned i=0;i<keys.size();++i){
        // Integer columns that still end exactly on the right margin.
        const unsigned x=left+(i%10)*(span+gap)/10,width=left+(i%10+1)*(span+gap)/10-gap-x,y=top+(i/10)*74;
        const bool special=i==40 || i==41 || i==48 || i==49,focused=i==key;
        const auto text=assistant::language::tr(keys[i]);const unsigned size=special?24:28;
        c.rounded(x,y,width,64,14,focused?accent:special?raised:surface);
        c.label(x+(width-c.text_width(text,size))/2,y+(special?17:14),text,size,focused?bg:special?soft:ink);
    }
}
void hints(Canvas& c){
    unsigned x=left+8;constexpr unsigned y=1010;
    hint(c,x,y,"△",assistant::language::tr("Dictate"),ink);
    hint(c,x,y,"□",assistant::language::tr("Keyboard"),ink);
    hint(c,x,y,"○",assistant::language::tr("Cancel"),ink);
    hint(c,x,y,"↑↓",assistant::language::tr("Scroll"),ink);
    const auto quit=assistant::language::tr("Quit");
    x=right-8-50-c.text_width(quit,24);touchpad(c,x,y+4,muted);c.label(x+50,y,quit,24,muted);
    if(assistant::language::selectable){
        const std::string_view other=assistant::language::isFrench()?"English":"Français";
        x-=36+hintWidth(c,"",other);unsigned pen=x;hint(c,pen,y,"",other,muted);
    }
    const auto login=assistant::language::tr("Sign in");
    x-=36+hintWidth(c,"",login);hint(c,x,y,"",login,muted);
}
// Centered dialog over a dimmed screen. Returns the content origin.
void sheetHints(Canvas& c,unsigned x,unsigned y,bool scrollable){
    const auto choose=assistant::language::tr("Choose"),confirm=assistant::language::tr("Confirm"),read=assistant::language::tr("Scroll");
    unsigned width=hintWidth(c,"←→",choose)+36+hintWidth(c,"×",confirm);
    if(scrollable)width+=36+hintWidth(c,"↑↓",read);
    x-=width;
    if(scrollable)hint(c,x,y,"↑↓",read,muted);
    hint(c,x,y,"←→",choose,muted);hint(c,x,y,"×",confirm,muted);
}
void approvalSheet(Canvas& c,const assistant::Approval& approval){
    constexpr unsigned width=1120,inner=width-128,visible=9;
    const auto rows=wrap(c,approval.description,inner-64,24);
    const unsigned shown=std::min<unsigned>(static_cast<unsigned>(rows.size()),visible);
    approvalScroll=std::min(approvalScroll,rows.size()>visible?static_cast<unsigned>(rows.size()-visible):0U);
    const unsigned block=56+shown*36,height=404+block;
    const unsigned x=(1920-width)/2,y=(1080-height)/2;
    c.shade(184);c.rounded(x,y,width,height,32,edge);c.rounded(x+1,y+1,width-2,height-2,31,surface);
    unsigned cy=y+56;const unsigned cx=x+64;
    c.circle(cx+5,cy+14,5,accent);tracked(c,cx+24,cy,assistant::language::tr("APPROVAL NEEDED"),20,accent,2);cy+=44;
    const auto title=approval.method=="item/commandExecution/requestApproval"?"Codex wants to run a command":
        approval.method=="item/fileChange/requestApproval"?"Codex wants to change files":"Codex needs your approval";
    c.label(cx,cy,assistant::language::tr(title),44,ink);cy+=68;
    c.label(cx,cy,assistant::language::tr(approval.reviewable?"Review it carefully. Nothing runs unless you choose Allow.":"Details are incomplete: this action can only be declined."),24,approval.reviewable?muted:danger);cy+=72;
    c.rounded(cx,cy,inner,block,20,well);c.rounded(cx+1,cy+1,inner-2,block-2,19,bg);
    for(unsigned i=0;i<shown;++i){
        const auto& row=rows[approvalScroll+i];
        const Color color=row.paragraph==0?(approval.reviewable?codeInk:danger):muted;
        c.label(cx+32,cy+28+i*36,std::string_view(approval.description).substr(row.start,row.length),24,color);
    }
    cy+=block+44;
    button(c,cx,cy,assistant::language::tr("Decline"),!approve);
    button(c,cx+256,cy,assistant::language::tr("Allow"),approve,approval.reviewable?soft:dim);
    sheetHints(c,x+width-64,cy+19,rows.size()>visible);
}
void quitSheet(Canvas& c){
    constexpr unsigned width=1080,height=388;
    constexpr unsigned x=(1920-width)/2,y=(1080-height)/2,cx=x+64;
    c.shade(184);c.rounded(x,y,width,height,32,edge);c.rounded(x+1,y+1,width-2,height-2,31,surface);
    c.label(cx,y+56,assistant::language::tr("Quit Codex PS5?"),44,ink);
    const auto body=assistant::language::tr("Dictation will stop and Codex will be disconnected.\nThis conversation cannot be resumed in the application.");
    unsigned line=0;for(const auto& row:wrap(c,body,width-128,28))c.label(cx,y+132+40*line++,std::string_view(body).substr(row.start,row.length),28,soft);
    const unsigned buttons=y+height-56-72;
    button(c,cx,buttons,assistant::language::tr("Cancel"),!confirmQuit);
    button(c,cx+256,buttons,assistant::language::tr("Quit"),confirmQuit,danger,danger,ink);
    sheetHints(c,x+width-64,buttons+19,false);
}
void render(Canvas& c,const assistant::State& state,const assistant::VoiceState& mic){
    c.clear(bg);header(c,state);
    const unsigned composerBottom=keyboard?660:986;
    composerBox(c,state,mic,composerBottom);
    const unsigned composerTop=composerBottom-78-std::clamp<unsigned>(static_cast<unsigned>(wrap(c,composer,1460,28).size()),1,3)*42;
    const auto notice=!composerNotice.empty()?composerNotice:mic.status!="△: dictate"?mic.status:std::string{};
    unsigned threadBottom=composerTop-32;
    if(!notice.empty()){
        const auto text=assistant::language::tr(notice);
        c.circle(left+13,composerTop-30,5,accent);c.label(left+30,composerTop-47,text,24,ink);threadBottom=composerTop-72;
    }
    if(!state.code.empty())signIn(c,state,168,threadBottom);else conversation(c,state,168,threadBottom);
    if(keyboard)keyboardPanel(c,composerBottom+20);else hints(c);
    if(quitting)quitSheet(c);
    else if(!state.approvals.empty())approvalSheet(c,state.approvals.front());
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
    if(c.take_resumed()){pressed=0;approve=false;confirmQuit=false;++uiRevision;}
    auto state=client.snapshot();auto mic=voice.snapshot();
    const auto text=voice.takeTranscript();if(!text.empty()){
        if(composer.size()+text.size()+1<=16384){if(!composer.empty())composer+=' ';composer+=text;composerNotice.clear();}
        else composerNotice=assistant::language::tr("Prompt too long: the new dictation was not added");
        keyboard=false;
    }
    const auto commandStart=composer.find_first_not_of(" \t\r\n");
    const auto commandEnd=composer.find_last_not_of(" \t\r\n");
    const bool quitCommand=commandStart!=std::string::npos &&
        composer.substr(commandStart,commandEnd-commandStart+1)=="/quit";
    if(!quitting && ((pressed&PS5_PAD_BUTTON_TOUCH_PAD) ||
        ((pressed&PS5_PAD_BUTTON_OPTIONS) && quitCommand))){
        quitting=true;confirmQuit=false;pressed=0;++uiRevision;
        if(quitCommand)composer.clear();
    }
    if(quitting){
        if(pressed&(PS5_PAD_BUTTON_LEFT|PS5_PAD_BUTTON_RIGHT))confirmQuit=!confirmQuit;
        if(pressed&PS5_PAD_BUTTON_CIRCLE)quitting=false;
        else if(pressed&PS5_PAD_BUTTON_CROSS){
            if(confirmQuit)return false;
            quitting=false;
        }
    } else if(!state.approvals.empty()){
        const auto identity=state.approvals.front().id.dump();
        if(identity!=approvalIdentity){approvalIdentity=identity;approve=false;pressed=0;approvalScroll=0;}
        if(pressed&PS5_PAD_BUTTON_UP && approvalScroll)--approvalScroll;
        if(pressed&PS5_PAD_BUTTON_DOWN && approvalScroll<8192)++approvalScroll;
        if((pressed&(PS5_PAD_BUTTON_LEFT|PS5_PAD_BUTTON_RIGHT)) && state.approvals.front().reviewable)approve=!approve;
        if(pressed&PS5_PAD_BUTTON_CROSS){client.decide(approve);approvalIdentity.clear();}
        if(pressed&PS5_PAD_BUTTON_CIRCLE){client.decide(false);approvalIdentity.clear();}
    } else {
        approvalIdentity.clear();
        if((pressed&PS5_PAD_BUTTON_R1) && !state.busy && !mic.recording && !mic.working)assistant::language::toggle();
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
    render(c,state,mic);
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
    voice.cancel();client.stop();voice.shutdown();if(pad>=0)scePadClose(pad);
    return 0;
}
