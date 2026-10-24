import QtQuick 2.15
import QtQuick.Dialogs 1.3

FileDialog {
    id: root
    title: "Selecionar imagem"
    modality: Qt.ApplicationModal
    selectExisting: true
    selectMultiple: false
    nameFilters: ["Imagens (*.png *.jpg *.jpeg *.gif *.webp *.bmp)", "Todos os arquivos (*)"]

    signal imageSelected(url selectedUrl)

    onAccepted: root.imageSelected(root.fileUrl)
}
