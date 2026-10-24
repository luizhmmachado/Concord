#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:?usage: build-deb.sh <version> <binary> [output-dir]}"
BINARY="${2:?usage: build-deb.sh <version> <binary> [output-dir]}"
OUTPUT_DIR="${3:-dist}"
ARCH="amd64"
PACKAGE="concord"
ROOT="build/deb/${PACKAGE}_${VERSION}_${ARCH}"

if [[ ! -x "${BINARY}" ]]; then
    echo "error: binary not found or not executable: ${BINARY}" >&2
    exit 1
fi

if [[ ! "${VERSION}" =~ ^[0-9][0-9A-Za-z.+:~-]*$ ]]; then
    echo "error: invalid Debian version: ${VERSION}" >&2
    exit 1
fi

rm -rf "${ROOT}"
install -Dm755 "${BINARY}" "${ROOT}/usr/bin/concord"
install -Dm644 packaging/linux/concord.desktop "${ROOT}/usr/share/applications/concord.desktop"
install -Dm644 packaging/linux/concord.svg "${ROOT}/usr/share/icons/hicolor/scalable/apps/concord.svg"
install -Dm644 README.md "${ROOT}/usr/share/doc/concord/README.md"

mkdir -p "${ROOT}/DEBIAN"
INSTALLED_SIZE="$(du -sk "${ROOT}" | cut -f1)"

cat > "${ROOT}/DEBIAN/control" <<EOF
Package: ${PACKAGE}
Version: ${VERSION}
Section: net
Priority: optional
Architecture: ${ARCH}
Installed-Size: ${INSTALLED_SIZE}
Maintainer: Concord Maintainers <maintainers@concord.invalid>
Homepage: https://github.com/luizhmmachado/Concord
Depends: libc6 (>= 2.35), libxcb1, libqt5core5a (>= 5.15.2), libqt5gui5 (>= 5.15.2), libqt5network5 (>= 5.15.2), libqt5qml5 (>= 5.15.2), libqt5quick5 (>= 5.15.2), libqt5quickcontrols2-5 (>= 5.15.2), libqt5multimedia5 (>= 5.15.2), libqt5multimedia5-plugins, libqt5websockets5 (>= 5.15.2), libqt5widgets5 (>= 5.15.2), qml-module-qtquick2, qml-module-qtquick-controls2, qml-module-qtquick-layouts, qml-module-qtquick-window2, qml-module-qt-labs-platform, qml-module-qt-labs-folderlistmodel, qml-module-qtmultimedia, libgstreamer1.0-0 (>= 1.20), libgstreamer-plugins-base1.0-0 (>= 1.20), libgstreamer-plugins-bad1.0-0 (>= 1.20), gstreamer1.0-plugins-base, gstreamer1.0-plugins-good, gstreamer1.0-plugins-bad, gstreamer1.0-x, gstreamer1.0-alsa, gstreamer1.0-pulseaudio | gstreamer1.0-pipewire
Description: Concord desktop client
 Concord is a Qt/QML desktop client for real-time chat, servers,
 direct messages, image sharing and voice/video calls backed by Supabase.
EOF

mkdir -p "${OUTPUT_DIR}"
dpkg-deb --root-owner-group --build "${ROOT}" \
    "${OUTPUT_DIR}/${PACKAGE}_${VERSION}_${ARCH}.deb"
