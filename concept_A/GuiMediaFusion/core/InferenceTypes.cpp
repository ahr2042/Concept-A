#include "InferenceTypes.h"

namespace {

// Value of `key=` in a "k=v k=v" line. Values never contain spaces except the
// trailing ones (path, label), which callers pull out with tailValue().
QString value(const QStringList& tokens, const QString& key)
{
    const QString prefix = key + QLatin1Char('=');
    for (const QString& t : tokens)
        if (t.startsWith(prefix))
            return t.mid(prefix.size());
    return {};
}

// Everything after `key=` to end of line, for values that may contain spaces.
QString tailValue(const QString& line, const QString& key)
{
    const QString prefix = key + QLatin1Char('=');
    const int     at     = line.indexOf(prefix);
    return at < 0 ? QString() : line.mid(at + prefix.size()).trimmed();
}

} // namespace

bool inferenceparser::parseModels(const QString& reply, QVector<DetectorModel>& models)
{
    models.clear();
    if (!reply.startsWith(QLatin1String("OK")))
        return false;

    const QStringList lines = reply.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (!line.startsWith(QLatin1String("name=")))
            continue;

        const QStringList tokens = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        DetectorModel     m;
        m.name       = value(tokens, QStringLiteral("name"));
        m.classCount = value(tokens, QStringLiteral("classes")).toInt();
        m.inputSize  = value(tokens, QStringLiteral("input")).toInt();
        m.path       = tailValue(line, QStringLiteral("path"));
        if (!m.name.isEmpty())
            models.push_back(m);
    }
    return true;
}

bool inferenceparser::parseStats(const QString& reply, InferenceSnapshot& snapshot)
{
    snapshot = InferenceSnapshot{};
    if (!reply.startsWith(QLatin1String("OK")))
        return false;

    const QStringList lines = reply.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();

        if (line.startsWith(QLatin1String("det "))) {
            const QStringList tokens = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            DetectionBox      d;
            d.confidence = value(tokens, QStringLiteral("conf")).toDouble();
            d.label      = tailValue(line, QStringLiteral("label"));

            const QStringList box = value(tokens, QStringLiteral("box"))
                                        .split(QLatin1Char(','), Qt::SkipEmptyParts);
            if (box.size() == 4) {
                d.x      = box[0].toInt();
                d.y      = box[1].toInt();
                d.width  = box[2].toInt();
                d.height = box[3].toInt();
            }
            snapshot.detections.push_back(d);
        }
        else if (line.startsWith(QLatin1String("error="))) {
            snapshot.error = tailValue(line, QStringLiteral("error"));
        }
        else if (line.startsWith(QLatin1String("OK"))) {
            const QStringList tokens = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            snapshot.modelName      = value(tokens, QStringLiteral("model"));
            snapshot.loaded         = value(tokens, QStringLiteral("loaded")).toInt() != 0;
            snapshot.inferenceMs    = value(tokens, QStringLiteral("infer_ms")).toDouble();
            snapshot.avgInferenceMs = value(tokens, QStringLiteral("avg_ms")).toDouble();
            snapshot.framesInferred = value(tokens, QStringLiteral("inferred")).toULongLong();
            snapshot.framesSkipped  = value(tokens, QStringLiteral("skipped")).toULongLong();
            snapshot.objectCount    = value(tokens, QStringLiteral("objects")).toInt();
            snapshot.avgConfidence  = value(tokens, QStringLiteral("avg_conf")).toDouble();
        }
    }
    return true;
}
