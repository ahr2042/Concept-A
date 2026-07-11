#include "Shell.h"

#include "../theme/Theme.h"
#include "Components.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

// ── NavTabBar ────────────────────────────────────────────────────────────────

NavTabBar::NavTabBar(const QStringList& tabs, QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    for (int i = 0; i < tabs.size(); ++i) {
        auto* b = new QPushButton(tabs[i], this);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFont(theme::monoFont(10, QFont::Bold, 112.0));
        b->setStyleSheet(QStringLiteral(
            "QPushButton { background: transparent; border: none; border-bottom: 2px solid transparent;"
            "  border-radius: 0; color: %1; padding: 10px 12px; }"
            "QPushButton:hover { color: %2; }"
            "QPushButton:checked { color: %2; border-bottom: 2px solid %3; }")
            .arg(theme::kOnSurfaceVariant, theme::kOnSurface, theme::palette().accent.name()));
        connect(b, &QPushButton::clicked, this, [this, i] { setCurrent(i); });
        m_buttons.append(b);
        lay->addWidget(b);
    }
    if (!m_buttons.isEmpty())
        m_buttons[0]->setChecked(true);
}

void NavTabBar::setCurrent(int index)
{
    if (index >= m_buttons.size())
        return;
    // index < 0 clears the highlight (e.g. while the Settings page is shown)
    for (int i = 0; i < m_buttons.size(); ++i)
        m_buttons[i]->setChecked(i == index);
    if (index < 0)
        return;
    const bool changed = (m_current != index);
    m_current = index;
    if (changed)
        emit currentChanged(index);
}

// ── TopBar ───────────────────────────────────────────────────────────────────

namespace {

QPushButton* iconButton(const QString& label, const QString& tip, QWidget* parent)
{
    auto* b = new QPushButton(label, parent);
    b->setProperty("vosRole", QStringLiteral("ghost"));
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedHeight(30);
    b->setFont(theme::monoFont(9, QFont::Bold, 108.0));
    return b;
}

} // namespace

TopBar::TopBar(const QStringList& pages, QWidget* parent)
    : QFrame(parent)
{
    setFixedHeight(56);
    setStyleSheet(QStringLiteral(
        "TopBar { background: %1; border-bottom: 1px solid %2; }")
        .arg(theme::kSurfaceLowest, theme::kOutlineVariant));

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(20, 0, 16, 0);
    lay->setSpacing(18);

    // Logo: VISION_OS / ALPHA with the design's glow accent on the first word.
    auto* logo = new QLabel(this);
    logo->setTextFormat(Qt::RichText);
    logo->setText(QStringLiteral(
        "<span style='color:%1;'>VISION_OS</span>"
        "<span style='color:%2;'> / MFGCV</span>")
        .arg(theme::palette().accent.name(), theme::kOnSurface));
    logo->setFont(theme::displayFont(15, QFont::Bold));
    lay->addWidget(logo);

    m_nav = new NavTabBar(pages, this);
    lay->addWidget(m_nav);
    lay->addStretch(1);

    auto* record = iconButton(QStringLiteral("● REC"), QStringLiteral("Record — PLANNED (file sink not wired)"), this);
    vos::markPlanned(record);
    auto* snap = iconButton(QStringLiteral("SNAP"), QStringLiteral("Snapshot active viewport → PNG"), this);
    auto* gear = iconButton(QStringLiteral("CONFIG"), QStringLiteral("System settings"), this);
    connect(record, &QPushButton::clicked, this, &TopBar::recordRequested);
    connect(snap,   &QPushButton::clicked, this, &TopBar::snapshotRequested);
    connect(gear,   &QPushButton::clicked, this, &TopBar::settingsRequested);
    lay->addWidget(record);
    lay->addWidget(snap);
    lay->addWidget(gear);

    // Operator chip (design: ENGINEER_01 / LVL_4_AUTH).
    auto* op = new QWidget(this);
    auto* opLay = new QVBoxLayout(op);
    opLay->setContentsMargins(12, 0, 0, 0);
    opLay->setSpacing(1);
    QString user = qEnvironmentVariable("USER", QStringLiteral("operator")).toUpper();
    auto* name = new QLabel(user, op);
    name->setFont(theme::monoFont(9, QFont::Bold, 110.0));
    name->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kOnSurface));
    name->setAlignment(Qt::AlignRight);
    auto* lvl = new QLabel(QStringLiteral("LVL_4_AUTH"), op);
    lvl->setFont(theme::monoFont(8, QFont::Medium, 110.0));
    lvl->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kOnSurfaceVariant));
    lvl->setAlignment(Qt::AlignRight);
    opLay->addWidget(name);
    opLay->addWidget(lvl);
    lay->addWidget(op);
}

// ── SideRail ─────────────────────────────────────────────────────────────────

SideRail::SideRail(QWidget* parent)
    : QFrame(parent)
{
    setFixedWidth(232);
    setStyleSheet(QStringLiteral(
        "SideRail { background: %1; border-right: 1px solid %2; }")
        .arg(theme::kSurfaceLowest, theme::kOutlineVariant));

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 18, 16, 14);
    lay->setSpacing(6);

    // SOURCES ● / Live Telemetry Active
    auto* head = new QWidget(this);
    auto* headLay = new QHBoxLayout(head);
    headLay->setContentsMargins(0, 0, 0, 0);
    headLay->setSpacing(8);
    auto* title = new QLabel(QStringLiteral("SOURCES"), head);
    title->setFont(theme::monoFont(10, QFont::Bold, 116.0));
    title->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kOnSurface));
    m_led = new vos::LedDot(QColor(theme::palette().accent), head);
    m_led->setPulsing(true);
    headLay->addWidget(m_led);
    headLay->addWidget(title);
    headLay->addStretch(1);
    lay->addWidget(head);

    m_liveCaption = new QLabel(QStringLiteral("Control Link Offline"), this);
    m_liveCaption->setFont(theme::monoFont(8, QFont::Medium, 106.0));
    m_liveCaption->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kOnSurfaceVariant));
    lay->addWidget(m_liveCaption);

    lay->addSpacing(10);
    lay->addWidget(vos::makeHSeparator(this));
    lay->addSpacing(10);

    lay->addWidget(vos::capsLabel(QStringLiteral("CONNECTED_DEVICES"), 8, this));
    lay->addSpacing(4);

    m_usbButton = addCategory(QStringLiteral("▸"), QStringLiteral("USB SOURCES"), true);
    lay->addWidget(m_usbButton);
    auto* rtsp = addCategory(QStringLiteral("▸"), QStringLiteral("RTSP STREAMS"), false);
    auto* gige = addCategory(QStringLiteral("▸"), QStringLiteral("GIGE VISION"), false);
    auto* stor = addCategory(QStringLiteral("▸"), QStringLiteral("STORAGE"), false);
    auto* net  = addCategory(QStringLiteral("▸"), QStringLiteral("NETWORK"), false);
    for (auto* b : { rtsp, gige, stor, net })
        lay->addWidget(b);

    lay->addSpacing(14);

    auto* add = new QPushButton(QStringLiteral("+ ADD SOURCE"), this);
    add->setProperty("vosRole", QStringLiteral("outline"));
    add->setCursor(Qt::PointingHandCursor);
    add->setFixedHeight(38);
    connect(add, &QPushButton::clicked, this, &SideRail::addSourceRequested);
    lay->addWidget(add);

    lay->addStretch(1);

    auto* help = addCategory(QStringLiteral("?"), QStringLiteral("HELP"), true);
    auto* sys  = addCategory(QStringLiteral(">_"), QStringLiteral("SYSTEM"), true);
    connect(help, &QPushButton::clicked, this, &SideRail::helpRequested);
    connect(sys,  &QPushButton::clicked, this, &SideRail::systemRequested);
    lay->addWidget(help);
    lay->addWidget(sys);
}

QPushButton* SideRail::addCategory(const QString& icon, const QString& name, bool enabled)
{
    auto* b = new QPushButton(QStringLiteral("%1   %2").arg(icon, name), this);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedHeight(36);
    b->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; border: none; border-radius: 3px; text-align: left;"
        "  padding-left: 10px; color: %1; }"
        "QPushButton:hover { background: %2; color: %3; }"
        "QPushButton:disabled { color: #55555a; }")
        .arg(theme::kOnSurfaceVariant, theme::kSurfaceHigh, theme::palette().accent.name()));
    b->setFont(theme::monoFont(9, QFont::Bold, 110.0));
    if (!enabled)
        b->setToolTip(QStringLiteral("PLANNED — protocol not yet supported by the backend"));
    b->setEnabled(enabled);
    if (enabled && !name.startsWith(QStringLiteral("HELP")) && !name.startsWith(QStringLiteral("SYSTEM")))
        connect(b, &QPushButton::clicked, this, [this, name] { emit categorySelected(name); });
    return b;
}

void SideRail::setLinkOnline(bool online)
{
    m_led->setColor(online ? QColor(theme::palette().accent) : QColor(theme::kError));
    m_liveCaption->setText(online ? QStringLiteral("Live Telemetry Active")
                                  : QStringLiteral("Control Link Offline"));
}

void SideRail::setUsbCount(int devices)
{
    m_usbButton->setText(QStringLiteral("▸   USB SOURCES  [%1]").arg(devices));
}
