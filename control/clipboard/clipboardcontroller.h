#ifndef CLIPBOARDCONTROLLER_H
#define CLIPBOARDCONTROLLER_H

#include <QObject>

// Reads images from the system clipboard for the chat input fields. When the
// clipboard contains raw image data it is written to a temporary PNG; when it
// contains local image file URLs those paths are returned directly.
class ClipboardController : public QObject
{
    Q_OBJECT
public:
    explicit ClipboardController(QObject *parent = nullptr);

    // Returns a local path for a pasted image, or an empty string when the
    // clipboard does not contain an image.
    Q_INVOKABLE QString pastedImagePath();
};

#endif // CLIPBOARDCONTROLLER_H
