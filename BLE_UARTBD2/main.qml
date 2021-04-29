import QtQuick 2.15
import QtQuick.Controls 2.15
ApplicationWindow {
    id: applicationWindow
    width: 480
    height: 720
    visible: true
    title: qsTr("Scroll")
    Frame {
        id: frameTop
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 10
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        height: 70
        Button {
            id: scanButton
            width: parent.width
            text: "Scan"
            onClicked: {
                bledevice.startScan()
                scanButton.enabled=false
                sendButton.enabled=false
                listView.enabled=false
                busyIndicator.running=true;
            }
        }
    }
    Frame {
        id: frameScroll
        anchors.top: frameTop.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: frameBottom.top
        anchors.topMargin: 10
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 10
        clip: true
        ListView {
            id: listView
            anchors.fill: parent
            width: parent.width
            model: bledevice.deviceListModel
            delegate: ItemDelegate {
                text: (index+1)+". "+modelData
                width: listView.width
                onClicked: {
                    console.log("Click", modelData, index)
                    bledevice.startConnect(index)
                    sendButton.enabled=false;
                    busyIndicator.running=true;
                }
            }
        }
    }

    Frame {
        id: frameBottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottomMargin: 10
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        height: 130
        Row {
            spacing: 40
            anchors.fill: parent
            Button {
                id: sendButton
                enabled: false
                width: 100
                text: "Send"
                onClicked: {
                    bledevice.writeData(textField.text.toString())
                }
            }
        }
        TextField {
            id: textField
            width: parent.width/3
            placeholderText: qsTr("Send Text")
            y:40
            color: "orange"
            font.pixelSize: 18
        }
        Text {
            id: text1
            y:90
            width: parent.width/3
            text: qsTr("Recive Text")
            color: "orange"
            font.pixelSize: 18
        }
    }

    BusyIndicator {
        id: busyIndicator
        anchors.centerIn: parent
        running: false;
    }
    Connections {
        target: bledevice
        function onNewData(data) {
            text1.text=data
        }
        function onScanningFinished() {
            listView.enabled=true
            busyIndicator.running=false
            scanButton.enabled=true
            console.log("ScanningFinished")
        }
        function onConnectionStart() {
            sendButton.enabled=true
            busyIndicator.running=false
            console.log("ConnectionStart")
        }
    }

}
