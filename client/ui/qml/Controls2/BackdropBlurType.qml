import QtQuick
import QtQuick.Effects

import Style 1.0

Item {
    id: root

    property Item sourceItem
    property real blurPadding: 40
    property real blurRadius: 50
    property color tintColor: Qt.alpha(AmneziaStyle.color.textStaticWhite, 0.10)
    property color borderColor: Qt.alpha(AmneziaStyle.color.textStaticWhite, 0.75)

    Item {
        anchors.fill: parent

        layer.enabled: true
        layer.effect: OpacityMaskEffect {
            maskSource: backdropMask
        }

        Item {
            x: -root.blurPadding
            y: -root.blurPadding
            width: root.width + root.blurPadding * 2
            height: root.height + root.blurPadding * 2

            ShaderEffectSource {
                id: backdrop

                anchors.fill: parent
                sourceItem: root.sourceItem
                sourceRect: {
                    if (!root.sourceItem) {
                        return Qt.rect(0, 0, 0, 0)
                    }
                    root.x; root.y; root.width; root.height; root.sourceItem.x; root.sourceItem.y;
                    var pos = root.mapToItem(root.sourceItem, -root.blurPadding, -root.blurPadding)
                    return Qt.rect(pos.x, pos.y, width, height)
                }
                visible: false
            }

            // MultiEffect replaces Qt5Compat's FastBlur here. Unlike the
            // ColorOverlay/OpacityMask cases this is purely decorative frosted
            // glass, so the two blurs' differing kernels are not observable.
            // MultiEffect expresses radius as blur (0..1) scaled by blurMax
            // (pixels, clamped by Qt to 2..64).
            MultiEffect {
                anchors.fill: parent
                source: backdrop
                autoPaddingEnabled: false
                blurEnabled: true
                blur: 1.0
                blurMax: Math.max(2, Math.min(64, Math.round(root.blurRadius)))
            }
        }
    }

    Rectangle {
        id: backdropMask

        anchors.fill: parent
        radius: height / 2
        visible: false
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2

        color: root.tintColor
        border.color: root.borderColor
        border.width: 1
    }
}
