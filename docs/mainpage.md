/**
 * @mainpage Dragon Nest Item Maker - Developer Documentation
 *
 * @section intro_sec Introduction
 * Welcome to the developer and contributor documentation for **DNItemMaker** (Dragon Nest Item Maker).
 * This project is a production-grade, open-source Dragon Nest binary table (`.dnt`) editor and 3D workstation
 * implemented in modern **ISO C++23** with authentic native Windows MFC architecture.
 *
 * @section arch_sec Architecture Overview
 * The codebase is structured into five distinct, loosely coupled layers:
 *
 * - @ref DNItemMaker::Data "Data Layer":
 *   - @ref DNItemMaker::Data::DNTReader "DNTReader" - High-speed binary `.dnt` parser and serializer with 100% round-trip fidelity.
 *   - @ref DNItemMaker::Data::ItemManager "ItemManager" - Unified data manager joining 60+ item tables, weapon details, and parts.
 *   - @ref DNItemMaker::Data::UIStringManager "UIStringManager" - Fast XML parser for `uistring.xml` with live token formatting and MID allocation.
 *   - @ref DNItemMaker::Data::CrashRecoveryDB "CrashRecoveryDB" - Real-time SQLite WAL recovery journal for dirty drafts.
 *
 * - @ref DNItemMaker::Engine "Engine Layer":
 *   - @ref DNItemMaker::Engine::WorkspaceEngine "WorkspaceEngine" - Asynchronous resource worker, in-memory asset caching, undo/redo stack.
 *
 * - @ref DNItemMaker::Graphics "Graphics Layer":
 *   - @ref DNItemMaker::Graphics::DDSParser "DDSParser" - Pure software DirectDraw Surface (DDS) decompressor supporting DXT1, DXT3, DXT5, and RGBA.
 *   - @ref DNItemMaker::Graphics::MeshParser "MeshParser" - Eternity Engine binary 3D mesh (`.msh`) parser.
 *   - @ref DNItemMaker::Graphics::SkinParser "SkinParser" - Character/equipment skin file (`.skn`) parser.
 *   - @ref DNItemMaker::Graphics::MeshRenderer "MeshRenderer" - Fast scanline software rasterizer featuring z-buffering, Blender-style 3D ground plane tiles, and interactive XYZ orientation gizmo.
 *
 * - @ref DNItemMaker::UI "UI Layer":
 *   - @ref DNItemMaker::UI::CMainFrame "CMainFrame" - Classic MFC frame window hosting 7 tabbed modules, virtual list controls, and native Win32 group boxes.
 *   - @ref DNItemMaker::UI::CModelPreviewWnd "CModelPreviewWnd" - Interactive 3D viewport window handling mouse orbit, zoom, pan, and lighting.
 *   - @ref DNItemMaker::UI::CIconPickerDlg "CIconPickerDlg", @ref DNItemMaker::UI::CNewItemDlg "CNewItemDlg", @ref DNItemMaker::UI::CUIStringSearchDlg "CUIStringSearchDlg", @ref DNItemMaker::UI::CAboutDlg "CAboutDlg" - Native dialog interfaces.
 *
 * - @ref Common "Common Layer":
 *   - @ref Common::Crash::Initialize "CrashHandler" - Structured Exception Handling (SEH) and MiniDumpWriteDump integration.
 *   - @ref Common::Logs "Logs" - Lock-free and thread-safe file/console diagnostic logging.
 *
 * @section contrib_guide Contributor Guide
 * 1. **Branch Model**: All active development and pull requests target the `development` branch.
 * 2. **Coding Standards**: Use standard C++23 features (`std::format`, `std::span`, `std::jthread`). Avoid external daemons.
 * 3. **Binary Fidelity**: Before submitting changes, always execute `DNItemMaker.exe --verify-headless <path>` to ensure 1:1 round-trip verification passes on all tables.
 *
 * For full guidelines, see the repository [CONTRIBUTING.md](file:///C:/Users/Canonical/Desktop/DNItemMaker/CONTRIBUTING.md).
 */
