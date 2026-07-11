#include "AppLog.h"

#include <QFile>
#include <QTextStream>

AppLog::AppLog(QObject* parent) : QObject(parent) {}

AppLog& AppLog::instance()
{
    static AppLog log;
    return log;
}

void AppLog::log(Level level, const QString& tag, const QString& message)
{
    Entry e{ QDateTime::currentDateTime(), level, tag, message.trimmed() };
    if (e.message.isEmpty())
        return;
    m_entries.append(e);
    // Bounded so a chatty stream can't grow memory forever.
    if (m_entries.size() > 5000)
        m_entries.remove(0, 1000);
    emit entryAdded(e);
}

QString AppLog::levelName(Level l)
{
    switch (l) {
        case Debug: return QStringLiteral("DEBUG");
        case Info:  return QStringLiteral("INFO");
        case Warn:  return QStringLiteral("WARN");
        case Err:   return QStringLiteral("ERR");
    }
    return {};
}

bool AppLog::exportCsv(const QString& filePath) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out(&f);
    out << "timestamp,level,tag,message\n";
    for (const Entry& e : m_entries) {
        QString msg = e.message;
        msg.replace('"', QStringLiteral("\"\""));
        out << e.ts.toString(Qt::ISODateWithMs) << ',' << levelName(e.level) << ','
            << e.tag << ",\"" << msg << "\"\n";
    }
    return true;
}
