#include "VideoTile.h"

#include "../StreamReceiver.h"
#include "../theme/Theme.h"
#include "Components.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QStackedLayout>
#include <QTime>
#include <QVBoxLayout>

// Painted offline state: dark scanline field + NO_SIGNAL caption.
class NoSignalWidget : public QWidget
{
public:
    explicit NoSignalWidget(const QString& slotName, QWidget* parent = nullptr)
        : QWidget(parent), m_slot(slotName) {}
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(theme::kSurfaceLowest));
        p.setPen(QColor(24, 24, 27));
        for (int y = 0; y < height(); y += 4)
            p.drawLine(0, y, width(), y);
        // corner brackets, like the design's viewport frame
        p.setPen(QPen(QColor(theme::kOutlineVariant), 1));
        const int m = 14, s = 26;
        p.drawPolyline(QPolygon({ {m, m + s}, {m, m}, {m + s, m} }));
        p.drawPolyline(QPolygon({ {width() - m - s, m}, {width() - m, m}, {width() - m, m + s} }));
        p.drawPolyline(QPolygon({ {m, height() - m - s}, {m, height() - m}, {m + s, height() - m} }));
        p.drawPolyline(QPolygon({ {width() - m - s, height() - m}, {width() - m, height() - m},
                                  {width() - m, height() - m - s} }));
        p.setPen(QColor("#6a6a6e"));
        p.setFont(theme::monoFont(11, QFont::Bold, 118.0));
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("NO_SIGNAL // %1_OFFLINE").arg(m_slot));
    }
private:
    QString m_slot;
};

VideoTile::VideoTile(const QString& slotName, QWidget* parent)
    : QFrame(parent), m_slotName(slotName)
{
    setProperty("vosCard", QStringLiteral("sunken"));
    setMinimumSize(360, 240);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(1, 1, 1, 1);
    outer->setSpacing(0);

    m_chromeTop = buildChrome();
    outer->addWidget(m_chromeTop);

    auto* stackHost = new QWidget(this);
    auto* stack = new QStackedLayout(stackHost);
    stack->setStackingMode(QStackedLayout::StackOne);
    m_placeholder = new NoSignalWidget(m_slotName, stackHost);
    m_receiver    = new StreamReceiver(stackHost);
    stack->addWidget(m_placeholder);
    stack->addWidget(m_receiver);
    stack->setCurrentWidget(m_placeholder);
    outer->addWidget(stackHost, 1);

    connect(m_receiver, &StreamReceiver::statsTick, this,
            [this](double fps, double mbps, QSize res, quint64 total) {
        m_fpsLabel->setText(QStringLiteral("%1 FPS").arg(fps, 0, 'f', 1));
        if (res.isValid() && res.width() > 0)
            m_resLabel->setText(QStringLiteral("%1x%2").arg(res.width()).arg(res.height()));
        emit statsTick(fps, mbps, res, total);
    });
    connect(m_receiver, &StreamReceiver::streamError, this, [this](const QString& e) {
        unbind();
        emit streamError(e);
    });
    connect(m_receiver, &StreamReceiver::streamEnded, this, [this] { unbind(); });
}

QWidget* VideoTile::buildChrome()
{
    auto* bar = new QFrame(this);
    bar->setFixedHeight(34);
    bar->setStyleSheet(QStringLiteral(
        "QFrame { background: %1; border: none; border-bottom: 1px solid %2; }")
        .arg(theme::kSurfaceLow, theme::kOutlineVariant));
    auto* lay = new QHBoxLayout(bar);
    lay->setContentsMargins(10, 0, 10, 0);
    lay->setSpacing(10);

    m_recDot = new vos::LedDot(QColor(theme::kError), bar);
    m_recDot->setVisible(false);
    m_recLabel = new QLabel(QStringLiteral("STANDBY"), bar);
    m_recLabel->setFont(theme::monoFont(9, QFont::Bold, 108.0));
    m_recLabel->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kOnSurfaceVariant));

    m_slotLabel = new QLabel(m_slotName, bar);
    m_slotLabel->setFont(theme::monoFont(9, QFont::Bold, 112.0));
    m_slotLabel->setStyleSheet(QStringLiteral(
        "color:%1; border: 1px solid %2; padding: 2px 8px; border-radius: 2px;")
        .arg(theme::palette().accent.name(), theme::palette().accent.name()));

    m_srcLabel = new QLabel(QStringLiteral("SOURCE: —"), bar);
    m_srcLabel->setFont(theme::monoFont(9, QFont::Medium, 106.0));
    m_srcLabel->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kOnSurfaceVariant));

    m_resLabel = new QLabel(QString(), bar);
    m_resLabel->setFont(theme::monoFont(9, QFont::Medium, 106.0));
    m_resLabel->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kOnSurfaceVariant));

    m_fpsLabel = new QLabel(QString(), bar);
    m_fpsLabel->setFont(theme::monoFont(10, QFont::Bold, 106.0));
    m_fpsLabel->setStyleSheet(QStringLiteral("color:%1;").arg(theme::palette().accent.name()));

    lay->addWidget(m_recDot);
    lay->addWidget(m_recLabel);
    lay->addSpacing(4);
    lay->addWidget(m_slotLabel);
    lay->addWidget(m_srcLabel);
    lay->addStretch(1);
    lay->addWidget(m_resLabel);
    lay->addWidget(m_fpsLabel);
    return bar;
}

bool VideoTile::bind(const std::string& videoSocket, const QString& sourceLabel)
{
    m_sourceLabel = sourceLabel;
    if (!m_receiver->start(videoSocket)) {
        m_sourceLabel.clear();
        return false;
    }
    auto* stack = static_cast<QStackedLayout*>(m_receiver->parentWidget()->layout());
    stack->setCurrentWidget(m_receiver);
    m_srcLabel->setText(QStringLiteral("SOURCE: %1").arg(sourceLabel.toUpper()));
    m_recDot->setVisible(true);
    m_recDot->setPulsing(true);
    m_elapsed.start();
    if (!m_clockTimer)
        m_clockTimer = startTimer(500);
    return true;
}

void VideoTile::unbind()
{
    if (m_clockTimer) { killTimer(m_clockTimer); m_clockTimer = 0; }
    m_receiver->stop();
    auto* stack = static_cast<QStackedLayout*>(m_receiver->parentWidget()->layout());
    stack->setCurrentWidget(m_placeholder);
    m_recDot->setVisible(false);
    m_recDot->setPulsing(false);
    m_recLabel->setText(QStringLiteral("STANDBY"));
    m_recLabel->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kOnSurfaceVariant));
    m_srcLabel->setText(QStringLiteral("SOURCE: —"));
    m_fpsLabel->clear();
    m_resLabel->clear();
    m_sourceLabel.clear();
}

bool VideoTile::active() const { return m_receiver->running(); }

void VideoTile::timerEvent(QTimerEvent*)
{
    const qint64 ms = m_elapsed.elapsed();
    const QTime t = QTime(0, 0).addMSecs(int(ms % 86400000));
    m_recLabel->setText(QStringLiteral("REC_%1").arg(t.toString(QStringLiteral("HH:mm:ss"))));
    m_recLabel->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kError));
}
