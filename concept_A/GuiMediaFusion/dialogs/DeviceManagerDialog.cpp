#include "DeviceManagerDialog.h"

#include "../core/BackendService.h"
#include "../theme/Theme.h"
#include "../widgets/Components.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

DeviceManagerDialog::DeviceManagerDialog(BackendService* service, QWidget* parent)
    : QDialog(parent), m_service(service)
{
    setWindowTitle(QStringLiteral("DEVICE MANAGEMENT"));
    setModal(true);
    resize(880, 560);
    setStyleSheet(theme::buildStyleSheet() +
                  QStringLiteral("QDialog { background: %1; border: 1px solid %2; }")
                      .arg(theme::kSurfaceLow, theme::kOutlineVariant));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── header ──
    auto* head = new QFrame(this);
    head->setStyleSheet(QStringLiteral("QFrame { background: %1; border-bottom: 1px solid %2; }")
                            .arg(theme::kSurfaceLowest, theme::kOutlineVariant));
    auto* headLay = new QHBoxLayout(head);
    headLay->setContentsMargins(18, 14, 14, 14);
    auto* titleBox = new QVBoxLayout;
    titleBox->setSpacing(2);
    auto* title = new QLabel(QStringLiteral("DEVICE MANAGEMENT"), head);
    title->setFont(theme::displayFont(14, QFont::Bold));
    auto* sub = new QLabel(QStringLiteral("Scanning for V4L2 sensor nodes (GStreamer device monitor)…"), head);
    sub->setFont(theme::monoFont(9));
    sub->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kOnSurfaceVariant));
    titleBox->addWidget(title);
    titleBox->addWidget(sub);
    auto* headTick = new vos::SectionHeader(QString(), head);
    headTick->setFixedWidth(8);
    headLay->addWidget(headTick);
    headLay->addLayout(titleBox);
    headLay->addStretch(1);
    auto* close = new QPushButton(QStringLiteral("✕"), head);
    close->setProperty("vosRole", QStringLiteral("ghost"));
    close->setFixedSize(30, 30);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    headLay->addWidget(close);
    outer->addWidget(head);

    // ── body ──
    auto* body = new QWidget(this);
    auto* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(18, 16, 18, 16);
    bodyLay->setSpacing(16);

    // left: scan card + protocol filter
    auto* left = new QWidget(body);
    left->setFixedWidth(240);
    auto* leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(10);

    auto* scanCard = vos::makeCard("sunken", left);
    auto* scanLay = new QVBoxLayout(scanCard);
    scanLay->setContentsMargins(14, 18, 14, 18);
    scanLay->setSpacing(8);
    auto* glyph = new QLabel(QStringLiteral("◎"), scanCard);
    glyph->setFont(theme::displayFont(28, QFont::Bold));
    glyph->setAlignment(Qt::AlignCenter);
    glyph->setStyleSheet(QStringLiteral(
        "color:%1; border:1px solid %1; border-radius:8px; padding:10px;")
        .arg(theme::palette().accent.name()));
    scanLay->addWidget(glyph, 0, Qt::AlignHCenter);
    auto* scanTitle = vos::dataLabel(QStringLiteral("ACTIVE SCAN"), 10, scanCard);
    scanTitle->setAlignment(Qt::AlignCenter);
    scanLay->addWidget(scanTitle);
    auto* scanSub = vos::capsLabel(QStringLiteral("BUS: V4L2 // LOCAL"), 8, scanCard);
    scanSub->setAlignment(Qt::AlignCenter);
    scanLay->addWidget(scanSub);
    auto* refresh = new QPushButton(QStringLiteral("REFRESH LIST"), scanCard);
    connect(refresh, &QPushButton::clicked, m_service, &BackendService::refreshDevices);
    scanLay->addWidget(refresh);
    leftLay->addWidget(scanCard);

    leftLay->addWidget(vos::capsLabel(QStringLiteral("PROTOCOL FILTER"), 8, left));
    auto* grid = new QGridLayout;
    grid->setSpacing(6);
    auto addChip = [left](const QString& text, bool enabled) {
        auto* chip = new QPushButton(text, left);
        chip->setProperty("vosRole", QStringLiteral("chip"));
        chip->setCheckable(true);
        if (!enabled)
            vos::markPlanned(chip, QStringLiteral("PLANNED — protocol not supported by backend yet"));
        return chip;
    };
    auto* usb = addChip(QStringLiteral("USB 3.0"), true);
    usb->setChecked(true);
    grid->addWidget(usb, 0, 0);
    grid->addWidget(addChip(QStringLiteral("GIGE"), false), 0, 1);
    grid->addWidget(addChip(QStringLiteral("RTSP"), false), 1, 0);
    grid->addWidget(addChip(QStringLiteral("COAXPRESS"), false), 1, 1);
    leftLay->addLayout(grid);
    leftLay->addStretch(1);
    bodyLay->addWidget(left);

    // right: discovered devices
    auto* right = new QWidget(body);
    auto* rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(10);

    auto* countRow = new QHBoxLayout;
    m_countLabel = vos::dataLabel(QStringLiteral("0 DEVICES DISCOVERED"), 10, right);
    countRow->addWidget(m_countLabel);
    countRow->addStretch(1);
    auto* led = new vos::LedDot(QColor(theme::palette().accent), right);
    led->setPulsing(true);
    countRow->addWidget(led);
    countRow->addWidget(vos::capsLabel(QStringLiteral("SIGNAL DETECTED"), 8, right));
    rightLay->addLayout(countRow);

    auto* scroll = new QScrollArea(right);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    auto* cardsHost = new QWidget(scroll);
    m_cardsLay = new QVBoxLayout(cardsHost);
    m_cardsLay->setContentsMargins(0, 0, 0, 0);
    m_cardsLay->setSpacing(10);
    m_cardsLay->addStretch(1);
    scroll->setWidget(cardsHost);
    rightLay->addWidget(scroll, 1);
    bodyLay->addWidget(right, 1);

    outer->addWidget(body, 1);

    // ── footer ──
    auto* foot = new QFrame(this);
    foot->setStyleSheet(QStringLiteral("QFrame { background: %1; border-top: 1px solid %2; }")
                            .arg(theme::kSurfaceLowest, theme::kOutlineVariant));
    auto* footLay = new QHBoxLayout(foot);
    footLay->setContentsMargins(18, 10, 18, 10);
    footLay->addWidget(vos::capsLabel(QStringLiteral("ASSIGNED WORKSPACE:"), 8, foot));
    auto* ws = new QComboBox(foot);
    ws->addItem(QStringLiteral("MAIN_VIEWPORT"));
    vos::markPlanned(ws, QStringLiteral("PLANNED — multiple workspaces"));
    footLay->addWidget(ws);
    footLay->addStretch(1);
    auto* cancel = new QPushButton(QStringLiteral("CANCEL"), foot);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto* proceed = new QPushButton(QStringLiteral("PROCEED"), foot);
    proceed->setProperty("vosRole", QStringLiteral("primary"));
    connect(proceed, &QPushButton::clicked, this, [this] {
        if (m_chosenDevice >= 0)
            emit deviceChosen(m_chosenDevice, m_chosenCap);
        accept();
    });
    footLay->addWidget(cancel);
    footLay->addWidget(proceed);
    outer->addWidget(foot);

    connect(m_service, &BackendService::devicesChanged, this, &DeviceManagerDialog::onDevices);
    onDevices(m_service->devices());
    m_service->refreshDevices();
}

void DeviceManagerDialog::onDevices(const QVector<DeviceInfo>& devices)
{
    m_devices = devices;
    m_countLabel->setText(QStringLiteral("%1 DEVICE%2 DISCOVERED")
                              .arg(devices.size())
                              .arg(devices.size() == 1 ? QString() : QStringLiteral("S")));
    rebuildCards();
}

void DeviceManagerDialog::rebuildCards()
{
    // wipe everything above the trailing stretch
    while (m_cardsLay->count() > 1) {
        QLayoutItem* it = m_cardsLay->takeAt(0);
        delete it->widget();
        delete it;
    }

    for (const DeviceInfo& dev : m_devices) {
        auto* card = vos::makeCard("raised", nullptr);
        auto* lay = new QVBoxLayout(card);
        lay->setContentsMargins(14, 12, 14, 12);
        lay->setSpacing(10);

        auto* row = new QHBoxLayout;
        auto* icon = new QLabel(QStringLiteral("⎆"), card);
        icon->setFont(theme::displayFont(16, QFont::Bold));
        icon->setFixedSize(38, 38);
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet(QStringLiteral("background:%1; border:1px solid %2; border-radius:4px;")
                                .arg(theme::kSurfaceHighest, theme::kOutlineVariant));
        row->addWidget(icon);
        auto* text = new QVBoxLayout;
        text->setSpacing(2);
        auto* name = new QLabel(dev.name, card);
        name->setFont(theme::bodyFont(12, QFont::DemiBold));
        auto* meta = vos::capsLabel(QStringLiteral("V4L2 SOURCE | DEV_%1 | %2 MODES")
                                        .arg(dev.index).arg(dev.caps.size()), 8, card);
        text->addWidget(name);
        text->addWidget(meta);
        row->addLayout(text);
        row->addStretch(1);
        row->addWidget(new vos::Badge(QStringLiteral("READY"), vos::Badge::Secondary, card));
        lay->addLayout(row);

        auto* actRow = new QHBoxLayout;
        actRow->setSpacing(8);
        auto* capsBox = new QComboBox(card);
        for (const CapInfo& c : dev.caps)
            capsBox->addItem(c.label, c.index);
        actRow->addWidget(capsBox, 1);
        auto* connectBtn = new QPushButton(QStringLiteral("CONNECT"), card);
        connectBtn->setProperty("vosRole", QStringLiteral("primary"));
        const int devIndex = dev.index;
        connect(connectBtn, &QPushButton::clicked, this, [this, devIndex, capsBox] {
            m_chosenDevice = devIndex;
            m_chosenCap    = capsBox->currentData().isValid() ? capsBox->currentData().toInt() : 0;
            emit deviceChosen(m_chosenDevice, m_chosenCap);
            accept();
        });
        actRow->addWidget(connectBtn);
        lay->addLayout(actRow);

        m_cardsLay->insertWidget(m_cardsLay->count() - 1, card);
    }

    if (m_devices.isEmpty()) {
        auto* empty = vos::capsLabel(QStringLiteral("NO DEVICES — CHECK CAMERA / CONTROL LINK"), 9, nullptr);
        empty->setAlignment(Qt::AlignCenter);
        m_cardsLay->insertWidget(0, empty);
    }
}
