# Composants tiers

* **PS5 native app boilerplate** — BlackBearReloaded, GPL-3.0-or-later.
  Renderer, plateforme VideoOut et outillage conservés avec leur attribution.
  Snapshot d'outillage issu d'OpenNOW PS5 (Portablelle), révision dans
  `upstream-lock.json`. La base du renderer et du transport provient de Botty+.
* **ps5-ai-cli** — saawant12 et contributeurs. Snapshot sous `vendor/ps5-ai-cli`.
  Consulter sa licence GPL et ses `LICENSES/` pour Codex (Apache-2.0), Dash,
  sbase, le SDK, Mio et les autres composants transitifs. Les adaptations de
  ce projet se trouvent dans `backend/` et `tools/prepare-backend.py`.
* **Recherche AudioIn PS5** — BlackBearReloaded, GPL-3.0. Référence des signatures
  et de la configuration de capture :
  https://github.com/blackbearreloaded/ps5-audio-decoding-research,
  `examples/native-audio-poc/microphone/src/main.cpp`.
* **whisper.cpp / ggml** — ggml-org et contributeurs, MIT.
  Version 1.8.3 ; licence dans `licenses/Whisper-MIT.txt`.
  Modèle Whisper `base` multilingue, provenance et empreinte dans le verrou.
* **JSON for Modern C++** — Niels Lohmann et contributeurs, MIT.
  Version 3.12.0, attribution et licence incluses dans `vendor/json.hpp`.
* **Manrope** — Mikhail Sharanda et contributeurs, SIL OFL 1.1.
  Police source sous `vendor/manrope`, licence sous `licenses/`.

Les stubs AudioIn sont des fournisseurs d'imports lors de l'édition de liens.
Ils ne reproduisent pas le module Sony et ne doivent pas être distribués dans
l'application ; le module AudioIn de la console fournit les fonctions réelles.
