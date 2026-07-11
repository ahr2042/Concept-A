#include "LogsPage.h"

#include "../theme/Theme.h"
#include "../widgets/Components.h"

#include <QButtonGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

LogsPage::LogsPage(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(12);

    auto* head = new QHBoxLayout;
    head->addWidget(new vos::SectionHeader(QStringLiteral("SYSTEM EVENT LOG"), this));
    head->addSpacing(16);

    auto* chips = new QButtonGroup(this);
    const QStringList names = { QStringLiteral("ALL"), QStringLiteral("INFO+"),
                                QStringLiteral("WARN+"), QStringLiteral("ERR") };
    for (int i = 0; i < names.size(); ++i) {
        auto* chip = new QPushButton(names[i], this);
        chip->setProperty("vosRole", QStringLiteral("chip"));
        chip->setCheckable(true);
        chip->setChecked(i == 0);
        chips->addButton(chip, i);
        head->addWidget(chip);
    }
    connect(chips, &QButtonGroup::idClicked, this, [this](int id) {
        m_chipLevel = id;      // chip ids align with AppLog levels
        rebuild();
    });

    head->addStretch(1);
    auto* exportBtn = new QPushButton(QStringLiteral("EXPORT .CSV"), this);
    connect(exportBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Export event log"),
            QDir::homePath() + QStringLiteral("/visionos_events.csv"),
            QStringLiteral("CSV (*.csv)"));
        if (!path.isEmpty() && AppLog::instance().exportCsv(path))
            logInfo("GUI", QStringLiteral("event log exported to %1").arg(path));
    });
    head->addWidget(exportBtn);
    lay->addLayout(head);

    m_table = new QTreeWidget(this);
    m_table->setColumnCount(4);
    m_table->setHeaderLabels({ QStringLiteral("TIME"), QStringLiteral("LEVEL"),
                               QStringLiteral("TAG"), QStringLiteral("MESSAGE") });
    m_table->setRootIsDecorated(false);
    m_table->setAlternatingRowColors(false);
    m_table->setUniformRowHeights(true);
    m_table->header()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 110);
    m_table->setColumnWidth(1, 70);
    m_table->setColumnWidth(2, 80);
    lay->addWidget(m_table, 1);

    connect(&AppLog::instance(), &AppLog::entryAdded, this, &LogsPage::onEntry);
    rebuild();
}

void LogsPage::setMinLevel(int lvl)
{
    m_minLevel = lvl;
    rebuild();
}

void LogsPage::rebuild()
{
    m_table->clear();
    for (const AppLog::Entry& e : AppLog::instance().entries())
        addRow(e);
    m_table->scrollToBottom();
}

void LogsPage::onEntry(const AppLog::Entry& e)
{
    addRow(e);
    m_table->scrollToBottom();
}

void LogsPage::addRow(const AppLog::Entry& e)
{
    if (int(e.level) < qMax(m_minLevel, m_chipLevel))
        return;
    auto* item = new QTreeWidgetItem({
        e.ts.toString(QStringLiteral("HH:mm:ss.zzz")),
        AppLog::levelName(e.level),
        e.tag,
        e.message });
    QColor c(theme::kOnSurfaceVariant);
    if (e.level == AppLog::Warn) c = QColor(theme::kWarn);
    if (e.level == AppLog::Err)  c = QColor(theme::kError);
    if (e.level == AppLog::Debug) c = QColor("#6a6a6e");
    for (int col = 0; col < 4; ++col)
        item->setForeground(col, c);
    item->setForeground(0, QColor(theme::palette().accent));
    m_table->addTopLevelItem(item);
    // bound the visible table
    while (m_table->topLevelItemCount() > 2000)
        delete m_table->takeTopLevelItem(0);
}
