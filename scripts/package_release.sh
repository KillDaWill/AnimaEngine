#!/usr/bin/env bash
set -euo pipefail

platform="${1:-}"
version="${2:-dev}"

if [[ -z "$platform" ]]; then
    echo "usage: scripts/package_release.sh <platform> [version]" >&2
    exit 2
fi

version="${version#refs/tags/}"
version="${version:-dev}"
package_name="AnimaEngine-${version}-${platform}"
stage="release/${package_name}"

find_binary() {
    local base="$1"
    if [[ -f "$base" ]]; then
        printf '%s\n' "$base"
    elif [[ -f "${base}.exe" ]]; then
        printf '%s\n' "${base}.exe"
    else
        echo "missing required binary: $base" >&2
        exit 1
    fi
}

copy_common_files() {
    mkdir -p "$stage"
    cp "$(find_binary AnimaEngine)" "$stage/"
    cp "$(find_binary AnimaEngineGUI)" "$stage/"
    cp README.md LICENSE THIRD_PARTY_NOTICES.md "$stage/"
    if [[ -d images ]]; then
        mkdir -p "$stage/images"
        cp images/* "$stage/images/" 2>/dev/null || true
    fi
}

write_unix_launchers() {
    cat > "$stage/run-gui.sh" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$DIR/AnimaEngineGUI" "$@"
SH
    cat > "$stage/run-cli.sh" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$DIR/AnimaEngine" "$@"
SH
    chmod +x "$stage/run-gui.sh" "$stage/run-cli.sh"
}

copy_linux_runtime_libs() {
    mkdir -p "$stage/lib"
    for bin in "$stage/AnimaEngine" "$stage/AnimaEngineGUI"; do
        ldd "$bin" 2>/dev/null \
            | awk '/libraylib|libpng|libz/ { if ($3 != "") print $3 }' \
            | sort -u \
            | while read -r lib; do
                [[ -f "$lib" ]] && cp -L "$lib" "$stage/lib/"
            done
    done
    find "$stage/lib" -type f | grep -q . || rmdir "$stage/lib"
}

copy_windows_runtime_libs() {
    for exe in "$stage"/*.exe; do
        ldd "$exe" 2>/dev/null \
            | awk '/mingw64|ucrt64|clang64/ { if ($3 != "") print $3; else if ($1 ~ /^\//) print $1 }' \
            | sort -u \
            | while read -r dll; do
                [[ -f "$dll" ]] && cp -L "$dll" "$stage/"
            done
    done
}

copy_macos_runtime_libs() {
    mkdir -p "$stage/lib"
    for bin in "$stage/AnimaEngine" "$stage/AnimaEngineGUI"; do
        otool -L "$bin" \
            | awk '/libraylib|libpng/ { print $1 }' \
            | while read -r dep; do
                [[ "$dep" == /usr/lib/* || "$dep" == /System/* ]] && continue
                if [[ -f "$dep" ]]; then
                    base="$(basename "$dep")"
                    if [[ ! -f "$stage/lib/$base" ]]; then
                        cp -L "$dep" "$stage/lib/$base"
                        chmod u+w "$stage/lib/$base" || true
                    fi
                    install_name_tool -change "$dep" "@executable_path/lib/$base" "$bin" || true
                fi
            done
    done
    for dylib in "$stage"/lib/*.dylib; do
        [[ -f "$dylib" ]] || continue
        install_name_tool -id "@rpath/$(basename "$dylib")" "$dylib" || true
    done
    cat > "$stage/run-gui.command" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export DYLD_LIBRARY_PATH="$DIR/lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
exec "$DIR/AnimaEngineGUI" "$@"
SH
    chmod +x "$stage/run-gui.command"
}

rm -rf "$stage"
mkdir -p release
copy_common_files

case "$platform" in
    linux-*)
        write_unix_launchers
        copy_linux_runtime_libs
        tar -C release -czf "release/${package_name}.tar.gz" "$package_name"
        ;;
    windows-*)
        copy_windows_runtime_libs
        (cd release && zip -qr "${package_name}.zip" "$package_name")
        ;;
    macos-*)
        copy_macos_runtime_libs
        tar -C release -czf "release/${package_name}.tar.gz" "$package_name"
        ;;
    *)
        echo "unknown release platform: $platform" >&2
        exit 2
        ;;
esac

echo "Created release package for ${platform}: release/${package_name}"
