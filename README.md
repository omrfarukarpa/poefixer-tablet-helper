# Tablet Helper (PoeFixer)

**Version 1.0.0** — a [PoeFixer](https://github.com/POEFixer/PoeFixer) plugin for
**Path of Exile 2** that highlights **Precursor Tablets** in every open item
window (inventory, stash, special Tablet stash, guild stash, and merchant),
colored per type and filterable by their bonuses and remaining uses.

This is a C++ port of the ExileCore2 **Tablet Helper** plugin to PoeFixer,
structured after the QuickStash plugin (same Plugin SDK v6). The bonus catalog
and the detection/classification logic are ported from the original.

## Screenshots

Star highlights with per-tablet uses-left across a Tablet stash tab, plus the
settings panel:

![Star highlights and settings](https://i.hizliresim.com/21i8ybm.png)

Per-type configuration — Border style, every Breach highlighted, and the bonus
picker with a search box:

![Per-type config and bonus picker](https://i.hizliresim.com/t583lxh.png)

Bonus filtering — only tablets carrying the selected bonuses are highlighted,
with the matched bonus names on the label:

![Bonus filtering in action](https://i.hizliresim.com/fcr6gxp.png)

## Features

- **Detects Precursor Tablets** by their metadata path (`TowerAugment`) and
  classifies each into one of 7 types: Irradiated, Breach, Delirium, Abyss,
  Ritual, Overseer, Temple.
- **Per-type highlighting** — every type has its own on/off toggle and color.
  Out of the box, every tablet is highlighted in its type color.
- **Bonus filtering** — pick specific bonuses (from the ported catalog) per type;
  only tablets carrying enough of them are highlighted. Leave the list empty to
  highlight all tablets of that type.
- **Uses-left filter** — require a minimum number of remaining uses.
- **Global scope** — an extra config block whose filter applies across every
  tablet type (opt-in / disabled by default).
- **Highlight styles** — Border (item frame), Cross, or Star, with adjustable
  thickness.
- **Labels** — type name, optional uses-left count and matched-bonus names, with
  optional background box, outline, color mode, and scale.
- **Crash-safe settings** — a corrupt `settings.json` never takes down the host.

## Requirements

- PoeFixer with Plugin SDK **v6** support (same as QuickStash).
- Path of Exile 2 (Windows).

## Install

1. Build (below) or grab `TabletHelper.dll`.
2. Copy it into your PoeFixer install:
   ```
   <PoeFixer>\Plugins\TabletHelper\TabletHelper.dll
   ```
3. Start PoeFixer and enable **Tablet Helper** under **Plugins**.
4. Settings are saved to `Plugins\TabletHelper\config\settings.json`.

> Copying the DLL fails with "Device or resource busy" while PoeFixer is
> running — close PoeFixer first to overwrite a loaded DLL.

## Usage

Open any item window (inventory, a stash tab, the special Tablet stash, guild
stash, or a merchant). Matching tablets are highlighted immediately. Configure
colors and filters per type in the plugin's settings panel.

### The "Read item mods" option (important)

Uses-left and bonus filtering require reading each tablet's mods. This is the one
risky host call: on some items a bad entity address can fault the game
uncatchably. Mitigations are built in — mods are only ever read on confirmed
tablets, the address is probed with a cheaper call first, and each tablet is read
once and cached.

- **Default: ON.** If you ever get a crash when tablets are on screen, turn
  **Read item mods** off in the settings, or set `"read_mods": false` in
  `config/settings.json` and restart.
- **With it off**, tablets are still highlighted **by type** (color + type
  label); only uses-left and bonus filters are inactive.
- On first run, it's safest to check one tablet on screen before opening a full
  stash of them.

## Build from source

**Requirements:** Visual Studio 2022+ (MSVC v145), Windows SDK 10.0, C++20.

```bash
MSBuild.exe TabletHelper.sln -p:Configuration=Release -p:Platform=x64
```

Output: `bin\Release\TabletHelper.dll`. Only `TabletHelper.cpp` and the vendored
ImGui translation units compile; all plugin logic is header-only under `config/`
and `game/`.

## Project layout

```
TabletHelper.cpp            Plugin entry: lifecycle, scan cadence, overlay draw, settings UI
config/Settings.h           Crash-safe JSON settings (per-type config model)
game/TabletTypes.h          Tablet detection + 7-type classification + normalization
game/TabletBonusCatalog.h   Bonus catalog (Id -> label/category), ported 1:1
game/PanelDetector.h        On-screen geometry + hybrid item-rect resolver (from QuickStash)
game/TabletScanner.h        Enumerate inventories, filter tablets, read mods (cached)
game/TabletMatch.h          Match engine: own-type + Global -> color + labels
game/HighlightRenderer.h    Border/Cross/Star + labels via GetForegroundDrawList
sdk/                        PoeFixer Plugin SDK headers (v6)
imgui/, third_party/        Vendored (Dear ImGui, nlohmann/json)
```

## Credits & disclaimer

Ported from the original ExileCore2 **Tablet Helper** plugin; bonus catalog and
detection logic derive from it. This is an unofficial, third-party tool that
reads game memory. Use of third-party tools may violate the Path of Exile Terms
of Service and could put your account at risk. **Use at your own risk.** Not
affiliated with or endorsed by Grinding Gear Games or the PoeFixer authors.

Bundled third-party components keep their own licenses: **Dear ImGui** (MIT),
**nlohmann/json** (MIT). The **PoeFixer Plugin SDK** headers (`sdk/`) belong to
the PoeFixer project and are included only so the plugin can be built.
