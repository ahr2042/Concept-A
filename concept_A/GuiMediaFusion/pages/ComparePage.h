#pragma once

// Stream Comparison — design screen "VISION_OS / Stream Comparison".
// RAW vs AI-PROCESSED side-by-side needs the backend to tee one source into
// two branches, which it cannot do yet, and the AI stage itself is not built.
// This page ships the full design chrome (split panes, model rack, transport
// bar) as an honest PLANNED surface so a later iteration only adds plumbing.

#include <QWidget>

class BackendService;

class ComparePage : public QWidget
{
    Q_OBJECT
public:
    explicit ComparePage(BackendService* service, QWidget* parent = nullptr);

private:
    QWidget* buildPane(const QString& badge, const QString& title, bool accent);
    QWidget* buildModelRack();
    QWidget* buildTransport();
};
