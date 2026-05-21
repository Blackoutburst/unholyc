#!/bin/bash
set -e

cd "$(dirname "$0")"

PREFIX="${1:-$HOME/.local}"

if [ ! -f "dist/bin/unholyc" ]; then
    echo "Error: dist/bin/unholyc not found. Run build-all.sh first." >&2
    exit 1
fi

mkdir -p "$PREFIX/bin"
mkdir -p "$PREFIX/lib"

cp dist/bin/unholyc "$PREFIX/bin/unholyc"
chmod +x "$PREFIX/bin/unholyc"

cp -r dist/include/. "$PREFIX/include/"

cp dist/lib/libuhc.a "$PREFIX/lib/libuhc.a"
if [ -f "dist/lib/libuhcgraphics.a" ]; then
    cp dist/lib/libuhcgraphics.a "$PREFIX/lib/libuhcgraphics.a"
fi

echo "Installed to $PREFIX"
echo "  bin:     $PREFIX/bin/unholyc"
echo "  headers: $PREFIX/include/"
echo "  libs:    $PREFIX/lib/"

if [[ ":$PATH:" != *":$PREFIX/bin:"* ]]; then
    EXPORT_LINE="export PATH=\"$PREFIX/bin:\$PATH\""

    # Pick profile file based on current shell
    case "${SHELL##*/}" in
        zsh)  PROFILE="$HOME/.zshrc" ;;
        fish) PROFILE="$HOME/.config/fish/config.fish"
              EXPORT_LINE="fish_add_path \"$PREFIX/bin\"" ;;
        *)    PROFILE="$HOME/.bashrc" ;;
    esac

    # Fallback: if chosen profile doesn't exist, try common alternatives
    if [ ! -f "$PROFILE" ]; then
        for f in "$HOME/.bash_profile" "$HOME/.profile"; do
            if [ -f "$f" ]; then PROFILE="$f"; break; fi
        done
    fi

    # Only append if not already present
    if ! grep -qF "$PREFIX/bin" "$PROFILE" 2>/dev/null; then
        echo "" >> "$PROFILE"
        echo "# Added by unholyc installer" >> "$PROFILE"
        echo "$EXPORT_LINE" >> "$PROFILE"
        echo ""
        echo "Added $PREFIX/bin to PATH in $PROFILE"
        echo "Run: source $PROFILE  (or open a new terminal)"
    else
        echo ""
        echo "$PREFIX/bin already in $PROFILE — skipped"
    fi
fi
