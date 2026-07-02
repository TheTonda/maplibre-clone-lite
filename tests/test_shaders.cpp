// Shader source well-formedness checks (GLSL itself cannot be compiled
// without a GL context in a headless unit test).
#include <gtest/gtest.h>

#include <string>

#include "shaders/fill_vert.h"
#include "shaders/fill_frag.h"

using shader_source::fill_fragment;
using shader_source::fill_vertex;

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

// ── Vertex shader ─────────────────────────────────────────────────────

TEST(ShaderSourceTest, FillVertexIsNonEmptyCString) {
    ASSERT_NE(fill_vertex, nullptr);
    EXPECT_GT(std::string(fill_vertex).size(), 0u);
}

TEST(ShaderSourceTest, FillVertexContainsRequiredTokens) {
    const std::string src(fill_vertex);
    EXPECT_TRUE(contains(src, "a_position"));
    EXPECT_TRUE(contains(src, "u_proj"));
    EXPECT_TRUE(contains(src, "u_view"));
    EXPECT_TRUE(contains(src, "u_tile_offset"));
    EXPECT_TRUE(contains(src, "gl_Position"));
}

TEST(ShaderSourceTest, FillVertexHasNoVersionDirective) {
    EXPECT_FALSE(contains(fill_vertex, "#version"));
}

TEST(ShaderSourceTest, FillVertexHasNoDeprecatedFragColor) {
    EXPECT_FALSE(contains(fill_vertex, "gl_FragColor"));
}

// ── Fragment shader ───────────────────────────────────────────────────

TEST(ShaderSourceTest, FillFragmentIsNonEmptyCString) {
    ASSERT_NE(fill_fragment, nullptr);
    EXPECT_GT(std::string(fill_fragment).size(), 0u);
}

TEST(ShaderSourceTest, FillFragmentContainsRequiredTokens) {
    const std::string src(fill_fragment);
    EXPECT_TRUE(contains(src, "u_color"));
    EXPECT_TRUE(contains(src, "frag_color"));
    EXPECT_TRUE(contains(src, "void main"));
}

TEST(ShaderSourceTest, FillFragmentHasNoVersionDirective) {
    EXPECT_FALSE(contains(fill_fragment, "#version"));
}

TEST(ShaderSourceTest, FillFragmentHasNoDeprecatedFragColor) {
    EXPECT_FALSE(contains(fill_fragment, "gl_FragColor"));
}

// ── Namespace check ───────────────────────────────────────────────────

TEST(ShaderSourceTest, SymbolsInShaderSourceNamespace) {
    // If the symbols were not in namespace shader_source, the using-declarations
    // at the top of this file would fail to compile. Re-affirm by taking
    // addresses through the qualified names.
    const char* v = shader_source::fill_vertex;
    const char* f = shader_source::fill_fragment;
    EXPECT_NE(v, nullptr);
    EXPECT_NE(f, nullptr);
}
