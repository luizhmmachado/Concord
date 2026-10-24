import QtQuick 2.15
import "qrc:/style"

Rectangle {
    id: root
    property string imageSource: ""
    property int maxWidth: 360
    property int maxHeight: 320

    visible: imageSource.length > 0
    width: Math.min(parent ? parent.width : maxWidth, maxWidth)
    height: image.status === Image.Ready && image.implicitWidth > 0
            ? Math.min(width * image.implicitHeight / image.implicitWidth, maxHeight)
            : 160
    radius: Theme.radiusSmall
    color: Theme.surfaceAlt
    clip: true

    Image {
        id: image
        anchors.fill: parent
        source: root.imageSource
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: true
        smooth: true
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: imageController.openImage(root.imageSource)
    }
}
