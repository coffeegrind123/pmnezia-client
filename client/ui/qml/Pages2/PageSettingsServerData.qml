import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import SortFilterProxyModel 0.2

import PageEnum 1.0
import Style 1.0

import "../Controls2"
import "../Controls2/TextTypes"
import "../Components"
import "../Config"

PageType {
    id: root

    signal lastItemTabClickedSignal()

    Connections {
        target: SettingsController
        function onChangeSettingsFinished(finishedMessage) {
            PageController.showNotificationMessage(finishedMessage)
        }
    }

    ListViewType {
        id: listView

        anchors.fill: parent

        model: serverActions

        delegate: ColumnLayout {
            width: listView.width

            ListItemType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                visible: isVisible

                text: title
                subtitle: description
                destructive: isDestructive

                onClicked: {
                    clickedHandler()
                }
            }
        }

        footer: Item {
            width: listView.width
            height: 16 + PageController.safeAreaBottomMargin
        }
    }

    property list<QtObject> serverActions: [
        remove,
    ]

    QtObject {
        id: remove

        property bool isVisible: true
        readonly property string title: qsTr("Remove server from application")
        readonly property string description: ""
        readonly property bool isDestructive: true
        readonly property var clickedHandler: function() {
            var headerText = qsTr("Do you want to remove the server from application?")
            var descriptionText = qsTr("All installed AmneziaVPN services will still remain on the server.")
            var yesButtonText = qsTr("Continue")
            var noButtonText = qsTr("Cancel")

            var yesButtonFunction = function() {
                if (ServersUiController.isDefaultServerCurrentlyProcessed() && ConnectionController.isConnected) {
                    PageController.showNotificationMessage(qsTr("Cannot remove server during active connection"))
                } else {
                    PageController.showBusyIndicator(true)
                    ServersUiController.removeServer(ServersUiController.processedServerId)
                    PageController.showBusyIndicator(false)
                }
            }
            var noButtonFunction = function() {

            }

            showQuestionDrawer(headerText, descriptionText, yesButtonText, noButtonText, yesButtonFunction, noButtonFunction)
        }
    }


}
