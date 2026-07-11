#include "Components.h"
#include "../theme/Theme.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

namespace vos {

namespace { bool g_ledPulse = true; }

QFrame* makeCard(const char* variant, QWidget* parent)
{
    auto* f = new QFrame(parent);
    f->setProperty("vosCard", QString::fromLatin1(variant));
    return f;
}

QFrame* makeHSeparator(QWidget* parent)
{
    auto* f = new QFrame(parent);
    f->setProperty("vosSeparator", true);
    f->setFixedHeight(1);
    return f;
}

// ── SectionHeader ────────────────────────────────────────────────────────────

SectionHeader::SectionHeader(const QString& text, QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 0, 0, 0);      // room for the accent tick
    m_label = new QLabel(text.toUpper(), this);
    m_label->setFont(theme::monoFont(9, QFont::Bold, 112.0));
    m_label->setStyleSheet(QStringLiteral("color:%1; background:transparent;")
                               .arg(theme::kOnSurface));
    lay->addWidget(m_label);
    lay->addStretch(1);
}

void SectionHeader::setText(const QString& text) { m_label->setText(text.toUpper()); }

void SectionHeader::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(QRect(0, 2, 3, height() - 4), theme::palette().accent);
}

// ── LedDot ───────────────────────────────────────────────────────────────────

LedDot::LedDot(const QColor& color, QWidget* parent)
    : QWidget(parent), m_color(color)
{
    setFixedSize(10, 10);
}

void LedDot::setColor(const QColor& c) { m_color = c; update(); }

void LedDot::setPulsing(bool on)
{
    m_pulsing = on;
    if (on && !m_timerId)      m_timerId = startTimer(1000);
    else if (!on && m_timerId) { killTimer(m_timerId); m_timerId = 0; m_dimPhase = false; }
    update();
}

void LedDot::setPulseEnabled(bool on) { g_ledPulse = on; }

void LedDot::timerEvent(QTimerEvent*)
{
    m_dimPhase = !m_dimPhase;
    update();
}

void LedDot::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor c = m_color;
    if (m_pulsing && g_ledPulse && m_dimPhase)
        c = c.darker(220);
    // soft glow halo + core
    QColor halo = c; halo.setAlpha(70);
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(rect());
    p.setBrush(c);
    p.drawEllipse(rect().adjusted(2, 2, -2, -2));
}

// ── Badge ────────────────────────────────────────────────────────────────────

Badge::Badge(const QString& text, Kind kind, QWidget* parent)
    : QLabel(text.toUpper(), parent), m_kind(kind)
{
    setFont(theme::monoFont(8, QFont::Bold, 110.0));
    setAlignment(Qt::AlignCenter);
    restyle();
}

void Badge::setKind(Kind kind) { m_kind = kind; restyle(); }

void Badge::restyle()
{
    QString bg, fg, border = QStringLiteral("transparent");
    switch (m_kind) {
        case Accent:    bg = theme::palette().accent.name(); fg = theme::palette().onAccent.name(); break;
        case Ok:        bg = "#123a24"; fg = theme::kOk;   border = "#1e5c39"; break;
        case Warn:      bg = "#3a3212"; fg = theme::kWarn; border = "#5c511e"; break;
        case Error:     bg = theme::kErrorContainer; fg = "#ffdad6"; break;
        case Planned:   bg = "transparent"; fg = "#8a8a8e"; border = "#3a3a3e"; break;
        case Secondary: bg = "#3d0057"; fg = theme::kSecondaryLight; border = theme::kSecondary; break;
        case Neutral:
        default:        bg = theme::kSurfaceHighest; fg = theme::kOnSurfaceVariant; break;
    }
    setStyleSheet(QStringLiteral(
        "background:%1; color:%2; border:1px solid %3; border-radius:2px; padding:2px 8px;")
        .arg(bg, fg, border));
}

// ── ToggleSwitch ─────────────────────────────────────────────────────────────

ToggleSwitch::ToggleSwitch(QWidget* parent) : QWidget(parent)
{
    setFixedSize(sizeHint());
    setCursor(Qt::PointingHandCursor);
}

void ToggleSwitch::setChecked(bool on)
{
    if (m_checked == on) return;
    m_checked = on;
    update();
}

void ToggleSwitch::mouseReleaseEvent(QMouseEvent* e)
{
    if (!isEnabled()) return;
    if (e->button() == Qt::LeftButton) {
        m_checked = !m_checked;
        update();
        emit toggled(m_checked);
    }
}

void ToggleSwitch::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int h = height(), w = width();
    QColor track = m_checked ? theme::palette().accent : QColor(theme::kSurfaceHighest);
    QColor knob  = m_checked ? QColor("#ffffff")       : QColor(theme::kOutline);
    if (!isEnabled()) { track = QColor("#2c2c2f"); knob = QColor("#5a5a5e"); }
    p.setPen(Qt::NoPen);
    p.setBrush(track);
    p.drawRoundedRect(rect(), h / 2.0, h / 2.0);
    const int d = h - 6;
    const int x = m_checked ? w - d - 3 : 3;
    p.setBrush(knob);
    p.drawEllipse(QRect(x, 3, d, d));
}

// ── StatTile ─────────────────────────────────────────────────────────────────

StatTile::StatTile(const QString& label, const QString& value,
                   const QColor& valueColor, QWidget* parent)
    : QFrame(parent)
{
    setProperty("vosCard", QStringLiteral("sunken"));
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(4);
    // Tighter tracking than the default: two of these share the side panel
    // width and the longest labels (AVG_CONFIDENCE) must fit unclipped.
    lay->addWidget(capsLabel(label, 7, this, 102.0));
    m_value = new QLabel(value, this);
    m_value->setFont(theme::displayFont(20, QFont::Bold));
    m_value->setStyleSheet(QStringLiteral("color:%1; background:transparent; border:none;")
                               .arg(valueColor.name()));
    lay->addWidget(m_value);
}

void StatTile::setValue(const QString& v) { m_value->setText(v); }

void StatTile::setValueColor(const QColor& c)
{
    m_value->setStyleSheet(QStringLiteral("color:%1; background:transparent; border:none;")
                               .arg(c.name()));
}

// ── MiniBars ─────────────────────────────────────────────────────────────────

MiniBars::MiniBars(int capacity, QWidget* parent)
    : QWidget(parent), m_capacity(capacity), m_color(theme::palette().accentDim)
{
    setMinimumHeight(40);
}

void MiniBars::push(double v)
{
    m_values.append(v);
    while (m_values.size() > m_capacity)
        m_values.removeFirst();
    update();
}

void MiniBars::setBarColor(const QColor& c) { m_color = c; update(); }

void MiniBars::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    if (m_values.isEmpty()) {
        p.setPen(QColor("#5a5a5e"));
        p.setFont(theme::monoFont(8));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("NO_DATA"));
        return;
    }
    double maxV = 1e-9;
    for (double v : m_values) maxV = qMax(maxV, v);
    const int n = m_values.size();
    const double bw = double(width()) / m_capacity;
    for (int i = 0; i < n; ++i) {
        const double v = m_values[i];
        const int bh = int((v / maxV) * (height() - 4));
        QRectF r(width() - (n - i) * bw + 1, height() - bh, bw - 2, bh);
        QColor fill = m_color; fill.setAlpha(150);
        p.fillRect(r, fill);
        p.fillRect(QRectF(r.left(), r.top(), r.width(), 1.5), m_color);
    }
}

// ── LineChart ────────────────────────────────────────────────────────────────

LineChart::LineChart(int capacity, QWidget* parent)
    : QWidget(parent), m_capacity(capacity)
{
    setMinimumHeight(120);
}

void LineChart::push(double v)
{
    m_values.append(v);
    while (m_values.size() > m_capacity)
        m_values.removeFirst();
    update();
}

void LineChart::clear() { m_values.clear(); update(); }

void LineChart::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // horizontal grid
    p.setPen(QPen(QColor(theme::kOutlineVariant), 1, Qt::DotLine));
    for (int i = 1; i <= 3; ++i) {
        const int y = height() * i / 4;
        p.drawLine(0, y, width(), y);
    }

    if (m_values.size() < 2) {
        p.setPen(QColor("#5a5a5e"));
        p.setFont(theme::monoFont(9));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("AWAITING_TELEMETRY"));
        return;
    }

    double maxV = 1e-9, minV = 1e18;
    for (double v : m_values) { maxV = qMax(maxV, v); minV = qMin(minV, v); }
    if (maxV - minV < 1e-9) { maxV += 1; minV -= 1; }
    const double span = maxV - minV;
    const double pad  = span * 0.15;
    maxV += pad; minV = qMax(0.0, minV - pad);

    // Build path anchored to the right edge (newest sample rightmost).
    QPainterPath line;
    const int n = m_values.size();
    for (int i = 0; i < n; ++i) {
        const double x = width() - double(n - 1 - i) * (double(width()) / (m_capacity - 1));
        const double t = (m_values[i] - minV) / (maxV - minV);
        const QPointF pt(x, height() - t * (height() - 8) - 4);
        if (i == 0) line.moveTo(pt); else line.lineTo(pt);
    }

    QPainterPath area = line;
    area.lineTo(width(), height());
    area.lineTo(width() - double(n - 1) * (double(width()) / (m_capacity - 1)), height());
    area.closeSubpath();

    QLinearGradient g(0, 0, 0, height());
    QColor top = theme::palette().accent; top.setAlpha(70);
    QColor bot = top; bot.setAlpha(0);
    g.setColorAt(0, top);
    g.setColorAt(1, bot);
    p.fillPath(area, g);
    p.setPen(QPen(theme::palette().accent, 1.6));
    p.drawPath(line);

    // last-value marker + caption
    p.setFont(theme::monoFont(8));
    p.setPen(QColor(theme::kOnSurfaceVariant));
    p.drawText(QRect(6, 4, width() - 12, 14), Qt::AlignLeft,
               QStringLiteral("MAX %1%2").arg(maxV, 0, 'f', 1).arg(m_unit));
}

// ── KeyValueRow ──────────────────────────────────────────────────────────────

KeyValueRow::KeyValueRow(const QString& key, const QString& value,
                         const QColor& valueColor, QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);
    auto* k = capsLabel(key, 8, this);
    k->setAlignment(Qt::AlignRight);
    m_value = new QLabel(value, this);
    m_value->setFont(theme::monoFont(12, QFont::Bold));
    m_value->setAlignment(Qt::AlignRight);
    m_value->setStyleSheet(QStringLiteral("color:%1; background:transparent;")
                               .arg(valueColor.name()));
    lay->addWidget(k);
    lay->addWidget(m_value);
}

void KeyValueRow::setValue(const QString& v) { m_value->setText(v); }

// ── helpers ──────────────────────────────────────────────────────────────────

QLabel* capsLabel(const QString& text, int pt, QWidget* parent, qreal tracking)
{
    auto* l = new QLabel(text.toUpper(), parent);
    l->setFont(theme::monoFont(pt, QFont::Bold, tracking));
    l->setStyleSheet(QStringLiteral("color:%1; background:transparent; border:none;")
                         .arg(theme::kOnSurfaceVariant));
    return l;
}

QLabel* dataLabel(const QString& text, int pt, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setFont(theme::monoFont(pt, QFont::Medium));
    l->setStyleSheet(QStringLiteral("color:%1; background:transparent; border:none;")
                         .arg(theme::kOnSurface));
    return l;
}

void markPlanned(QWidget* w, const QString& note)
{
    w->setEnabled(false);
    w->setToolTip(note.isEmpty()
        ? QStringLiteral("PLANNED — not yet implemented in the MediaFusionGCV backend")
        : note);
}

} // namespace vos
