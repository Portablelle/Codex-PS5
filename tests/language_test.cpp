// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/language.hpp"
#include "../src/protocol.hpp"
#include <cassert>
using namespace assistant;
int main(){
    for(int id=-1;id<48;++id){
        language::initialize(id);
        const bool french=id==2 || id==22;
        assert(language::selectable==french && language::isFrench()==french);
        assert(language::toggle()==french);
        assert(!language::isFrench());
        if(french){assert(language::toggle());assert(language::isFrench());}
    }
    language::initialize(2);
    assert(language::tr("Codex is thinking...")=="Codex réfléchit...");
    assert(language::tr("Error: example")=="Erreur: example");
    assert(language::tr("Your prompt...")=="Ton prompt...");
    assert(language::tr("Quit Codex PS5?")=="Quitter Codex PS5 ?");
    assert(language::tr("Quit")=="Quitter" && language::tr("Cancel")=="Annuler");
    assert(language::tr("Decline")=="Refuser" && language::tr("Allow")=="Autoriser");
    assert(language::tr("Choose")=="Choisir" && language::tr("Confirm")=="Confirmer");
    assert(language::tr("Dictate")=="Dicter" && language::tr("Keyboard")=="Clavier");
    assert(language::tr("Scroll")=="Défiler" && language::tr("Send")=="Envoyer");
    assert(language::tr("Sign in")=="Connexion");
    // Short labels must not capture longer messages that share their prefix.
    assert(language::tr("Ready")=="Pret");
    assert(language::tr("Sign in on your phone using the displayed code.").starts_with("Connecte-toi"));
    assert(language::tr("Dictation cancelled")=="Dictee annulee");
    assert(language::tr("APPROVAL NEEDED")=="AUTORISATION REQUISE");
    assert(language::tr("Codex wants to run a command")=="Codex veut exécuter une commande");
    assert(language::tr("Codex wants to change files")=="Codex veut modifier des fichiers");
    assert(language::tr("Review it carefully. Nothing runs unless you choose Allow.").starts_with("Vérifie bien"));
    assert(language::tr("Details are incomplete: this action can only be declined.").starts_with("Détails incomplets"));
    assert(language::tr("SIGN IN TO CHATGPT")=="CONNEXION À CHATGPT");
    assert(language::tr("Finish signing in on your phone.")=="Termine la connexion sur ton téléphone.");
    assert(language::tr("1 · Open")=="1 · Ouvre" && language::tr("2 · Enter the code")=="2 · Saisis le code");
    assert(language::tr("Dictation will stop and Codex will be disconnected.\nThis conversation cannot be resumed in the application.").starts_with("La dictee sera arretee"));
    assert(language::tr("\nDirectory: /data")=="\nDossier : /data");
    assert(language::tr("/data/notes.txt")=="/data/notes.txt");
    language::initialize(1);
    assert(language::tr("Codex is thinking...")=="Codex is thinking...");
    assert(language::tr("Error: example")=="Error: example");
    Protocol p;p.begin();p.ingest(R"({"id":1,"result":{}})");
    p.ingest(R"({"id":2,"result":{"account":{"type":"chatgpt"}}})");
    assert(Json::parse(p.outgoing.back())["params"]["developerInstructions"].get<std::string>().starts_with("Follow the user's requested response language; otherwise keep the language of the conversation."));
    language::initialize(2);
    // UI switching cannot touch the protocol, even while thread/start is pending.
    auto size=p.outgoing.size();assert(language::toggle());
    assert(!language::isFrench() && p.state.authenticated && size==p.outgoing.size());
    p.ingest(R"({"id":3,"result":{"thread":{"id":"existing"}}})");
    p.submit("Réponds en français.");
    auto firstTurn=Json::parse(p.outgoing.back());
    assert(firstTurn["params"]["input"][0]["text"]=="Réponds en français.");
    p.ingest(Json({{"id",firstTurn["id"]},{"result",{{"turn",{{"id","first-turn"}}}}}}).dump());
    p.ingest(R"({"method":"item/completed","params":{"item":{"id":"reply","type":"agentMessage","text":"Bonjour, voici **la réponse**."}}})");
    p.ingest(R"({"method":"turn/completed","params":{"turn":{"status":"completed"}}})");
    const auto before=p.state;
    const auto outgoing=p.outgoing;
    // Repeated switches preserve authentication, thread, history and response bytes.
    for(int i=0;i<5;++i){
        assert(language::toggle());
        assert(p.outgoing==outgoing);
        assert(p.state.connected==before.connected && p.state.ready==before.ready);
        assert(p.state.authenticated==before.authenticated && p.state.busy==before.busy);
        assert(p.state.thread==before.thread && p.state.turn==before.turn);
        assert(p.state.code==before.code && p.state.url==before.url);
        assert(p.state.status==before.status && p.state.response==before.response);
        assert(p.state.revision==before.revision && p.state.chatRevision==before.chatRevision);
        assert(p.state.messages.size()==before.messages.size() && p.state.approvals.empty());
        for(std::size_t j=0;j<before.messages.size();++j){
            assert(p.state.messages[j].user==before.messages[j].user);
            assert(p.state.messages[j].text==before.messages[j].text);
            assert(p.state.messages[j].itemId==before.messages[j].itemId);
        }
        // Pressing the sign-in button while connected must not replace the account.
        p.login();assert(p.outgoing==outgoing);
    }
    for(int i=0;i<4;++i){
        p.submit("Continue\nexactly as before.");
        auto turn=Json::parse(p.outgoing.back());
        assert(turn["method"]=="turn/start" && turn["params"]["threadId"]=="existing");
        assert(turn["params"]["input"][0]["text"]=="Continue\nexactly as before.");
        assert(p.state.messages.back().text=="Continue\nexactly as before.");
        p.ingest(Json({{"id",turn["id"]},{"result",{{"turn",{{"id","turn"}}}}}}).dump());
        p.ingest(R"({"method":"turn/completed","params":{"turn":{"status":"completed"}}})");
        size=p.outgoing.size();assert(language::toggle());
        assert(p.state.authenticated && p.state.ready && size==p.outgoing.size());
        assert(p.state.thread=="existing" && p.state.messages[1].text==before.messages[1].text);
    }
    // A pending device login also survives an interface change.
    Protocol signingIn;signingIn.begin();signingIn.ingest(R"({"id":1,"result":{}})");
    signingIn.ingest(R"({"id":2,"result":{"account":null}})");
    signingIn.login();auto login=Json::parse(signingIn.outgoing.back());
    assert(login["method"]=="account/login/start");
    signingIn.ingest(Json({{"id",login["id"]},{"result",{{"userCode","ABCD"},{"verificationUrl","https://example.com/login"}}}}).dump());
    size=signingIn.outgoing.size();assert(language::toggle());
    assert(signingIn.state.code=="ABCD" && signingIn.state.url=="https://example.com/login");
    signingIn.login();assert(signingIn.outgoing.size()==size);
}
