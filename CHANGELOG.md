# Changelog

All notable changes to AnimaEngine are documented in this file. Dates are
shown in YYYY-MM-DD format. Versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Trademark disclaimer for Pokémon and individual species names, with a
  matching notice in `THIRD_PARTY_NOTICES.md` and a header comment in
  `source/pokemon_catalog.c`.
- GitHub Actions CI workflow (`.github/workflows/ci.yml`) that builds the
  CLI, GUI, and unit tests on Linux and Windows for every push to `main`
  and every pull request.
- GitHub Pages workflow (`.github/workflows/docs.yml`) that publishes the
  Doxygen reference to <https://killdawill.github.io/AnimaEngine/>.
- Minimal unit-test harness under `tests/` with `make test`. Covers LZ10
  detection and roundtrip, the catalog size, and spot-checked species.
- "How it works" / architecture section in the README.

### Changed
- `Doxyfile` moved to `docs/Doxyfile`; output now lives under `docs/build/`.
- README downloads section splits macOS into Apple Silicon and Intel, and
  links to a real docs URL.
- `make` removes the `print` debug target; the new `make test` target runs
  the unit tests.

### Fixed
- "Downloading A Release" header casing in the README.
- Unnecessary `\_` escape inside a code block in the README.

## [1.0.0] - 2026-05-21

### Added
- Initial public release. C99 sprite extraction and preview toolchain for
  Nintendo DS Pokémon Black, White, Black 2, and White 2.
- Nitro parsers for NCGR, NCLR, NCER, NANR, NMCR, and NMAR.
- CLI and raylib-based GUI sharing a common `anima_backend`.
- Prebuilt Linux, Windows, and macOS (arm64) release packages.
