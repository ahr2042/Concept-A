#pragma once

// Structured forms of the daemon's `models` and `stats <id>` replies, plus the
// parsers that produce them — the inference-stage counterpart to DeviceParser.
//
//   models                     model 0 yolov4-tiny        stats 0
//   -----------------------    --------------------       ------------------------
//   OK 1 model(s)              OK yolov4-tiny             OK model=yolov4-tiny loaded=1
//   name=yolov4-tiny classes=80                             infer_ms=22.4 avg_ms=23.1
//     input=416 path=/…/models/yolov4-tiny.onnx             inferred=91 skipped=140
//                                                           objects=2 avg_conf=0.78
//                                                         det conf=0.91 box=1,2,3,4 label=person
//                                                         error=<text>   (only on failure)

#include <QMetaType>
#include <QString>
#include <QVector>

struct DetectorModel {
    QString name;                 // "yolov4-tiny" — what `model <id> <name>` takes
    int     classCount = 0;
    int     inputSize  = 0;
    QString path;
};

struct DetectionBox {
    QString label;
    double  confidence = 0.0;
    int     x = 0, y = 0, width = 0, height = 0;
};

struct InferenceSnapshot {
    QString modelName;
    bool    loaded         = false;
    double  inferenceMs    = 0.0;
    double  avgInferenceMs = 0.0;
    quint64 framesInferred = 0;
    quint64 framesSkipped  = 0;
    int     objectCount    = 0;
    double  avgConfidence  = 0.0;
    QString error;                       // detector-reported failure, else empty

    QVector<DetectionBox> detections;

    // True once the stage has actually run — the console shows OFFLINE until then.
    bool active() const { return loaded && framesInferred > 0; }
};

namespace inferenceparser {

// Both return true when the reply started with "OK".
bool parseModels(const QString& reply, QVector<DetectorModel>& models);
bool parseStats(const QString& reply, InferenceSnapshot& snapshot);

} // namespace inferenceparser

Q_DECLARE_METATYPE(DetectorModel)
Q_DECLARE_METATYPE(InferenceSnapshot)
