#include "stdafx.h"
#include "GuiMediaFusion.h"

GuiMediaFusion::GuiMediaFusion(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
}

GuiMediaFusion::~GuiMediaFusion()
{}

errorStateGui GuiMediaFusion::setSourcesCombobox(QStringList sourcesList)
{
    errorStateGui errorState = NO_ERR;    
    ui.sourcesCombobox;
    return errorStateGui();
}

void GuiMediaFusion::handleUserInput()
{
    QString buttonName = QObject::sender()->objectName();
    if (buttonName == "sourcesCombobox")
    {

    }
}