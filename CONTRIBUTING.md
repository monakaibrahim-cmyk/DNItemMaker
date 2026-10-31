# Contributing to DNItemMaker

Thank you for your interest in contributing to **DNItemMaker**! This project is an open-source, high-performance Dragon Nest item editor and binary table studio built with modern C++23, native Win32/MFC, and an internal software Direct3D rasterizer.

---

## 1. Branch Strategy

We follow a Git flow model where active development takes place on the `development` branch:

* **`master` / `main`**: Production-ready, tagged releases only.
* **`development`**: Active integration branch for new features, optimizations, and bug fixes.
* **`feature/<name>`** or **`fix/<name>`**: Topic branches created off `development`.

When submitting pull requests, **always target the `development` branch**.

```bash
git checkout development
git pull origin development
git checkout -b feature/my-cool-feature
```

---

## 2. Architecture Overview

Before making changes, familiarize yourself with the 5 core layers of the codebase:

```
src/
├── Common/          # Core cross-cutting infrastructure
│   ├── Crash/       # MiniDumpWriteDump handler, SEH filter, signal interception
│   └── Logs/        # Fast thread-safe file & console logger
├── Data/            # Pure data management & binary format parsers
│   ├── DNTReader    # High-speed Dragon Nest binary table reader (1:1 round-trip)
│   ├── ItemManager  # Unified inventory item model, 60+ DNT table cross-joins
│   ├── UIStringManager # uistring.xml fast parser, allocator & MID collision detector
│   └── CrashRecoveryDB # SQLite WAL journal for real-time draft crash recovery
├── Engine/          # Workspace business logic
│   └── WorkspaceEngine # Async resource loading, memory caches, undo/redo stack
├── Graphics/        # 3D software rendering & image processing
│   ├── DDSParser    # DXT1/DXT3/DXT5 and uncompressed RGBA texture decoder
│   ├── MeshParser   # .msh 3D geometry reader (submeshes, vertex buffers)
│   ├── SkinParser   # .skn skin binder & bone weights
│   └── MeshRenderer # High-performance scanline rasterizer (depth-buffer, tiles, XYZ gizmo)
└── UI/              # Native Win32/MFC user interface
    ├── App          # Application entry point, single-instance mutex enforcement
    ├── MainFrame    # Classic MFC 7-tab editor window, virtual list, group boxes
    ├── ModelPreviewWnd # Interactive 3D viewport control (yaw, pitch, zoom, pan)
    └── ModalDialogs # Atlas icon picker, New item creator, UIString lookup, About
```

---

## 3. Development Prerequisites

* **OS**: Windows 10 / 11 (x64)
* **Compiler**: Microsoft Visual C++ (MSVC) 2022 v143 or Visual Studio 2026 Preview v144+ with C++23 support (`/std:c++23`)
* **MFC**: Microsoft Foundation Classes for C++ (included in "Desktop development with C++" in Visual Studio Installer)
* **Build System**: CMake 3.25+
* **Documentation**: Doxygen 1.9+ (optional, for building HTML docs)

---

## 4. Building the Project

We provide a convenient one-click batch script `build.bat`:

```powershell
# Clean build (Release x64)
.\clean.bat
.\build.bat
```

Or configure and build manually via CMake:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The resulting executable will be placed in `build/Release/DNItemMaker.exe`.

### Running Verification Tests

DNItemMaker includes a headless 1:1 round-trip verification mode that verifies byte-exact binary integrity across all 116 DNT tables in your game directory:

```powershell
build\Release\DNItemMaker.exe --verify-headless "D:\GameRes\resource"
```

---

## 5. Coding Standards

* **Language Standard**: ISO C++23 (`/std:c++23`). Use modern language features (`std::format`, `std::string_view`, `std::span`, `std::jthread`, `std::ranges`) wherever appropriate.
* **UI Design Philosophy**: Authentic, clean native MFC UI. Standard Win32 controls (`CButton`, `CTabCtrl`, `BS_GROUPBOX`, `COLOR_BTNFACE`). Avoid owner-drawn dark-mode hacks or custom title bars.
* **Memory Safety & RAII**: Zero raw memory ownership. Use smart pointers (`std::unique_ptr`, `std::shared_ptr`), `std::vector`, or RAII wrappers.
* **No External Daemons**: Keep the tool lightweight and standalone. Do not introduce dependencies that require background services (e.g. Redis). All caching must be thread-safe in-memory.
* **Doxygen Comments**: All public interfaces, classes, methods, and important member structs must have standard Doxygen docstrings (`///` format) explaining parameters, return values, thread safety, and behavior.

---

## 6. Documentation (Doxygen)

To generate the modern developer documentation:

```powershell
doxygen Doxyfile
```

The generated HTML documentation will be located in `docs/html/index.html`.

---

## 7. Submitting a Pull Request

1. Fork the repository and create your feature branch from `development`.
2. Ensure the code compiles cleanly with 0 warnings on MSVC Release x64.
3. Test your changes against real game files and verify headless round-trip passes.
4. Push your commits with clear, descriptive commit messages.
5. Open a Pull Request targeting `development` with a summary of changes, motivation, and test results.
