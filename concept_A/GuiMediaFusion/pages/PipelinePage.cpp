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
    srcLay->addWidget(vos::capsLabel(QStringLiteral("DEVICE"), 8, src));
    m_deviceBox = new QComboBox(src);
    srcLay->addWidget(m_deviceBox);
    srcLay->addWidget(vos::capsLabel(QStringLiteral("CAPTURE MODE"), 8, src));
    m_capsBox = new QComboBox(src);
    srcLay->addWidget(m_capsBox);
    srcLay->addWidget(vos::capsLabel(QStringLiteral("RAW CAPS"), 8, src));
    auto* rawView = new QTextEdit(src);
    rawView->setReadOnly(true);
    rawView->setFixedHeight(120);
    srcLay->addWidget(rawView);
    rawView->setObjectName(QStringLiteral("rawCaps"));
    connect(m_deviceBox, &QComboBox::currentIndexChanged, this, [this](int idx) {
        m_capsBox->clear();
        if (idx >= 0 && idx < m_devices.size()) {
            for (const CapInfo& c : m_devices[idx].caps)
                m_capsBox->addItem(c.label, c.index);
            m_canvas->setNodeTitle(0, m_devices[idx].name.toUpper().left(20));
        }
    });
    connect(m_capsBox, &QComboBox::currentIndexChanged, this, [this](int cidx) {
        auto* view = findChild<QTextEdit*>(QStringLiteral("rawCaps"));
        const int didx = m_deviceBox->currentIndex();
        if (view && didx >= 0 && didx < m_devices.size()
            && cidx >= 0 && cidx < m_devices[didx].caps.size())
            view->setPlainText(m_devices[didx].caps[cidx].raw);
    });
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
    auto* algoHost = new QWidget(proc);
    algoHost->setObjectName(QStringLiteral("pipeAlgoHost"));
    auto* algoLay = new QVBoxLayout(algoHost);
    algoLay->setContentsMargins(0, 0, 0, 0);
    algoLay->setSpacing(6);
    procLay->addWidget(algoHost);

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
    procLay->addWidget(m_modelBox);
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
    m_deviceBox->clear();
    for (const DeviceInfo& d : devices)
        m_deviceBox->addItem(d.name.toUpper(), d.index);
    m_deployBtn->setEnabled(!devices.isEmpty() && m_sessionId < 0);
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
    auto* host = findChild<QWidget*>(QStringLiteral("pipeAlgoHost"));
    if (!host) return;
    QLayoutItem* item;
    while ((item = host->layout()->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_algoBoxes.clear();
    for (const QString& a : algos) {
        auto* cb = new QCheckBox(a.toUpper(), host);
        connect(cb, &QCheckBox::toggled, this, [this] {
            QStringList active;
            for (QCheckBox* b : m_algoBoxes)
                if (b->isChecked()) active << b->text();
            m_canvas->setNodeTitle(1, active.isEmpty() ? QStringLiteral("PASSTHROUGH")
                                                       : active.join(QStringLiteral(" → ")));
        });
        m_algoBoxes.append(cb);
        host->layout()->addWidget(cb);
    }
    m_canvas->setNodeTitle(1, QStringLiteral("PASSTHROUGH"));
}

void PipelinePage::onDeploy()
{
    if (!m_deviceBox->currentData().isValid())
        return;
    BackendService::DeploySpec spec;
    spec.deviceIndex = m_deviceBox->currentData().toInt();
    spec.capIndex    = m_capsBox->currentData().isValid() ? m_capsBox->currentData().toInt() : 0;
    QStringList active;
    for (QCheckBox* b : m_algoBoxes)
        if (b->isChecked()) active << b->text().toLower();
    spec.algosCsv   = active.join(',');
    spec.screenSink = m_sinkScreen->isChecked();
    spec.name       = QStringLiteral("pipeline-editor");
    if (m_modelBox->isEnabled())
        spec.detectorModel = m_modelBox->currentData().toString();
    m_sessionId = m_service->deploy(spec);
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
    m_deployBtn->setEnabled(m_deviceBox->count() > 0);
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
    m_deployBtn->setEnabled(m_deviceBox->count() > 0);
    m_haltBtn->setEnabled(false);
    m_statusChip->setText(QStringLiteral("FAILED"));
    m_statusChip->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kError));
}

#include "PipelinePage.moc"
