#pragma once
#include <QMainWindow>
#include <QPushButton>
#include <QListWidget>
#include <QProgressBar>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    
private slots:
    void onCompress();
    void onExtract();
    void onList();
    
private:
    void setupUI();
    void connectSignals();
    
    QPushButton* compressBtn;
    QPushButton* extractBtn;
    QPushButton* listBtn;
    QListWidget* fileList;
    QProgressBar* progress;
    QLabel* statusLabel;
    QComboBox* modeCombo;
    QLineEdit* passwordInput;
    QCheckBox* passwordCheck;
};
