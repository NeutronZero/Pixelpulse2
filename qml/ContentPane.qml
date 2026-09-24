import QtQuick 2.1
import QtQuick.Layouts 1.0
import QtQuick.Controls 1.1
import QtQuick.Controls.Styles 1.1
import "jsutils.js" as JSUtils
import "sesssave.js" as StateSave

ColumnLayout {
  id: cLayout
  spacing: 12

  ToolbarStyle {
    Layout.fillWidth: true
    Layout.minimumWidth: parent.Layout.minimumWidth
    Layout.maximumWidth: parent.Layout.maximumWidth
    height: toolbarHeight
  }

  property string latestText: ""
  property string debugText: ""
  property color outputColor: "#fff"

  TextArea {
    id: outField
    readOnly: true
    Layout.fillWidth: true
    Layout.minimumWidth: parent.Layout.minimumWidth
    Layout.maximumWidth: parent.Layout.maximumWidth
    Layout.fillHeight: true
    selectByKeyboard: true
    selectByMouse: true
    backgroundVisible: false
    text: "Built: " + versions.build_date + "    " + "Version: " + versions.git_version + "\n" + latestText + debugText
    Component.onCompleted: {
      JSUtils.checkLatest(function(text) { latestText = text; }, function() {});
    }
    style: TextAreaStyle {
        textColor: cLayout.outputColor
        selectionColor: "steelblue"
        selectedTextColor: "#eee"
        backgroundColor: "#eee"
    }
  }

  TextInput {
    id: inField
    Layout.fillWidth: true
    Layout.minimumWidth: parent.Layout.minimumWidth
    Layout.maximumWidth: parent.Layout.maximumWidth
	cursorVisible: true
	text: "type here."
	color: "#FFF"
    onAccepted: {
    var out;
    try {
      out = JSUtils.toJSON(JSON.parse(text), 5, 10, "  ");
      cLayout.outputColor = "#fff";
      cLayout.debugText = "\n" + out;
    } catch (e) {
      out = e.message;
      cLayout.outputColor = "#f77";
    };
    cLayout.debugText = "\n" + out;
    }
	selectByMouse: true
  }
  MouseArea {
    anchors.fill: inField
    onPressed: { mouse.accepted = false; if (inField.text == "type here.") {inField.text = "" }}
  }
}
