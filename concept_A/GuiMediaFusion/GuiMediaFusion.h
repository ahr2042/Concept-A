#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_GuiMediaFusion.h"
#include "errorStateGui.h"
#include "comboboxes.h"

class GuiMediaFusion : public QMainWindow
{
    Q_OBJECT

public:
    GuiMediaFusion(QWidget *parent = nullptr);
    ~GuiMediaFusion();    
    
    errorStateGui setCombobox(COMBOBOXES, QStringList);

public slots:
    void handleUserInput();

signals:
    void streamViewRequest(QString);
    void guiManagementViewRequest(QString);
    void processingViewRequest(QString);

private:
    Ui::GuiMediaFusionClass ui;
};
