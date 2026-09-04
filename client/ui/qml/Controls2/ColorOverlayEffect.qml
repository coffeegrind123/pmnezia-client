import QtQuick

// Drop-in replacement for Qt5Compat.GraphicalEffects' ColorOverlay.
//
// Mirrors that type's structure (Item root wrapping an anchored ShaderEffect)
// and uses the same fragment-shader maths, so it behaves identically both as a
// `layer.effect:` and as a standalone item with an explicit `source`.
//
// MultiEffect is deliberately NOT used here: its colorization multiplies by the
// source luminance (`gray * colorizationColor.rgb`), so it only matches
// ColorOverlay for a pure-white source. This app's icons are #CBCBCB/#D7D8DB
// and darker, which MultiEffect would render at the wrong brightness.
Item {
    id: root

    property variant source
    property color color: "transparent"

    ShaderEffect {
        anchors.fill: parent

        property variant source: root.source
        property color color: root.color

        fragmentShader: "qrc:/shaders/coloroverlay.frag.qsb"
    }
}
