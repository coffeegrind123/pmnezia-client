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

    ColumnLayout {
        id: header

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        anchors.topMargin: 20 + PageController.safeAreaTopMargin

        BackButtonType {
            id: backButton
        }

        BaseHeaderType {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            headerText: qsTr("Servers")
        }
    }

    ListViewType {
        id: servers
        objectName: "servers"

        width: parent.width
        anchors.top: header.bottom
        anchors.topMargin: 16
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right


        model: ServersModel

        delegate: Item {
            implicitWidth: servers.width
            implicitHeight: delegateContent.implicitHeight

            ColumnLayout {
                id: delegateContent

                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right

                LabelWithButtonType {
                    id: server
                    Layout.fillWidth: true

                    text: name


                    descriptionText: hostName
                    rightImageSource: "qrc:/images/controls/chevron-right.svg"

                    clickedFunction: function() {
                        ServersUiController.setProcessedServerId(serverId)

                        PageController.goToPage(PageEnum.PageSettingsServerInfo)
                    }
                }

                DividerType {}
            }
        }
    }
}
