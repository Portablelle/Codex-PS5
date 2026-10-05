# Codex PS5 0.0.2 — prototype natif

Assistant pour les fichiers d'une PS5 jailbreakée, avec une interface native à
la manette et une dictée locale. Ce projet n'est pas une application officielle
OpenAI ou Sony. La cible de validation est le firmware 13.00 avec
Relapse/kstuff/ShadowMount. La connexion ChatGPT, les commandes locales et le chat ont été testés sur cette console.

## Fonctionnement

Deux composants tournent **sur la PS5** :

* `assistant-service.elf` : moteur Codex 0.160.0 porté par
  [ps5-ai-cli](https://github.com/saawant12/ps5-ai-cli), adapté pour exposer
  `codex app-server` sur une socket **127.0.0.2:49322**. Les outils shell et les
  modifications de fichiers s'exécutent localement.
* `PPSA99105` : application graphique native, construite avec les outils de
  [OpenNOW PS5](https://github.com/Portablelle/opennow-ps5) et le renderer Botty+.
  Elle utilise le protocole JSONL de l'app-server, sans navigateur.

Le portail Botty peut démarrer le moteur avec l'option **Codex PS5**, une fois
les fichiers installés par FTP. Le moteur peut également être lancé avec le
chargeur ELF de Relapse ou Payload Manager. Le portail vérifie son empreinte,
mais ne télécharge pas automatiquement les mises à jour de cette application.

L'identifiant PPSA99105 est provisoire : vérifier qu'il est libre avant une
première installation. Les scripts de compilation n'installent rien sur console.

Connexion ChatGPT par code affiché sur la télévision et validé sur un téléphone.
Les réponses Codex nécessitent Internet et un compte disposant de l'accès Codex.
Le modèle sélectionné est **GPT-6.1 SOL**, avec l'effort de raisonnement **low**.
Il n'y a pas de connexion SSH dans l'application.

## Langue

Au lancement, la langue système de la PS5 détermine celle de Codex : français
(France ou Canada) pour une console en français, anglais pour toutes les autres
langues ou si la lecture du réglage échoue. Sur une console en français uniquement,
**R1** bascule entre français et anglais lorsque Codex et le micro sont au repos.
Le choix vaut pour la session ; le prochain lancement reprend la langue système.
L’interface, la dictée et les consignes de réponse suivent ce choix. La conversation
est conservée, et les messages déjà reçus restent dans leur langue d’origine.

## Dictée

Triangle démarre la capture AudioIn de l'utilisateur PS5, normalement le micro
DualSense. Un casque peut changer la route audio. Triangle termine la capture,
limitée à 30 secondes. Rond annule.

Whisper.cpp 1.8.3 transcrit dans la langue sélectionnée (français ou anglais) sur le CPU de la PS5, avec le modèle
multilingue `base`. L'audio reste en mémoire dans la console ; aucune API de
transcription ni clé supplémentaire. Le texte apparaît dans le prompt pour
relecture. **Options envoie le texte à Codex.** Le texte envoyé est donc traité
par le service Codex en ligne.

Le prototype réserve un heap natif de 1 Gio et active Flash Attention sur CPU.
Le premier essai sur PS5 a dépassé le précédent heap de 512 Mio lors de
l'initialisation de Whisper. La dictée refuse maintenant de charger le modèle
si le heap obtenu n'a pas au moins 768 Mio libres. Les autres allocations
conservent le repli vers l'allocateur système.

La capture AudioIn et la connexion ChatGPT ont été confirmées sur 13.00.
La transcription avec ce nouveau budget reste à confirmer sur console.
Le filtre d'énergie élimine le silence évident,
mais ne garantit pas l'absence de texte incorrect : relire la dictée.

## Installation du prototype

Après création du paquet avec `tools/package.py` :

1. Vérifier que l'identifiant PPSA99105 est libre sur la console.
2. Copier le dossier `bundle/PPSA99105` complet sous
   `/data/homebrew/PPSA99105`, selon le flux Folder de ShadowMount.
3. Copier `bundle/assistant-service.elf` dans
   `/data/codex-ps5/payloads/assistant-service/assistant-service.elf`.
   Sans ce dossier, le service cherche aussi un import Payload Manager nommé
   `assistant-service` ou `assistant-service-*` sous `/data/pldmgr/payloads`.
   Créer aussi `/data/ps5-ai-cli` : le chargeur ELF hérité utilise encore ce
   dossier pour son verrou ; les données Codex restent sous `/data/codex-ps5`.
4. Lancer le payload avec le chargeur ELF de Relapse (port 9021), ou avec
   Payload Manager si utilisé, puis ouvrir Codex PS5 depuis la bibliothèque.
   Le service doit être relancé après chaque redémarrage de la console.
5. Utiliser L1 pour l'authentification ChatGPT, puis tester d'abord une demande
   de lecture des fichiers du dossier de travail.

Le dossier de l'application doit contenir `assets/ggml-base.bin` (environ
148 Mo). Le ZIP Folder produit par la compilation seule n'inclut pas encore
le modèle ; utiliser le paquet final `codex-ps5-0.0.2.zip`.

## Commandes

| Touche | Action |
| --- | --- |
| Triangle | Démarrer / terminer la dictée |
| Carré | Ouvrir / fermer le clavier à la manette |
| Croix | Saisir une touche / confirmer un choix |
| Options | Envoyer le prompt |
| L1 | Se connecter à ChatGPT |
| R1 | Français / anglais, uniquement sur une PS5 en français et au repos |
| Rond | Annuler la dictée / interrompre Codex / refuser une action |
| Haut / bas | Parcourir la réponse, lorsque le clavier est fermé |

Les demandes d'approbation pour les commandes et changements de fichiers
s'affichent dans une fenêtre, avec défilement des détails. Le client accepte
automatiquement les commandes simples de consultation reconnues : `ls`, `cat`,
`head`, `tail`, `wc`, `grep`, `du`, `df`, `ps`, `pwd`, `uname`, `whoami`,
`readlink`, `ps5-tools` et la lecture de propriétés nommées avec `sysctl`.
Cette acceptation vaut uniquement pour la commande demandée, jamais pour toute
la session. Les scripts, chemins d'exécutables arbitraires, commandes composées,
redirections, expansions shell et demandes de permissions supplémentaires
restent soumis à confirmation, même si leur intention semble être une lecture.
Une commande absente de la demande n'est jamais acceptée automatiquement à partir
des détails d'une commande précédente. Les changements de
fichiers montrent le chemin et le diff transmis par Codex. Un détail manquant
ou trop long ne peut pas être approuvé dans ce prototype. Le choix initial est **Refuser**. L'application ne
réutilise pas une pression de Croix antérieure pour approuver une nouvelle action.

## Boîte à outils locale

Le runtime embarque le shell Dash, 83 utilitaires sbase et les commandes
natives `df`, `ps`, `sysctl` et `ps5-tools`. Le catalogue `ps5-tools` décrit
l'ensemble installé ; Codex choisit ses commandes selon la demande.

Les outils couvrent les fichiers et liens (`cp`, `mv`, `rm`, `find`, `ln`,
`readlink`), l'édition et le texte (`ed`, `sed`, `grep`, `sponge`, `strings`),
les permissions (`chmod`, `chown`, `chgrp`), les archives et contrôles
(`tar`, `dd`, `cksum`, les sommes SHA), les capacités (`df`, `du`), les
processus (`ps`, `kill`, `nice`, `renice`) et l'information système
(`uname`, `hostname`, `date`, `sysctl`). La liste source est dans
`backend/runtime-tools.txt`. Les options sont celles de sbase, pas toutes
les extensions GNU ; une option inconnue doit produire une erreur.
La création de liens dépend aussi du système de fichiers ; elle a été refusée
sur le dossier de test interne de la console, même avec la commande présente.

`ps` interroge la liste réelle des processus Prospero et affiche PID/nom.
Il n'expose pas les arguments ou des statistiques CPU. Un hook du kernel
ou un payload terminé après installation de ses hooks n'est pas nécessairement
un processus : leur absence dans `ps` ne prouve pas qu'ils sont inactifs.
`sysctl` lit des propriétés nommées ; les valeurs opaques nécessitent `-b`.
Cette boîte à outils ne constitue pas encore tout le userland FreeBSD :
`curl`, `netstat`, `git`, Python et le montage/démontage ne sont pas fournis.

## Accès aux fichiers et limites

État et identifiants Codex : `/data/codex-ps5/home/.codex`.
Dossier de travail initial : `/data/codex-ps5/workspace`.
Le moteur dispose des droits du payload ; il n'existe **pas de sandbox système**.
La politique Codex `untrusted` demande l'approbation des actions non considérées
comme sûres. Les consignes sur `/data`, `/mnt/usb*` et les fichiers système sont
des instructions au modèle, pas une restriction imposée par le système.
La socket locale n'est pas authentifiée : les applications locales ayant accès
au réseau de la console doivent être considérées comme de confiance.

Premier prototype : une conversation active avec messages chronologiques,
réponse en streaming, états de réflexion/réponse et saisie à la manette.
Haut/bas parcourt les messages de la session (64 messages récents, jusqu'à
1 Mio de texte). Pas encore de reprise de conversation après
fermeture, de navigateur de fichiers ou de vue de diff complète. Une déconnexion
arrête l'app-server de cette session ; une action commencée peut continuer
ou avoir déjà modifié des fichiers. Elle n'est pas annulée. Les prompts ne sont jamais renvoyés automatiquement.

## Compilation

Docker doit tourner sur un **hôte Linux distant** ; les scripts du poste de travail
synchronisent les sources et récupèrent les artefacts. Définir `PS5_BUILD_HOST`
(alias SSH) et `PS5_BUILD_DIRECTORY` selon l'environnement de compilation.

```sh
bash tools/test.sh
# Optionnel avec Codex 0.160.0 installé sur le poste :
python3 tests/app_server_handshake.py
PS5_BUILD_HOST=build-host bash tools/build-remote.sh
PS5_BUILD_HOST=build-host bash tools/build-backend-remote.sh
python3 tools/fetch-model.py
python3 tools/package.py
```

La première compilation du moteur télécharge le SDK et Rust puis compile Codex ;
elle est nettement plus longue que celle de l'interface. Prévoir plusieurs Go de
RAM et suffisamment d'espace disque. L'hôte doit permettre `sudo -n docker`,
avoir Python, Git, npm et rsync. Le builder natif utilise l'image Linux
`opennow-ps5-builder:latest`, à construire avec
`docker build -f tools/Dockerfile.native -t opennow-ps5-builder:latest .`
sur l'hôte Linux si elle est absente.

`upstream-lock.json` et les verrous de ps5-ai-cli fixent les révisions. Le modèle
Whisper est téléchargé séparément et vérifié par SHA-256, puis inclus par le
script de packaging. Les poids ne sont pas versionnés.

## Attribution

Voir `THIRD_PARTY.md`, `LICENSE`, `licenses/` et les licences du snapshot
`vendor/ps5-ai-cli`. Le source des adaptations et les scripts de compilation sont fournis.

## Mises à jour via Botty+

Avec Codex coché, LAUNCH installe et met à jour le titre natif, Whisper et le moteur avant son démarrage. Les téléchargements sont vérifiés par blocs de 1 Mio. Le dossier de travail et la connexion ChatGPT sont conservés. Fermer les applications natives avant LAUNCH ; les installations précédentes restent en sauvegarde. Le moteur 0.0.2 expose un contrôle local sur 127.0.0.1:49323 pour son état et son arrêt propre à vide. Le moteur antérieur nécessite un redémarrage complet après la première mise à jour.
