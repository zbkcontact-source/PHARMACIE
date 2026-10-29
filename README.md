# PHARMACY

Plugin d'effets audio (VST3 + AU) : cinq outils de studio, chacun avec ON/OFF, une dose (macro), deux réglages avancés et un mix, plus une section CLEAN globale. Moteur suréchantillonné ×2, transparent quand rien n'est activé.

| Module | Outil | Réglages avancés | Ce que ça fait |
|---|---|---|---|
| WEED | TAPE | SPEED (7,5–30 ips), HISS | Saturation bande avec pré/dé-emphase, head bump et coupure des aigus selon la vitesse, compression douce, souffle |
| COKE | PUNCH | ATTACK (5–50 ms), SUSTAIN (−100/+100) | Transient designer, présence 4 kHz, soft clipper |
| KETA | GRAIN | SIZE (20–420 ms), SPRAY (0–1,5 s) | Nuage granulaire, gel du son à 100 % |
| LSD | COLOR | RATE (0,1–6 Hz), DEPTH | Chorus d'ensemble, phaser 6 étages, largeur stéréo |
| MDMA | ENRICH | TONE (warm ↔ bright), PULSE | Sub psychoacoustique, exciter, air, compression type OTT, glue, trémolo calé sur le tempo (croches) |

**CLEAN** (sous les modules) : de-harsh dynamique 2,5–6 kHz, chaleur dans les bas-médiums, aigus adoucis, leveler doux. Le pharmacien passe en lumière chaude et se détend.

En bas : **MIX** (dry/wet global) et **OUTPUT** (volume en dB).

## Obtenir le plugin et un lien à partager (sans rien installer pour coder)

Le projet se compile tout seul sur les serveurs de GitHub (Mac et Windows), gratuitement.

1. Crée un compte sur github.com si tu n'en as pas, puis installe **GitHub Desktop** (desktop.github.com).
2. Dézippe ce dossier, ouvre GitHub Desktop, fais *File > Add Local Repository*, choisis le dossier, puis *Create a repository*.
3. Clique **Publish repository** et **décoche "Keep this code private"** (sinon ton ami ne pourra pas télécharger).
4. Sur la page du repo sur github.com, onglet **Actions** : la compilation tourne (environ 10–15 min).
5. Quand c'est vert, va dans **Releases** à droite de la page d'accueil du repo. Le lien à partager est :
   `https://github.com/TON-PSEUDO/TON-REPO/releases/latest`

Chaque fois que tu modifies le code et que tu fais *Push* dans GitHub Desktop, une nouvelle version est publiée sur le même lien.

## Installer sur Mac

1. Télécharge `PHARMACY-Mac.pkg`.
2. Le plugin n'est pas signé par Apple : fais **clic droit > Ouvrir** sur le .pkg (pas double-clic). Si macOS bloque quand même : *Réglages Système > Confidentialité et sécurité > Ouvrir quand même*.
3. Installe. Le VST3 va dans `/Library/Audio/Plug-Ins/VST3`, l'AU dans `/Library/Audio/Plug-Ins/Components`.
4. Ableton : *Réglages > Plug-ins*, active **Use Audio Units v2** et/ou **Use VST3 Plug-In System Folders**, puis **Rescan**.
5. Glisse **PHARMACY** (rubrique Plug-ins, fabricant MARY) sur une piste ou un bus.

## Installer sur Windows

Dézippe `PHARMACY-Windows-VST3.zip` dans `C:\Program Files\Common Files\VST3`, puis Rescan dans Ableton.

## Compiler soi-même (optionnel)

Il faut CMake 3.22+ et Xcode (Mac) ou Visual Studio 2022 (Windows). JUCE est téléchargé automatiquement.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Les plugins sortent dans `build/Pharmacie_artefacts/Release/`.

Code : `Source/PluginProcessor.cpp` (le son), `Source/PluginEditor.cpp` (l'interface). Basé sur JUCE 8 (licence AGPLv3 pour un usage libre).
