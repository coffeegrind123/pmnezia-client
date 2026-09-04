import QtQuick

// Drop-in replacement for Qt5Compat.GraphicalEffects' OpacityMask.
//
// The mask is routed through a ShaderEffectSource because every call site
// passes a plain Rectangle, and a Rectangle is not a texture provider. This
// mirrors what Qt5Compat's own SourceProxy does for non-texture-provider
// inputs.
Item {
    id: root

    property variant source
    property variant maskSource

    ShaderEffectSource {
        id: maskProxy

        anchors.fill: parent
        sourceItem: root.maskSource
        hideSource: false
        live: true
        smooth: true
        visible: false
    }

    ShaderEffect {
        anchors.fill: parent

        property variant source: root.source
        property variant maskSource: maskProxy

        fragmentShader: "qrc:/shaders/opacitymask.frag.qsb"
    }
}
