# Release Process

This project publishes source code through git and precompiled GUI packages
through GitHub Releases.

## What Goes Into Git

Commit source, headers, documentation, build scripts, workflows, and screenshots.
Do not commit generated binaries, `build/`, generated Doxygen HTML, local export
folders, ROMs, saves, or extracted copyrighted game data.

The `.gitignore` file blocks the common accidental cases, including `.nds`
files and generated release archives.

## Binary Packages

The GitHub release workflow builds packages for:

- Linux x86_64
- Windows x86_64
- macOS x86_64
- macOS arm64

Packages include `AnimaEngineGUI` and `AnimaEngine` plus bundled runtime
libraries where needed, so users do not need to install raylib to launch the GUI.

## Creating A Release

1. Update `PROJECT_NUMBER` in `Doxyfile` if the version changed.
2. Run local checks:

   ```bash
   make
   make gui
   make docs
   make release-clean
   ```

3. Commit the source release changes.
4. Push a version tag:

   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```

5. The `release.yml` workflow builds packages and creates a GitHub Release for
   the tag with all archives attached.

## Local Linux Package Smoke Test

On a Linux machine with raylib available:

```bash
make
make gui
scripts/package_release.sh linux-x86_64 dev
```

The generated archive appears in `release/`.
