#pragma once

// Pipeline Editor — design screen "VISION_OS / Pipeline Editor".
// The backend supports exactly one linear topology today (camera → optional
// OpenCV chain → app/screen sink), so the canvas renders that chain as three
// fixed nodes with live connection curves; selecting a node shows its
// properties in the right panel. DEPLOY PIPELINE runs the real
// create → set-device → algos → start sequence; free node graphs stay PLANNED.

#include "../core/BackendService.h"
#include "../widgets/ChainEditor.h"

#include <QWidget>

class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSlider;
class QStackedWidget;

class NodeCanvas;

namespace vos { class ParamPanel; class ToggleSwitch; }

class PipelinePage : public QWidget
{
    Q_OBJECT
public:
    explicit PipelinePage(BackendService* service, QWidget* parent = nullptr);

private slots:
    void onDevices(const QVector<DeviceInfo>& devices);
    void onAlgorithms(const QStringList& algos);
    void onModels(const QVector<DetectorModel>& models);
    void onDeploy();
    void publishChain();
    void showSource(const BackendService::DeploySpec& cfg);
    void refreshAccelToggle();
    void onDetectorSettingChanged();
    void onHalt();
    void onSessionStarted(int sessionId, const QString& socket, const QString& desc);
    void onSessionStopped(int sessionId);
    void onSessionFailed(int sessionId, const QString& error);

private:
    QWidget* buildPropertiesPanel();
    void showNodeProps(int node);

    BackendService* m_service;
    NodeCanvas*     m_canvas = nullptr;
    int             m_sessionId = -1;

    QStackedWidget* m_propsStack = nullptr;
    // Read-only now: the source is chosen in the rail, and the node shows what
    // that resolved to.
    QLabel*         m_deviceLabel = nullptr;
    QLabel*         m_capsLabel   = nullptr;
    QComboBox*      m_modelBox    = nullptr;
    // The processing chain, in the one widget that owns it console-wide.
    vos::ChainEditor* m_chain = nullptr;

    // Detector thresholds and the pipeline-level acceleration choice, both moved
    // here from the Dashboard: they configure a deploy rather than watch one.
    QSlider*           m_confSlider  = nullptr;
    QLabel*            m_confLabel   = nullptr;
    QLabel*            m_accelHw     = nullptr;
    vos::ToggleSwitch* m_accelToggle = nullptr;
    QRadioButton*   m_sinkApp    = nullptr;
    QRadioButton*   m_sinkScreen = nullptr;
    QPushButton*    m_deployBtn  = nullptr;
    QPushButton*    m_haltBtn    = nullptr;
    QLabel*         m_statusChip = nullptr;

    QVector<DeviceInfo> m_devices;
};
