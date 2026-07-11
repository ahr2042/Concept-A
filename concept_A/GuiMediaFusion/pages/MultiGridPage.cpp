#include "MultiGridPage.h"

#include "../theme/Theme.h"
#include "../widgets/Components.h"
#include "../widgets/VideoTile.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTime>
#include <QVBoxLayout>

MultiGridPage::MultiGridPage(BackendService* service, QWidget* parent)
    : QWidget(parent), m_service(service)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(14);

    // ── global bar ──
    auto* bar = vos::makeCard("true", this);
    auto* barLay = new QHBoxLayout(bar);
    barLay->setContentsMargins(14, 8, 14, 8);
    barLay->setSpacing(16);

    auto* sync = vos::dataLabel(QStringLiteral("GLOBAL SYNC: OFF"), 10, bar);
    sync->setToolTip(QStringLiteral("PLANNED — cross-stream clock sync"));
    barLay->addWidget(sync);
    barLay->addWidget(vos::capsLabel(QStringLiteral("MASTER CLOCK:"), 8, bar));
    m_clock = vos::dataLabel(QStringLiteral("--:--:--"), 11, bar);
    m_clock->setStyleSheet(QStringLiteral("color:%1;").arg(theme::palette().accent.name()));
    barLay->addWidget(m_clock);
    barLay->addStretch(1);

    auto* recAll = new QPushButton(QStringLiteral("● REC ALL"), bar);
    recAll->setProperty("vosRole", QStringLiteral("danger"));
    vos::markPlanned(recAll, QStringLiteral("PLANNED — file sink recording"));
    auto* freeze = new QPushButton(QStringLiteral("FREEZE"), bar);
    vos::markPlanned(freeze, QStringLiteral("PLANNED — synchronized frame freeze"));
    auto* stopAll = new QPushButton(QStringLiteral("STOP ALL"), bar);
    connect(stopAll, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_slots.size(); ++i)
            if (m_slots[i].sessionId >= 0)
                m_service->stop(m_slots[i].sessionId);
    });
    barLay->addWidget(recAll);
    barLay->addWidget(freeze);
    barLay->addWidget(stopAll);
    lay->addWidget(bar);

    // ── 2×2 grid ──
    auto* grid = new QGridLayout;
    grid->setSpacing(14);
    m_slots.resize(4);
    for (int i = 0; i < 4; ++i)
        grid->addWidget(buildSlot(i), i / 2, i % 2);
    auto* gridHost = new QWidget(this);
    gridHost->setLayout(grid);
    lay->addWidget(gridHost, 1);

    connect(m_service, &BackendService::devicesChanged,  this, &MultiGridPage::onDevices);
    connect(m_service, &BackendService::sessionStarted,  this, &MultiGridPage::onSessionStarted);
    connect(m_service, &BackendService::sessionStopped,  this, &MultiGridPage::onSessionStopped);
    connect(m_service, &BackendService::sessionFailed,   this, &MultiGridPage::onSessionFailed);

    startTimer(500);
}

void MultiGridPage::timerEvent(QTimerEvent*)
{
    m_clock->setText(QTime::currentTime().toString(QStringLiteral("HH:mm:ss:zzz")).left(11));
}

QWidget* MultiGridPage::buildSlot(int index)
{
    auto* host = new QWidget(this);
    auto* lay = new QVBoxLayout(host);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    Slot& s = m_slots[index];
    s.tile = new VideoTile(QStringLiteral("CAM_%1").arg(index + 1, 2, 10, QLatin1Char('0')), host);
    lay->addWidget(s.tile, 1);

    auto* foot = new QHBoxLayout;
    foot->setSpacing(6);
    s.deviceBox = new QComboBox(host);
    s.deviceBox->setToolTip(QStringLiteral("Video source for this slot"));
    s.algoBox = new QComboBox(host);
    s.algoBox->addItem(QStringLiteral("MODE: RAW"), QString());
    s.algoBox->addItem(QStringLiteral("MODE: GRAYSCALE"), QStringLiteral("grayscale"));
    s.algoBox->addItem(QStringLiteral("MODE: EDGE MAP"), QStringLiteral("canny"));
    s.algoBox->addItem(QStringLiteral("MODE: GRAY+EDGE"), QStringLiteral("grayscale,canny"));
    s.button = new QPushButton(QStringLiteral("START"), host);
    s.button->setProperty("vosRole", QStringLiteral("primary"));
    s.button->setFixedWidth(90);
    connect(s.button, &QPushButton::clicked, this, [this, index] { toggleSlot(index); });
    foot->addWidget(s.deviceBox, 1);
    foot->addWidget(s.algoBox, 1);
    foot->addWidget(s.button);
    lay->addLayout(foot);
    return host;
}

void MultiGridPage::toggleSlot(int index)
{
    Slot& s = m_slots[index];
    if (s.sessionId >= 0) {                       // running → stop
        m_service->stop(s.sessionId);
        s.button->setEnabled(false);
        return;
    }
    if (!s.deviceBox->currentData().isValid())
        return;
    BackendService::DeploySpec spec;
    spec.deviceIndex = s.deviceBox->currentData().toInt();
    spec.capIndex    = 0;                          // default mode; fine-grain via Dashboard
    spec.algosCsv    = s.algoBox->currentData().toString();
    spec.name        = QStringLiteral("grid%1").arg(index);
    s.sessionId = m_service->deploy(spec);
    s.button->setEnabled(false);
}

void MultiGridPage::onDevices(const QVector<DeviceInfo>& devices)
{
    m_devices = devices;
    for (Slot& s : m_slots) {
        s.deviceBox->clear();
        for (const DeviceInfo& d : devices)
            s.deviceBox->addItem(d.name.toUpper(), d.index);
        s.button->setEnabled(!devices.isEmpty());
    }
}

void MultiGridPage::onSessionStarted(int sessionId, const QString& socket, const QString& desc)
{
    Q_UNUSED(desc);
    for (Slot& s : m_slots) {
        if (s.sessionId != sessionId)
            continue;
        if (socket.isEmpty() || !s.tile->bind(socket.toStdString(), s.deviceBox->currentText())) {
            m_service->stop(sessionId);
            s.sessionId = -1;
            s.button->setEnabled(true);
            return;
        }
        s.button->setText(QStringLiteral("STOP"));
        s.button->setProperty("vosRole", QStringLiteral("danger"));
        s.button->style()->unpolish(s.button);
        s.button->style()->polish(s.button);
        s.button->setEnabled(true);
        return;
    }
}

void MultiGridPage::onSessionStopped(int sessionId)
{
    for (Slot& s : m_slots) {
        if (s.sessionId != sessionId)
            continue;
        s.sessionId = -1;
        s.tile->unbind();
        s.button->setText(QStringLiteral("START"));
        s.button->setProperty("vosRole", QStringLiteral("primary"));
        s.button->style()->unpolish(s.button);
        s.button->style()->polish(s.button);
        s.button->setEnabled(!m_devices.isEmpty());
        return;
    }
}

void MultiGridPage::onSessionFailed(int sessionId, const QString& error)
{
    Q_UNUSED(error);
    for (Slot& s : m_slots) {
        if (s.sessionId != sessionId)
            continue;
        s.sessionId = -1;
        s.button->setEnabled(true);
        return;
    }
}
