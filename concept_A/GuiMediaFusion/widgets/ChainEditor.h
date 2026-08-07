#pragma once

// The processing-chain editor: which algorithms are in the chain, in what order
// the engine will apply them, and the knobs of whichever one is being tuned.
//
// This used to exist twice — once on the Dashboard and once on the Pipeline page
// — as two copies of the same checkbox-and-panel building code. Every algorithm
// the engine gained had to be made to work in both. It is one widget now, owned
// by the Pipeline page, which is where the console does its configuring.

#include "core/BackendService.h"

#include <QHash>
#include <QStringList>
#include <QWidget>

class QCheckBox;
class QToolButton;
class QVBoxLayout;

namespace vos {

class ParamPanel;

class ChainEditor : public QWidget
{
    Q_OBJECT
public:
    explicit ChainEditor(BackendService* service, QWidget* parent = nullptr);

    // The chain as the protocol wants it: lowercase names, comma separated, in
    // menu order (which is the order the engine applies them).
    QString           algosCsv() const;

    // Tuning for the ticked stages only — an unticked stage is not in the chain,
    // so sending its values would be noise.
    AlgorithmSettings algoParams() const;

public slots:
    // Rebuild the rows from the daemon's algorithm list. Ticks and tuning
    // survive, so a reconnect does not silently empty an operator's chain.
    void rebuild(const QStringList& algos);

signals:
    // The chain itself changed (a stage ticked, or a knob committed).
    void chainChanged(const QString& algosCsv, const AlgorithmSettings& params);

    // One stage's knobs were committed. Separate from chainChanged because a
    // live session can be retuned with `algo-set` without redeploying.
    void stageRetuned(const QString& algo, const QVariantMap& values);

private:
    void setExpanded(const QString& algo);
    void emitChanged();

    BackendService*             m_service = nullptr;
    QVBoxLayout*                m_rows    = nullptr;
    QHash<QString, QCheckBox*>  m_boxes;
    QHash<QString, ParamPanel*> m_panels;
    QHash<QString, QToolButton*> m_carets;
    QStringList                 m_order;      // menu order, for algosCsv()

    // Only one stage shows its knobs at a time. With five algorithms expanding
    // everything merely looked busy; the engine is heading for twelve, and the
    // panel has to stay the same height when it gets there.
    QString                     m_expanded;
};

} // namespace vos
