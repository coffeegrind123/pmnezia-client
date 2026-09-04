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

        model: MasterDnsVpnConfigModel

        delegate: ColumnLayout {
            width: listView.width

            property alias focusItemId: domainsTextArea.textField

            spacing: 0

            BaseHeaderType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                headerText: qsTr("MasterDnsVPN settings")
                descriptionText: qsTr("DNS-tunnel transport — encrypted TCP traffic carried inside DNS queries " +
                                      "that traverse public resolvers. Requires you to own a domain and create an " +
                                      "NS delegation pointing the tunnel subdomain at this server's public IP.")
            }

            TextFieldWithHeaderType {
                id: domainsTextArea

                Layout.fillWidth: true
                Layout.topMargin: 32
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled

                headerText: qsTr("Tunnel domains")
                textField.text: (domains && domains.length) ? domains.join(", ") : ""
                textField.placeholderText: qsTr("v.example.com, tunnel.example.com")

                textField.onEditingFinished: {
                    var raw = textField.text.split(",")
                    var cleaned = []
                    for (var i = 0; i < raw.length; ++i) {
                        var t = raw[i].trim()
                        if (t.length > 0) cleaned.push(t)
                    }
                    domains = cleaned
                }

                checkEmptyText: true
            }

            TextFieldWithHeaderType {
                id: portTextField

                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled

                headerText: qsTr("Server UDP port")
                textField.text: port
                textField.maximumLength: 5
                textField.validator: IntValidator { bottom: 1; top: 65535 }

                textField.onEditingFinished: {
                    if (textField.text !== port) {
                        port = textField.text
                    }
                }

                checkEmptyText: true
            }

            // Encryption method picker. Values match the integer codes mdnsvpn
            // accepts on the wire: 0=None, 1=XOR, 2=ChaCha20, 3..5=AES-GCM.
            DropDownType {
                id: encryptionMethodDropDown

                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled

                rootButtonText: {
                    switch (encryptionMethod) {
                    case 0: return qsTr("None (no encryption)")
                    case 1: return qsTr("XOR (lightweight)")
                    case 2: return qsTr("ChaCha20")
                    case 3: return qsTr("AES-128-GCM")
                    case 4: return qsTr("AES-192-GCM")
                    case 5: return qsTr("AES-256-GCM (strongest)")
                    default: return qsTr("XOR (lightweight)")
                    }
                }

                headerText: qsTr("Encryption method")

                listView: ListViewWithRadioButtonType {
                    rootWidth: root.width

                    model: ListModel {
                        ListElement { name: qsTr("None (no encryption)"); value: 0 }
                        ListElement { name: qsTr("XOR (lightweight)"); value: 1 }
                        ListElement { name: qsTr("ChaCha20"); value: 2 }
                        ListElement { name: qsTr("AES-128-GCM"); value: 3 }
                        ListElement { name: qsTr("AES-192-GCM"); value: 4 }
                        ListElement { name: qsTr("AES-256-GCM (strongest)"); value: 5 }
                    }

                    clickedFunction: function() {
                        encryptionMethod = selectedValue
                        encryptionMethodDropDown.menuVisible = false
                    }

                    Component.onCompleted: {
                        for (var i = 0; i < model.count; ++i) {
                            if (model.get(i).value === encryptionMethod) {
                                currentIndex = i
                                break
                            }
                        }
                    }
                }
            }

            // Only AES-GCM (3..5) authenticates the ciphertext. None is
            // plaintext, XOR is a repeating-key xor, and this core's ChaCha20
            // is bare ChaCha20 without Poly1305 — so on 0/1/2 an active
            // on-path attacker can tamper with tunnel payloads undetected.
            // Mirrors the advisory awg-easy-rs writes into server_config.toml.
            WarningType {
                objectName: "nonAeadEncryptionWarning"

                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                visible: encryptionMethod < 3 || encryptionMethod > 5

                backGroundColor: AmneziaStyle.color.transparent
                iconPath: "qrc:/images/controls/alert-circle.svg"
                imageColor: AmneziaStyle.color.goldenApricot
                textColor: AmneziaStyle.color.goldenApricot
                textString: qsTr("This method is not authenticated encryption — an active on-path attacker can tamper with tunnel payloads undetected. Use AES-256-GCM unless you have a specific reason not to. Changing it requires re-issuing every client config, since the method is baked into each one.")
            }

            // Outbound mode: SOCKS5 = clients choose the destination per stream;
            // TCP = server forwards every connection to a fixed forwardIp:port
            // (useful for chaining mdnsvpn into a downstream proxy panel).
            DropDownType {
                id: protocolTypeDropDown

                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled

                rootButtonText: protocolType !== "" ? protocolType : qsTr("SOCKS5")

                headerText: qsTr("Outbound mode")

                listView: ListViewWithRadioButtonType {
                    rootWidth: root.width

                    model: ListModel {
                        ListElement { name: qsTr("SOCKS5 (per-stream destination)"); value: "SOCKS5" }
                        ListElement { name: qsTr("TCP (forward to fixed target)"); value: "TCP" }
                    }

                    clickedFunction: function() {
                        protocolType = selectedValue
                        protocolTypeDropDown.menuVisible = false
                    }

                    Component.onCompleted: {
                        var pt = protocolType !== "" ? protocolType : "SOCKS5"
                        for (var i = 0; i < model.count; ++i) {
                            if (model.get(i).value === pt) {
                                currentIndex = i
                                break
                            }
                        }
                    }
                }
            }

            // Local SOCKS5 listen port the bundled client-side mdnsvpn binary
            // opens on 127.0.0.1. tun2socks dials this on connect; the value
            // is also baked into the share-config TOML.
            TextFieldWithHeaderType {
                id: listenPortTextField

                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled

                headerText: qsTr("Local SOCKS5 port")
                textField.text: listenPort
                textField.maximumLength: 5
                textField.validator: IntValidator { bottom: 1; top: 65535 }

                textField.onEditingFinished: {
                    if (textField.text !== listenPort) {
                        listenPort = textField.text
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

                enabled: portTextField.errorText === "" && listenPortTextField.errorText === ""

                text: qsTr("Save")

                onClicked: function() {
                    forceActiveFocus()

                    var headerText = qsTr("Save settings?")
                    var descriptionText = qsTr("Only the settings for this device will be changed")
                    var yesButtonText = qsTr("Continue")
                    var noButtonText = qsTr("Cancel")

                    var yesButtonFunction = function() {
                        if (ConnectionController.isConnected && ServersModel.getDefaultServerData("defaultContainer") === ServersUiController.processedContainerIndex) {
                            PageController.showNotificationMessage(qsTr("Unable change settings while there is an active connection"))
                            return
                        }

                        ServersUiController.updateClientConfig(ServersUiController.processedServerId,
                                                              ServersUiController.processedContainerIndex,
                                                              ProtocolEnum.MasterDnsVpn)
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
