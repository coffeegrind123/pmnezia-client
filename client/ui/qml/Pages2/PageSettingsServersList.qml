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

    ListViewType {
        id: servers
        objectName: "servers"

        anchors.fill: parent

        header: NavigationBarType {
            width: servers.width
            height: implicitHeight + 16

            title: qsTr("Servers")
        }

        model: ServersModel

        delegate: Item {
            implicitWidth: servers.width
            implicitHeight: server.implicitHeight

            ListItemType {
                id: server

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 16
                anchors.rightMargin: 16

                text: name

                subtitle: hostName
                trailingIconSource: "qrc:/images/controls/chevron-right.svg"

                onClicked: {
                    ServersUiController.setProcessedServerId(serverId)
                    PageController.goToPage(PageEnum.PageSettingsServerInfo)
                }
            }
        }

        footer: Item {
            width: servers.width
            height: 16 + PageController.safeAreaBottomMargin
        }
    }
}
