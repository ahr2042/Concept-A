#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_gui.h"

#include "errorState.h"
#include "getterLib.h"



class gui : public QMainWindow
{
    Q_OBJECT

public:
    gui(QWidget *parent = nullptr);
    ~gui();

private slots:
    void f_v_handleButtonPress();

private:
    Ui::guiClass ui;
    int m_i_deviceId = -1;

    e_ErrorState f_e_updateDevicesCombobox();
    int f_i_getDeviceIdFromCombobox();
    e_ErrorState f_e_startStreaming();
    e_ErrorState f_e_stopStreaming();
    void f_v_updateLogWithNewInfo(QString i_o_newInfo);
    QString f_o_interpreteError(e_ErrorState i_e_errorId);

};
