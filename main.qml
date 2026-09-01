import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import "qrc:/style"

Window {
    id: window
    width: 1280
    height: 800
    visible: true
    title: "Concord"
    color: Theme.background

    Component.onCompleted: authController.tryRestoreSession()

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: authController.authenticated ? mainComponent : loginComponent
    }

    Connections {
        target: authController
        function onAuthenticatedChanged() {
            stack.replace(authController.authenticated ? mainComponent : loginComponent)
        }
    }

    Component {
        id: loginComponent
        LoginPage {
            onGoToRegister: stack.push(registerComponent)
            onEmailUnconfirmed: stack.push(confirmationComponent)
        }
    }

    Component {
        id: registerComponent
        RegisterPage {
            onGoToLogin: stack.pop()
            onRegistered: stack.push(confirmationComponent)
        }
    }

    Component {
        id: confirmationComponent
        EmailConfirmationPage {
            onGoToLogin: stack.pop()
        }
    }

    Component {
        id: mainComponent
        MainWindow {}
    }
}
