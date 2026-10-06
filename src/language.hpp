// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <string>
#include <string_view>
namespace assistant::language {
enum class Language { English, French };
inline std::atomic<Language> current{Language::English};
inline bool selectable=false;
// Sony system language IDs: French (France) and French (Canada).
inline void initialize(int systemLanguage) {
    selectable=systemLanguage==2 || systemLanguage==22;
    current=selectable?Language::French:Language::English;
}
inline bool isFrench(){return current.load()==Language::French;}
inline bool toggle(){if(!selectable)return false;current=isFrench()?Language::English:Language::French;return true;}
// English source strings; French translations retain the console language support.
inline std::string tr(std::string_view english) {
    if(!isFrench())return std::string(english);
    struct Entry {std::string_view english,french;};
    static constexpr Entry entries[]={
        {"Dictate or type a request to work on PS5 files.","Dicte ou saisis une demande pour agir sur les fichiers de la PS5."},
        {"Prompt too long: the new dictation was not added","Prompt trop long : la nouvelle dictee n'a pas ete ajoutee"},
        {"Your prompt...","Ton prompt..."},
        {"Quit Codex PS5?","Quitter Codex PS5 ?"},
        {"Dictation will stop and Codex will be disconnected.\nThis conversation cannot be resumed in the application.","La dictee sera arretee et Codex sera deconnecte.\nCette conversation ne pourra pas etre reprise dans l'application."},
        {"Codex needs your approval","Codex demande ton autorisation"},
        {"space","espace"},
        {"delete","effacer"},
        {"shift","maj"},
        {"close","fermer"},
        {"Starting engine...","Demarrage du moteur..."},
        {"Engine disconnected. The request will not be resent.","Moteur deconnecte. La requete ne sera pas renvoyee."},
        {"Codex is thinking...","Codex réfléchit..."},
        {"Codex is resuming...","Codex reprend..."},
        {"Next action needs approval","Action suivante a confirmer"},
        {"\nDirectory: ","\nDossier : "},
        {"\nRequested root: ","\nRacine demandee : "},
        {"\nReason: ","\nMotif : "},
        {"\nPermissions: ","\nPermissions : "},
        {"Details incomplete or too long: decline only.\n","Details incomplets ou trop longs : refus uniquement.\n"},
        {"Action awaiting approval","Action en attente de confirmation"},
        {"Interaction unsupported by the PS5 prototype","Interaction non prise en charge par le prototype PS5"},
        {"Error: ","Erreur: "},
        {"Sign in on your phone using the displayed code.","Connecte-toi sur un telephone avec le code affiche."},
        {"Ready","Pret"},
        {"\uE000: sign in to ChatGPT","\uE000 : connexion au compte ChatGPT"},
        {"Codex is running a command...","Codex exécute une commande..."},
        {"Codex is replying...","Codex répond..."},
        {"File: ","Fichier : "},
        {"Completed","Termine"},
        {"Interrupted","Interrompu"},
        {"Failed: ","Echec: "},
        {"Sign-in failed: ","Connexion echouee: "},
        {"Engine unavailable: launch assistant-service in Payload Manager","Moteur absent : lance assistant-service dans Payload Manager"},
        {"△: dictate","△ : dicter"},
        {"Recording locally (30 s max). △: finish","Enregistrement local (30 s max). △ : terminer"},
        {"Unable to start microphone","Impossible de demarrer le micro"},
        {"PS5 user unavailable","Utilisateur PS5 indisponible"},
        {"Microphone unavailable. Check your controller microphone.","Micro indisponible. Verifie le micro de la manette."},
        {"Dictation cancelled","Dictee annulee"},
        {"Microphone capture error","Erreur de capture du micro"},
        {"No voice detected. Check the mute button.","Aucune voix detectee. Verifie le bouton mute."},
        {"Transcribing locally on PS5...","Transcription locale sur PS5..."},
        {"Not enough memory for local dictation. You can still use the keyboard.","Memoire insuffisante pour la dictee locale. Le clavier reste disponible."},
        {"Whisper model missing or unreadable in assets/ggml-small-q5_1.bin","Modele Whisper absent ou illisible dans assets/ggml-small-q5_1.bin"},
        {"Transcription failed","Transcription echouee"},
        {"Dictation too long","Dictee trop longue"},
        {"Dictation ready: review, then press ☰ to send","Dictee prete : relis puis ☰ pour envoyer"},
        {"Codex wants to run a command","Codex veut exécuter une commande"},
        {"Codex wants to change files","Codex veut modifier des fichiers"},
        {"Review it carefully. Nothing runs unless you choose Allow.","Vérifie bien : rien ne s'exécute sans « Autoriser »."},
        {"Details are incomplete: this action can only be declined.","Détails incomplets : cette action peut seulement être refusée."},
        {"APPROVAL NEEDED","AUTORISATION REQUISE"},
        {"SIGN IN TO CHATGPT","CONNEXION À CHATGPT"},
        {"Finish signing in on your phone.","Termine la connexion sur ton téléphone."},
        {"1 · Open","1 · Ouvre"},
        {"2 · Enter the code","2 · Saisis le code"},
        {"Moving a file","Déplacement d'un fichier"},
        {"Moving files","Déplacement de fichiers"},
        {"Copying a folder","Copie d'un dossier"},
        {"Copying files","Copie de fichiers"},
        {"Copying a file","Copie d'un fichier"},
        {"Copying raw data","Copie de données brutes"},
        {"Deleting folders and their contents","Suppression de dossiers et de leur contenu"},
        {"Deleting files","Suppression de fichiers"},
        {"Deleting a file","Suppression d'un fichier"},
        {"Deleting an empty folder","Suppression d'un dossier vide"},
        {"Creating a folder","Création d'un dossier"},
        {"Creating or updating a file","Création ou mise à jour d'un fichier"},
        {"Creating a link","Création d'un lien"},
        {"Changing file permissions","Modification des permissions de fichiers"},
        {"Changing file ownership","Modification du propriétaire de fichiers"},
        {"Changing a process priority","Modification de la priorité d'un processus"},
        {"Working with an archive","Manipulation d'une archive"},
        {"Editing a file","Modification d'un fichier"},
        {"Transforming text","Transformation de texte"},
        {"Writing to a file","Écriture dans un fichier"},
        {"Stopping a process","Arrêt d'un processus"},
        {"Listing files","Liste des fichiers"},
        {"Listing processes","Liste des processus"},
        {"Searching for files","Recherche de fichiers"},
        {"Searching text in files","Recherche de texte dans des fichiers"},
        {"Reading a file","Lecture d'un fichier"},
        {"Checking storage space","Vérification de l'espace de stockage"},
        {"Computing checksums","Calcul de sommes de contrôle"},
        {"Reading system information","Lecture des informations système"},
        {"Running several commands","Exécution de plusieurs commandes"},
        // Short labels stay last: tr() matches the first English prefix.
        {"Dictate","Dicter"},
        {"Keyboard","Clavier"},
        {"Scroll","Défiler"},
        {"Choose","Choisir"},
        {"Confirm","Confirmer"},
        {"Decline","Refuser"},
        {"Allow","Autoriser"},
        {"Cancel","Annuler"},
        {"Send","Envoyer"},
        {"Sign in","Connexion"},
        {"Quit","Quitter"},
    };
    for(const auto& entry:entries)if(english.starts_with(entry.english))
        return std::string(entry.french)+std::string(english.substr(entry.english.size()));
    return std::string(english);
}
}
