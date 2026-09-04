// Verbatim copy of the maths in Qt5Compat.GraphicalEffects'
// shaders_ng/opacitymask.frag (qt5compat 6.10, LGPL-3.0/GPL-2.0/GPL-3.0).
// Kept in-tree so the client does not depend on the qt5compat module.
//
// Multiplies the source by the mask's alpha. Note this is a linear multiply,
// not the smoothstep threshold QtQuick.Effects' MultiEffect mask applies, so
// antialiased mask edges (the rounded rectangles used here) stay smooth.

#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    // qt_Matrix and qt_Opacity must always be both present
    // if the built-in vertex shader is used.
    mat4 qt_Matrix;
    float qt_Opacity;
};

layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D maskSource;

void main()
{
    fragColor = texture(source, qt_TexCoord0.st) * (texture(maskSource, qt_TexCoord0.st).a) * qt_Opacity;
}
