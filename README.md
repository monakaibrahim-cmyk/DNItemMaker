# Dragon Nest Item Maker (DNItemMaker)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/23)
[![Platform](https://img.shields.io/badge/Platform-Windows%20x64-0078D6.svg?style=flat-square&logo=windows)](https://microsoft.com)
[![Framework](https://img.shields.io/badge/GUI-Native%20MFC%20Win32-purple.svg?style=flat-square)](https://learn.microsoft.com/en-us/cpp/mfc/mfc-desktop-applications)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](LICENSE)
[![Build Status](https://img.shields.io/badge/Build-Passing%20(Release%20x64)-brightgreen.svg?style=flat-square)](#building)

**DNItemMaker** is a high-performance, studio-grade Dragon Nest binary table (`.dnt`) editor and 3D item asset workstation. Engineered from the ground up in modern **ISO C++23** with authentic native Windows MFC architecture, it combines high-speed multi-threaded parsing, 1:1 round-trip binary fidelity, interactive 3D model inspection, and SQLite crash-recovery journaling.

---

## Key Features

* **High-Speed Binary DNT Engine**: Direct memory-mapped parsing and writing across 60+ joined item tables (`itemtable*.dnt`, `itemtable_weapon.dnt`, `itemtable_parts.dnt`), loading 160,000+ items and 230,000+ strings in under 350 ms.
* **100% Round-Trip Binary Fidelity**: Verified byte-exact binary compatibility ensuring exported files never corrupt client or server game data.
* **Pure Classic MFC UI Design**: Clean, responsive native Win32 controls (`CTabCtrl`, `BS_GROUPBOX`, `CListCtrl` with virtual owner-data mode, high-DPI awareness).
* **Blender-Style 3D Viewport**: Built-in Direct3D software rasterizer with depth testing, Blender-like 3D ground plane tiles (Red X / Blue Z axes), rotating XYZ orientation gizmo, and visual skin/mesh cloning.
* **Real-Time Localization & MID Allocation**: Live preview of `uistring.xml` entries with parameter interpolation (`{0}`, `{1}`), color markup insertion (`#y`, `#r`, `#b`, `#g`, `#v`), and smart MID conflict detection.
* **Stat Archetypes & Automated Math Scaling**: 10 potential state slots with pre-configured archetypes (Phys DPS, Magic DPS, Tank, Critical, Support) and non-linear polynomial leveling curves.
* **Automation & Batch Studio**:
  * **Tier Set Generator**: Generate complete Normal through Ancient tier progressions in one click.
  * **Class Series Generator**: Clone equipment across all 10 Dragon Nest base classes with automatic stat adaptations.
  * **Mass Multi-Item Editor**: Filter and batch-modify thousands of items simultaneously (scale stats, sanitize, adjust level limits).
* **Folder-Based Non-Destructive Saving**: Save modified tables and strings directly into a separate target directory preserving game directory structure (`dnt/`, `uistring/`), leaving original game files untouched.
* **SQLite Crash Recovery Journal**: Real-time write-ahead logging (WAL) auto-saves drafts so work is never lost across crashes or unexpected power failures.
* **Zero External Services**: Pure in-memory caching for 50x50 icon tiles and textures—no Redis daemon or background processes required.
* **Single-Instance Enforcement**: Win32 mutex prevents accidental concurrent runs from overwriting files simultaneously.

---

## Architectural Layout

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ DNItemMaker Application Architecture                                        │
├─────────────────────────────────────────────────────────────────────────────┤
│  Native Win32 MFC Interface Layer (CMainFrame, CTabCtrl, CModelPreviewWnd)  │
├─────────────────────────────────────────────────────────────────────────────┤
│  Workspace Engine & In-Memory Native Cache (std::unordered_map, mutex)      │
├──────────────────────┬──────────────────────┬───────────────────────────────┤
│ Data Management      │ Graphics Engine      │ System Infrastructure         │
│ • DNTReader (Binary) │ • DDSParser (DXT1-5) │ • CrashHandler (MiniDump)     │
│ • ItemManager (60+T) │ • MeshParser (.msh)  │ • CrashRecoveryDB (SQLite WAL)│
│ • UIStringManager    │ • SkinParser (.skn)  │ • Thread-Safe Logger          │
│   (230K+ XML Strings)│ • 3D MeshRenderer    │                               │
└──────────────────────┴──────────────────────┴───────────────────────────────┘
```

---

## Editor Tabs Walkthrough

| Tab | Name | Capabilities |
| :---: | :--- | :--- |
| **1** | **Properties** | Basic attributes (Rank, Icon Index, Level, Hero Level, Max Stack), Economy (Sell Copper, Gear Score, Durability, Cash/Storage flags), and Job Class restriction matrix. |
| **2** | **Localization** | Live UIString editor, automatic next-MID allocator, search & bind existing MIDs, quick color tag formatting (`#y`, `#r`, `#b`, `#g`, `#v`, `#w`), and collision warnings. |
| **3** | **Stats & Scaling** | 10 State slots with probability, min, and max values. Archetype presets, automated mathematical leveling curves, spread tools, and copy/paste clipboard. |
| **4** | **3D & Visuals** | Interactive software 3D viewport (Yaw/Pitch orbit, Zoom, Pan, Auto-spin, Blender ground grid, XYZ gizmo), and visual appearance cloner (donor weapon/skin model copying). |
| **5** | **Raw DNT** | Low-level DNT column and cell inspector across all joined tables. Direct cell editing, type inspections, and TSV clipboard export. |
| **6** | **Automation** | Batch tier set generator, job class series generator, and mass multi-item property/stat editor. |
| **7** | **Tools & Packs** | Full workspace structural audit, 116-table binary round-trip verification, native memory cache clearing, minidump diagnostics, and JSON Item Pack import/export. |

---

## Building from Source

### Prerequisites

* Windows 10 or 11 (64-bit)
* Visual Studio 2022 (v143) or Visual Studio 2026 Preview (v144)
  * Workload: **Desktop development with C++**
  * Component: **C++ MFC for latest v143/v144 build tools (x86 & x64)**
* CMake 3.25+

### Build Instructions

1. **Clone the repository**:
   ```bash
   git clone --recursive -b development https://github.com/monakaibrahim-cmyk/DNItemMaker.git
   cd DNItemMaker
   ```

2. **Build using the automated script**:
   ```powershell
   .\build.bat
   ```

3. **Or build with CMake**:
   ```powershell
   cmake -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ```

The compiled binary will be located at:
```
build\Release\DNItemMaker.exe
```

---

## Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Ctrl + O` | Open Resource Workspace Folder |
| `F5` | Reload Workspace from Disk |
| `Ctrl + S` | Save Modified Tables to Folder... |
| `Ctrl + N` | Create New Item... |
| `Ctrl + D` | Duplicate Selected Item... |
| `Ctrl + Enter` | Apply Current Item Edits |
| `Ctrl + Z` | Undo Last Session Action |
| `Ctrl + I` | Browse Icon Atlas (`itemicon*.dds`) |
| `Ctrl + F` | Focus Item Search Box |
| `Ctrl + 1` .. `Ctrl + 7` | Switch Editor Tabs (1: Properties ... 7: Tools & Packs) |

---

## Headless Verification Mode

To verify that the engine parses and writes your game's binary tables with 100% byte fidelity without launching the GUI:

```powershell
build\Release\DNItemMaker.exe --verify-headless "D:\GameRes\resource"
```

---

## Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of conduct, branch conventions (`development` branch), C++23 standards, and pull request procedures.

---

## License

DNItemMaker is open-source software licensed under the [MIT License](LICENSE).
Dragon Nest is a registered trademark of Eyedentity Games. This project is an independent tool designed for game researchers and server administrators.
