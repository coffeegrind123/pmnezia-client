import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import SortFilterProxyModel 0.2

import PageEnum 1.0
import ContainerProps 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"
import "../Components"

PageType {
    id: root

    property bool isOutdatedAwgContainer: ServersUiController.isProcessedContainerOutdatedAwg()

    BackButtonType {
        id: backButton

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 20 + PageController.safeAreaTopMargin
        
        onFocusChanged: {
            if (this.activeFocus) {
                listView.positionViewAtBeginning()
            }
        }
    }

    ListViewType {
        id: listView

        anchors.top: backButton.bottom
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.left: parent.left

        header: ColumnLayout {
            width: listView.width

            BaseHeaderType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: root.isOutdatedAwgContainer ? 16 : 32

                headerText: ContainersModel.getProcessedContainerName() + qsTr(" settings")
            }

            WarningType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 16

                visible: root.isOutdatedAwgContainer

                iconPath: "qrc:/images/controls/alert-circle.svg"
                imageColor: AmneziaStyle.color.goldenApricot
                textColor: AmneziaStyle.color.goldenApricot
                textString: qsTr("AmneziaWG 2.0 is outdated and does not include the latest security improvements, but it will continue to work. Moving to AmneziaWG 3.1 by deploying a new container on the server is recommended for stronger protocol security")
            }
        }

        model: ProtocolsModel

        delegate: ColumnLayout {
            id: delegateContent

            width: listView.width

            property bool isClientSettingsVisible: true

            LabelWithButtonType {
                id: clientSettings

                Layout.fillWidth: true

                text: protocolName + qsTr(" connection settings")
                rightImageSource: "qrc:/images/controls/chevron-right.svg"
                visible: delegateContent.isClientSettingsVisible

                clickedFunction: function() {
                    if (isClientProtocolExists) {
                        ServersUiController.openClientSettings(ServersUiController.processedServerId, ServersUiController.processedContainerIndex, protocolIndex)
                        PageController.goToPage(clientProtocolPage);
                    } else {
                        PageController.showNotificationMessage(qsTr("Click the \"connect\" button to create a connection configuration"))
                    }
                }

                MouseArea {
                    anchors.fill: clientSettings
                    cursorShape: Qt.PointingHandCursor
                    enabled: false
                }
            }

            DividerType {
                visible: delegateContent.isClientSettingsVisible
            }

        }
    }
}
