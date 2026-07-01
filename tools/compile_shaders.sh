#!/usr/bin/env bash
# compile_shaders.sh
# Compiles all GLSL shader sources to SPIR-V binary (.spv).
# Output directory mirrors the source layout under $OUT_DIR.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/../src/shaders"
OUT_DIR="${SCRIPT_DIR}/../_build/shaders"

GLSLC="${GLSLC:-glslc}"

if ! command -v "$GLSLC" &>/dev/null; then
    echo "ERROR: glslc not found. Install Vulkan SDK or set GLSLC env var."
    exit 1
fi

mkdir -p "$OUT_DIR/2d" "$OUT_DIR/3d"

compile_shaders() {
    local variant="$1"  # 2d or 3d
    local src_dir="${SRC_DIR}/${variant}"
    local out_dir="${OUT_DIR}/${variant}"

    if [ ! -d "$src_dir" ]; then
        return
    fi

    for src in "$src_dir"/*.vert "$src_dir"/*.frag; do
        [ -f "$src" ] || continue
        local base
        base="$(basename "$src")"
        local out="${out_dir}/${base}.spv"
        echo "  [SPIRV] $base"
        "$GLSLC" "$src" -o "$out"
    done
}

echo "Compiling shaders..."
compile_shaders "2d"
compile_shaders "3d"
echo "Done. Output in $OUT_DIR"
