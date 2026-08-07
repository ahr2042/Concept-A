#include "ChainEditor.h"

#include "Components.h"
#include "theme/Theme.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QToolButton>
#include <QVBoxLayout>

namespace vos {

ChainEditor::ChainEditor(BackendService* service, QWidget* parent)
    : QWidget(parent), m_service(service)
{
    m_rows = new QVBoxLayout(this);
    m_rows->setContentsMargins(0, 0, 0, 0);
    m_rows->setSpacing(4);
}

void ChainEditor::rebuild(const QStringList& algos)
{
    // Carry the operator's work across the rebuild. `algorithmsChanged` fires on
    // every reconnect, and emptying a configured chain because the daemon
    // restarted would be its own bug.
    const QString           keptChain  = algosCsv();
    const AlgorithmSettings keptParams = algoParams();

    QLayoutItem* item = nullptr;
    while ((item = m_rows->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_boxes.clear();
    m_panels.clear();
    m_carets.clear();
    m_order.clear();

    if (algos.isEmpty()) {
        m_rows->addWidget(capsLabel(QStringLiteral("NO ALGORITHMS REPORTED"), 8, this));
        return;
    }

    const QStringList wasTicked = keptChain.split(QLatin1Char(','), Qt::SkipEmptyParts);

    for (const QString& algo : algos) {
        m_order << algo;

        auto* row    = new QWidget(this);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->setSpacing(6);

        auto* cb = new QCheckBox(algo.toUpper(), row);
        // The id travels as a property, not as the caption. It used to be read
        // back out of the display text via toUpper()/toLower(), which quietly
        // required every algorithm name to be lowercase to survive the trip.
        cb->setProperty("algo", algo);
        cb->setToolTip(m_service->algorithmSummary(algo));
        cb->setChecked(wasTicked.contains(algo));
        rowLay->addWidget(cb);
        rowLay->addStretch(1);
        m_boxes.insert(algo, cb);

        const QVector<AlgorithmParamSpec> schema = m_service->algorithmParams(algo);
        if (!schema.isEmpty()) {
            auto* caret = new QToolButton(row);
            caret->setText(QStringLiteral("▾"));
            caret->setAutoRaise(true);
            caret->setToolTip(QStringLiteral("Show this stage's parameters"));
            rowLay->addWidget(caret);
            m_carets.insert(algo, caret);
            connect(caret, &QToolButton::clicked, this, [this, algo] {
                setExpanded(m_expanded == algo ? QString() : algo);
            });
        }

        m_rows->addWidget(row);
        connect(cb, &QCheckBox::toggled, this, [this] { emitChanged(); });

        if (schema.isEmpty())
            continue;

        auto* panel = new ParamPanel(algo, schema, this);
        panel->setVisible(false);
        m_panels.insert(algo, panel);
        m_rows->addWidget(panel);

        connect(panel, &ParamPanel::changed, this,
                [this](const QString& a, const QVariantMap& v) {
            emit stageRetuned(a, v);
            emitChanged();
        });
    }

    // Restore tuning after the panels exist, then the expansion, so the panel
    // that was open stays open across a reconnect.
    for (auto it = keptParams.constBegin(); it != keptParams.constEnd(); ++it)
        if (ParamPanel* p = m_panels.value(it.key(), nullptr))
            p->setValues(it.value());
    setExpanded(m_panels.contains(m_expanded) ? m_expanded : QString());
}

void ChainEditor::setExpanded(const QString& algo)
{
    m_expanded = algo;
    for (auto it = m_panels.constBegin(); it != m_panels.constEnd(); ++it)
        it.value()->setVisible(it.key() == algo);
    for (auto it = m_carets.constBegin(); it != m_carets.constEnd(); ++it)
        it.value()->setText(it.key() == algo ? QStringLiteral("▴") : QStringLiteral("▾"));
}

QString ChainEditor::algosCsv() const
{
    // Menu order, not click order: it is the order the engine applies them, so
    // it is the order the operator is actually configuring.
    QStringList active;
    for (const QString& algo : m_order)
        if (QCheckBox* cb = m_boxes.value(algo, nullptr); cb && cb->isChecked())
            active << algo;
    return active.join(QLatin1Char(','));
}

AlgorithmSettings ChainEditor::algoParams() const
{
    AlgorithmSettings out;
    for (const QString& algo : m_order) {
        QCheckBox* cb = m_boxes.value(algo, nullptr);
        if (!cb || !cb->isChecked())
            continue;
        if (ParamPanel* p = m_panels.value(algo, nullptr))
            out.insert(algo, p->values());
    }
    return out;
}

void ChainEditor::emitChanged()
{
    emit chainChanged(algosCsv(), algoParams());
}

} // namespace vos
