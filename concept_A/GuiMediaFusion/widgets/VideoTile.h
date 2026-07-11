#pragma once

// VideoTile — the design's video viewport: a StreamReceiver wrapped in command-
// center chrome (REC dot + timecode, source chip, FPS badge) with a painted
// NO_SIGNAL placeholder when no stream is bound. Used 1× on the Dashboard and
// 4× on the Multi-Grid page.

#include <QElapsedTimer>
#include <QFrame>
#include <QLabel>

#include <string>

class StreamReceiver;
namespace vos { class LedDot; class Badge; }

class VideoTile : public QFrame
{
    Q_OBJECT
public:
    explicit VideoTile(const QString& slotName, QWidget* parent = nullptr);

    // Bind to a backend stream socket (from BackendService::sessionStarted).
    bool bind(const std::string& videoSocket, const QString& sourceLabel);
    void unbind();
    bool active() const;

    StreamReceiver* receiver() const { return m_receiver; }
    QString sourceLabel() const      { return m_sourceLabel; }

signals:
    void statsTick(double fps, double mbps, QSize res, quint64 totalFrames);
    void streamError(const QString& message);

protected:
    void timerEvent(QTimerEvent*) override;

private:
    QWidget* buildPlaceholder();
    QWidget* buildChrome();

    StreamReceiver* m_receiver    = nullptr;
    QWidget*        m_placeholder = nullptr;
    QWidget*        m_chromeTop   = nullptr;

    QLabel*      m_recLabel   = nullptr;
    QLabel*      m_srcLabel   = nullptr;
    QLabel*      m_fpsLabel   = nullptr;
    QLabel*      m_resLabel   = nullptr;
    vos::LedDot* m_recDot     = nullptr;
    QLabel*      m_slotLabel  = nullptr;

    QString       m_slotName;
    QString       m_sourceLabel;
    QElapsedTimer m_elapsed;
    int           m_clockTimer = 0;
};
