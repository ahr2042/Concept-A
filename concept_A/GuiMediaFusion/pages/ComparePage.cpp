#include "ComparePage.h"

#include "../theme/Theme.h"
#include "../widgets/Components.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

// Dark pane with corner brackets and a centered PLANNED note.
class PlannedPane : public QWidget
{
public:
    PlannedPane(const QString& caption, QWidget* parent = nullptr)
        : QWidget(parent), m_caption(caption)
    {
        setMinimumHeight(320);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(theme::kSurfaceLowest));
        p.setPen(QColor(24, 24, 27));
        for (int y = 0; y < height(); y += 4)
            p.drawLine(0, y, width(), y);
        p.setPen(QColor("#6a6a6e"));
        p.setFont(theme::monoFont(10, QFont::Bold, 114.0));
        p.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, m_caption);
    }
private:
    QString m_caption;
};

} // namespace

ComparePage::ComparePage(BackendService* service, QWidget* parent)
    : QWidget(parent)
{
    Q_UNUSED(service);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(14);

    auto* banner = vos::makeCard("raised", this);
    auto* bannerLay = new QHBoxLayout(banner);
    bannerLay->setContentsMargins(14, 10, 14, 10);
    bannerLay->setSpacing(10);
    bannerLay->addWidget(new vos::Badge(QStringLiteral("PLANNED"), vos::Badge::Planned, banner));
    auto* note = new QLabel(QStringLiteral(
        "Side-by-side comparison requires a backend tee (one source → raw + processed "
        "branches) and the ONNX inference stage. Layout ships now; plumbing lands in a "
        "later iteration."), banner);
    note->setProperty("vosHint", true);
    note->setWordWrap(true);
    bannerLay->addWidget(note, 1);
    lay->addWidget(banner);

    auto* panes = new QHBoxLayout;
    panes->setSpacing(14);
    panes->addWidget(buildPane(QStringLiteral("RAW SOURCE"),
                               QStringLiteral("CAM_RAW // DIRECT_FEED"), true), 1);
    panes->addWidget(buildPane(QStringLiteral("AI PROCESSED"),
                               QStringLiteral("ONNX_RUNTIME // OVERLAY_FEED"), false), 1);
    panes->addWidget(buildModelRack());
    lay->addLayout(panes, 1);

    lay->addWidget(buildTransport());
}

QWidget* ComparePage::buildPane(const QString& badge, const QString& title, bool accent)
{
    auto* card = vos::makeCard("sunken", this);
    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(1, 1, 1, 1);
    lay->setSpacing(0);

    auto* head = new QFrame(card);
    head->setStyleSheet(QStringLiteral("QFrame { background:%1; border:none; border-bottom:1px solid %2; }")
                            .arg(theme::kSurfaceLow, theme::kOutlineVariant));
    auto* headLay = new QHBoxLayout(head);
    headLay->setContentsMargins(10, 8, 10, 8);
    headLay->setSpacing(10);
    headLay->addWidget(new vos::Badge(badge, accent ? vos::Badge::Accent : vos::Badge::Secondary, head));
    auto* t = vos::dataLabel(title, 11, head);
    headLay->addWidget(t);
    headLay->addStretch(1);
    lay->addWidget(head);

    lay->addWidget(new PlannedPane(
        accent ? QStringLiteral("AWAITING_TEE_SUPPORT //\nBIND RAW BRANCH HERE")
               : QStringLiteral("AI_STAGE_OFFLINE //\nONNX RUNTIME (AMD) PLANNED"), card), 1);
    return card;
}

QWidget* ComparePage::buildModelRack()
{
    auto* rack = vos::makeCard("true", this);
    rack->setFixedWidth(250);
    auto* lay = new QVBoxLayout(rack);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);
    lay->addWidget(new vos::SectionHeader(QStringLiteral("AI MODELS"), rack));

    struct Model { const char* name; const char* desc; };
    const Model models[] = {
        { "YOLO_V8_TURBO",       "High-speed object detection in low light." },
        { "SSD_MOBILENET_V3",    "Lightweight architecture for edge deploys." },
        { "POSE_NET_INDUSTRIAL", "Skeletal tracking for ergonomic safety." },
        { "SEGMENT_ANYTHING_V2", "Pixel-perfect semantic segmentation." },
    };
    for (const Model& m : models) {
        auto* card = vos::makeCard("raised", rack);
        auto* cLay = new QVBoxLayout(card);
        cLay->setContentsMargins(10, 8, 10, 8);
        cLay->setSpacing(3);
        cLay->addWidget(vos::dataLabel(QString::fromLatin1(m.name), 9, card));
        auto* d = new QLabel(QString::fromLatin1(m.desc), card);
        d->setProperty("vosHint", true);
        d->setWordWrap(true);
        cLay->addWidget(d);
        vos::markPlanned(card, QStringLiteral("PLANNED — model zoo arrives with the ONNX stage"));
        lay->addWidget(card);
    }
    lay->addStretch(1);

    auto* cfg = new QPushButton(QStringLiteral("CONFIGURE HYPERPARAMS"), rack);
    vos::markPlanned(cfg);
    lay->addWidget(cfg);
    return rack;
}

QWidget* ComparePage::buildTransport()
{
    auto* bar = vos::makeCard("true", this);
    auto* lay = new QHBoxLayout(bar);
    lay->setContentsMargins(14, 8, 14, 8);
    lay->setSpacing(10);
    for (const char* glyph : { "<<", "▶", ">>" }) {
        auto* b = new QPushButton(QString::fromUtf8(glyph), bar);
        b->setFixedSize(38, 32);
        vos::markPlanned(b, QStringLiteral("PLANNED — DVR transport needs recorded streams"));
        lay->addWidget(b);
    }
    auto* tc = vos::dataLabel(QStringLiteral("00:00:00:00"), 11, bar);
    tc->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kOnSurfaceVariant));
    lay->addWidget(tc);
    lay->addStretch(1);
    lay->addWidget(new vos::Badge(QStringLiteral("DVR: PLANNED"), vos::Badge::Planned, bar));
    return bar;
}
