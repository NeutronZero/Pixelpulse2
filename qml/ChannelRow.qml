import QtQuick 2.1
import QtQuick.Window 2.0
import QtQuick.Layouts 1.0
import QtQuick.Controls 1.0
import QtQuick.Controls.Styles 1.1

Rectangle {
  id: channelBlock
  property var channel
  property var device
  property int deviceIndex: -1
  property int channelIndex: -1
  property alias signalRepeater: signalRepeater
  property bool modeMenuOpen: false
  property double modeMenuHideTime: 0
  color: '#333'

  function applyMode() {
    if (!channel || deviceIndex < 0 || channelIndex < 0)
      return;
    var deviceRepeaterItem = xyPane.devRep.itemAt(deviceIndex);
    if (!deviceRepeaterItem)
      return;
    var xyPlot = deviceRepeaterItem.itemAt(channelIndex);
    if (!xyPlot)
      return;

    xyPlot.ysignal = (channel.mode == 1) ? xyPlot.isignal : xyPlot.vsignal;
    xyPlot.xsignal = (channel.mode == 1) ? xyPlot.vsignal : xyPlot.isignal;
  }

  Button {
    id: modeButton
    anchors.top: parent.top
    anchors.left: parent.left
    width: timelinePane.spacing
    height: timelinePane.spacing

    property var icons: [
      'mv',
      'svmi',
      'simv',
    ]
    iconSource: channel ? 'qrc:/icons/' + icons[channel.mode] + '.png' : ''

    style: ButtonStyle {
      background: Rectangle {
        opacity: control.pressed ? 0.3 : control.checked ? 0.2 : 0.1
        color: 'black'
      }
    }

    onClicked: {
      if (Date.now() - modeMenuHideTime < 350) {
        modeMenuOpen = false
        return
      }
      if (modeMenuOpen) {
        modeMenuOpen = false
        modeMenu.__dismissMenu()
      } else {
        modeMenuOpen = true
        modeMenu.__popup(Qt.rect(0, modeButton.height, 0, 0), 0)
      }
    }
  }

  Menu {
    id: modeMenu
    __visualItem: modeButton
    __minimumWidth: modeButton.width
    onAboutToShow: modeMenuOpen = true
    onAboutToHide: {
      modeMenuOpen = false
      modeMenuHideTime = Date.now()
    }

    MenuItem { text: "Measure Voltage"
      onTriggered: channel.mode = 0
    }
    MenuItem { text: "Source Voltage, Measure Current"
      onTriggered: channel.mode = 1
    }
    MenuItem { text: "Source Current, Measure Voltage"
      onTriggered: channel.mode = 2
    }
  }

  Connections {
    target: channel
    onModeChanged: channelBlock.applyMode()
  }

  Text {
    text: "Channel " + (channel ? channel.label : "")
    color: 'white'
    rotation: -90
    transformOrigin: Item.TopLeft
    font.pixelSize: session.devices.length > 0 ? 18 / session.devices.length : 18
    y: width + timelinePane.spacing + 8
    x: (timelinePane.spacing - height) / 2
  }

  ColumnLayout {
    anchors.fill: parent
    anchors.leftMargin: timelinePane.spacing
    spacing: 0

    Repeater {
      id: signalRepeater
      model: modelData.signals

      SignalRow {
        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.minimumHeight: channelBlock.height / 2

        signal: model
        channel: channelBlock.channel
        device: channelBlock.device
        channelRow: channelBlock
        deviceIndex: channelBlock.deviceIndex
        channelIndex: channelBlock.channelIndex
        xaxis: timeline_xaxis
      }
    }
  }
}
