import QtQuick 2.2
import QtQuick.Layouts 1.0
import Plot 1.0
import QtQuick.Controls 1.0
import QtQuick.Controls.Styles 1.1
import QtQml 2.15

Rectangle {
  id: signalBlock
  property alias ymin: axes.ymin;
  property alias ymax: axes.ymax;
   property var xaxis
   property var signal
   property var channel
   property var device
   property var channelRow
   property int deviceIndex: -1
   property int channelIndex: -1
   property bool waveMenuOpen: false
   property double waveMenuHideTime: 0
   property int ygridticks: axes.ygridticks
  color: '#444'
  property int currentFontSize: 11;

  visible: signal && channel && !(signal.label === "Current" && channel.mode === 0)

  property color gradColor: Qt.rgba(1,1,1,0.08)
  property color gradColor2: Qt.rgba(0,0,0,0.0)

  function numericValue(value, fallback) {
     var parsed;
     try {
         parsed = Number.fromLocaleString(value);
     } catch (error) {
         parsed = NaN;
     }
     if (!isFinite(parsed))
         parsed = parseFloat(value);
     return isFinite(parsed) ? parsed : fallback;
  }

  function constrainValue(value, min, max) {
     if (!isFinite(value))
         return min;
     if (value < min)
         value = min;
     else if (value > max)
         value = max;
     return value;
  }

  function updateMode() {
    if (!channel || !signal)
      return;
    channel.mode = {'Voltage': 1, 'Current': 2}[signal.label] || 0;
    for (var d = 0; d < deviceRepeater.count; d++) {
      var currentDevice = deviceRepeater.itemAt(d);
      if (!currentDevice || !currentDevice.channelRepeater)
        continue;
      for (var c = 0; c < currentDevice.channelRepeater.count; c++) {
        var currentChannel = currentDevice.channelRepeater.itemAt(c);
        if (!currentChannel || !currentChannel.signalRepeater)
          continue;
        for (var s = 0; s < currentChannel.signalRepeater.count; s++) {
          var currentSignal = currentChannel.signalRepeater.itemAt(s);
          if (currentSignal)
            currentChannel.applyMode();
        }
      }
    }
  }

  function switchToConstant() {
     signalBlock.updateMode()
     signal.src.src = 'constant'
  }

  function switchToPeriodic(type) {
    signalBlock.updateMode()
    if (signal.src.src == 'constant') {
      if ((signal.src.v1 == 0) || (signal.src.v2 == 0))
          signal.src.v2 = (channel.mode == 1) ? 2.5 : 0.050;
      else
          signal.src.v2 = 0;
      signal.src.v1 = 0
      signal.src.period = 100
    }
    signal.src.src = type
  }

  Button {
    id: waveButton
    anchors.top: parent.top
    anchors.left: parent.left
    width: timelinePane.spacing
    height: timelinePane.spacing

    iconSource: signal && signal.src ? 'qrc:/icons/' + signal.src.src + '.png' : ''

    style: ButtonStyle {
      background: Rectangle {
        opacity: control.pressed ? 0.3 : control.checked ? 0.2 : 0.1
        color: 'black'
      }
    }

    onClicked: {
      if (Date.now() - waveMenuHideTime < 350) {
        waveMenuOpen = false
        return
      }
      if (waveMenuOpen) {
        waveMenuOpen = false
        waveMenu.__dismissMenu()
      } else {
        waveMenuOpen = true
        waveMenu.__popup(Qt.rect(0, waveButton.height, 0, 0), 0)
      }
    }
  }

  Menu {
    id: waveMenu
    __visualItem: waveButton
    __minimumWidth: waveButton.width
    onAboutToShow: waveMenuOpen = true
    onAboutToHide: {
      waveMenuOpen = false
      waveMenuHideTime = Date.now()
    }

    MenuItem { text: "Constant"
      onTriggered: signalBlock.switchToConstant()
    }
    MenuItem { text: "Sine"
      onTriggered: signalBlock.switchToPeriodic('sine')
    }
    MenuItem { text: "Triangle"
      onTriggered: signalBlock.switchToPeriodic('triangle')
    }
    MenuItem { text: "Sawtooth"
      onTriggered: signalBlock.switchToPeriodic('sawtooth')
    }
    MenuItem { text: "Stairstep"
      onTriggered: signalBlock.switchToPeriodic('stairstep')
    }
    MenuItem { text: "Square"
      onTriggered: signalBlock.switchToPeriodic('square')
    }
  }

  Text {
    color: 'white'
    text: signal ? signal.label : ''
    rotation: -90
    transformOrigin: Item.TopLeft
     font.pixelSize: session.devices.length > 0 ? 18 / session.devices.length : 18
    y: width + timelinePane.spacing + 8
    x: (timelinePane.spacing - height) / 2
  }

  Rectangle {
    id: idRectangle
    x: parent.width
    width: xaxis.width
    anchors.top: parent.top
    height: timelinePane.spacing

    gradient: Gradient {
        GradientStop { position: 1.0; color: gradColor }
        GradientStop { position: 0.0; color: gradColor2 }
    }

    RowLayout {
      id: editWaveform
      anchors.fill: parent
      property bool isVoltageSignal: signal.label === "Voltage" ? true : false
      property real up_dn_Sensitivity : isVoltageSignal ? 0.01 : 0.001;
      property real pgUp_pgDn_Sensitivity : isVoltageSignal ? 1.0 : 0.1;

        // V1
        TextInput {
          id: v1TextBox
           text: ""
          color: "#FFF"
          selectByMouse: true
          font.pixelSize: currentFontSize
          readOnly: !signal.isOutput
           property real position: idRectangle.x + idRectangle.width * 10/100;

           Binding {
             target: v1TextBox
             property: "text"
             value: signal.isOutput ? signal.src.v1.toFixed(4) : signal.peak_to_peak.toFixed(4)
             restoreMode: Binding.RestoreBindingOrValue
           }



          onEditingFinished: {
            if (readOnly)
              return;

            var value = constrainValue(numericValue(text, signal.src.v1), axes.ymin + axes.overrangeSpan, axes.ymax - axes.overrangeSpan);
            text = value.toFixed(4);
            signal.src.v1 = value;

            signalBlock.updateMode()
          }

          Keys.onPressed: {
            var value;

            if (readOnly)
              return;

            switch (event.key) {
              case Qt.Key_Escape:
                text = signal.src.v1.toFixed(4);
                break;

              case Qt.Key_Down:
                  value = constrainValue(numericValue(text, signal.src.v1) - editWaveform.up_dn_Sensitivity, axes.ymin + axes.overrangeSpan, axes.ymax - axes.overrangeSpan);
                  text = value.toFixed(4);
                  signal.src.v1 = value;
                  event.accepted = true;
                  break;

              case Qt.Key_Up:
                  value = constrainValue(numericValue(text, signal.src.v1) + editWaveform.up_dn_Sensitivity, axes.ymin + axes.overrangeSpan, axes.ymax - axes.overrangeSpan);
                  text = value.toFixed(4);
                  signal.src.v1 = value;
                  event.accepted = true;
                  break;

              case Qt.Key_PageDown:
                  value = constrainValue(numericValue(text, signal.src.v1) - editWaveform.pgUp_pgDn_Sensitivity, axes.ymin + axes.overrangeSpan, axes.ymax - axes.overrangeSpan);
                  text = value.toFixed(4);
                  signal.src.v1 = value;
                  event.accepted = true;
                  break;

              case Qt.Key_PageUp:
                  value = constrainValue(numericValue(text, signal.src.v1) + editWaveform.pgUp_pgDn_Sensitivity, axes.ymin + axes.overrangeSpan, axes.ymax - axes.overrangeSpan);
                  text = value.toFixed(4);
                  signal.src.v1 = value;
                  event.accepted = true;
                  break;
            }
          }

          validator: DoubleValidator {
            bottom: signal.min - axes.overrangeSpan
            top: signal.max + axes.overrangeSpan
            notation: DoubleValidator.StandardNotation
          }
          anchors.left: parent.left
          anchors.leftMargin: idRectangle.width * 10/100;
        }
        Text {
           id: v1UnitLabel
           color: 'white'
           text: (signal.label == "Voltage" ? " Volts" : " Amperes") + (signal.isOutput ? "" : " (peak to peak)");
           anchors.left: v1TextBox.right
           anchors.leftMargin: idRectangle.width * 1/100
           font.pixelSize: currentFontSize
           property real position: v1TextBox.position + v1TextBox.width + idRectangle.width * 1/100;

        }
        // Resistance
        Text {
          color: 'white'
          visible: signal.src.src == 'constant' && signal.isOutput == true
          text: {
             var r = Math.abs((channel.signals[0].measurement / channel.signals[1].measurement)).toFixed();
             (Math.abs(channel.signals[1].measurement) > 0.001) ? "    " + r + " Ohms" : ""
          }
          anchors.left: v1UnitLabel.right
          anchors.leftMargin: idRectangle.width * 1/100
          font.pixelSize: currentFontSize
          property real position: v1TextBox.position + v1TextBox.width + idRectangle.width * 1/100;
        }
        // V2
        TextInput {
          id: v2TextBox
          visible: signal.isOutput ;
           text: ""
          color: "#FFF"
          selectByMouse: true
          font.pixelSize: currentFontSize
           property real position: v1UnitLabel.position + v1UnitLabel.width + idRectangle.width * 10/100

           Binding {
             target: v2TextBox
             property: "text"
             value: overlay_periodic.visible ? signal.src.v2.toFixed(4) : ""
             restoreMode: Binding.RestoreBindingOrValue
           }


          onEditingFinished: {
            if (readOnly)
              return;

            var value = constrainValue(numericValue(text, signal.src.v2), axes.ymin + axes.overrangeSpan, axes.ymax - axes.overrangeSpan);
            text = value.toFixed(4);
            signal.src.v2 = value;
          }

          Keys.onPressed: {
            var value;

            if (readOnly)
              return;

            switch (event.key) {
              case Qt.Key_Escape:
                text = signal.src.v2.toFixed(4);
                break;

              case Qt.Key_Down:
                  value = constrainValue(numericValue(text, signal.src.v2) - editWaveform.up_dn_Sensitivity, axes.ymin + axes.overrangeSpan, axes.ymax - axes.overrangeSpan);
                  text = value.toFixed(4);
                  signal.src.v2 = value;
                  event.accepted = true;
                  break;

              case Qt.Key_Up:
                  value = constrainValue(numericValue(text, signal.src.v2) + editWaveform.up_dn_Sensitivity, axes.ymin + axes.overrangeSpan, axes.ymax - axes.overrangeSpan);
                  text = value.toFixed(4);
                  signal.src.v2 = value;
                  event.accepted = true;
                  break;

              case Qt.Key_PageDown:
                  value = constrainValue(numericValue(text, signal.src.v2) - editWaveform.pgUp_pgDn_Sensitivity, axes.ymin + axes.overrangeSpan, axes.ymax - axes.overrangeSpan);
                  text = value.toFixed(4);
                  signal.src.v2 = value;
                  event.accepted = true;
                  break;

              case Qt.Key_PageUp:
                  value = constrainValue(numericValue(text, signal.src.v2) + editWaveform.pgUp_pgDn_Sensitivity, axes.ymin + axes.overrangeSpan, axes.ymax - axes.overrangeSpan);
                  text = value.toFixed(4);
                  signal.src.v2 = value;
                  event.accepted = true;
                  break;
            }
          }

          validator: DoubleValidator {
            bottom: signal.min - axes.overrangeSpan
            top: signal.max + axes.overrangeSpan
            notation: DoubleValidator.StandardNotation
          }
          anchors.left: v1UnitLabel.right
          anchors.leftMargin: idRectangle.width * 10/100
        }
        Text {
           id: v2UnitLabel
           color: 'white'
           text: overlay_periodic.visible ? (signal.label == "Voltage" ? " Volts" : " Amperes") : ""
           anchors.left: v2TextBox.right
           anchors.leftMargin: idRectangle.width * 1/100
           font.pixelSize: currentFontSize
           property real position: v2TextBox.position + v2TextBox.width + idRectangle.width * 1/100;
        }

        // Freq
        TextInput {
          id: perTextBox
          visible: (perUnitLabel.position + perUnitLabel.width <= idRectangle.x + idRectangle.width) && !(signal.isOutput && signal.src.src === 'constant')
           text: ""
          color: "#FFF"
          selectByMouse: true
          font.pixelSize: currentFontSize
          readOnly: !signal.isOutput

           property real position: (signal.isOutput ? v2UnitLabel.position + v2UnitLabel.width : v1UnitLabel.position + v1UnitLabel.width) + idRectangle.width * 10/100

           Binding {
             target: perTextBox
             property: "text"
             value: signal.isOutput ? (signal.src.src != 'constant' ? Math.abs(controller.sampleRate / signal.src.period).toFixed(3) : "") : signal.rms.toFixed(4)
             restoreMode: Binding.RestoreBindingOrValue
           }

          property real up_dn_freq_Sensivity: 1
          property real pgUp_pgDn_freq_Sensivity: 100

          onEditingFinished: {
            if (readOnly)
              return;

            var value = constrainValue(numericValue(text, 0), controller.minOutSignalFreq, controller.maxOutSignalFreq);
            text = value.toFixed(3)
            if (value > 0)
              signal.src.period = controller.sampleRate / value;
          }

          Keys.onPressed: {
            var value;

            if (readOnly)
              return;

            switch (event.key) {
              case Qt.Key_Escape:
                text = signal.isOutput && signal.src.src != 'constant' ? Math.abs(controller.sampleRate / signal.src.period).toFixed(3) : signal.rms.toFixed(4);
                break;

              case Qt.Key_Down:
                  value = numericValue(text, 0) - up_dn_freq_Sensivity;
                  value = constrainValue(value, controller.minOutSignalFreq, controller.maxOutSignalFreq);
                  text = value.toFixed(3);
                  if (value > 0)
                    signal.src.period = controller.sampleRate / value;
                  event.accepted = true;
                  break;

              case Qt.Key_Up:
                  value = numericValue(text, 0) + up_dn_freq_Sensivity;
                  value = constrainValue(value, controller.minOutSignalFreq, controller.maxOutSignalFreq);
                  text = value.toFixed(3);
                  if (value > 0)
                    signal.src.period = controller.sampleRate / value;
                  event.accepted = true;
                  break;

              case Qt.Key_PageDown:
                  value = numericValue(text, 0) - pgUp_pgDn_freq_Sensivity;
                  value = constrainValue(value, controller.minOutSignalFreq, controller.maxOutSignalFreq);
                  text = value.toFixed(3);
                  if (value > 0)
                    signal.src.period = controller.sampleRate / value;
                  event.accepted = true;
                  break;

              case Qt.Key_PageUp:
                  value = numericValue(text, 0) + pgUp_pgDn_freq_Sensivity;
                  value = constrainValue(value, controller.minOutSignalFreq, controller.maxOutSignalFreq);
                  text = value.toFixed(3);
                  if (value > 0)
                    signal.src.period = controller.sampleRate / value;
                  event.accepted = true;
                  break;
            }
          }

          validator: DoubleValidator {
            bottom: controller.minOutSignalFreq
            top: controller.maxOutSignalFreq
            notation: DoubleValidator.StandardNotation
          }
          anchors.left: signal.isOutput ? v2UnitLabel.right : v1UnitLabel.right;
          anchors.leftMargin: idRectangle.width * 10/100
        }
        Text {
           id: perUnitLabel
           color: 'white'
           visible: perUnitLabel.position + perUnitLabel.width <= idRectangle.x + idRectangle.width;
           anchors.left: perTextBox.right
           anchors.leftMargin: idRectangle.width * 1/100
           text: signal.isOutput ? (signal.src.src != 'constant' ? "Hertz" : ""): "RMS (AC)";
           font.pixelSize: currentFontSize
           property real position: perTextBox.position + perTextBox.width + idRectangle.width * 1/100;
        }

        Text {
            id: averageTextBox
            visible: averageLabel.position + averageLabel.width <= idRectangle.x + idRectangle.width;
             text: ""
            color: "#FFF"
            font.pixelSize: currentFontSize

             property real position: perUnitLabel.position + perUnitLabel.width + idRectangle.width * 10/100

             Binding {
               target: averageTextBox
               property: "text"
               value: signal.isOutput ? "" : signal.mean.toFixed(4)
               restoreMode: Binding.RestoreBindingOrValue
             }

             anchors.left: perUnitLabel.right
            anchors.leftMargin: idRectangle.width * 10/100
        }
        Text {
            id: averageLabel;
            color: "white";
            visible: averageLabel.position + averageLabel.width <= idRectangle.x + idRectangle.width;
            anchors.left: averageTextBox.right;
            anchors.leftMargin: idRectangle.width * 1/100;
            text: signal.isOutput ? "" : "Average";
            font.pixelSize: currentFontSize;
            property real position: averageTextBox.position + averageTextBox.width + idRectangle.width * 1/100;
        }

        //Max Input
        Text {
            id: maxLabel;
            color: "white";
            visible:(signal.isOutput ? (maxLabel.position > perUnitLabel.position+perUnitLabel.width+10) :
                                       (maxLabel.position > averageTextBox.position+averageTextBox.width+10));

            anchors.right: maxInput.left;
            anchors.rightMargin: idRectangle.width * 1/100;
            text: (signal.label == "Voltage" ? "Max V" : "Max A")
            font.pixelSize: currentFontSize;
            property real position: maxInput.position-idRectangle.width*1/100-maxLabel.width;
        }
        TextInput {
          id: maxInput
          visible: maxLabel.visible
           text: ""
          color: "#FFF"
          selectByMouse: true
          font.pixelSize: currentFontSize
          readOnly: false;

           property real position: minLabel.position-idRectangle.width*3/100-maxInput.width;

           Binding {
             target: maxInput
             property: "text"
             value: axes.ymax.toFixed(3)
             restoreMode: Binding.RestoreBindingOrValue
           }


          onEditingFinished: {
            var value = constrainValue(numericValue(text, axes.ymax), signal.min-axes.overrangeSpan, signal.max+axes.overrangeSpan)
            value = Math.max(value, axes.ymin + Math.max(signal.resolution, 0.000000001));
            text = value.toFixed(3)
            axes.ymax = value;
          }

          validator: DoubleValidator {
            bottom: signal.min - axes.overrangeSpan
            top: signal.max + axes.overrangeSpan
            notation: DoubleValidator.StandardNotation
          }
          anchors.right: minLabel.left;
          anchors.rightMargin: idRectangle.width * 3/100;
        }
        //Min Input
        Text {
            id: minLabel;
            color: "white";
            visible: (signal.isOutput ? minLabel.position > perUnitLabel.position+perUnitLabel.width+10 :
                                        minLabel.position > averageTextBox.position+averageTextBox.width+10);
            anchors.right: minInput.left;
            anchors.rightMargin: idRectangle.width * 1/100;
            text: (signal.label == "Voltage" ? "Min V" : "Min A")
            font.pixelSize: currentFontSize;
            property real position: parent.width-minInput.width-idRectangle.width*1/100+35-minLabel.width;
        }
        TextInput {
          id: minInput
          visible: minLabel.visible;
           text: ""
          color: "#FFF"
          selectByMouse: true
          font.pixelSize: currentFontSize
          readOnly: false;

           property real position: parent.width+35-minInput.width;

           Binding {
             target: minInput
             property: "text"
             value: axes.ymin.toFixed(3)
             restoreMode: Binding.RestoreBindingOrValue
           }


          onEditingFinished: {
            var value = constrainValue(numericValue(text, axes.ymin), signal.min-axes.overrangeSpan, signal.max+axes.overrangeSpan)
            value = Math.min(value, axes.ymax - Math.max(signal.resolution, 0.000000001));
            text = value.toFixed(3)
            axes.ymin = value;
          }

          validator: DoubleValidator {
            bottom: signal.min - axes.overrangeSpan
            top: signal.max + axes.overrangeSpan
            notation: DoubleValidator.StandardNotation
          }
          anchors.right: parent.right;
          anchors.rightMargin: -35;
        }
     }
  }

  Item {
      id: vertAxes
      anchors.top: parent.top
      anchors.bottom: parent.bottom
      anchors.left: axes.right
      anchors.topMargin: timelinePane.spacing
      width: timelinePane.width - xaxis.width - signalsPane.width

      MouseArea {
        anchors.fill: parent

        property var zoomParams
        acceptedButtons: Qt.RightButton
        onPressed: {
          if (mouse.button == Qt.RightButton) {
            zoomParams = {
              firstY : mouse.y,
              prevY : mouse.y,
            }
          } else {
            mouse.accepted = false;
          }
        }
        onReleased: {
          zoomParams = null
        }
        onPositionChanged: {
          if (zoomParams) {
            var delta = (mouse.y - zoomParams.prevY)
            zoomParams.prevY = mouse.y
            var s = Math.pow(1.01, delta)
            var y = axes.pxToY(zoomParams.firstY);

            var resolution = signal.label === 'Current' ? signal.resolution * 10 : signal.resolution;


            if (axes.ymax - axes.ymin < resolution * ygridticks && s < 1) return;

            axes.ymin = Math.max(y - s * (y - axes.ymin), signal.min-axes.overrangeSpan);
            axes.ymax = Math.min(y - s * (y - axes.ymax), signal.max+axes.overrangeSpan);

          }
        }
      }
  }

  Axes {
    id: axes
    property real overrangeSpan: (signal.max - signal.min) * 0.02 // 2% of full range
    x: parent.width
    width: xaxis.width

    anchors.top: parent.top
    anchors.bottom: parent.bottom
    anchors.topMargin: timelinePane.spacing

    ymin: signal.min - overrangeSpan
    ymax: signal.max + overrangeSpan
    xgridticks: 2
    yleft: false
    yright: true
    xbottom: false

    gridColor: window.signalAxesColor
    textColor: '#FFF'

    states: [
      State {
        name: "floating"
        PropertyChanges { target: axes
          anchors.top: undefined
          anchors.bottom: undefined
          gridColor: '#111'
          textColor: '#444'
        }
        PropertyChanges { target: axes_mouse_area
          drag.target: axes
          drag.axis: Drag.YAxis
        }
        PropertyChanges { target: overlay_periodic; visible: false }
        PropertyChanges { target: overlay_constant; visible: false }
      }
    ]

    MouseArea {
      id: axes_mouse_area
      anchors.fill: parent

      acceptedButtons: Qt.LeftButton | Qt.MiddleButton
       property var panStart
       property var floatingZState: []
      onPressed: {
        if (mouse.button == Qt.MiddleButton) {
           axesBackground.opacity = 0;
           floatingZState = [];
           for(var d = 0; d < deviceRepeater.count; d++) {
            var dev = deviceRepeater.itemAt(d);
            if (!dev)
              continue;
            floatingZState.push({item: dev, z: dev.z});
            dev.z = -2;
            for(var c = 0; c < dev.channelRepeater.count; c++){
              var ch = dev.channelRepeater.itemAt(c);
              if (!ch)
                continue;
              floatingZState.push({item: ch, z: ch.z});
              ch.z = -2;
              for(var s = 0; s < ch.signalRepeater.count; s++){
                var signalItem = ch.signalRepeater.itemAt(s);
                if (signalItem) {
                    floatingZState.push({item: signalItem, z: signalItem.z});
                    signalItem.z = -2;
                }
              }
            }
          }
           floatingZState.push({item: signalBlock, z: signalBlock.z});
           signalBlock.z = 2;

          axes.state = "floating"
        } else if (mouse.button == Qt.LeftButton && mouse.modifiers & Qt.ShiftModifier) {
          panStart = {
            y: mouse.y,
            ymin: axes.ymin,
            ymax: axes.ymax,
          }
        } else {
          mouse.accepted = false;
        }
      }
       onReleased: {
         axesBackground.opacity = 1;
         axes.state = "";
         for (var i = 0; i < floatingZState.length; i++)
             floatingZState[i].item.z = floatingZState[i].z;
         floatingZState = [];
         panStart = null
       }
       onCanceled: {
         axesBackground.opacity = 1;
         axes.state = "";
         for (var i = 0; i < floatingZState.length; i++)
             floatingZState[i].item.z = floatingZState[i].z;
         floatingZState = [];
         panStart = null
       }
       onPositionChanged: {
        // Shift + drag for Y-axis pan
        if (panStart) {
          var delta = (mouse.y - panStart.y) / axes.yscale
          delta = Math.max(delta, signal.min - panStart.ymin)
          delta = Math.min(delta, signal.max - panStart.ymax)
          axes.ymin = panStart.ymin + delta
          axes.ymax = panStart.ymax + delta
        }
      }
      onWheel: {
        // Shift + scroll for Y-axis zoom
        if (wheel.modifiers & Qt.ShiftModifier) {
          var s = Math.pow(1.15, -wheel.angleDelta.y/120);
          var y = axes.pxToY(wheel.y);

          var resolution = signal.label === 'Current' ? signal.resolution * 10 : signal.resolution;
          if (axes.ymax - axes.ymin < resolution * ygridticks && s < 1) return;

          axes.ymin = Math.max(y - s * (y - axes.ymin), signal.min - axes.overrangeSpan);
          axes.ymax = Math.min(y - s * (y - axes.ymax), signal.max + axes.overrangeSpan);
        }
        else {
          wheel.accepted = false
        }
      }
    }

    Rectangle {
      anchors.top: parent.bottom
      anchors.left: parent.left
      anchors.right: parent.right
      height: 2
      color: window.signalAxesColor
    }

    Rectangle {
      anchors.bottom: parent.top
      anchors.left: parent.left
      anchors.right: parent.right
      height: 1
      color: window.signalAxesColor
    }


    Rectangle {
      id: axesBackground
      anchors.fill: parent
      color: window.signalRowColor
      z: -1
      opacity: 1
    }

    PhosphorRender {
        id: line
        anchors.fill: parent
        clip: true
        buffer: signal.buffer
        pointSize: Math.min(25, Math.max(2, xaxis.xscale/session.sampleRate*3) * window.dotSizeSignal * 10)
        color: signal.label == 'Current' ? window.dotSignalCurrent : window.dotSignalVoltage
        xmin: xaxis.visibleMin
        xmax: xaxis.visibleMax
        ymin: axes.ymin
        ymax: axes.ymax

        Component.onCompleted: {
          signal.buffer.setIgnoredFirstSamplesCount(controller.delaySampleCount)
        }
    }

    OverlayPeriodic {
      id: overlay_periodic
      visible: (signal.src.src == 'sine' || signal.src.src == 'triangle' || signal.src.src == 'sawtooth' || signal.src.src == 'stairstep' || signal.src.src == 'square') && (channel.mode == {'Voltage': 1, 'Current': 2}[signal.label])
    }

    OverlayConstant {
      id: overlay_constant
      visible: signal.src.src == 'constant'
    }

  }
}
