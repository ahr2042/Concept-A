#include "DeviceParser.h"

#include <QRegularExpression>
#include <QStringList>

namespace deviceparser {

namespace {

// g_strdup_value_contents renders strings quoted, fractions as N/D (sometimes
// annotated "(fraction)30/1"), lists inside "{ }". Extract something readable.
QString field(const QString& block, const QString& key)
{
    const QRegularExpression re(QStringLiteral("^\\s*%1: (.*)$").arg(key),
                                QRegularExpression::MultilineOption);
    const auto m = re.match(block);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

QString firstFraction(QString v)
{
    const QRegularExpression fr(QStringLiteral("(\\d+)\\s*/\\s*(\\d+)"));
    const auto m = fr.match(v);
    if (!m.hasMatch())
        return {};
    const double num = m.captured(1).toDouble();
    const double den = qMax(1.0, m.captured(2).toDouble());
    return QString::number(num / den, 'g', 4);
}

QString makeLabel(int index, const QString& block)
{
    const QString w  = field(block, QStringLiteral("width"));
    const QString h  = field(block, QStringLiteral("height"));
    QString fmt      = field(block, QStringLiteral("format"));
    fmt.remove('"');
    const QString fps = firstFraction(field(block, QStringLiteral("framerate")));

    QString label;
    if (!w.isEmpty() && !h.isEmpty())
        label = QStringLiteral("%1x%2").arg(w, h);
    if (!fps.isEmpty())
        label += QStringLiteral(" @ %1fps").arg(fps);
    if (!fmt.isEmpty())
        label += QStringLiteral("  %1").arg(fmt);
    if (label.isEmpty())
        label = QStringLiteral("cap %1").arg(index);
    return label.trimmed();
}

} // namespace

bool parse(const QString& reply, QVector<DeviceInfo>& devices)
{
    devices.clear();
    if (!reply.startsWith(QStringLiteral("OK")))
        return false;

    const QRegularExpression devRe(QStringLiteral("^Device \\[(\\d+)\\]: (.*)$"));
    const QRegularExpression capRe(QStringLiteral("^  Cap \\[(\\d+)\\]: (.*)$"));

    DeviceInfo* dev = nullptr;
    CapInfo*    cap = nullptr;

    const QStringList lines = reply.split('\n');
    for (const QString& line : lines) {
        if (auto m = devRe.match(line); m.hasMatch()) {
            devices.append(DeviceInfo{ m.captured(1).toInt(), m.captured(2).trimmed(), {} });
            dev = &devices.last();
            cap = nullptr;
            continue;
        }
        if (dev) {
            if (auto m = capRe.match(line); m.hasMatch()) {
                dev->caps.append(CapInfo{ m.captured(1).toInt(), {}, {} });
                cap = &dev->caps.last();
                cap->raw = line.trimmed() + '\n';
                continue;
            }
            if (cap && line.startsWith(QStringLiteral("    ")))
                cap->raw += line.trimmed() + '\n';
        }
    }

    for (DeviceInfo& d : devices)
        for (CapInfo& c : d.caps)
            c.label = makeLabel(c.index, c.raw);

    return true;
}

} // namespace deviceparser
