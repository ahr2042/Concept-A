#include "stdafx.h"
#include "gui.h"



gui::gui(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
}

gui::~gui()
{}

e_ErrorState gui::f_e_updateDevicesCombobox()
{
    ui.m_cmbBx_devicesList->clear();
    char* v_pc_devicesNames = new char[MAX_PARAMETER_SIZE];
    API_f_v_getDevicesNames(v_pc_devicesNames);
    QString v_o_deviesNames(v_pc_devicesNames);
    
    delete[] v_pc_devicesNames;
    v_pc_devicesNames = nullptr;

    if (v_o_deviesNames.isEmpty())
    {
        return MF_NO_VIDEO_DEVICE_ERR;
    }
    QStringList v_o_namesList = v_o_deviesNames.split(";");
    ui.m_cmbBx_devicesList->addItems(v_o_namesList);       
    for (QString v_o_device : v_o_namesList)
    {        
        f_v_updateLogWithNewInfo(v_o_device);
    }            
}

int gui::f_i_getDeviceIdFromCombobox()
{    
    m_i_deviceId = ui.m_cmbBx_devicesList->currentIndex();
    return m_i_deviceId;
}

e_ErrorState gui::f_e_startStreaming()
{
    e_ErrorState v_e_errorState = NO_ERR;
    if (m_i_deviceId < 0)
    {
        f_i_getDeviceIdFromCombobox();
        if (m_i_deviceId < 0)
        {
            return INVALID_DEVICE_ID_ERR;
        }
    }
    v_e_errorState = (e_ErrorState)API_f_i_setDevice(m_i_deviceId);
    if (v_e_errorState != NO_ERR)
    {
        // update log error
        return v_e_errorState;
    }
    v_e_errorState = (e_ErrorState)API_f_i_startStreaming();
    return v_e_errorState;
}

e_ErrorState gui::f_e_stopStreaming()
{
    return (e_ErrorState)API_f_i_stopStreaming();
}



void gui::f_v_updateLogWithNewInfo(QString i_o_newInfo)
{
    QString v_o_Log = ui.m_lbl_log->text() + i_o_newInfo + "\n";
    ui.m_lbl_log->setText(v_o_Log);
}

QString gui::f_o_interpreteError(e_ErrorState i_e_errorId)
{
    char* v_pc_devicesNames = new char[MAX_PARAMETER_SIZE];
    API_f_v_interpreteError((int)i_e_errorId, v_pc_devicesNames);
    QString v_o_deviesNames(v_pc_devicesNames);

    delete[] v_pc_devicesNames;
    v_pc_devicesNames = nullptr;

    return v_o_deviesNames;
}



void gui::f_v_handleButtonPress()
{
    QString v_o_buttonName = QObject::sender()->objectName();    
    e_ErrorState v_e_errorState = (e_ErrorState)NO_ERR;
    if (v_o_buttonName == ui.m_btn_init->objectName())
    {        
        API_f_v_create();
        v_e_errorState = (e_ErrorState)API_f_i_init();
        if (v_e_errorState == NO_ERR)
        {            
            v_e_errorState = f_e_updateDevicesCombobox();
        }
    }
    else if (v_o_buttonName == ui.m_btn_start->objectName())
    {
        v_e_errorState = f_e_startStreaming();
    }
    else if (v_o_buttonName == ui.m_btn_stop->objectName())
    {
        v_e_errorState = f_e_stopStreaming();
    }
    else if (v_o_buttonName == ui.m_cmbBx_devicesList->objectName())
    {
        f_i_getDeviceIdFromCombobox();        
    }


    if (v_e_errorState != NO_ERR)
    {
        f_v_updateLogWithNewInfo(f_o_interpreteError(v_e_errorState));
    }
}


