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

    readonly property int pageSettingsServerProtocols: 0
    readonly property int pageSettingsServerData: 1

    property var processedServer

    Connections {
        target: ServersUiController

        function onProcessedServerIdChanged() {
            root.processedServer = proxyServersModel.get(0)
        }
    }

    Connections {
        target: ServersModel

        function onModelReset() {
            root.processedServer = proxyServersModel.get(0)
        }
    }

    SortFilterProxyModel {
        id: proxyServersModel
        objectName: "proxyServersModel"

        sourceModel: ServersModel
        filters: [
            ValueFilter {
                roleName: "serverId"
                value: ServersUiController.processedServerId
            }
        ]

        Component.onCompleted: {
            root.processedServer = proxyServersModel.get(0)
        }
    }

    ColumnLayout {
        objectName: "mainLayout"

        anchors.fill: parent

        spacing: 0

        NavigationBarType {
            id: headerContent
            objectName: "headerContent"

            Layout.fillWidth: true

            title: root.processedServer != null ? root.processedServer.name : ""
            description: {
                if (root.processedServer == null) {
                    return ""
                }
                return root.processedServer.hostName
            }

            IconButtonType {
                id: editButton
                objectName: "editButton"

                size: IconButtonType.Small
                icon.source: "qrc:/images/controls/edit-3.svg"

                onClicked: serverNameEditDrawer.open()
            }
        }

        RenameServerDrawer {
            id: serverNameEditDrawer

            parent: root

            serverNameText: root.processedServer != null ? root.processedServer.name : ""
        }

        TabsType {
            id: tabBar

            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            currentIndex: root.pageSettingsServerProtocols

            TabButtonType {
                id: protocolsTab
                visible: protocolsPage.installedProtocolsCount
                width: protocolsPage.installedProtocolsCount ? undefined : 0
                text: qsTr("Protocols")
            }

            TabButtonType {
                id: dataTab
                text: qsTr("Management")
            }
        }

        StackLayout {
            id: nestedStackView

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 24

            currentIndex: tabBar.currentIndex

            PageSettingsServerProtocols {
                id: protocolsPage
                stackView: root.stackView
            }

            PageSettingsServerData {
                id: dataPage
                stackView: root.stackView
            }
        }
    }
}
