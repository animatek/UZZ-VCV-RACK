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

## Adding a Module
Never invent a tag or a category for `plugin.json`. Only the canonical VCV tags are accepted, spelled exactly as the SDK spells them — the authoritative list is `tagAliases` in Rack's `src/tag.cpp` (documented at <https://vcvrack.com/manual/Manifest#modules-tags>), and any alias in the same row is equally valid. An unrecognised tag is silently dropped, so the module ends up unfindable in the browser and on the library website.

Every widget that draws must also work with `module == nullptr`: the module browser and the library website render the panel with no instance. Bail out on a missing font or SVG if you like, but never on a missing module — fall back to the `configParam` defaults instead, or the panel shows up blank there. VCV's automated pattern check flags this on submission.

## Release Workflow
Both steps are needed, and a git tag alone is not a release:
1. Bump `version` in `plugin.json`, add the `CHANGELOG.md` section, and commit.
2. `git tag -a vX.Y.Z` and `git push --follow-tags`.
3. `gh release create vX.Y.Z --verify-tag --title "vX.Y.Z"` with notes in English: an `Animatek X.Y.Z` line, then `Highlights:` bullets. No binaries are attached — the library builds from source.

Before tagging, run `cppcheck --enable=warning --std=c++11 -I src -I ../Rack-SDK/include src/`; the VCV library runs static analysis on submission and files an issue on this repo when it fails. Note that `clamp()` does not satisfy cppcheck's bounds analysis — it does not follow the return value, so guard indices with an explicit comparison.
