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
// English source strings; French translations retain the console language support.
inline std::string tr(std::string_view english) {
    if(!isFrench())return std::string(english);
    struct Entry {std::string_view english,french;};
    static constexpr Entry entries[]={
        {"Dictate or type a request to work on PS5 files.","Dicte ou saisis une demande pour agir sur les fichiers de la PS5."},
        {"Prompt too long: the new dictation was not added","Prompt trop long : la nouvelle dictee n'a pas ete ajoutee"},
        {"Sign in to ChatGPT on your phone","Connexion ChatGPT sur ton telephone"},
        {"Your prompt...","Ton prompt..."},
        {"You","Toi"},
        {"Triangle: dictate   Square: keyboard   Options: send   L1: sign in   Circle: cancel","Triangle : dicter   Carre : clavier   Options : envoyer   L1 : connexion   Rond : annuler"},
        {"Codex needs your approval","Codex demande ton autorisation"},
        {"  Decline          [ Allow ]","  Refuser          [ Autoriser ]"},
        {"[ Decline ]          Allow","[ Refuser ]          Autoriser"},
        {"Up/down: read. Left/right: choose. Cross: confirm. Circle: decline.","Haut/bas : lire. Gauche/droite : choisir. Croix : confirmer. Rond : refuser."},
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
        {"L1: sign in to ChatGPT","L1 : connexion au compte ChatGPT"},
        {"Codex is running a command...","Codex exécute une commande..."},
        {"Codex is replying...","Codex répond..."},
        {"File: ","Fichier : "},
        {"Completed","Termine"},
        {"Interrupted","Interrompu"},
        {"Failed: ","Echec: "},
        {"Sign-in failed: ","Connexion echouee: "},
        {"Engine unavailable: launch assistant-service in Payload Manager","Moteur absent : lance assistant-service dans Payload Manager"},
        {"Triangle: dictate","Triangle : dicter"},
        {"Recording locally (30 s max). Triangle: finish","Enregistrement local (30 s max). Triangle : terminer"},
        {"Unable to start microphone","Impossible de demarrer le micro"},
        {"PS5 user unavailable","Utilisateur PS5 indisponible"},
        {"Microphone unavailable. Check your controller microphone.","Micro indisponible. Verifie le micro de la manette."},
        {"Dictation cancelled","Dictee annulee"},
        {"Microphone capture error","Erreur de capture du micro"},
        {"No voice detected. Check the mute button.","Aucune voix detectee. Verifie le bouton mute."},
        {"Transcribing locally on PS5...","Transcription locale sur PS5..."},
        {"Not enough memory for local dictation. You can still use the keyboard.","Memoire insuffisante pour la dictee locale. Le clavier reste disponible."},
        {"Whisper model missing or unreadable in assets/ggml-base.bin","Modele Whisper absent ou illisible dans assets/ggml-base.bin"},
        {"Transcription failed","Transcription echouee"},
        {"Dictation too long","Dictee trop longue"},
        {"Dictation ready: review, then press Options to send","Dictee prete : relis puis Options pour envoyer"},
        {"R1: language (French / English)","R1 : langue (francais / anglais)"},
    };
    for(const auto& entry:entries)if(english.starts_with(entry.english))
        return std::string(entry.french)+std::string(english.substr(entry.english.size()));
    return std::string(english);
}
}
