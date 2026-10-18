import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import SortFilterProxyModel 0.2

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"
import "../Components"

PageType {
    id: root

    ListViewType {
        id: listView

        anchors.fill: parent

        header: ColumnLayout {
            width: listView.width

            spacing: 0

            NavigationBarType {
                Layout.fillWidth: true
                Layout.bottomMargin: 16

                title: ContainersModel.getProcessedContainerName() + qsTr(" settings")
            }

        }

        model: ProtocolsModel

        delegate: ColumnLayout {
            id: delegateContent

            width: listView.width

            spacing: 0

            property bool isClientSettingsVisible: true

            ListItemType {
                id: clientSettings

                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                text: protocolName + qsTr(" connection settings")
                trailingIconSource: "qrc:/images/controls/chevron-right.svg"
                visible: delegateContent.isClientSettingsVisible

                onClicked: {
                    if (isClientProtocolExists) {
                        ServersUiController.openClientSettings(ServersUiController.processedServerId, ServersUiController.processedContainerIndex, protocolIndex)
                        PageController.goToPage(clientProtocolPage);
                    } else {
                        PageController.showNotificationMessage(qsTr("Click the \"connect\" button to create a connection configuration"))
                    }
                }
            }

        }

        footer: ColumnLayout {
            width: listView.width

            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 16 + PageController.safeAreaBottomMargin
            }
        }
    }
}
