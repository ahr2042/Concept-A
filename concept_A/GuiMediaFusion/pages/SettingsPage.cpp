#include "SettingsPage.h"

#include "../core/AppLog.h"
#include "../core/BackendService.h"
#include "../theme/Theme.h"
#include "../widgets/Components.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSlider>
#include <QStackedWidget>
#include <QVBoxLayout>

int SettingsPage::savedVerbosity()
{
    return QSettings().value(QStringLiteral("ui/verbosity"), int(AppLog::Info)).toInt();
}

SettingsPage::SettingsPage(BackendService* service, QWidget* parent)
    : QWidget(parent), m_service(service)
{
    // Apply the persisted backend selection to the service before any deploy.
    m_service->setAccelSelection(
        QSettings().value(QStringLiteral("runtime/accel"), QStringLiteral("auto")).toString());

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(14);

    // header
    auto* head = new QHBoxLayout;
    auto* titleBox = new QVBoxLayout;
    titleBox->setSpacing(4);
    auto* title = new QLabel(QStringLiteral("SYSTEM SETTINGS"), this);
    title->setFont(theme::displayFont(26, QFont::Bold));
    auto* sub = vos::capsLabel(QStringLiteral("GLOBAL CONFIGURATION & CORE RUNTIME OPTIMIZATION"), 9, this);
    titleBox->addWidget(title);
    titleBox->addWidget(sub);
    head->addLayout(titleBox, 1);
    auto* exportBtn = new QPushButton(QStringLiteral("EXPORT CONFIG"), this);
    connect(exportBtn, &QPushButton::clicked, this, &SettingsPage::exportJson);
    auto* saveBtn = new QPushButton(QStringLiteral("SAVE CHANGES"), this);
    saveBtn->setProperty("vosRole", QStringLiteral("primary"));
    connect(saveBtn, &QPushButton::clicked, this, &SettingsPage::save);
    head->addWidget(exportBtn);
    head->addWidget(saveBtn);
    lay->addLayout(head);

    // tabs
    auto* tabs = new QHBoxLayout;
    tabs->setSpacing(6);
    auto* stack = new QStackedWidget(this);
    stack->addWidget(buildConnectionTab());
    stack->addWidget(buildRuntimeTab());
    stack->addWidget(buildUiTab());
    stack->addWidget(buildPlannedTab(QStringLiteral("NETWORK")));
    stack->addWidget(buildPlannedTab(QStringLiteral("STORAGE")));
    stack->addWidget(buildPlannedTab(QStringLiteral("API ACCESS")));

    const QStringList names = { QStringLiteral("CONNECTION"), QStringLiteral("CORE RUNTIME"),
                                QStringLiteral("UI CUSTOMIZATION"), QStringLiteral("NETWORK"),
                                QStringLiteral("STORAGE"), QStringLiteral("API ACCESS") };
    auto* tabGroup = new QButtonGroup(this);
    for (int i = 0; i < names.size(); ++i) {
        auto* chip = new QPushButton(names[i], this);
        chip->setProperty("vosRole", QStringLiteral("chip"));
        chip->setCheckable(true);
        chip->setChecked(i == 0);
        tabGroup->addButton(chip, i);
        tabs->addWidget(chip);
    }
    tabs->addStretch(1);
    connect(tabGroup, &QButtonGroup::idClicked, stack, &QStackedWidget::setCurrentIndex);
    lay->addLayout(tabs);
    lay->addWidget(stack, 1);
}

QWidget* SettingsPage::buildConnectionTab()
{
    auto* host = new QWidget(this);
    auto* lay = new QVBoxLayout(host);
    lay->setContentsMargins(0, 8, 0, 0);
    lay->setSpacing(12);

    auto* card = vos::makeCard("true", host);
    auto* cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(16, 14, 16, 14);
    cardLay->setSpacing(10);
    cardLay->addWidget(new vos::SectionHeader(QStringLiteral("CONTROL LINK"), card));

    QSettings s;
    cardLay->addWidget(vos::capsLabel(QStringLiteral("CONTROL SOCKET PATH"), 8, card));
    m_socketEdit = new QLineEdit(m_service->controlSocketPath(), card);
    cardLay->addWidget(m_socketEdit);

    cardLay->addWidget(vos::capsLabel(QStringLiteral("BACKEND BINARY (MediaFusionGCV)"), 8, card));
    m_binaryEdit = new QLineEdit(m_service->backendBinary(), card);
    cardLay->addWidget(m_binaryEdit);

    m_autoStart = new QCheckBox(QStringLiteral("AUTOSTART DAEMON WHEN UNREACHABLE"), card);
    m_autoStart->setChecked(m_service->autostart());
    cardLay->addWidget(m_autoStart);

    auto* btnRow = new QHBoxLayout;
    auto* reconnect = new QPushButton(QStringLiteral("APPLY + RECONNECT"), card);
    reconnect->setProperty("vosRole", QStringLiteral("primary"));
    connect(reconnect, &QPushButton::clicked, this, [this] {
        save();
        m_service->ensureOnline();
    });
    btnRow->addWidget(reconnect);
    btnRow->addStretch(1);
    cardLay->addLayout(btnRow);
    lay->addWidget(card);

    auto* hint = new QLabel(QStringLiteral(
        "The GUI drives the backend daemon over this Unix socket (text protocol; one LF-"
        "terminated command per request, NUL-terminated reply). Video travels separately "
        "over per-stream unixfd sockets returned by `start` — zero-copy memfd handover."), host);
    hint->setProperty("vosHint", true);
    hint->setWordWrap(true);
    lay->addWidget(hint);
    lay->addStretch(1);
    return host;
}

QWidget* SettingsPage::buildRuntimeTab()
{
    auto* host = new QWidget(this);
    auto* lay = new QHBoxLayout(host);
    lay->setContentsMargins(0, 8, 0, 0);
    lay->setSpacing(14);

    // left: inference acceleration (adapted to the real AI plan: AMD → ONNX/ncnn)
    auto* accel = vos::makeCard("true", host);
    auto* accelLay = new QVBoxLayout(accel);
    accelLay->setContentsMargins(16, 14, 16, 14);
    accelLay->setSpacing(12);
    auto* accelHead = new QHBoxLayout;
    accelHead->addWidget(new vos::SectionHeader(QStringLiteral("INFERENCE ACCELERATION"), accel));
    accelHead->addStretch(1);
    accelHead->addWidget(new vos::Badge(QStringLiteral("AUTO-DETECTED"),
                                        vos::Badge::Accent, accel));
    accelLay->addLayout(accelHead);

    // Backend selector, built from what the daemon actually detected
    // (`accelerators`). Only available engines are selectable, plus an
    // always-present AUTO; with no GPU it collapses to CPU only. Applied on the
    // next Deploy (the choice shapes the pipeline topology, fixed once PLAYING).
    m_accelStatus = new QLabel(QStringLiteral("Detecting acceleration…"), accel);
    m_accelStatus->setProperty("vosHint", true);
    m_accelStatus->setWordWrap(true);
    accelLay->addWidget(m_accelStatus);

    m_accelGroup = new QButtonGroup(accel);
    m_accelGroup->setExclusive(true);
    auto* accelListHost = new QWidget(accel);
    m_accelList = new QVBoxLayout(accelListHost);
    m_accelList->setContentsMargins(0, 0, 0, 0);
    m_accelList->setSpacing(8);
    accelLay->addWidget(accelListHost);

    rebuildAccelerators(m_service->accelerators());
    connect(m_service, &BackendService::acceleratorsChanged,
            this, &SettingsPage::rebuildAccelerators);
    // Queued: this rebuild deletes and recreates the radios, and it can be
    // triggered by a radio's own toggled() handler (radio -> setAccelSelection ->
    // accelSelectionChanged). Deleting the button mid-signal would be a
    // use-after-free, so defer the rebuild to the next event-loop turn.
    connect(m_service, &BackendService::accelSelectionChanged, this,
            [this](const QString&) { rebuildAccelerators(m_service->accelerators()); },
            Qt::QueuedConnection);  // sync with Dashboard toggle
    m_service->refreshAccelerators();

    auto* batchCard = vos::makeCard("raised", accel);
    auto* batchLay = new QVBoxLayout(batchCard);
    batchLay->setContentsMargins(12, 10, 12, 10);
    batchLay->setSpacing(8);
    auto* batchHead = new QHBoxLayout;
    batchHead->addWidget(vos::dataLabel(QStringLiteral("Inference Batch Size"), 11, batchCard));
    batchHead->addStretch(1);
    batchHead->addWidget(vos::capsLabel(QStringLiteral("08 UNITS"), 9, batchCard));
    batchLay->addLayout(batchHead);
    auto* slider = new QSlider(Qt::Horizontal, batchCard);
    slider->setRange(1, 64);
    slider->setValue(8);
    vos::markPlanned(slider);
    batchLay->addWidget(slider);
    accelLay->addWidget(batchCard);
    accelLay->addStretch(1);
    lay->addWidget(accel, 3);

    // right: logging verbosity (real)
    auto* logCard = vos::makeCard("true", host);
    auto* logLay = new QVBoxLayout(logCard);
    logLay->setContentsMargins(16, 14, 16, 14);
    logLay->setSpacing(8);
    logLay->addWidget(new vos::SectionHeader(QStringLiteral("LOGGING VERBOSITY"), logCard));
    m_verbosity = new QButtonGroup(logCard);
    const QStringList levels = { QStringLiteral("TELEMETRY TRACE (DEBUG)"), QStringLiteral("DEVELOPER (INFO)"),
                                 QStringLiteral("WARNINGS ONLY"), QStringLiteral("ERRORS ONLY") };
    const int current = savedVerbosity();
    for (int i = 0; i < levels.size(); ++i) {
        auto* rb = new QRadioButton(levels[i], logCard);
        rb->setChecked(i == current);
        m_verbosity->addButton(rb, i);
        logLay->addWidget(rb);
    }
    auto* warnCard = vos::makeCard("sunken", logCard);
    auto* warnLay = new QHBoxLayout(warnCard);
    warnLay->setContentsMargins(10, 8, 10, 8);
    auto* warnText = new QLabel(QStringLiteral(
        "TRACE logs every control-protocol line (→/←). Useful for IPC debugging; noisy in ops."), warnCard);
    warnText->setProperty("vosHint", true);
    warnText->setWordWrap(true);
    warnLay->addWidget(warnText);
    logLay->addWidget(warnCard);
    logLay->addStretch(1);
    lay->addWidget(logCard, 2);
    return host;
}

QWidget* SettingsPage::buildUiTab()
{
    auto* host = new QWidget(this);
    auto* lay = new QVBoxLayout(host);
    lay->setContentsMargins(0, 8, 0, 0);
    lay->setSpacing(12);

    auto* card = vos::makeCard("true", host);
    auto* cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(16, 14, 16, 14);
    cardLay->setSpacing(10);
    cardLay->addWidget(new vos::SectionHeader(QStringLiteral("UI CUSTOMIZATION"), card));
    cardLay->addWidget(vos::capsLabel(QStringLiteral("ACCENT HUE"), 8, card));

    auto* swatches = new QHBoxLayout;
    swatches->setSpacing(8);
    m_accent = new QButtonGroup(card);
    const struct { theme::Accent a; const char* color; } hues[] = {
        { theme::Accent::Cyan,    "#00f2ff" },
        { theme::Accent::Magenta, "#e254ff" },
        { theme::Accent::Lilac,   "#d1bcff" },
        { theme::Accent::Peach,   "#ffb59d" },
    };
    const int savedAccent = QSettings().value(QStringLiteral("ui/accent"), 0).toInt();
    for (int i = 0; i < 4; ++i) {
        auto* b = new QPushButton(card);
        b->setCheckable(true);
        b->setFixedSize(40, 32);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(theme::accentName(hues[i].a));
        b->setStyleSheet(QStringLiteral(
            "QPushButton { background:%1; border:1px solid %2; border-radius:3px; }"
            "QPushButton:checked { border:2px solid %3; }")
            .arg(hues[i].color, theme::kOutlineVariant, theme::kOnSurface));
        b->setChecked(i == savedAccent);
        m_accent->addButton(b, i);
        swatches->addWidget(b);
    }
    swatches->addStretch(1);
    connect(m_accent, &QButtonGroup::idClicked, this, [this](int id) {
        theme::setAccent(static_cast<theme::Accent>(id));
        QSettings().setValue(QStringLiteral("ui/accent"), id);
        emit accentChanged();
        logInfo("GUI", QStringLiteral("accent hue → %1")
                           .arg(theme::accentName(static_cast<theme::Accent>(id))));
    });
    cardLay->addLayout(swatches);

    m_ledPulse = new QCheckBox(QStringLiteral("LED STATUS PULSE — active components blink at 0.5 Hz"), card);
    m_ledPulse->setChecked(QSettings().value(QStringLiteral("ui/ledPulse"), true).toBool());
    connect(m_ledPulse, &QCheckBox::toggled, this, [](bool on) {
        vos::LedDot::setPulseEnabled(on);
        QSettings().setValue(QStringLiteral("ui/ledPulse"), on);
    });
    cardLay->addWidget(m_ledPulse);

    auto* blur = new QCheckBox(QStringLiteral("MOTION BLUR (FX) — 4px gaussian on background shells"), card);
    vos::markPlanned(blur, QStringLiteral("PLANNED — needs a compositing pass"));
    cardLay->addWidget(blur);
    auto* gridPrecision = new QCheckBox(QStringLiteral("GRID PRECISION — 8PX BASE layout density"), card);
    vos::markPlanned(gridPrecision);
    cardLay->addWidget(gridPrecision);
    lay->addWidget(card);
    lay->addStretch(1);
    return host;
}

QWidget* SettingsPage::buildPlannedTab(const QString& what)
{
    auto* host = new QWidget(this);
    auto* lay = new QVBoxLayout(host);
    lay->setContentsMargins(0, 8, 0, 0);
    auto* card = vos::makeCard("sunken", host);
    auto* cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(20, 40, 20, 40);
    auto* badge = new vos::Badge(QStringLiteral("PLANNED"), vos::Badge::Planned, card);
    cardLay->addWidget(badge, 0, Qt::AlignHCenter);
    auto* text = vos::capsLabel(QStringLiteral("%1 CONFIGURATION LANDS IN A LATER ITERATION").arg(what), 9, card);
    text->setAlignment(Qt::AlignCenter);
    cardLay->addWidget(text);
    lay->addWidget(card);
    lay->addStretch(1);
    return host;
}

void SettingsPage::rebuildAccelerators(const QVector<AcceleratorOption>& accels)
{
    if (!m_accelList || !m_accelGroup)
        return;

    // Tear down the previous radios (toggled(false) fires on delete but the
    // handler ignores it — it only acts on the newly-checked button).
    const auto old = m_accelGroup->buttons();
    for (QAbstractButton* b : old) {
        m_accelGroup->removeButton(b);
        delete b;
    }

    const QString current = m_service->accelSelection();
    bool anyChecked = false;

    auto addRadio = [&](const QString& sel, const QString& text, bool enabled, bool checked) {
        auto* rb = new QRadioButton(text);
        rb->setEnabled(enabled);
        rb->setChecked(checked);
        if (checked) anyChecked = true;
        m_accelGroup->addButton(rb);
        m_accelList->addWidget(rb);
        connect(rb, &QRadioButton::toggled, this, [this, sel](bool on) {
            if (on) m_service->setAccelSelection(sel);
        });
    };

    // AUTO is always offered and resolves to the best available engine.
    addRadio(QStringLiteral("auto"), QStringLiteral("AUTO — best available"),
             true, current == QLatin1String("auto"));

    QStringList gpuNames;
    for (const AcceleratorOption& o : accels) {
        QString label = o.backend.toUpper();
        if (!o.device.isEmpty()) label += QStringLiteral("  ·  %1").arg(o.device);
        if (!o.available)        label += QStringLiteral("   (unavailable)");
        addRadio(o.backend, label, o.available, o.available && current == o.backend);
        if (o.available && o.backend != QLatin1String("cpu"))
            gpuNames << o.backend.toUpper();
    }

    // Persisted pick no longer available → snap back to AUTO (also corrects the
    // stored selection via the toggled handler).
    if (!anyChecked && !m_accelGroup->buttons().isEmpty())
        m_accelGroup->buttons().first()->setChecked(true);

    if (accels.isEmpty())
        m_accelStatus->setText(QStringLiteral("Detecting… (backend offline?)"));
    else if (gpuNames.isEmpty())
        m_accelStatus->setText(QStringLiteral("No GPU detected — running on CPU."));
    else
        m_accelStatus->setText(QStringLiteral("GPU available: %1").arg(gpuNames.join(QStringLiteral(", "))));
}

void SettingsPage::save()
{
    QSettings s;
    m_service->setControlSocketPath(m_socketEdit->text().trimmed());
    m_service->setBackendBinary(m_binaryEdit->text().trimmed());
    m_service->setAutostart(m_autoStart->isChecked());
    s.setValue(QStringLiteral("backend/socket"), m_socketEdit->text().trimmed());
    s.setValue(QStringLiteral("backend/binary"), m_binaryEdit->text().trimmed());
    s.setValue(QStringLiteral("backend/autostart"), m_autoStart->isChecked());
    s.setValue(QStringLiteral("runtime/accel"), m_service->accelSelection());

    const int lvl = qMax(0, m_verbosity->checkedId());
    s.setValue(QStringLiteral("ui/verbosity"), lvl);
    emit verbosityChanged(lvl);

    logInfo("GUI", QStringLiteral("settings saved"));
}

void SettingsPage::exportJson()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export configuration"),
        QDir::homePath() + QStringLiteral("/visionos_config.json"),
        QStringLiteral("JSON (*.json)"));
    if (path.isEmpty())
        return;
    QJsonObject o;
    o.insert(QStringLiteral("controlSocket"), m_socketEdit->text());
    o.insert(QStringLiteral("backendBinary"), m_binaryEdit->text());
    o.insert(QStringLiteral("autostart"), m_autoStart->isChecked());
    o.insert(QStringLiteral("accelSelection"), m_service->accelSelection());
    o.insert(QStringLiteral("verbosity"), m_verbosity->checkedId());
    o.insert(QStringLiteral("accent"), m_accent->checkedId());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
        logInfo("GUI", QStringLiteral("config exported to %1").arg(path));
    } else {
        logErr("GUI", QStringLiteral("cannot write %1").arg(path));
    }
}
