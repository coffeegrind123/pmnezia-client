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

        model: TrustTunnelConfigModel

        delegate: ColumnLayout {
            width: listView.width

            property alias focusItemId: addressesTextField.textField

            spacing: 0

            function splitList(text) {
                var raw = text.split(",")
                var cleaned = []
                for (var i = 0; i < raw.length; ++i) {
                    var t = raw[i].trim()
                    if (t.length > 0) cleaned.push(t)
                }
                return cleaned
            }

            BaseHeaderType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                headerText: qsTr("TrustTunnel settings")
                descriptionText: qsTr("Traffic rides HTTP/2 or HTTP/3 requests to the endpoint, which looks like an " +
                                      "ordinary website to anyone without the credentials. The hostname, user and " +
                                      "certificate come from the endpoint and cannot be changed here.")
            }

            TextFieldWithHeaderType {
                Layout.fillWidth: true
                Layout.topMargin: 32
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: false
                headerText: qsTr("Endpoint hostname")
                textField.text: hostname
            }

            TextFieldWithHeaderType {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: false
                headerText: qsTr("User")
                textField.text: username
            }

            TextFieldWithHeaderType {
                id: addressesTextField

                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled

                headerText: qsTr("Endpoint addresses (comma-separated host:port)")
                textField.text: (addresses && addresses.length) ? addresses.join(", ") : ""
                textField.placeholderText: qsTr("203.0.113.10:443")

                textField.onEditingFinished: {
                    var cleaned = splitList(textField.text)
                    if (cleaned.length > 0) {
                        addresses = cleaned
                    }
                }

                checkEmptyText: true
            }

            TextFieldWithHeaderType {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled

                headerText: qsTr("Custom SNI (optional)")
                textField.text: customSni
                textField.placeholderText: hostname

                textField.onEditingFinished: {
                    if (textField.text !== customSni) {
                        customSni = textField.text
                    }
                }
            }

            DropDownType {
                id: upstreamProtocolDropDown

                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled

                rootButtonText: upstreamProtocol === "http3" ? qsTr("HTTP/3 (QUIC over UDP)") : qsTr("HTTP/2 (TLS over TCP)")
                headerText: qsTr("Transport")

                listView: ListViewWithRadioButtonType {
                    rootWidth: root.width

                    model: ListModel {
                        ListElement { name: qsTr("HTTP/2 (TLS over TCP)"); value: "http2" }
                        ListElement { name: qsTr("HTTP/3 (QUIC over UDP)"); value: "http3" }
                    }

                    clickedFunction: function() {
                        upstreamProtocol = model.get(selectedIndex).value
                        upstreamProtocolDropDown.menuVisible = false
                    }

                    Component.onCompleted: {
                        selectedIndex = upstreamProtocol === "http3" ? 1 : 0
                    }
                }
            }

            SwitcherType {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled
                text: qsTr("Anti-DPI")
                descriptionText: qsTr("Shape the TLS handshake against deep packet inspection")
                checked: antiDpi
                onToggled: function() {
                    if (checked !== antiDpi) {
                        antiDpi = checked
                    }
                }
            }

            SwitcherType {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled
                text: qsTr("Route IPv6 through the endpoint")
                checked: hasIpv6
                onToggled: function() {
                    if (checked !== hasIpv6) {
                        hasIpv6 = checked
                    }
                }
            }

            SwitcherType {
                id: skipVerificationSwitch

                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled
                text: qsTr("Skip certificate verification")
                descriptionText: hasPinnedCertificate ? qsTr("The endpoint certificate is pinned by the imported profile")
                                                      : qsTr("The endpoint certificate is checked against the system store")
                checked: skipVerification
                onToggled: function() {
                    if (checked !== skipVerification) {
                        skipVerification = checked
                    }
                }
            }

            WarningType {
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                visible: skipVerification

                backGroundColor: AmneziaStyle.color.transparent
                iconPath: "qrc:/images/controls/alert-circle.svg"
                imageColor: AmneziaStyle.color.goldenApricot
                textColor: AmneziaStyle.color.goldenApricot
                textString: qsTr("Without verification anyone on the path can impersonate the endpoint and read the tunnel credentials.")
            }

            TextFieldWithHeaderType {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: listView.enabled

                headerText: qsTr("DNS upstreams inside the tunnel (comma-separated)")
                textField.text: (dnsUpstreams && dnsUpstreams.length) ? dnsUpstreams.join(", ") : ""
                textField.placeholderText: qsTr("tls://1.1.1.1")

                textField.onEditingFinished: {
                    dnsUpstreams = splitList(textField.text)
                }
            }

            BasicButtonType {
                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                textColor: AmneziaStyle.color.paleGray
                borderWidth: 1

                text: qsTr("Copy tt:// link")

                onClicked: function() {
                    GC.copyToClipBoard(TrustTunnelConfigModel.deepLink())
                    PageController.showNotificationMessage(qsTr("Copied"))
                }
            }

            BasicButtonType {
                id: saveButton

                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.bottomMargin: 24
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: addressesTextField.errorText === ""

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
                                                              ProtocolEnum.TrustTunnel)
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
