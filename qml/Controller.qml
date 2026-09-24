import QtQuick 2.0

Item {
  property bool sessionActive: session.active
  property bool continuous: false
  property bool repeat: true
  property real sampleRate: session.devices.length ? session.devices[0].DefaultRate : 0
  property real maxOutSignalFreq: sampleRate > 0 ? sampleRate / 5 : 0
  property real sampleTime: 0.1
  readonly property int sampleCount: sampleRate > 0 ? Math.min(5000000, Math.max(0, Math.round(sampleTime * sampleRate + delaySampleCount))) : 0
  property bool restartAfterStop: false
  property int delaySampleCount: 0

  property bool dlySmplCntChanged: false
  property int queueSize: session.queueSize
  property real minOutSignalFreq: sampleRate > 0 && queueSize > 0 ? sampleRate / queueSize : 0

//  function trigger() {
//    session.sampleRate = sampleRate
//    session.sampleCount = sampleCount

//      if (dlySmplCntChanged) {
//          for (var i = 0; i < session.devices.length; i++) {
//            for (var j = 0; j < session.devices[i].channels.length; j++) {
//              session.devices[i].channels[j].signals[0].buffer.setIgnoredFirstSamplesCount(delaySampleCount);
//              session.devices[i].channels[j].signals[1].buffer.setIgnoredFirstSamplesCount(delaySampleCount);
//            }
//          }
//          dlySmplCntChanged = false;
//      }

//    session.start(continuous);
//    if ( session.devices.length > 0 ) {
//      lastConfig = StateSave.saveState();
//    }
//  }

//  onSampleTimeChanged: {
//    if (continuous && enabled) {
//      enabled = false;
//      restartAfterStop = true;
//      session.cancel();
//      enabled = true;
//    }
//  }

//  Timer {
//    id: timer
//    interval: 100
//    onTriggered: { trigger() }
//  }

//  function toggle() {
//    if (!enabled) {
//      trigger();
//      enabled = true;
//    } else {
//      enabled = false;
//      if (continuous || sampleTime > 0.1) {
//        session.cancel();
//      }
//    }
//  }

  function toggle() {
      //console.log("queue size"+queueSize)
      //console.log("min freq"+minOutSignalFreq)
       if (!session.active) {
           applyDelay();
           session.sampleRate = sampleRate
           session.sampleCount = sampleCount
           session.sampleTime = sampleTime
           session.start(continuous);
      } else {
          session.cancel();
      }
  }
  onSampleCountChanged: {
        //console.log("onSampleCountChanged");
        //console.log(sampleCount);
        session.sampleCount = sampleCount;
  }

  onSampleTimeChanged: {
      session.sampleTime = sampleTime;
  }

//  Timer {
//    id: updateMeasurementsTimer
//    interval: 50
//    repeat: true
//    running: enabled && continuous
//    onTriggered: session.updateMeasurements()
//  }

//  Timer {
//    id: updateLabelsTimer
//    interval: 500
//    repeat: true
//    running: enabled
//    onTriggered: session.updateAllMeasurements()
//  }

  onContinuousChanged: {
    toolbar.acqusitionDialog.onContinuousModeChanged(continuous);
    if(session.active){
        session.cancel();
        session.start(continuous);
    }
  }

  function applyDelay() {
      for (var i = 0; i < session.devices.length; i++) {
          for (var j = 0; j < session.devices[i].channels.length; j++) {
              var signals = session.devices[i].channels[j].signals;
              for (var k = 0; k < signals.length; k++)
                  signals[k].buffer.setIgnoredFirstSamplesCount(delaySampleCount);
          }
      }
  }

  onDelaySampleCountChanged: {
      applyDelay();
  }
//  Connections {
//    target: session

//    onFinished: {
//      if (enabled && restartAfterStop) {
//        restartAfterStop = false;
//        timer.start()
//        return;
//      }

//      if (!continuous && repeat && enabled) {
//        timer.start();
//      }
//    }

//    onDetached: {
//      enabled = false;
//      continuous = false;
//    }
//  }

  Repeater {
    model: session.devices
    Item {
      Repeater {
        model: modelData.channels
        Item {
          Connections {
            target: modelData
            onModeChanged: {
                if(continuous)
                    session.restart();
            }
          }
        }
      }
    }
  }
}
