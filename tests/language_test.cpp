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
    // Every approval summary is translated.
    assert(language::tr("Building code")=="Compilation de code");
    assert(language::tr("Changing a process priority")=="Modification de la priorité d'un processus");
    assert(language::tr("Changing directory")=="Changement de dossier");
    assert(language::tr("Changing file ownership")=="Modification du propriétaire de fichiers");
    assert(language::tr("Changing file permissions")=="Modification des permissions de fichiers");
    assert(language::tr("Changing ownership of a folder and its contents")=="Modification du propriétaire d'un dossier et de son contenu");
    assert(language::tr("Changing permissions of a folder and its contents")=="Modification des permissions d'un dossier et de son contenu");
    assert(language::tr("Changing several files")=="Modification de plusieurs fichiers");
    assert(language::tr("Changing system settings")=="Modification de réglages système");
    assert(language::tr("Changing the host name")=="Modification du nom d'hôte");
    assert(language::tr("Changing the system date")=="Modification de la date système");
    assert(language::tr("Checking a condition")=="Vérification d'une condition");
    assert(language::tr("Checking storage space")=="Vérification de l'espace de stockage");
    assert(language::tr("Comparing files")=="Comparaison de fichiers");
    assert(language::tr("Computing a value")=="Calcul d'une valeur");
    assert(language::tr("Computing checksums")=="Calcul de sommes de contrôle");
    assert(language::tr("Connecting over the network")=="Connexion par le réseau");
    assert(language::tr("Copying a file")=="Copie d'un fichier");
    assert(language::tr("Copying a folder")=="Copie d'un dossier");
    assert(language::tr("Copying files")=="Copie de fichiers");
    assert(language::tr("Copying raw data")=="Copie de données brutes");
    assert(language::tr("Counting lines and words")=="Comptage de lignes et de mots");
    assert(language::tr("Creating a file")=="Création d'un fichier");
    assert(language::tr("Creating a folder")=="Création d'un dossier");
    assert(language::tr("Creating a link")=="Création d'un lien");
    assert(language::tr("Creating a named pipe")=="Création d'un tube nommé");
    assert(language::tr("Creating a temporary file")=="Création d'un fichier temporaire");
    assert(language::tr("Creating an archive")=="Création d'une archive");
    assert(language::tr("Creating files")=="Création de fichiers");
    assert(language::tr("Creating or updating a file")=="Création ou mise à jour d'un fichier");
    assert(language::tr("Decoding a file")=="Décodage d'un fichier");
    assert(language::tr("Deleting a file")=="Suppression d'un fichier");
    assert(language::tr("Deleting an empty folder")=="Suppression d'un dossier vide");
    assert(language::tr("Deleting files")=="Suppression de fichiers");
    assert(language::tr("Deleting folders and their contents")=="Suppression de dossiers et de leur contenu");
    assert(language::tr("Downloading data from the network")=="Téléchargement de données depuis le réseau");
    assert(language::tr("Editing a file")=="Modification d'un fichier");
    assert(language::tr("Editing files")=="Modification de fichiers");
    assert(language::tr("Encoding a file")=="Encodage d'un fichier");
    assert(language::tr("Extracting an archive")=="Extraction d'une archive");
    assert(language::tr("Finding and deleting files")=="Recherche et suppression de fichiers");
    assert(language::tr("Finding files and running a command on them")=="Recherche de fichiers et exécution d'une commande sur eux");
    assert(language::tr("Flushing data to disk")=="Écriture des données sur le disque");
    assert(language::tr("Inspecting paths")=="Examen de chemins");
    assert(language::tr("Listing an archive's contents")=="Liste du contenu d'une archive");
    assert(language::tr("Listing files")=="Liste des fichiers");
    assert(language::tr("Listing processes")=="Liste des processus");
    assert(language::tr("Making a file executable")=="Rendre un fichier exécutable");
    assert(language::tr("Mounting or unmounting a drive")=="Montage ou démontage d'un disque");
    assert(language::tr("Moving a file")=="Déplacement d'un fichier");
    assert(language::tr("Moving files")=="Déplacement de fichiers");
    assert(language::tr("Opening a shell")=="Ouverture d'un shell");
    assert(language::tr("Printing text")=="Affichage de texte");
    assert(language::tr("Reading a file")=="Lecture d'un fichier");
    assert(language::tr("Reading environment variables")=="Lecture des variables d'environnement");
    assert(language::tr("Reading system information")=="Lecture des informations système");
    assert(language::tr("Reading the date")=="Lecture de la date");
    assert(language::tr("Renaming a file")=="Renommage d'un fichier");
    assert(language::tr("Requesting additional permissions")=="Demande de permissions supplémentaires");
    assert(language::tr("Requesting network access")=="Demande d'accès au réseau");
    assert(language::tr("Requesting write access to a folder")=="Demande d'accès en écriture à un dossier");
    assert(language::tr("Restarting or shutting down the console")=="Redémarrage ou arrêt de la console");
    assert(language::tr("Running Git")=="Exécution de Git");
    assert(language::tr("Running a command on each input")=="Exécution d'une commande sur chaque entrée");
    assert(language::tr("Running a program")=="Exécution d'un programme");
    assert(language::tr("Running a script")=="Exécution d'un script");
    assert(language::tr("Running a shell script")=="Exécution d'un script shell");
    assert(language::tr("Running several commands")=="Exécution de plusieurs commandes");
    assert(language::tr("Searching for files")=="Recherche de fichiers");
    assert(language::tr("Searching text in files")=="Recherche de texte dans des fichiers");
    assert(language::tr("Sending input to a running command")=="Envoi de texte à une commande en cours");
    assert(language::tr("Setting a variable")=="Définition d'une variable");
    assert(language::tr("Splitting a file")=="Découpage d'un fichier");
    assert(language::tr("Stopping a process")=="Arrêt d'un processus");
    assert(language::tr("Transforming text")=="Transformation de texte");
    assert(language::tr("Waiting")=="Attente");
    assert(language::tr("Working with an archive")=="Manipulation d'une archive");
    assert(language::tr("Writing to a file")=="Écriture dans un fichier");
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
