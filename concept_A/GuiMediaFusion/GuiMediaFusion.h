#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_GuiMediaFusion.h"
#include "errorStateGui.h"
#include "guiElements.h"

class GuiMediaFusion : public QMainWindow
{
    Q_OBJECT

public:
    GuiMediaFusion(QWidget *parent = nullptr);
    ~GuiMediaFusion();    
    
    errorStateGui setCombobox(GUI_ELEMENTS, QStringList);
    errorStateGui getCombobox(GUI_ELEMENTS, int&);
    void updateLog(QString);

public slots:
    void handleUserInput();

signals:
    void viewClassRequest(GUI_ELEMENTS);
    

private:
    Ui::PreLaunchSettingsClass ui;
    QString darkModeStyleSheet;
    QString lightModeStyleSheet;
};
