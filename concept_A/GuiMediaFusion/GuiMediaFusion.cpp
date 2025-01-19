#include "stdafx.h"
#include "GuiMediaFusion.h"

GuiMediaFusion::GuiMediaFusion(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
}

GuiMediaFusion::~GuiMediaFusion()
{}

errorStateGui GuiMediaFusion::setCombobox(COMBOBOXES comboboxName, QStringList contentsList)
{    
    QComboBox* selectedCombobox = nullptr;
    switch (comboboxName)
    {
    case SOURCES:
        selectedCombobox = ui.sourcesCombobox;
        break;
    case SOURCE_CAPS:
        selectedCombobox = ui.sourceCapsComboBox;
        break;
    case SINKS:
        selectedCombobox = ui.sinksComboBox;
        break;
    case SINK_CAPS:
        selectedCombobox = ui.sinkCapsComboBox;
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

errorStateGui GuiMediaFusion::getCombobox(COMBOBOXES comboboxName, int& selectedIndex)
{
    switch (comboboxName)
    {
    case SOURCES:
        selectedIndex = ui.sourcesCombobox->currentIndex();
        break;
    case SOURCE_CAPS:
        selectedIndex = ui.sourceCapsComboBox->currentIndex();
        break;
    case SINKS:
        selectedIndex = ui.sinksComboBox->currentIndex();
        break;
    case SINK_CAPS:
        selectedIndex = ui.sinkCapsComboBox->currentIndex();
        break;
    default:
        return errorStateGui::GET_COMBOBOX_ERR;
        break;
    }    
    return errorStateGui::NO_ERR;
}


void GuiMediaFusion::handleUserInput()
{
    QString buttonName = QObject::sender()->objectName();
    if (buttonName.endsWith("Combobox",Qt::CaseInsensitive))
    {
        emit guiManagementViewRequest(buttonName);
    }
}