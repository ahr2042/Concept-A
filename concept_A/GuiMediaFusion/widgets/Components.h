#pragma once

// Small custom-painted widgets shared by all VISION_OS pages. Everything QSS
// cannot express lives here (LEDs, toggle switch, badges, mini charts); each
// widget follows the Stitch design's geometry.

#include <QFrame>
#include <QLabel>
#include <QVector>
#include <QWidget>

class QHBoxLayout;
class QVBoxLayout;

namespace vos {

// Styled QFrame the QSS targets via the vosCard property. variant: "true"
// (default card), "sunken", "raised".
QFrame*  makeCard(const char* variant = "true", QWidget* parent = nullptr);
QFrame*  makeHSeparator(QWidget* parent = nullptr);

// ── SectionHeader ── "▎PIPELINE_CONFIGURATION" mono caps with accent tick ────
class SectionHeader : public QWidget
{
    Q_OBJECT
public:
    explicit SectionHeader(const QString& text, QWidget* parent = nullptr);
    void setText(const QString& text);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QLabel* m_label;
};

// ── LedDot ── status LED, optionally pulsing (design: LED Status Pulse) ──────
class LedDot : public QWidget
{
    Q_OBJECT
public:
    explicit LedDot(const QColor& color, QWidget* parent = nullptr);
    void setColor(const QColor& c);
    void setPulsing(bool on);
    static void setPulseEnabled(bool on);      // global settings switch
protected:
    void paintEvent(QPaintEvent*) override;
    void timerEvent(QTimerEvent*) override;
private:
    QColor m_color;
    bool   m_pulsing  = false;
    bool   m_dimPhase = false;
    int    m_timerId  = 0;
};

// ── Badge ── mono caps chip: READY / TIMEOUT / PENDING / PLANNED … ───────────
class Badge : public QLabel
{
    Q_OBJECT
public:
    enum Kind { Neutral, Accent, Ok, Warn, Error, Planned, Secondary };
    explicit Badge(const QString& text, Kind kind = Neutral, QWidget* parent = nullptr);
    void setKind(Kind kind);
private:
    void restyle();
    Kind m_kind;
};

// ── ToggleSwitch ── the design's pill switch ─────────────────────────────────
class ToggleSwitch : public QWidget
{
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget* parent = nullptr);
    bool isChecked() const { return m_checked; }
    void setChecked(bool on);
signals:
    void toggled(bool on);
protected:
    void paintEvent(QPaintEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    QSize sizeHint() const override { return {40, 22}; }
private:
    bool m_checked = false;
};

// ── StatTile ── "TOTAL_OBJECTS / 24" numeric tile ────────────────────────────
class StatTile : public QFrame
{
    Q_OBJECT
public:
    StatTile(const QString& label, const QString& value,
             const QColor& valueColor, QWidget* parent = nullptr);
    void setValue(const QString& v);
    void setValueColor(const QColor& c);
private:
    QLabel* m_value;
};

// ── MiniBars ── the telemetry strip's small bar chart ────────────────────────
class MiniBars : public QWidget
{
    Q_OBJECT
public:
    explicit MiniBars(int capacity = 24, QWidget* parent = nullptr);
    void push(double v);                       // autoscales to max seen
    void setBarColor(const QColor& c);
protected:
    void paintEvent(QPaintEvent*) override;
    QSize sizeHint() const override { return {160, 56}; }
private:
    QVector<double> m_values;
    int    m_capacity;
    QColor m_color;
};

// ── LineChart ── analytics latency/fps chart with area fill ─────────────────
class LineChart : public QWidget
{
    Q_OBJECT
public:
    explicit LineChart(int capacity = 120, QWidget* parent = nullptr);
    void push(double v);
    void clear();
    void setUnit(const QString& u) { m_unit = u; }
protected:
    void paintEvent(QPaintEvent*) override;
    QSize sizeHint() const override { return {400, 180}; }
private:
    QVector<double> m_values;
    int     m_capacity;
    QString m_unit;
};

// ── KeyValueRow ── "THROUGHPUT  842.1 MB/s" pair used in headers ─────────────
class KeyValueRow : public QWidget
{
    Q_OBJECT
public:
    KeyValueRow(const QString& key, const QString& value,
                const QColor& valueColor, QWidget* parent = nullptr);
    void setValue(const QString& v);
private:
    QLabel* m_value;
};

// Mono ALL_CAPS label helpers (the design's label-caps / data-mono styles).
QLabel* capsLabel(const QString& text, int pt = 8, QWidget* parent = nullptr,
                  qreal tracking = 114.0);
QLabel* dataLabel(const QString& text, int pt = 10, QWidget* parent = nullptr);

// Standard tooltip + disabled state for not-yet-implemented controls.
void markPlanned(QWidget* w, const QString& note = QString());

} // namespace vos
