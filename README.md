# PHARMACIE

Plugin d'effets audio (VST3 + AU) : quatre modules, LSD, Cocaïne, Weed, Acide, chacun avec un interrupteur et une dose de 0 à 100 %. Tout est automatisable dans Ableton.

| Module | Ce que ça fait au son |
|---|---|
| LSD | Chorus/flanger qui flotte, panoramique qui tourne, grosse réverb |
| Cocaïne | Pitch qui monte, compression écrasée, aigus agressifs, trémolo nerveux |
| Weed | Pitch qui descend, son étouffé, bande qui pleure, réverb cotonneuse |
| Acide | Filtre résonant façon 303, saturation, grain numérique, modulation en anneau |

En bas : **Mix** (dry/wet global) et **Sortie** (volume en dB).

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

1. Télécharge `PHARMACIE-Mac.pkg`.
2. Le plugin n'est pas signé par Apple : fais **clic droit > Ouvrir** sur le .pkg (pas double-clic). Si macOS bloque quand même : *Réglages Système > Confidentialité et sécurité > Ouvrir quand même*.
3. Installe. Le VST3 va dans `/Library/Audio/Plug-Ins/VST3`, l'AU dans `/Library/Audio/Plug-Ins/Components`.
4. Ableton : *Réglages > Plug-ins*, active **Use Audio Units v2** et/ou **Use VST3 Plug-In System Folders**, puis **Rescan**.
5. Glisse **PHARMACIE** (rubrique Plug-ins, fabricant MARY) sur une piste ou un bus.

## Installer sur Windows

Dézippe `PHARMACIE-Windows-VST3.zip` dans `C:\Program Files\Common Files\VST3`, puis Rescan dans Ableton.

## Compiler soi-même (optionnel)

Il faut CMake 3.22+ et Xcode (Mac) ou Visual Studio 2022 (Windows). JUCE est téléchargé automatiquement.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Les plugins sortent dans `build/Pharmacie_artefacts/Release/`.

Code : `Source/PluginProcessor.cpp` (le son), `Source/PluginEditor.cpp` (l'interface). Basé sur JUCE 8 (licence AGPLv3 pour un usage libre).
