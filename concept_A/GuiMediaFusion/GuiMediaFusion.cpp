#include "stdafx.h"
#include "GuiMediaFusion.h"

#include "string.h"

GuiMediaFusion::GuiMediaFusion(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    QFile file(":/styleSheets/darkMode.qss");
    if (file.open(QFile::ReadOnly | QIODevice::Text))
    {
        darkModeStyleSheet = QLatin1StringView(file.readAll());
        file.close();
    }    
    
    file.setFileName(":/styleSheets/lightMode.qss");
    if (file.open(QFile::ReadOnly | QIODevice::Text))
    {
        lightModeStyleSheet = QLatin1StringView(file.readAll());
        file.close();
    }

    if (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark)
    {
        this->setStyleSheet(darkModeStyleSheet);
    }
    else
    {
        this->setStyleSheet(lightModeStyleSheet);
    }        
    ui.statusBar->showMessage("Ready");
}

GuiMediaFusion::~GuiMediaFusion()
{}

errorStateGui GuiMediaFusion::setCombobox(GUI_ELEMENTS comboboxName, QStringList contentsList)
{    
    QComboBox* selectedCombobox = nullptr;
    switch (comboboxName)
    {
    case SOURCES:
        selectedCombobox = ui.sourcesCombobox;
        break;
    case SOURCE_CAPS:
        //selectedCombobox = ui.sourceCapsComboBox;
        break;
    case SINKS:
        selectedCombobox = ui.sinksComboBox;
        break;
    case SINK_CAPS:
        //selectedCombobox = ui.sinkCapsComboBox;
        break;
    default:        
        break;
    }
    if (selectedCombobox == nullptr)
    {
        return errorStateGui::SET_COMBOBOX_ERR;
    }
    selectedCombobox->clear();    
    if (contentsList.isEmpty())
    {
        return errorStateGui::NO_ERR;
    }
    selectedCombobox->addItems(contentsList);
    return errorStateGui::NO_ERR;
}

errorStateGui GuiMediaFusion::getCombobox(GUI_ELEMENTS comboboxName, int& selectedIndex)
{
    switch (comboboxName)
    {
    case SOURCES:
        selectedIndex = ui.sourcesCombobox->currentIndex();
        break;
    case SOURCE_CAPS:
        //selectedIndex = ui.sourceCapsComboBox->currentIndex();
        break;
    case SINKS:
        selectedIndex = ui.sinksComboBox->currentIndex();
        break;
    case SINK_CAPS:
        //selectedIndex = ui.sinkCapsComboBox->currentIndex();
        break;
    default:
        return errorStateGui::GET_COMBOBOX_ERR;
        break;
    }    
    return errorStateGui::NO_ERR;
}

void GuiMediaFusion::updateLog(QString logInput)
{
    logInput.trimmed();
    if (!logInput.isEmpty())
    {
        //ui.log->append(logInput);
    }        
}


void GuiMediaFusion::handleUserInput()
{
    QString pressedButtonName = QObject::sender()->objectName();    
    if (pressedButtonName == ui.turnOnPushButton->objectName())
    {
        emit viewClassRequest(GUI_ELEMENTS::TURN_ON);
    }
    //else if (pressedButtonName == ui.initPushButton->objectName())
    //{
    //    emit viewClassRequest(GUI_ELEMENTS::INIT);
    //}
    else if (pressedButtonName == ui.sourcesCombobox->objectName())
    {
        emit viewClassRequest(GUI_ELEMENTS::SOURCES);
    }
    //else if (pressedButtonName == ui.sourceCapsComboBox->objectName())
    //{
    //    emit viewClassRequest(GUI_ELEMENTS::SOURCE_CAPS);
    //}
    else if (pressedButtonName == ui.sinksComboBox->objectName())
    {
        emit viewClassRequest(GUI_ELEMENTS::SINKS);
    }
    //else if (pressedButtonName == ui.sinkCapsComboBox->objectName())
    //{
    //    emit viewClassRequest(GUI_ELEMENTS::SINK_CAPS);
    //}
}