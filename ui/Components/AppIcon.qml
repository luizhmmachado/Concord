import QtQuick 2.15
import QtGraphicalEffects 1.15
import "qrc:/style"

Item {
    id: root
    property string name: ""
    property int size: 20
    property color color: Theme.textPrimary
    readonly property string source: name.length > 0 ? "qrc:/icons/" + name + ".svg" : ""

    implicitWidth: size
    implicitHeight: size
    width: size
    height: size

    Image {
        id: image
        anchors.fill: parent
        source: root.source
        sourceSize.width: Math.max(1, root.size * 2)
        sourceSize.height: Math.max(1, root.size * 2)
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        visible: false
    }

    ColorOverlay {
        anchors.fill: parent
        source: image
        color: root.color
        cached: true
    }
}
