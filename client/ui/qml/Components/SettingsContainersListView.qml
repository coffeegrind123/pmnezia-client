import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import SortFilterProxyModel 0.2

import PageEnum 1.0
import Style 1.0

import "../Controls2"
import "../Controls2/TextTypes"


ListViewType {
    id: root

    anchors.fill: parent

    delegate: ColumnLayout {
        width: root.width

        property bool isOutdatedAwgContainer: Boolean(isInstalled && ServersUiController.isContainerOutdatedAwg(root.model.mapToSource(index)))

        LabelWithButtonType {
            Layout.fillWidth: true

            text: name
            descriptionText: description
            rightWarningImageSource: isOutdatedAwgContainer ? "qrc:/images/controls/alert-circle.svg" : ""
            rightImageSource: isInstalled ? "qrc:/images/controls/chevron-right.svg" : ""

            clickedFunction: function() {
                if (!isInstalled) {
                    return
                }

                var containerIndex = root.model.mapToSource(index)
                ServersUiController.processedContainerIndex = containerIndex
                ServersUiController.updateProtocols(ServersUiController.processedServerId, containerIndex)

                if (isThirdPartyConfig || isIpsec) {
                    PageController.goToPage(PageEnum.PageProtocolRaw)
                } else {
                    PageController.goToPage(PageEnum.PageSettingsServerProtocol)
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                enabled: false
            }
        }

        DividerType {}
    }
}
