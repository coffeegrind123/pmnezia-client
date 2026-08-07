import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import SortFilterProxyModel 0.2

import PageEnum 1.0
import ProtocolEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"
import "../Components"

PageType {
    id: root

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
        anchors.left: parent.left
        anchors.right: parent.right

        enabled: ServersUiController.isProcessedServerHasWriteAccess()
        model: QqDnsConfigModel

        delegate: ColumnLayout {
            width: listView.width

            property alias focusItemId: sendDomainsTextArea.textField

            spacing: 0

            BaseHeaderType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                headerText: qsTr("QQ-DNS settings")
                descriptionText: qsTr("UDP-over-DNS transport — the AmneziaWG datapath itself carried inside DNS " +
                                      "queries that traverse public resolvers. A blackout-survival path for when only " +
                                      "port 53 escapes; not a low-latency one. Both ends must be authoritative for an " +
                                      "NS-delegated subdomain.")
            }

            TextFieldWithHeaderType {
                id: sendDomainsTextArea

                Layout.fillWidth: true
                Layout.topMargin: 32
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled

                headerText: qsTr("Server send-domains")
                textField.text: (sendDomains && sendDomains.length) ? sendDomains.join(", ") : ""
                textField.placeholderText: qsTr("nb.server.example.com")

                textField.onEditingFinished: {
                    var raw = textField.text.split(",")
                    var cleaned = []
                    for (var i = 0; i < raw.length; ++i) {
                        var t = raw[i].trim()
                        if (t.length > 0) cleaned.push(t)
                    }
                    sendDomains = cleaned
                }

                checkEmptyText: true
            }

            TextFieldWithHeaderType {
                id: recvDomainsTextArea

                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled

                headerText: qsTr("Client recv-domains")
                textField.text: (recvDomains && recvDomains.length) ? recvDomains.join(", ") : ""
                textField.placeholderText: qsTr("na.client.example.com")

                textField.onEditingFinished: {
                    var raw = textField.text.split(",")
                    var cleaned = []
                    for (var i = 0; i < raw.length; ++i) {
                        var t = raw[i].trim()
                        if (t.length > 0) cleaned.push(t)
                    }
                    recvDomains = cleaned
                }

                checkEmptyText: true
            }

            TextFieldWithHeaderType {
                id: dnsIpsTextArea

                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled

                headerText: qsTr("Resolvers (comma-separated)")
                textField.text: (dnsIps && dnsIps.length) ? dnsIps.join(", ") : ""
                textField.placeholderText: qsTr("8.8.8.8, 1.1.1.1")

                textField.onEditingFinished: {
                    var raw = textField.text.split(",")
                    var cleaned = []
                    for (var i = 0; i < raw.length; ++i) {
                        var t = raw[i].trim()
                        if (t.length > 0) cleaned.push(t)
                    }
                    dnsIps = cleaned
                }

                checkEmptyText: true
            }

            TextFieldWithHeaderType {
                id: receivePortTextField

                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled

                headerText: qsTr("Authoritative UDP port")
                textField.text: receivePort
                textField.maximumLength: 5
                textField.validator: IntValidator { bottom: 1; top: 65535 }

                textField.onEditingFinished: {
                    if (parseInt(textField.text) !== receivePort) {
                        receivePort = parseInt(textField.text)
                    }
                }

                checkEmptyText: true
            }

            TextFieldWithHeaderType {
                id: maxDomainLenTextField

                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled

                headerText: qsTr("Max domain length (must match server)")
                textField.text: maxDomainLen
                textField.maximumLength: 3
                textField.validator: IntValidator { bottom: 20; top: 253 }

                textField.onEditingFinished: {
                    if (parseInt(textField.text) !== maxDomainLen) {
                        maxDomainLen = parseInt(textField.text)
                    }
                }

                checkEmptyText: true
            }

            BasicButtonType {
                id: saveButton

                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.bottomMargin: 24
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: receivePortTextField.errorText === "" && maxDomainLenTextField.errorText === ""

                text: qsTr("Save")

                onClicked: function() {
                    forceActiveFocus()

                    var headerText = qsTr("Save settings?")
                    var descriptionText = qsTr("All users with whom you shared a connection with will no longer be able to connect to it.")
                    var yesButtonText = qsTr("Continue")
                    var noButtonText = qsTr("Cancel")

                    var yesButtonFunction = function() {
                        if (ConnectionController.isConnected && ServersModel.getDefaultServerData("defaultContainer") === ServersUiController.processedContainerIndex) {
                            PageController.showNotificationMessage(qsTr("Unable change settings while there is an active connection"))
                            return
                        }

                        PageController.goToPage(PageEnum.PageSetupWizardInstalling)
                        InstallController.updateContainer(ServersUiController.processedIndex,
                                                          ServersUiController.processedContainerIndex,
                                                          ProtocolEnum.QqDns)
                    }
                    var noButtonFunction = function() {
                        if (!GC.isMobile()) {
                            saveButton.forceActiveFocus()
                        }
                    }
                    showQuestionDrawer(headerText, descriptionText, yesButtonText, noButtonText, yesButtonFunction, noButtonFunction)
                }

                Keys.onEnterPressed: saveButton.clicked()
                Keys.onReturnPressed: saveButton.clicked()
            }
        }
    }
}
