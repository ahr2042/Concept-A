// GUIMediaFusion — VISION_OS operator console (Qt6 Widgets + GStreamer).
// Bootstraps GStreamer (needed by the in-process StreamReceiver), the bundled
// design fonts and the generated stylesheet, then hands over to MainWindow.
//
// --selftest-screenshot <base>   render every page to <base>_N_<name>.png and
//                                exit (used for offscreen design verification).

#include "MainWindow.h"
#include "theme/Theme.h"

#include <gst/gst.h>

#include <QApplication>
#include <QSettings>

int main(int argc, char* argv[])
{
    // GstVideoOverlay embedding needs a real X11 window id: under a Wayland
    // session run the GUI through XWayland, and pin GStreamer's GL stack to
    // X11 as well — otherwise libgstgl sees WAYLAND_DISPLAY, opens a Wayland
    // GL context, and segfaults when handed our X window handle.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")
        && !qEnvironmentVariableIsEmpty("DISPLAY"))
        qputenv("QT_QPA_PLATFORM", "xcb");
    if (!qEnvironmentVariableIsEmpty("DISPLAY"))
        qputenv("GST_GL_WINDOW", "x11");

    gst_init(&argc, &argv);

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ConceptA"));
    QCoreApplication::setApplicationName(QStringLiteral("VisionOS"));

    theme::loadBundledFonts();
    theme::setAccent(static_cast<theme::Accent>(
        QSettings().value(QStringLiteral("ui/accent"), 0).toInt()));
    app.setStyleSheet(theme::buildStyleSheet());
    app.setFont(theme::bodyFont(10));

    MainWindow window;
    window.show();

    const QStringList args = app.arguments();
    const int shotIdx = args.indexOf(QStringLiteral("--selftest-screenshot"));
    if (shotIdx >= 0 && shotIdx + 1 < args.size())
        window.screenshotTour(args.at(shotIdx + 1));
    if (args.contains(QStringLiteral("--selftest-stream")))
        window.streamSelfTest();

    return app.exec();
}
