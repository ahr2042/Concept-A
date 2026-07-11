#pragma once

// Logs — full-width live event table over the AppLog bus (every subsystem plus
// the raw control-protocol transcript at TRACE), with severity filter chips and
// CSV export. This is the design's LOGS nav destination.

#include "../core/AppLog.h"

#include <QWidget>

class QTreeWidget;

class LogsPage : public QWidget
{
    Q_OBJECT
public:
    explicit LogsPage(QWidget* parent = nullptr);

public slots:
    void setMinLevel(int lvl);

private slots:
    void onEntry(const AppLog::Entry& e);

private:
    void rebuild();
    void addRow(const AppLog::Entry& e);

    QTreeWidget* m_table = nullptr;
    int m_minLevel  = 0;
    int m_chipLevel = 0;      // filter chosen by chips (max of settings/chips wins)
};
