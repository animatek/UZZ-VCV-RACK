# Repository Guidelines

## Project Structure & Module Organization
`src/` contains the plugin entry point (`plugin.cpp`), shared declarations (`plugin.hpp`), and module logic such as `UZZ.cpp`; add new voices or utilities here and keep helpers in separate headers to avoid bloating translation units. UI art and editable layouts live in `res/` (`*.svg`, `*.afdesign`). Build intermediates go to `build/`, and releases land in `dist/`. Update `plugin.json` and the root `Makefile` whenever you add modules or parameters.

## Build, Test, and Development Commands
- `RACK_DIR=/path/to/RackSDK make`: compile the plugin against the Rack SDK.
- `RACK_DIR=/path/to/RackSDK make clean`: remove intermediates when toolchains or headers change.
- `RACK_DIR=/path/to/RackSDK make dist`: produce the distributable archive in `dist/Animatek/`.
- `RACK_DIR=/path/to/RackSDK make install`: copy the build into your local Rack plugins directory for hands-on testing.
Export `RACK_DIR` once per session or wrap these commands in a shell alias for faster iteration.

## Coding Style & Naming Conventions
Use modern C++ (Rack provides C++17). Indent with 4 spaces; tabs are acceptable only where Rack macros require them. Name classes and structs in `UpperCamelCase`, free functions and locals in `snake_case`, and constants in `SCREAMING_SNAKE_CASE` (`UI::TRIG_RIGHT_PAD`). Group related constants inside namespaces to mirror the existing layout helpers. Prefer `constexpr`, `std::array`, and span-like helpers over raw literals. Keep comments concise, English-first, and near the code they clarify.

## Testing Guidelines
There is no automated suite; validate every change inside VCV Rack. Exercise clock handling, triggers, and randomization at multiple tempos and pattern lengths. When you fix a regression, save a minimal `.vcv` patch under `res/patches/` (create if absent) and outline the manual test steps in the pull request.

## Commit & Pull Request Guidelines
Use Conventional Commits subjects (`feat: add trigger probability control`) written in the imperative and under 72 characters. Work on topic branches, rebase before opening a pull request, and include a concise summary, testing notes, before/after screenshots or GIFs for UI tweaks, and links to tracked issues. Request review from the module owner or asset designer.

## Asset & UI Workflow
Editable panels reside in `res/*.afdesign`; export updated SVGs with the same filename using Affinity Designer's "SVG (digital)" preset at 96 DPI. After exporting, confirm knob and port positions match the constants in `UZZ.cpp`, then run `make install` so Rack picks up the art.
