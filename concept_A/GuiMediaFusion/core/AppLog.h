#pragma once

// App-wide log bus. Every subsystem reports here; the Dashboard event feed,
// the Logs page and the Analytics event log all render from this single model,
// and Settings' LOGGING VERBOSITY selects the minimum level they show.

#include <QDateTime>
#include <QObject>
#include <QVector>

class AppLog : public QObject
{
    Q_OBJECT
public:
    enum Level { Debug = 0, Info = 1, Warn = 2, Err = 3 };
    Q_ENUM(Level)

    struct Entry {
        QDateTime ts;
        Level     level;
        QString   tag;      // subsystem: CTL, DAEMON, STREAM, GUI, SYS
        QString   message;
    };

    static AppLog& instance();

    void log(Level level, const QString& tag, const QString& message);
    const QVector<Entry>& entries() const { return m_entries; }

    static QString levelName(Level l);
    bool exportCsv(const QString& filePath) const;

signals:
    void entryAdded(const AppLog::Entry& entry);

private:
    explicit AppLog(QObject* parent = nullptr);
    QVector<Entry> m_entries;
};

// Convenience free functions.
inline void logDebug(const QString& tag, const QString& msg) { AppLog::instance().log(AppLog::Debug, tag, msg); }
inline void logInfo (const QString& tag, const QString& msg) { AppLog::instance().log(AppLog::Info,  tag, msg); }
inline void logWarn (const QString& tag, const QString& msg) { AppLog::instance().log(AppLog::Warn,  tag, msg); }
inline void logErr  (const QString& tag, const QString& msg) { AppLog::instance().log(AppLog::Err,   tag, msg); }
