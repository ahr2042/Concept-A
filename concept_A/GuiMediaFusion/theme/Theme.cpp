#include "Theme.h"

#include <QFontDatabase>
#include <QStringList>

namespace theme {

namespace {

Accent g_accent = Accent::Cyan;

bool     g_fontsLoaded = false;
QString  g_displayFamily;
QString  g_bodyFamily;
QString  g_monoFamily;

QString firstLoadedFamily(const QStringList& resources, const QString& fallback)
{
    for (const QString& res : resources) {
        int id = QFontDatabase::addApplicationFont(res);
        if (id >= 0) {
            const QStringList fams = QFontDatabase::applicationFontFamilies(id);
            if (!fams.isEmpty())
                return fams.front();
        }
    }
    return fallback;
}

} // namespace

void loadBundledFonts()
{
    if (g_fontsLoaded) return;
    g_fontsLoaded = true;

    g_monoFamily = firstLoadedFamily(
        { ":/fonts/JetBrainsMono-Regular.ttf",
          ":/fonts/JetBrainsMono-Medium.ttf",
          ":/fonts/JetBrainsMono-Bold.ttf" },
        QStringLiteral("Ubuntu Mono"));

    g_displayFamily = firstLoadedFamily(
        { ":/fonts/SpaceGrotesk.ttf" }, QStringLiteral("Ubuntu"));

    g_bodyFamily = firstLoadedFamily(
        { ":/fonts/Geist.ttf" }, QStringLiteral("Ubuntu"));
}

QString displayFamily() { loadBundledFonts(); return g_displayFamily; }
QString bodyFamily()    { loadBundledFonts(); return g_bodyFamily;    }
QString monoFamily()    { loadBundledFonts(); return g_monoFamily;    }

QFont displayFont(int pt, int weight)
{
    QFont f(displayFamily(), pt, weight);
    f.setStyleHint(QFont::SansSerif);
    return f;
}

QFont bodyFont(int pt, int weight)
{
    QFont f(bodyFamily(), pt, weight);
    f.setStyleHint(QFont::SansSerif);
    return f;
}

QFont monoFont(int pt, int weight, qreal letterSpacingPercent)
{
    QFont f(monoFamily(), pt, weight);
    f.setStyleHint(QFont::Monospace);
    if (letterSpacingPercent != 100.0)
        f.setLetterSpacing(QFont::PercentageSpacing, letterSpacingPercent);
    return f;
}

Accent accent() { return g_accent; }
void   setAccent(Accent a) { g_accent = a; }

QString accentName(Accent a)
{
    switch (a) {
        case Accent::Cyan:    return QStringLiteral("CYAN");
        case Accent::Magenta: return QStringLiteral("MAGENTA");
        case Accent::Lilac:   return QStringLiteral("LILAC");
        case Accent::Peach:   return QStringLiteral("PEACH");
    }
    return {};
}

Palette palette()
{
    switch (g_accent) {
        case Accent::Magenta: return { QColor("#e254ff"), QColor("#b600f8"), QColor("#2b0038") };
        case Accent::Lilac:   return { QColor("#d1bcff"), QColor("#a68cf1"), QColor("#23005b") };
        case Accent::Peach:   return { QColor("#ffb59d"), QColor("#f28b6e"), QColor("#3a0a00") };
        case Accent::Cyan:
        default:              return { QColor("#00f2ff"), QColor("#00dbe7"), QColor("#00363a") };
    }
}

QString buildStyleSheet()
{
    const Palette p  = palette();
    const QString ac = p.accent.name();
    const QString ad = p.accentDim.name();
    const QString on = p.onAccent.name();
    const QString mono = monoFamily();
    const QString body = bodyFamily();

    // The design's sharp, thin-bordered command-center look: 2–4 px radii,
    // 1 px outline-variant borders, mono ALL_CAPS labels, neon accent states.
    QString qss = QStringLiteral(R"QSS(
* { outline: none; }

QWidget {
    background: transparent;
    color: %ON_SURFACE%;
    font-family: "%BODY%";
    font-size: 13px;
}
QMainWindow, QDialog { background: %SURFACE%; }

QToolTip {
    background: %SURFACE_HIGHEST%;
    color: %ON_SURFACE%;
    border: 1px solid %OUTLINE_VARIANT%;
    padding: 4px 8px;
    font-family: "%MONO%";
    font-size: 11px;
}

/* ── Cards / panels ─────────────────────────────────────────────────────── */
QFrame[vosCard="true"] {
    background: %SURFACE_LOW%;
    border: 1px solid %OUTLINE_VARIANT%;
    border-radius: 4px;
}
QFrame[vosCard="sunken"] {
    background: %SURFACE_LOWEST%;
    border: 1px solid %OUTLINE_VARIANT%;
    border-radius: 4px;
}
QFrame[vosCard="raised"] {
    background: %SURFACE_CONTAINER%;
    border: 1px solid %OUTLINE_VARIANT%;
    border-radius: 4px;
}
QFrame[vosSeparator="true"] { background: %OUTLINE_VARIANT%; border: none; }

/* ── Buttons ────────────────────────────────────────────────────────────── */
QPushButton {
    background: %SURFACE_HIGH%;
    color: %ON_SURFACE%;
    border: 1px solid %OUTLINE_VARIANT%;
    border-radius: 3px;
    padding: 7px 14px;
    font-family: "%MONO%";
    font-size: 11px;
    font-weight: 600;
    letter-spacing: 1px;
}
QPushButton:hover  { border-color: %ACCENT%; color: %ACCENT%; }
QPushButton:pressed{ background: %SURFACE_LOWEST%; }
QPushButton:disabled {
    color: #6a6a6e;
    border-color: #2c2c2f;
    background: %SURFACE_LOW%;
}

QPushButton[vosRole="primary"] {
    background: %ACCENT%;
    color: %ON_ACCENT%;
    border: 1px solid %ACCENT%;
    font-weight: 700;
}
QPushButton[vosRole="primary"]:hover   { background: %ACCENT_DIM%; border-color: %ACCENT_DIM%; color: %ON_ACCENT%; }
QPushButton[vosRole="primary"]:disabled{ background: %SURFACE_HIGH%; color: #6a6a6e; border-color: #2c2c2f; }

QPushButton[vosRole="deploy"] {
    background: %SECONDARY%;
    color: #ffffff;
    border: 1px solid %SECONDARY%;
    font-weight: 700;
}
QPushButton[vosRole="deploy"]:hover    { background: #cf3dff; border-color: #cf3dff; color: #ffffff; }
QPushButton[vosRole="deploy"]:disabled { background: %SURFACE_HIGH%; color: #6a6a6e; border-color: #2c2c2f; }

QPushButton[vosRole="danger"] {
    background: transparent;
    color: %ERROR%;
    border: 1px solid %ERROR_CONTAINER%;
}
QPushButton[vosRole="danger"]:hover { background: %ERROR_CONTAINER%; color: #ffdad6; border-color: %ERROR%; }

QPushButton[vosRole="ghost"] {
    background: transparent;
    border: 1px solid transparent;
    color: %ON_SURFACE_VARIANT%;
}
QPushButton[vosRole="ghost"]:hover { color: %ACCENT%; border-color: %OUTLINE_VARIANT%; }

QPushButton[vosRole="outline"] {
    background: transparent;
    border: 1px solid %ACCENT%;
    color: %ACCENT%;
}
QPushButton[vosRole="outline"]:hover { background: rgba(0,242,255,0.10); }

/* chips (protocol filters, log filters) */
QPushButton[vosRole="chip"] {
    background: %SURFACE_LOW%;
    border: 1px solid %OUTLINE_VARIANT%;
    color: %ON_SURFACE_VARIANT%;
    padding: 5px 12px;
    font-size: 10px;
}
QPushButton[vosRole="chip"]:checked {
    border-color: %ACCENT%;
    color: %ACCENT%;
    background: %SURFACE_LOWEST%;
}

/* ── Inputs ─────────────────────────────────────────────────────────────── */
QComboBox, QLineEdit, QSpinBox {
    background: %SURFACE_HIGH%;
    border: 1px solid %OUTLINE_VARIANT%;
    border-radius: 3px;
    padding: 7px 10px;
    font-family: "%MONO%";
    font-size: 11px;
    selection-background-color: %ACCENT%;
    selection-color: %ON_ACCENT%;
}
QComboBox:hover, QLineEdit:focus { border-color: %ACCENT%; }
QComboBox:disabled, QLineEdit:disabled { color: #6a6a6e; background: %SURFACE_LOW%; }
QComboBox::drop-down { border: none; width: 26px; }
QComboBox::down-arrow {
    image: none;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid %ON_SURFACE_VARIANT%;
    margin-right: 8px;
}
QComboBox QAbstractItemView {
    background: %SURFACE_CONTAINER%;
    border: 1px solid %OUTLINE_VARIANT%;
    color: %ON_SURFACE%;
    font-family: "%MONO%";
    font-size: 11px;
    selection-background-color: %ACCENT%;
    selection-color: %ON_ACCENT%;
}

/* ── Sliders ────────────────────────────────────────────────────────────── */
QSlider::groove:horizontal {
    height: 3px;
    background: %SURFACE_HIGHEST%;
    border-radius: 1px;
}
QSlider::sub-page:horizontal { background: %ACCENT%; border-radius: 1px; }
QSlider::handle:horizontal {
    width: 14px; height: 14px;
    margin: -6px 0;
    border-radius: 7px;
    background: %ACCENT%;
}
QSlider::handle:horizontal:disabled { background: #5a5a5e; }
QSlider::sub-page:horizontal:disabled { background: #5a5a5e; }

/* ── Checkboxes / radios ────────────────────────────────────────────────── */
QCheckBox, QRadioButton {
    spacing: 8px;
    font-family: "%MONO%";
    font-size: 11px;
    color: %ON_SURFACE%;
}
QCheckBox::indicator, QRadioButton::indicator { width: 15px; height: 15px; }
QCheckBox::indicator {
    border: 1px solid %OUTLINE%;
    border-radius: 2px;
    background: %SURFACE_LOWEST%;
}
QCheckBox::indicator:checked { background: %ACCENT%; border-color: %ACCENT%; }
QCheckBox::indicator:disabled { border-color: #3a3a3e; background: %SURFACE_LOW%; }
QRadioButton::indicator {
    border: 1px solid %OUTLINE%;
    border-radius: 8px;
    background: %SURFACE_LOWEST%;
}
QRadioButton::indicator:checked {
    background: qradialgradient(cx:0.5, cy:0.5, radius:0.5,
                fx:0.5, fy:0.5, stop:0 %ACCENT%, stop:0.45 %ACCENT%,
                stop:0.55 %SURFACE_LOWEST%, stop:1 %SURFACE_LOWEST%);
    border-color: %ACCENT%;
}

/* ── Scrollbars ─────────────────────────────────────────────────────────── */
QScrollBar:vertical   { background: transparent; width: 8px;  margin: 0; }
QScrollBar:horizontal { background: transparent; height: 8px; margin: 0; }
QScrollBar::handle {
    background: %SURFACE_HIGHEST%;
    border-radius: 4px;
    min-height: 24px; min-width: 24px;
}
QScrollBar::handle:hover { background: %OUTLINE%; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* ── Text areas / lists ─────────────────────────────────────────────────── */
QTextEdit, QPlainTextEdit, QListWidget, QTreeWidget, QTableWidget {
    background: %SURFACE_LOWEST%;
    border: 1px solid %OUTLINE_VARIANT%;
    border-radius: 3px;
    font-family: "%MONO%";
    font-size: 11px;
}
QHeaderView::section {
    background: %SURFACE_LOW%;
    color: %ON_SURFACE_VARIANT%;
    border: none;
    border-bottom: 1px solid %OUTLINE_VARIANT%;
    padding: 6px 8px;
    font-family: "%MONO%";
    font-size: 10px;
    font-weight: 700;
    letter-spacing: 1px;
}

QStatusBar {
    background: %SURFACE_LOWEST%;
    border-top: 1px solid %OUTLINE_VARIANT%;
    font-family: "%MONO%";
    font-size: 10px;
    color: %ON_SURFACE_VARIANT%;
}
QStatusBar::item { border: none; }

QLabel[vosHint="true"] { color: %ON_SURFACE_VARIANT%; font-size: 11px; }
)QSS");

    qss.replace("%SURFACE_LOWEST%",   kSurfaceLowest);
    qss.replace("%SURFACE_LOW%",      kSurfaceLow);
    qss.replace("%SURFACE_CONTAINER%",kSurfaceContainer);
    qss.replace("%SURFACE_HIGHEST%",  kSurfaceHighest);
    qss.replace("%SURFACE_HIGH%",     kSurfaceHigh);
    qss.replace("%SURFACE%",          kSurface);
    qss.replace("%ON_SURFACE_VARIANT%", kOnSurfaceVariant);
    qss.replace("%ON_SURFACE%",       kOnSurface);
    qss.replace("%OUTLINE_VARIANT%",  kOutlineVariant);
    qss.replace("%OUTLINE%",          kOutline);
    qss.replace("%SECONDARY%",        kSecondary);
    qss.replace("%ERROR_CONTAINER%",  kErrorContainer);
    qss.replace("%ERROR%",            kError);
    qss.replace("%ACCENT_DIM%",       ad);
    qss.replace("%ACCENT%",           ac);
    qss.replace("%ON_ACCENT%",        on);
    qss.replace("%MONO%",             mono);
    qss.replace("%BODY%",             body);
    return qss;
}

} // namespace theme
