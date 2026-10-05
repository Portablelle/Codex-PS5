// SPDX-License-Identifier: GPL-3.0-or-later
#include "protocol.hpp"
#include "read_only_command.hpp"
#include "language.hpp"
#include <algorithm>
namespace assistant {
std::string field(const Json& j,const char* key) {
    if(!j.is_object())return {};
    auto it=j.find(key);return it!=j.end() && it->is_string()?it->get<std::string>():std::string{};
}
static const Json& object(const Json& j,const char* key) {
    static const Json empty=Json::object();
    if(!j.is_object())return empty;auto it=j.find(key);
    return it!=j.end() && it->is_object()?*it:empty;
}
void Protocol::emit(Json j){outgoing.push_back(j.dump(-1,' ',false,Json::error_handler_t::replace)+"\n");}
void Protocol::request(const char* method,Json params){
    const int id=next_++;pending_[id]=method;
    emit({{"id",id},{"method",method},{"params",std::move(params)}});
}
static std::string instructions(){
    return std::string(language::isFrench()?"Answer in French. ":"Answer in English. ")+
    "You are an assistant running locally on a jailbroken PS5. "
                    "Inspect before modifying. Work on user files in /data or /mnt/usb*. "
                    "Perform read-only inspection without asking the user for confirmation. "
                    "Prefer single direct commands from the bundled toolbox for inspection. "
                    "Do not change firmware, /system, exploit settings, running services, or delete files "
                    "unless the user explicitly asks. Avoid displaying tokens or credentials. "
                    "Discover the installed command capabilities with ps5-tools when needed. "
                    "Choose your own commands and inspect their results. Base claims about running services "
                    "on live observations; installed files alone do not establish that a process is running. "
                    "Report unsupported capabilities honestly instead of guessing. "
                    "Do not mention implementation details or sandboxing unless asked.";
}
void Protocol::changeLanguage(){
    if(state.busy || !state.approvals.empty() || !state.ready)return;
    for(const auto& entry:pending_)if(entry.second=="thread/start" || entry.second=="thread/resume")return;
    if(!language::toggle())return;
    if(!state.thread.empty()) {
        state.ready=false;
        request("thread/resume",{{"threadId",state.thread},{"developerInstructions",instructions()}});
    }
    ++state.revision;
}
void Protocol::startThread(){
    if(!state.authenticated || !state.thread.empty())return;
    for(const auto& item:pending_)if(item.second=="thread/start")return;
    request("thread/start",{{"model","gpt-6.1-sol"},{"allowProviderModelFallback",false},
                    {"config",{{"model_reasoning_effort","low"}}},
                    {"cwd","/data/codex-ps5/workspace"},{"approvalPolicy","untrusted"},
                    {"sandbox","danger-full-access"},{"developerInstructions",
                    instructions()}});
}
void Protocol::begin(){
    pending_.clear();items_.clear();outgoing.clear();state=State{};state.connected=true;
    request("initialize",{{"clientInfo",{{"name","codex_ps5"},{"title","Codex PS5"},{"version","0.0.2"}}},
                          {"capabilities",{{"experimentalApi",true}}}});
}
void Protocol::disconnect(){
    pending_.clear();items_.clear();outgoing.clear();state.connected=state.ready=state.busy=false;
    state.approvals.clear();state.thread.clear();state.turn.clear();
    state.status="Moteur deconnecte. La requete ne sera pas renvoyee.";++state.revision;
}
void Protocol::login(){
    if(!state.ready || !state.code.empty())return;
    for(const auto& item:pending_)if(item.second=="account/login/start")return;
    request("account/login/start",{{"type","chatgptDeviceCode"}});
}
void Protocol::submit(std::string_view text){
    if(!state.ready || state.busy || state.thread.empty() || text.empty() || text.size()>16384)return;
    state.busy=true;state.response.clear();state.status="Codex réfléchit...";
    ++turnSequence_;
    state.messages.push_back({true,std::string(text),{}});++state.chatRevision;
    while(state.messages.size()>64)state.messages.pop_front();
    request("turn/start",{{"threadId",state.thread},{"model","gpt-6.1-sol"},{"effort","low"},{"input",Json::array({{{"type","text"},{"text",text},{"text_elements",Json::array()}}})}});
    ++state.revision;
}
void Protocol::agentMessage(std::string id,std::string text,bool append){
    if(id.empty())id="local-turn-"+std::to_string(turnSequence_);
    auto found=std::find_if(state.messages.begin(),state.messages.end(),[&](const Message& m){return !m.user && m.itemId==id;});
    if(found==state.messages.end())state.messages.push_back({false,std::move(text),std::move(id)});
    else if(append)found->text+=text;
    else found->text=std::move(text);
    std::size_t bytes=0;for(const auto& m:state.messages)bytes+=m.text.size();
    // Keep recent conversation text bounded without truncating the current response.
    while(state.messages.size()>2 && (state.messages.size()>64 || bytes>1048576)){
        bytes-=state.messages.front().text.size();state.messages.pop_front();
    }
    ++state.chatRevision;
}
void Protocol::interrupt(){if(state.busy && !state.turn.empty())request("turn/interrupt",{{"threadId",state.thread},{"turnId",state.turn}});}
void Protocol::decide(bool accept){
    if(state.approvals.empty())return;
    auto a=state.approvals.front();state.approvals.pop_front();
    emit({{"id",a.id},{"result",{{"decision",accept && a.reviewable?"accept":"decline"}}}});
    state.status=state.approvals.empty()?"Codex reprend...":"Action suivante a confirmer";
    ++state.revision;
}
bool Protocol::ingest(std::string_view line){
    if(line.size()>1048576)return false;
    auto j=Json::parse(line,nullptr,false);if(j.is_discarded() || !j.is_object())return false;
    const auto method=field(j,"method");const auto& p=object(j,"params");
    auto id=j.find("id");
    if(!method.empty() && id!=j.end()) {
        if(!(id->is_string() || id->is_number_integer()))return false;
        if(method=="item/commandExecution/requestApproval" || method=="item/fileChange/requestApproval"){
            // Approve only the exact command supplied with this request, never a
            // cached parent command or a request for broader/network permissions.
            const auto hasContext=[&](const char* key){return p.contains(key) && !p[key].is_null();};
            if(method=="item/commandExecution/requestApproval" &&
               (!hasContext("kind") || field(p,"kind")=="command") &&
               !hasContext("additionalPermissions") && !hasContext("networkApprovalContext") &&
               !hasContext("grantRoot") && readOnlyCommand(field(p,"command"))){
                emit({{"id",*id},{"result",{{"decision","accept"}}}});
                ++state.revision;return true;
            }
            if(state.approvals.size()>=16)return false;
            const auto cached=items_.find(field(p,"itemId"));
            std::string description=field(p,"command");
            if(description.empty() && cached!=items_.end())description=cached->second;
            const bool detailAvailable=!description.empty() && field(p,"kind")!="stdin";
            const auto cwd=field(p,"cwd"),reason=field(p,"reason"),root=field(p,"grantRoot");
            if(!cwd.empty())description+=language::tr("\nDossier : ")+cwd;
            if(!root.empty())description+=language::tr("\nRacine demandee : ")+root;
            if(!reason.empty())description+=language::tr("\nMotif : ")+reason;
            if(p.contains("additionalPermissions") && !p["additionalPermissions"].is_null())description+=language::tr("\nPermissions : ")+p["additionalPermissions"].dump();
            const bool complete=detailAvailable && description.size()<=8192;
            if(!complete)description=language::tr("Details incomplets ou trop longs : refus uniquement.\n")+description;
            state.approvals.push_back({*id,method,description.substr(0,8192),complete});
            state.status="Action en attente de confirmation";
        } else emit({{"id",*id},{"error",{{"code",-32601},{"message",language::tr("Interaction non prise en charge par le prototype PS5")}}}});
    } else if(method.empty() && id!=j.end() && id->is_number_integer()) {
        if(!id->is_number_unsigned() && id->get<std::int64_t>()<0)return false;
        const auto wide=id->get<std::uint64_t>();if(wide>2147483647)return false;
        auto it=pending_.find(static_cast<int>(wide));if(it==pending_.end())return true;
        const auto requestMethod=it->second;pending_.erase(it);
        if(j.contains("error")) {
            state.status="Erreur: "+field(object(j,"error"),"message");
            if(requestMethod=="turn/start")state.busy=false;
            if(requestMethod=="thread/resume"){language::toggle();state.ready=true;}
        } else {
            const auto& r=object(j,"result");
            if(requestMethod=="initialize") {
                emit({{"method","initialized"}});state.ready=true;
                request("account/read",{{"refreshToken",false}});

            } else if(requestMethod=="thread/start")state.thread=field(object(r,"thread"),"id");
            else if(requestMethod=="thread/resume"){state.ready=true;state.status="Pret";}
            else if(requestMethod=="turn/start")state.turn=field(object(r,"turn"),"id");
            else if(requestMethod=="account/login/start") {
                state.code=field(r,"userCode");state.url=field(r,"verificationUrl");
                state.status="Connecte-toi sur un telephone avec le code affiche.";
            } else if(requestMethod=="account/read") {
                state.authenticated=r.contains("account") && r["account"].is_object();
                state.status=state.authenticated?"Pret":"L1 : connexion au compte ChatGPT";
                startThread();
            }
        }
    } else if(method=="item/started" || method=="item/completed") {
        const auto& item=object(p,"item");const auto type=field(item,"type");
        const auto itemId=field(item,"id");std::string details;
        if(state.busy && state.approvals.empty()){
            if(method=="item/started" && type=="commandExecution")state.status="Codex exécute une commande...";
            else if(method=="item/started" && type=="agentMessage")state.status="Codex répond...";
            else if(type=="reasoning" || method=="item/completed")state.status="Codex réfléchit...";
        }
        if(type=="agentMessage" && method=="item/completed"){
            const auto text=field(item,"text");if(text.size()>262144)return false;
            if(!text.empty())agentMessage(itemId,text,false);
        }
        if(type=="commandExecution")details=field(item,"command");
        else if(type=="fileChange" && item.contains("changes") && item["changes"].is_array()) {
            for(const auto& change:item["changes"]){
                details+=language::tr("Fichier : ")+field(change,"path")+"\n"+field(change,"diff")+"\n";
                if(details.size()>8192)break;
            }
        }
        if(!itemId.empty() && !details.empty()){
            if(items_.size()>=64)items_.erase(items_.begin());
            items_[itemId]=details.substr(0,8193);
        }
    } else if(method=="item/agentMessage/delta") {
        const auto delta=field(p,"delta");
        if(state.response.size()+delta.size()>262144)return false;
        state.response+=delta;
        agentMessage(field(p,"itemId"),delta,true);
        if(state.approvals.empty())state.status="Codex répond...";
    } else if(method=="item/reasoning/textDelta" || method=="item/reasoning/summaryTextDelta" || method=="item/reasoning/summaryPartAdded"){
        if(state.busy && state.approvals.empty())state.status="Codex réfléchit...";
    } else if(method=="turn/started") {state.turn=field(object(p,"turn"),"id");state.busy=true;}
    else if(method=="turn/completed") {
        state.busy=false;state.turn.clear();state.approvals.clear();items_.clear();
        const auto& turn=object(p,"turn");const auto status=field(turn,"status");
        state.status=status=="completed"?"Termine":status=="interrupted"?"Interrompu":"Echec: "+field(object(turn,"error"),"message");
    } else if(method=="account/login/completed") {
        if(p.contains("success") && p["success"]==true) {
            state.code.clear();state.url.clear();request("account/read",{{"refreshToken",false}});
        } else {state.code.clear();state.url.clear();state.status="Connexion echouee: "+field(p,"error");}
    } else if(method=="error")state.status="Erreur: "+field(object(p,"error"),"message");
    ++state.revision;return true;
}
}
