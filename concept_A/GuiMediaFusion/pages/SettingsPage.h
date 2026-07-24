#pragma once

// System Settings — design screen "VISION_OS / Settings".
// Real: CONNECTION (control socket, backend binary, autostart, reconnect),
// LOGGING VERBOSITY (event-feed threshold), UI CUSTOMIZATION (accent hue —
// live restyle, LED pulse). Adapted/PLANNED: CORE RUNTIME inference toggles
// (ONNX Runtime on AMD — no CUDA/TensorRT on this stack), NETWORK / STORAGE /
// API ACCESS tabs. SAVE CHANGES persists via QSettings; EXPORT CONFIG → JSON.

#include <QWidget>

#include "../core/InferenceTypes.h"   // AcceleratorOption

class BackendService;
class QLineEdit;
class QCheckBox;
class QButtonGroup;
class QVBoxLayout;
class QLabel;

class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(BackendService* service, QWidget* parent = nullptr);

    static int savedVerbosity();          // AppLog::Level as int (default Info)

signals:
    void accentChanged();                 // MainWindow reapplies the stylesheet
    void verbosityChanged(int minLevel);

private:
    QWidget* buildConnectionTab();
    QWidget* buildRuntimeTab();
    QWidget* buildUiTab();
    QWidget* buildPlannedTab(const QString& what);
    void     save();
    void     exportJson();
    // Repopulates the backend-selector radios from a detection result — only
    // available engines are enabled, plus an always-present AUTO.
    void     rebuildAccelerators(const QVector<AcceleratorOption>& accels);

    BackendService* m_service;
    QLineEdit*    m_socketEdit = nullptr;
    QLineEdit*    m_binaryEdit = nullptr;
    QCheckBox*    m_autoStart  = nullptr;
    QButtonGroup* m_verbosity  = nullptr;
    QButtonGroup* m_accent     = nullptr;
    QCheckBox*    m_ledPulse   = nullptr;
    QButtonGroup* m_accelGroup  = nullptr;   // backend radios (auto/cpu/vulkan/cuda)
    QVBoxLayout*  m_accelList   = nullptr;   // container repopulated on detection
    QLabel*       m_accelStatus = nullptr;   // "GPU available: …" / "CPU only"
};
