import QtQuick
import QtQuick.Controls.impl
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "../Controls2"

ListViewType {
    id: root

    anchors.fill: parent

    delegate: Item {
        id: delegateRoot

        implicitWidth: root.width
        implicitHeight: containerItem.implicitHeight

        ListItemType {
            id: containerItem

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 16
            anchors.rightMargin: 16

            text: name
            subtitle: description
            trailingIconSource: isInstalled ? "qrc:/images/controls/chevron-right.svg" : ""

            onClicked: {
                if (!isInstalled) {
                    return
                }

                var containerIndex = root.model.mapToSource(index)
                ServersUiController.processedContainerIndex = containerIndex
                ServersUiController.updateProtocols(ServersUiController.processedServerId, containerIndex)

                // Imported configs open as raw text, unless the protocol has a
                // structured settings page (its config is not a text blob).
                if (isThirdPartyConfig && !hasClientSettingsPage) {
                    PageController.goToPage(PageEnum.PageProtocolRaw)
                } else {
                    PageController.goToPage(PageEnum.PageSettingsServerProtocol)
                }
            }
        }
    }

    footer: Item {
        width: root.width
        height: 16 + PageController.safeAreaBottomMargin
    }
}
