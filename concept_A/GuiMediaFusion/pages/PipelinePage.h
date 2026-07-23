#pragma once

// Pipeline Editor — design screen "VISION_OS / Pipeline Editor".
// The backend supports exactly one linear topology today (camera → optional
// OpenCV chain → app/screen sink), so the canvas renders that chain as three
// fixed nodes with live connection curves; selecting a node shows its
// properties in the right panel. DEPLOY PIPELINE runs the real
// create → set-device → algos → start sequence; free node graphs stay PLANNED.

#include "../core/BackendService.h"

#include <QWidget>

class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QStackedWidget;

class NodeCanvas;

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
    QComboBox*      m_deviceBox  = nullptr;
    QComboBox*      m_capsBox    = nullptr;
    QComboBox*      m_modelBox   = nullptr;
    QList<QCheckBox*> m_algoBoxes;
    QRadioButton*   m_sinkApp    = nullptr;
    QRadioButton*   m_sinkScreen = nullptr;
    QPushButton*    m_deployBtn  = nullptr;
    QPushButton*    m_haltBtn    = nullptr;
    QLabel*         m_statusChip = nullptr;

    QVector<DeviceInfo> m_devices;
};
