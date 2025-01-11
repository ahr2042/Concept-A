#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_GuiMediaFusion.h"


class GuiMediaFusion : public QMainWindow
{
    Q_OBJECT

public:
    GuiMediaFusion(QWidget *parent = nullptr);
    ~GuiMediaFusion();    
    

public slots:
    void handleUserInput();

signals:
    void guiRequest(QString);

private:
    Ui::GuiMediaFusionClass ui;
};
