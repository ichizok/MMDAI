/**

 Copyright (c) 2010-2014  hkrn

 All rights reserved.

 Redistribution and use in source and binary forms, with or
 without modification, are permitted provided that the following
 conditions are met:

 - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
 - Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following
   disclaimer in the documentation and/or other materials provided
   with the distribution.
 - Neither the name of the MMDAI project team nor the names of
   its contributors may be used to endorse or promote products
   derived from this software without specific prior written
   permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.

*/

import QtQuick 2.2
import QtQuick.Controls 1.1
import QtQuick.Window 2.1
import QtQuick.Layouts 1.1
import com.github.mmdai.VPMM 1.0 as VPMM

ApplicationWindow {
    readonly property bool isOSX: Qt.platform.os === "osx"
    readonly property int applicationLayoutAnchorMargin : 10
    property bool isFullSceneMode: false
    minimumWidth: 960
    minimumHeight: 620
    title: "%1 - %2".arg(Qt.application.name).arg(scene.project.title)

    function __handleApplicationStateChange() {
        var state = Qt.application.state
        scene.state = Qt.ApplicationActive ? scene.lastStateAtSuspend : "suspend"
    }
    function updateWindowRect() {
        x = applicationPreference.windowRect.x
        y = applicationPreference.windowRect.y
        width = applicationPreference.windowRect.width
        height = applicationPreference.windowRect.height
    }
    function exitApplication() {
        scene.state = "suspend"
        applicationPreference.windowRectChanged.disconnect(updateWindowRect)
        applicationPreference.windowRect.x = x
        applicationPreference.windowRect.y = y
        applicationPreference.windowRect.width = width
        applicationPreference.windowRect.height = height
        applicationPreference.sync()
        Qt.quit()
    }
    Component.onCompleted: {
        scene.project.initializeOnce()
        updateWindowRect()
        applicationPreference.windowRectChanged.connect(updateWindowRect)
        Qt.application.stateChanged.connect(__handleApplicationStateChange)
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("File")
            MenuItem {
                text: qsTr("Exit")
                onTriggered: Qt.quit();
            }
        }
    }

    ListModel {
        id: verticesModel
        function indexOf(value) {
            if (value) {
                for (var i = 0, l = count, uuid = value.uuid; i < l; i++) {
                    var item = get(i).item
                    if (item && item.uuid === uuid) {
                        return i
                    }
                }
            }
            return 0
        }
        function __handleCurrentModelChanged() {
            var model = scene.project.currentModel
            if (model) {
                var vertices = model.allVertices
                clear()
                for (var i in vertices) {
                    var vertex = vertices[i]
                    append({ "item": vertex, "text": vertex.name })
                }
            }
        }
        Component.onCompleted: scene.project.currentModelChanged.connect(__handleCurrentModelChanged)
    }
    ListModel {
        id: materialsModel
        function indexOf(value) {
            if (value) {
                for (var i = 0, l = count, uuid = value.uuid; i < l; i++) {
                    var item = get(i).item
                    if (item && item.uuid === uuid) {
                        return i
                    }
                }
            }
            return 0
        }
        function __handleCurrentModelChanged() {
            var model = scene.project.currentModel
            if (model) {
                var materials = model.allMaterials
                clear()
                append({ "item": null, "text": qsTr("None") })
                for (var i in materials) {
                    var material = materials[i]
                    append({ "item": material, "text": material.name })
                }
            }
        }
        Component.onCompleted: scene.project.currentModelChanged.connect(__handleCurrentModelChanged)
    }
    ListModel {
        id: bonesModel
        function indexOf(value) {
            if (value) {
                for (var i = 0, l = count, uuid = value.uuid; i < l; i++) {
                    var item = get(i).item
                    if (item && item.uuid === uuid) {
                        return i
                    }
                }
            }
            return 0
        }
        function __handleCurrentModelChanged() {
            var model = scene.project.currentModel
            if (model) {
                var bones = model.allBones
                clear()
                append({ "item": null, "text": qsTr("None") })
                for (var i in bones) {
                    var bone = bones[i]
                    append({ "item": bone, "text": bone.name })
                }
            }
        }
        Component.onCompleted: scene.project.currentModelChanged.connect(__handleCurrentModelChanged)
    }
    ListModel {
        id: labelsModel
        function indexOf(value) {
            if (value) {
                for (var i = 0, l = count, uuid = value.uuid; i < l; i++) {
                    var item = get(i).item
                    if (item && item.uuid === uuid) {
                        return i
                    }
                }
            }
            return 0
        }
        function __handleCurrentModelChanged() {
            var model = scene.project.currentModel
            if (model) {
                var labels = model.allLabels
                clear()
                append({ "item": null, "text": qsTr("None") })
                for (var i in labels) {
                    var label = labels[i]
                    append({ "item": label, "text": label.name })
                }
            }
        }
        Component.onCompleted: {
            scene.project.currentModelChanged.connect(__handleCurrentModelChanged)
        }
    }
    ListModel {
        id: morphsModel
        function indexOf(value) {
            if (value) {
                for (var i = 0, l = count, uuid = value.uuid; i < l; i++) {
                    var item = get(i).item
                    if (item && item.uuid === uuid) {
                        return i
                    }
                }
            }
            return 0
        }
        function __handleCurrentModelChanged() {
            var model = scene.project.currentModel
            if (model) {
                var morphs = model.allMorphs
                clear()
                append({ "item": null, "text": qsTr("None") })
                for (var i in morphs) {
                    var morph = morphs[i]
                    append({ "item": morph, "text": morph.name })
                }
            }
        }
        Component.onCompleted: scene.project.currentModelChanged.connect(__handleCurrentModelChanged)
    }
    ListModel {
        id: rigidBodiesModel
        function indexOf(value) {
            if (value) {
                for (var i = 0, l = count, uuid = value.uuid; i < l; i++) {
                    var item = get(i).item
                    if (item && item.uuid === uuid) {
                        return i
                    }
                }
            }
            return 0
        }
        function __handleCurrentModelChanged() {
            var model = scene.project.currentModel
            if (model) {
                var bodies = model.allRigidBodies
                clear()
                append({ "item": null, "text": qsTr("None") })
                for (var i in bodies) {
                    var body = bodies[i]
                    append({ "item": body, "text": body.name })
                }
            }
        }
        Component.onCompleted: scene.project.currentModelChanged.connect(__handleCurrentModelChanged)
    }
    ListModel {
        id: jointsModel
        function indexOf(value) {
            if (value) {
                var uuid = value.uuid
                for (var i = 0, l = count; i < l; i++) {
                    var item = get(i).item
                    if (item && item.uuid === uuid) {
                        return i
                    }
                }
            }
            return 0
        }
        function __handleCurrentModelChanged() {
            var model = scene.project.currentModel
            if (model) {
                var joints = model.allJoints
                clear()
                append({ "item": null, "text": qsTr("None") })
                for (var i in joints) {
                    var joint = joints[i]
                    append({ "item": joint, "text": joint.name })
                }
            }
        }
        Component.onCompleted: scene.project.currentModelChanged.connect(__handleCurrentModelChanged)
    }
    ListModel {
        id: softBodiesModel
        function __handleCurrentModelChanged() {
            var model = scene.project.currentModel
            if (model) {
            }
        }
        Component.onCompleted: scene.project.currentModelChanged.connect(__handleCurrentModelChanged)
    }

    ListModel {
        id: uvTypeModel
        ListElement { text: "Default"; value: 0 }
        ListElement { text: "UVA1"; value: 1 }
        ListElement { text: "UVA2"; value: 2 }
        ListElement { text: "UVA3"; value: 3 }
        ListElement { text: "UVA4"; value: 4 }
    }
    ListModel {
        id: transformTypeModel
        ListElement { text: "BDEF1"; value: 0 }
        ListElement { text: "BDEF2"; value: 1 }
        ListElement { text: "BDEF4"; value: 3 }
        ListElement { text: "SDEF";  value: 2 }
        ListElement { text: "QDEF";  value: 4 }
        function indexOf(type) {
            switch (type) {
            case VPMM.Vertex.Bdef1:
            default:
                return 0
            case VPMM.Vertex.Bdef2:
                return 1
            case VPMM.Vertex.Bdef4:
                return 2
            case VPMM.Vertex.Sdef:
                return 3
            case VPMM.Vertex.Qdef:
                return 4
            }
        }
    }
    ListModel {
        id: languageModel
        ListElement { text: "Japanese" }
        ListElement { text: "English" }
    }

    SystemPalette { id: systemPalette }
    color: systemPalette.window
    statusBar: StatusBar {
        Label {
            id: statusBarLabel
            Layout.fillWidth: true
        }
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal
        Layout.fillHeight: true
        Rectangle {
            id: modelViewPanel
            Layout.minimumWidth: 370
            Layout.fillHeight: true
            color: systemPalette.window
            width: 370
            ColumnLayout {
                anchors.fill: parent
                RowLayout {
                    Layout.alignment: Qt.AlignCenter
                    Label {
                        visible: objectType.visible
                        text: qsTr("Type")
                    }
                    ComboBox {
                        id: objectType
                        visible: stackView.depth === 1
                        model: [
                            { "text": qsTr("Vertex"), "path": "VertexView.qml", "reference": verticesModel },
                            { "text": qsTr("Material"), "path": "MaterialView.qml", "reference": materialsModel },
                            { "text": qsTr("Bone"), "path": "BoneView.qml", "reference": bonesModel },
                            { "text": qsTr("Morph"), "path": "MorphView.qml", "reference": morphsModel },
                            { "text": qsTr("Label"), "path": "LabelView.qml", "reference": labelsModel },
                            { "text": qsTr("RigidBody"), "path": "RigidBodyView.qml", "reference": rigidBodiesModel },
                            { "text": qsTr("Joint"), "path": "JointView.qml", "reference": jointsModel },
                            { "text": qsTr("SoftBody"), "path": "SoftBodyView.qml", "reference": softBodiesModel }
                        ]
                        onCurrentIndexChanged: objectListView.model = model[currentIndex].reference
                    }
                    Button {
                        visible: stackView.depth > 1
                        text: "Back To List"
                        onClicked: stackView.pop(null)
                    }
                }
                StackView {
                    id: stackView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    initialItem: Item {
                        width: parent.width
                        height: parent.height
                        TableView {
                            id: objectListView
                            anchors.fill: parent
                            frameVisible: false
                            sortIndicatorVisible: false
                            TableViewColumn {
                                role: "index"
                                title: "Index"
                                resizable: true
                            }
                            TableViewColumn {
                                role: "text"
                                title: "Name"
                                resizable: true
                            }
                            onDoubleClicked: {
                                var targetObject = model.get(row).item,
                                        path = objectType.model[objectType.currentIndex].path
                                var arguments = {
                                    "item": Qt.resolvedUrl(path),
                                    "properties": { "targetObject": targetObject }
                                }
                                stackView.push(arguments)
                            }
                        }
                    }
                    delegate: StackViewDelegate {
                        function transitionFinished(properties) {
                            properties.exitItem.opacity = 1
                        }
                        property Component pushTransition: StackViewTransition {
                            PropertyAnimation {
                                target: enterItem
                                property: "opacity"
                                from: 0
                                to: 1
                            }
                            PropertyAnimation {
                                target: exitItem
                                property: "opacity"
                                from: 1
                                to: 0
                            }
                        }
                    }
                }
            }
        }
        SplitView {
            orientation: Qt.Vertical
            Scene {
                id: scene
                Layout.fillWidth: true
                Layout.fillHeight: true
                offsetX: modelViewPanel.width
                offsetY: statusBar.height * statusBar.visible + propertyPanel.height * propertyPanel.visible
            }
            Rectangle {
                id: propertyPanel
                visible: false
                Layout.minimumHeight: 240
                Layout.maximumHeight: 400
                height: 240
                color: systemPalette.window
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    RowLayout {
                        Label {
                            text: qsTr("Type")
                        }
                        ComboBox {
                            model: [ "PMD 1.0", "PMX 2.0", "PMX 2.1" ]
                        }
                        Label {
                            text: qsTr("Encoding")
                        }
                        ComboBox {
                            model: [ "Shift_JIS", "UTF-8", "UTF-16" ]
                        }
                        Label {
                            text: qsTr("Additional UV")
                        }
                        SpinBox {
                            minimumValue: 0
                            maximumValue: 4
                        }
                    }
                    TextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("Input Model Name Here")
                    }
                    TextArea {
                        Layout.fillWidth: true
                    }
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }
                }
            }
        }
    }
}
