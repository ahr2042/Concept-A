#include "PipelinePage.h"

#include "../core/AppLog.h"
#include "../theme/Theme.h"
#include "../widgets/Components.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QTextEdit>
#include <QVBoxLayout>

// ── NodeCanvas ── dotted grid + three fixed node cards + connection curves ───

class NodeCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit NodeCanvas(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(520, 420);
        const QStringList kinds  = { "SOURCE", "PRE-PROCESS", "OUTPUT" };
        const QStringList titles = { "V4L2_CAMERA", "OPENCV_CHAIN", "APP_SINK (GUI)" };
        for (int i = 0; i < 3; ++i) {
            auto* node = new QPushButton(this);
            node->setCheckable(true);
            node->setCursor(Qt::PointingHandCursor);
            node->setFixedSize(180, 92);
            m_nodes.append(node);
            m_kind.append(kinds[i]);
            m_title.append(titles[i]);
            styleNode(i);
            connect(node, &QPushButton::clicked, this, [this, i] { select(i); });
        }
        select(0);
    }

    void setNodeTitle(int i, const QString& t) { m_title[i] = t; styleNode(i); }
    void setNodeLive(bool live) { m_live = live; update(); }

signals:
    void nodeSelected(int index);

protected:
    void resizeEvent(QResizeEvent*) override
    {
        const int cy = height() / 2 - 46;
        const int gap = qMax(30, (width() - 3 * 180 - 80) / 2);
        int x = 40;
        for (auto* n : m_nodes) {
            n->move(x, cy + (x == 40 ? -40 : (x > width() / 2 ? 40 : 0)));
            x += 180 + gap;
        }
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(theme::kSurfaceLowest));
        // dotted grid, 22 px pitch (design's canvas)
        p.setPen(QPen(QColor(42, 42, 45), 1));
        for (int y = 8; y < height(); y += 22)
            for (int x = 8; x < width(); x += 22)
                p.drawPoint(x, y);

        // connection curves between node edges
        p.setRenderHint(QPainter::Antialiasing);
        QPen wire(m_live ? theme::palette().accent : QColor(theme::kOutline), 1.6,
                  m_live ? Qt::SolidLine : Qt::DashLine);
        p.setPen(wire);
        for (int i = 0; i + 1 < m_nodes.size(); ++i) {
            const QRect a = m_nodes[i]->geometry();
            const QRect b = m_nodes[i + 1]->geometry();
            const QPointF p0(a.right(), a.center().y());
            const QPointF p1(b.left(),  b.center().y());
            QPainterPath path(p0);
            const double dx = (p1.x() - p0.x()) * 0.5;
            path.cubicTo(p0 + QPointF(dx, 0), p1 - QPointF(dx, 0), p1);
            p.drawPath(path);
            // port dots
            p.setBrush(m_live ? theme::palette().accent : QColor(theme::kOutline));
            p.drawEllipse(p0, 3, 3);
            p.drawEllipse(p1, 3, 3);
        }
    }

private:
    void styleNode(int i)
    {
        const bool sel = (m_selected == i);
        m_nodes[i]->setText(QString());
        m_nodes[i]->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; border: 1px solid %2; border-radius: 4px; text-align: left; }")
            .arg(theme::kSurfaceContainer,
                 sel ? theme::palette().accent.name() : theme::kOutlineVariant));
        // compose label via rich child (simplest: set as button text lines)
        m_nodes[i]->setText(QStringLiteral("  %1\n\n  %2").arg(m_kind[i], m_title[i]));
        m_nodes[i]->setFont(theme::monoFont(9, QFont::Bold, 108.0));
    }

    void select(int i)
    {
        m_selected = i;
        for (int k = 0; k < m_nodes.size(); ++k) {
            m_nodes[k]->setChecked(k == i);
            styleNode(k);
        }
        emit nodeSelected(i);
    }

    QVector<QPushButton*> m_nodes;
    QStringList m_kind, m_title;
    int  m_selected = 0;
    bool m_live = false;

    friend class PipelinePage;
};

// ── PipelinePage ─────────────────────────────────────────────────────────────

PipelinePage::PipelinePage(BackendService* service, QWidget* parent)
    : QWidget(parent), m_service(service)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(14);

    // canvas column with toolbar
    auto* canvasHost = vos::makeCard("sunken", this);
    auto* canvasLay = new QVBoxLayout(canvasHost);
    canvasLay->setContentsMargins(1, 1, 1, 1);
    canvasLay->setSpacing(0);

    auto* toolbar = new QFrame(canvasHost);
    toolbar->setStyleSheet(QStringLiteral("QFrame { background:%1; border:none; border-bottom:1px solid %2; }")
                               .arg(theme::kSurfaceLow, theme::kOutlineVariant));
    auto* tbLay = new QHBoxLayout(toolbar);
    tbLay->setContentsMargins(8, 6, 8, 6);
    tbLay->setSpacing(4);
    for (const auto& [glyph, tip] : std::initializer_list<std::pair<QString, QString>>{
             { QStringLiteral("+"), QStringLiteral("Zoom in") },
             { QStringLiteral("-"), QStringLiteral("Zoom out") },
             { QStringLiteral("[]"), QStringLiteral("Fit view") } }) {
        auto* b = new QPushButton(glyph, toolbar);
        b->setProperty("vosRole", QStringLiteral("ghost"));
        b->setFixedSize(30, 26);
        vos::markPlanned(b, QStringLiteral("PLANNED — canvas zoom (fixed layout for the current linear backend)"));
        b->setToolTip(tip + QStringLiteral(" — PLANNED"));
        tbLay->addWidget(b);
    }
    tbLay->addStretch(1);
    m_statusChip = new QLabel(QStringLiteral("IDLE"), toolbar);
    m_statusChip->setFont(theme::monoFont(9, QFont::Bold, 110.0));
    m_statusChip->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kOnSurfaceVariant));
    tbLay->addWidget(m_statusChip);
    canvasLay->addWidget(toolbar);

    m_canvas = new NodeCanvas(canvasHost);
    canvasLay->addWidget(m_canvas, 1);
    lay->addWidget(canvasHost, 1);

    lay->addWidget(buildPropertiesPanel());

    connect(m_canvas, &NodeCanvas::nodeSelected, this, &PipelinePage::showNodeProps);
    connect(m_service, &BackendService::devicesChanged,    this, &PipelinePage::onDevices);
    connect(m_service, &BackendService::algorithmsChanged, this, &PipelinePage::onAlgorithms);
    connect(m_service, &BackendService::configChanged, this, &PipelinePage::showSource);
    connect(m_service, &BackendService::modelsChanged,     this, &PipelinePage::onModels);
    connect(m_service, &BackendService::sessionStarted,    this, &PipelinePage::onSessionStarted);
    connect(m_service, &BackendService::sessionStopped,    this, &PipelinePage::onSessionStopped);
    connect(m_service, &BackendService::sessionFailed,     this, &PipelinePage::onSessionFailed);
}

QWidget* PipelinePage::buildPropertiesPanel()
{
    auto* panel = vos::makeCard("true", this);
    panel->setFixedWidth(300);
    auto* outer = new QVBoxLayout(panel);
    outer->setContentsMargins(14, 14, 14, 14);
    outer->setSpacing(10);

    outer->addWidget(new vos::SectionHeader(QStringLiteral("NODE PROPERTIES"), panel));

    m_propsStack = new QStackedWidget(panel);

    // page 0: SOURCE
    auto* src = new QWidget(m_propsStack);
    auto* srcLay = new QVBoxLayout(src);
    srcLay->setContentsMargins(0, 0, 0, 0);
    srcLay->setSpacing(8);
    auto* srcTitle = new QLabel(QStringLiteral("V4L2_CAMERA"), src);
    srcTitle->setFont(theme::displayFont(15, QFont::Bold));
    srcTitle->setStyleSheet(QStringLiteral("color:%1;").arg(theme::palette().accent.name()));
    srcLay->addWidget(srcTitle);
    srcLay->addWidget(vos::capsLabel(QStringLiteral("ID: NODE_SRC_0001"), 8, src));

    // The selection itself lives in the rail, where it is visible from every
    // page. What belongs to the node is what it resolved to: the device, the
    // mode, and the caps the pipeline will actually negotiate.
    srcLay->addWidget(vos::capsLabel(QStringLiteral("DEVICE"), 8, src));
    m_deviceLabel = vos::dataLabel(QStringLiteral("—"), 10, src);
    m_deviceLabel->setWordWrap(true);
    srcLay->addWidget(m_deviceLabel);
    srcLay->addWidget(vos::capsLabel(QStringLiteral("CAPTURE MODE"), 8, src));
    m_capsLabel = vos::dataLabel(QStringLiteral("—"), 10, src);
    m_capsLabel->setWordWrap(true);
    srcLay->addWidget(m_capsLabel);
    srcLay->addWidget(vos::capsLabel(QStringLiteral("RAW CAPS"), 8, src));
    auto* rawView = new QTextEdit(src);
    rawView->setReadOnly(true);
    rawView->setFixedHeight(120);
    srcLay->addWidget(rawView);
    rawView->setObjectName(QStringLiteral("rawCaps"));
    srcLay->addStretch(1);
    m_propsStack->addWidget(src);

    // page 1: PRE-PROCESS
    auto* proc = new QWidget(m_propsStack);
    auto* procLay = new QVBoxLayout(proc);
    procLay->setContentsMargins(0, 0, 0, 0);
    procLay->setSpacing(8);
    auto* procTitle = new QLabel(QStringLiteral("OPENCV_CHAIN"), proc);
    procTitle->setFont(theme::displayFont(15, QFont::Bold));
    procTitle->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kSecondaryLight));
    procLay->addWidget(procTitle);
    procLay->addWidget(vos::capsLabel(QStringLiteral("ID: NODE_PROC_0002 — IN-PLACE PAD PROBE"), 8, proc));
    procLay->addWidget(vos::capsLabel(QStringLiteral("REAL-TIME ALGORITHMS"), 8, proc));
    m_chain = new vos::ChainEditor(m_service, proc);
    procLay->addWidget(m_chain);
    connect(m_chain, &vos::ChainEditor::chainChanged, this,
            [this](const QString&, const AlgorithmSettings&) { publishChain(); });
    // A knob committed on a running session takes effect within a frame or two;
    // without one the value simply waits in the config for the next deploy.
    //
    // primarySession(), not this page's own id: the chain is edited here but the
    // stream is just as often started from the Dashboard, and keying off the
    // local id would silently stop retuning in exactly that case.
    connect(m_chain, &vos::ChainEditor::stageRetuned, this,
            [this](const QString& algo, const QVariantMap& values) {
        if (m_service->primarySession() >= 0)
            m_service->setAlgorithmParams(m_service->primarySession(), algo, values);
    });

    procLay->addWidget(vos::makeHSeparator(proc));
    auto* aiRow = new QHBoxLayout;
    aiRow->addWidget(vos::capsLabel(QStringLiteral("AI INFERENCE (OPENCV DNN)"), 8, proc));
    aiRow->addStretch(1);
    procLay->addLayout(aiRow);
    procLay->addWidget(vos::capsLabel(QStringLiteral("DETECTION MODEL"), 8, proc));
    m_modelBox = new QComboBox(proc);
    m_modelBox->addItem(QStringLiteral("QUERYING…"));
    m_modelBox->setEnabled(false);
    m_modelBox->setToolTip(QStringLiteral(
        "The model the DETECT algorithm runs. Tick DETECT above to put the\n"
        "inference stage in this node's chain."));
    connect(m_modelBox, &QComboBox::currentIndexChanged, this,
            &PipelinePage::onDetectorSettingChanged);
    procLay->addWidget(m_modelBox);

    // The detector's threshold sits with the detector, next to the model it
    // applies to, rather than on the page that watches the stream run.
    auto* confRow = new QHBoxLayout;
    confRow->addWidget(vos::capsLabel(QStringLiteral("CONFIDENCE"), 8, proc));
    confRow->addStretch(1);
    m_confLabel = vos::dataLabel(QStringLiteral("0.25"), 10, proc);
    confRow->addWidget(m_confLabel);
    procLay->addLayout(confRow);

    m_confSlider = new QSlider(Qt::Horizontal, proc);
    m_confSlider->setRange(5, 95);            // 0.05 … 0.95
    m_confSlider->setValue(25);
    connect(m_confSlider, &QSlider::valueChanged, this, [this](int v) {
        m_confLabel->setText(QString::number(v / 100.0, 'f', 2));
    });
    // Commit on release only: dragging would fire a control command per pixel.
    connect(m_confSlider, &QSlider::sliderReleased, this,
            &PipelinePage::onDetectorSettingChanged);
    procLay->addWidget(m_confSlider);

    auto* aiHint = new QLabel(QStringLiteral(
        "Inference runs on a worker thread; the pad probe overlays the newest "
        "result, so detection cost does not throttle the stream."), proc);
    aiHint->setWordWrap(true);
    aiHint->setProperty("vosHint", true);
    procLay->addWidget(aiHint);
    procLay->addStretch(1);
    m_propsStack->addWidget(proc);

    // page 2: OUTPUT
    auto* out = new QWidget(m_propsStack);
    auto* outLay = new QVBoxLayout(out);
    outLay->setContentsMargins(0, 0, 0, 0);
    outLay->setSpacing(8);
    auto* outTitle = new QLabel(QStringLiteral("STREAM_OUTPUT"), out);
    outTitle->setFont(theme::displayFont(15, QFont::Bold));
    outTitle->setStyleSheet(QStringLiteral("color:%1;").arg(theme::palette().accent.name()));
    outLay->addWidget(outTitle);
    outLay->addWidget(vos::capsLabel(QStringLiteral("ID: NODE_OUT_0003"), 8, out));
    m_sinkApp = new QRadioButton(QStringLiteral("APP_SINK — UNIXFD → GUI VIEWPORT"), out);
    m_sinkApp->setChecked(true);
    m_sinkScreen = new QRadioButton(QStringLiteral("SCREEN_SINK — BACKEND WINDOW"), out);
    outLay->addWidget(m_sinkApp);
    outLay->addWidget(m_sinkScreen);
    auto* hint = new QLabel(QStringLiteral(
        "APP_SINK streams zero-copy (memfd) into the Dashboard viewport. "
        "SCREEN_SINK opens a native autovideosink window on the backend."), out);
    hint->setWordWrap(true);
    hint->setProperty("vosHint", true);
    outLay->addWidget(hint);
    connect(m_sinkApp, &QRadioButton::toggled, this, [this](bool app) {
        m_canvas->setNodeTitle(2, app ? QStringLiteral("APP_SINK (GUI)")
                                      : QStringLiteral("SCREEN_SINK"));
    });
    outLay->addStretch(1);
    m_propsStack->addWidget(out);

    outer->addWidget(m_propsStack, 1);

    // Pipeline-level, not node-level: acceleration applies to the whole deploy,
    // so it belongs beside DEPLOY rather than inside any one node's properties.
    auto* accelCard = vos::makeCard("raised", panel);
    auto* accelLay = new QHBoxLayout(accelCard);
    accelLay->setContentsMargins(10, 8, 10, 8);
    auto* accelText = new QVBoxLayout;
    accelText->setSpacing(2);
    accelText->addWidget(vos::dataLabel(QStringLiteral("GPU_ACCELERATION"), 10, accelCard));
    m_accelHw = vos::capsLabel(QStringLiteral("DETECTING…"), 8, accelCard);
    accelText->addWidget(m_accelHw);
    accelLay->addLayout(accelText);
    accelLay->addStretch(1);
    m_accelToggle = new vos::ToggleSwitch(accelCard);
    connect(m_accelToggle, &vos::ToggleSwitch::toggled, this, [this](bool on) {
        // ON = let the detector use the GPU (AUTO resolves to Vulkan); OFF = CPU.
        m_service->setAccelSelection(on ? QStringLiteral("auto") : QStringLiteral("cpu"));
    });
    connect(m_service, &BackendService::acceleratorsChanged, this,
            [this](const QVector<AcceleratorOption>&) { refreshAccelToggle(); });
    connect(m_service, &BackendService::accelSelectionChanged, this,
            [this](const QString&) { refreshAccelToggle(); },
            Qt::QueuedConnection);   // sync with the Settings radios
    accelLay->addWidget(m_accelToggle);
    refreshAccelToggle();
    m_service->refreshAccelerators();
    outer->addWidget(accelCard);

    m_deployBtn = new QPushButton(QStringLiteral("DEPLOY PIPELINE"), panel);
    m_deployBtn->setProperty("vosRole", QStringLiteral("deploy"));
    m_deployBtn->setFixedHeight(40);
    connect(m_deployBtn, &QPushButton::clicked, this, &PipelinePage::onDeploy);
    m_haltBtn = new QPushButton(QStringLiteral("HALT PIPELINE"), panel);
    m_haltBtn->setProperty("vosRole", QStringLiteral("danger"));
    m_haltBtn->setEnabled(false);
    connect(m_haltBtn, &QPushButton::clicked, this, &PipelinePage::onHalt);
    outer->addWidget(m_deployBtn);
    outer->addWidget(m_haltBtn);
    return panel;
}

void PipelinePage::showNodeProps(int node)
{
    if (m_propsStack)
        m_propsStack->setCurrentIndex(node);
}

void PipelinePage::onDevices(const QVector<DeviceInfo>& devices)
{
    m_devices = devices;
    m_deployBtn->setEnabled(!devices.isEmpty() && m_sessionId < 0);
    showSource(m_service->config());
}

// Reflect whatever source the rail has selected into the SOURCE node.
void PipelinePage::showSource(const BackendService::DeploySpec& cfg)
{
    const DeviceInfo* device = nullptr;
    for (const DeviceInfo& d : m_devices)
        if (d.index == cfg.deviceIndex)
            device = &d;

    const CapInfo* cap = nullptr;
    if (device)
        for (const CapInfo& c : device->caps)
            if (c.index == cfg.capIndex)
                cap = &c;

    m_deviceLabel->setText(device ? device->name.toUpper() : QStringLiteral("—"));
    m_capsLabel->setText(cap ? cap->label : QStringLiteral("—"));
    if (auto* view = findChild<QTextEdit*>(QStringLiteral("rawCaps")))
        view->setPlainText(cap ? cap->raw : QString());
    if (device)
        m_canvas->setNodeTitle(0, device->name.toUpper().left(20));
}

void PipelinePage::onModels(const QVector<DetectorModel>& models)
{
    if (!m_modelBox)
        return;
    m_modelBox->clear();
    if (models.isEmpty()) {
        m_modelBox->addItem(QStringLiteral("NO_MODEL_INSTALLED"));
        m_modelBox->setEnabled(false);
        m_modelBox->setToolTip(QStringLiteral("No weights in models/ — run scripts/fetch-models.sh"));
        return;
    }
    for (const DetectorModel& m : models)
        m_modelBox->addItem(QStringLiteral("%1 · %2px").arg(m.name.toUpper()).arg(m.inputSize),
                            m.name);
    m_modelBox->setEnabled(true);
}

void PipelinePage::onAlgorithms(const QStringList& algos)
{
    if (m_chain)
        m_chain->rebuild(algos);
}

// Mirror the chain editor into the shared working config, and keep the canvas
// node reading as what the chain actually does.
void PipelinePage::publishChain()
{
    if (!m_chain)
        return;
    const QString csv = m_chain->algosCsv();
    m_service->setChain(csv, m_chain->algoParams());

    const QStringList active = csv.split(QLatin1Char(','), Qt::SkipEmptyParts);
    m_canvas->setNodeTitle(1, active.isEmpty()
        ? QStringLiteral("PASSTHROUGH")
        : active.join(QStringLiteral(" → ")).toUpper());

    if (m_modelBox->isEnabled())
        m_service->setDetectorSettings(m_modelBox->currentData().toString(),
                                       m_service->config().confidence,
                                       m_service->config().nms, true);
}

// Capability-driven: the toggle offers the GPU only when one was detected, and
// names the device that was found so the operator knows what "on" means.
void PipelinePage::refreshAccelToggle()
{
    if (!m_accelToggle)
        return;
    QString device;
    for (const AcceleratorOption& o : m_service->accelerators())
        if (o.available && o.backend != QLatin1String("cpu")) {
            device = o.device;
            break;
        }
    const bool gpu = !device.isEmpty();

    m_accelHw->setText(gpu ? device.toUpper() : QStringLiteral("CPU ONLY — NO GPU DETECTED"));
    m_accelToggle->setEnabled(gpu);
    m_accelToggle->setChecked(gpu && m_service->accelSelection() != QLatin1String("cpu"));
    m_accelToggle->setToolTip(gpu
        ? QStringLiteral("Run the detector on the GPU (ncnn + Vulkan). Applied on the next deploy.")
        : QStringLiteral("No GPU detected — CPU only."));
}

// Model or threshold moved. Both live in the working config; a live session also
// gets them straight away, since the detector reloads off the streaming thread.
void PipelinePage::onDetectorSettingChanged()
{
    if (!m_modelBox->isEnabled())
        return;
    const QString model = m_modelBox->currentData().toString();
    const double  conf  = m_confSlider->value() / 100.0;
    m_service->setDetectorSettings(model, conf, m_service->config().nms, true);
    // Same reasoning as the chain knobs: whichever page started the stream.
    if (m_service->primarySession() >= 0)
        m_service->setDetector(m_service->primarySession(), model, conf,
                               m_service->config().nms, true);
}

void PipelinePage::onDeploy()
{
    if (m_devices.isEmpty())
        return;
    // Deploy the working config rather than this page's widgets. It used to
    // build its own spec here, which is why it never sent the confidence.
    publishChain();
    m_sessionId = m_service->deployWorkingConfig(QStringLiteral("pipeline-editor"),
                                                 m_sinkScreen->isChecked());
    m_deployBtn->setEnabled(false);
    m_statusChip->setText(QStringLiteral("DEPLOYING…"));
    m_statusChip->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kWarn));
}

void PipelinePage::onHalt()
{
    if (m_sessionId >= 0)
        m_service->stop(m_sessionId);
    m_haltBtn->setEnabled(false);
}

void PipelinePage::onSessionStarted(int sessionId, const QString& socket, const QString& desc)
{
    if (sessionId != m_sessionId) return;
    Q_UNUSED(socket); Q_UNUSED(desc);
    m_canvas->setNodeLive(true);
    m_haltBtn->setEnabled(true);
    m_statusChip->setText(QStringLiteral("LIVE"));
    m_statusChip->setStyleSheet(QStringLiteral("color:%1;").arg(theme::palette().accent.name()));
    if (m_sinkApp->isChecked())
        logInfo("GUI", QStringLiteral("pipeline deployed — open DASHBOARD or MULTI-GRID and "
                                      "bind a viewport, or redeploy from those pages"));
}

void PipelinePage::onSessionStopped(int sessionId)
{
    if (sessionId != m_sessionId) return;
    m_sessionId = -1;
    m_canvas->setNodeLive(false);
    m_deployBtn->setEnabled(!m_devices.isEmpty());
    m_haltBtn->setEnabled(false);
    m_statusChip->setText(QStringLiteral("IDLE"));
    m_statusChip->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kOnSurfaceVariant));
}

void PipelinePage::onSessionFailed(int sessionId, const QString& error)
{
    if (sessionId != m_sessionId) return;
    Q_UNUSED(error);
    m_sessionId = -1;
    m_canvas->setNodeLive(false);
    m_deployBtn->setEnabled(!m_devices.isEmpty());
    m_haltBtn->setEnabled(false);
    m_statusChip->setText(QStringLiteral("FAILED"));
    m_statusChip->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kError));
}

#include "PipelinePage.moc"
