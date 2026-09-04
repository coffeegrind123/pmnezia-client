// Verbatim copy of the maths in Qt5Compat.GraphicalEffects'
// shaders_ng/coloroverlay.frag (qt5compat 6.10, LGPL-3.0/GPL-2.0/GPL-3.0).
// Kept in-tree so the client does not depend on the qt5compat module.
//
// Un-premultiplies both the source pixel and the (premultiplied) overlay
// colour uniform, mixes by the overlay alpha, then re-premultiplies by the
// source alpha. For an opaque overlay colour this replaces the source RGB
// outright while preserving the source alpha, which is what every call site
// in this app relies on for tinting monochrome icons.

#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    // qt_Matrix and qt_Opacity must always be both present
    // if the built-in vertex shader is used.
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 color;
};

layout(binding = 1) uniform sampler2D source;

void main()
{
    vec4 pixelColor = texture(source, qt_TexCoord0);
    fragColor = vec4(mix(pixelColor.rgb / max(pixelColor.a, 0.00390625), color.rgb / max(color.a, 0.00390625), color.a) * pixelColor.a, pixelColor.a) * qt_Opacity;
}
