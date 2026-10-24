import QtQuick 2.15
import Qt.labs.platform 1.1

FileDialog {
    id: root
    title: "Selecionar imagem"
    modality: Qt.ApplicationModal
    fileMode: FileDialog.OpenFile
    nameFilters: ["Imagens (*.png *.jpg *.jpeg *.gif *.webp *.bmp)", "Todos os arquivos (*)"]

    signal imageSelected(url selectedUrl)

    onAccepted: root.imageSelected(root.file)
}
