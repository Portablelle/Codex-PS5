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
inline const char* code(){return isFrench()?"fr":"en";}
inline std::string tr(std::string_view french) {
    if(isFrench())return std::string(french);
    struct Entry {std::string_view french,english;};
    static constexpr Entry entries[]={
        {"Toi","You"},
        {"Dicte ou saisis une demande pour agir sur les fichiers de la PS5.","Dictate or type a request to work on PS5 files."},
        {"Prompt trop long : la nouvelle dictee n'a pas ete ajoutee","Prompt too long: the new dictation was not added"},
        {"Connexion ChatGPT sur ton telephone","Sign in to ChatGPT on your phone"},
        {"Ton prompt...","Your prompt..."},
        {"Triangle : dicter   Carre : clavier   Options : envoyer   L1 : connexion   Rond : annuler","Triangle: dictate   Square: keyboard   Options: send   L1: sign in   Circle: cancel"},
        {"Codex demande ton autorisation","Codex needs your approval"},
        {"  Refuser          [ Autoriser ]","  Decline          [ Allow ]"},
        {"[ Refuser ]          Autoriser","[ Decline ]          Allow"},
        {"Haut/bas : lire. Gauche/droite : choisir. Croix : confirmer. Rond : refuser.","Up/down: read. Left/right: choose. Cross: confirm. Circle: decline."},
        {"espace","space"},
        {"effacer","delete"},
        {"maj","shift"},
        {"fermer","close"},
        {"Demarrage du moteur...","Starting engine..."},
        {"Moteur deconnecte. La requete ne sera pas renvoyee.","Engine disconnected. The request will not be resent."},
        {"Codex réfléchit...","Codex is thinking..."},
        {"Codex reprend...","Codex is resuming..."},
        {"Action suivante a confirmer","Next action needs approval"},
        {"\nDossier : ","\nDirectory: "},
        {"\nRacine demandee : ","\nRequested root: "},
        {"\nMotif : ","\nReason: "},
        {"\nPermissions : ","\nPermissions: "},
        {"Details incomplets ou trop longs : refus uniquement.\n","Details incomplete or too long: decline only.\n"},
        {"Action en attente de confirmation","Action awaiting approval"},
        {"Interaction non prise en charge par le prototype PS5","Interaction unsupported by the PS5 prototype"},
        {"Erreur: ","Error: "},
        {"Connecte-toi sur un telephone avec le code affiche.","Sign in on your phone using the displayed code."},
        {"Pret","Ready"},
        {"L1 : connexion au compte ChatGPT","L1: sign in to ChatGPT"},
        {"Codex exécute une commande...","Codex is running a command..."},
        {"Codex répond...","Codex is replying..."},
        {"Fichier : ","File: "},
        {"Termine","Completed"},
        {"Interrompu","Interrupted"},
        {"Echec: ","Failed: "},
        {"Connexion echouee: ","Sign-in failed: "},
        {"Moteur absent : lance assistant-service dans Payload Manager","Engine unavailable: launch assistant-service in Payload Manager"},
        {"Triangle : dicter","Triangle: dictate"},
        {"Enregistrement local (30 s max). Triangle : terminer","Recording locally (30 s max). Triangle: finish"},
        {"Impossible de demarrer le micro","Unable to start microphone"},
        {"Utilisateur PS5 indisponible","PS5 user unavailable"},
        {"Micro indisponible. Verifie le micro de la manette.","Microphone unavailable. Check your controller microphone."},
        {"Dictee annulee","Dictation cancelled"},
        {"Erreur de capture du micro","Microphone capture error"},
        {"Aucune voix detectee. Verifie le bouton mute.","No voice detected. Check the mute button."},
        {"Transcription locale sur PS5...","Transcribing locally on PS5..."},
        {"Memoire insuffisante pour la dictee locale. Le clavier reste disponible.","Not enough memory for local dictation. You can still use the keyboard."},
        {"Modele Whisper absent ou illisible dans assets/ggml-base.bin","Whisper model missing or unreadable in assets/ggml-base.bin"},
        {"Transcription echouee","Transcription failed"},
        {"Dictee trop longue","Dictation too long"},
        {"Dictee prete : relis puis Options pour envoyer","Dictation ready: review, then press Options to send"},
        {"R1 : langue (francais / anglais)","R1: language (French / English)"},
    };
    for(const auto& entry:entries)if(french.starts_with(entry.french))
        return std::string(entry.english)+std::string(french.substr(entry.french.size()));
    return std::string(french);
}
}
