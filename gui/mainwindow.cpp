#include "mainwindow.h"
#include "nekoarchive/archive.h"
#include "nekoarchive/compressor.h"
#include "nekoarchive/decompressor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QThread>
#include <QFileInfo>
#include <filesystem>

class CompressionWorker : public QThread {
    Q_OBJECT
public:
    std::vector<std::string> files;
    std::string output_path;
    NekoArchive::CompressionMode mode;
    bool result;
    
    void run() override {
        NekoArchive::Archive archive;
        result = archive.create(output_path, files);
    }
};

class ExtractionWorker : public QThread {
    Q_OBJECT
public:
    std::string archive_path;
    std::string output_dir;
    bool result;
    
    void run() override {
        NekoArchive::Archive archive;
        result = archive.extract(archive_path, output_dir);
    }
};

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUI();
    connectSignals();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI() {
    setWindowTitle("🐱 NekoArchive");
    resize(800, 600);
    
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    
    // Toolbar
    QHBoxLayout* toolbar = new QHBoxLayout();
    compressBtn = new QPushButton("🐱 Compress", this);
    extractBtn = new QPushButton("📦 Extract", this);
    listBtn = new QPushButton("📋 List", this);
    toolbar->addWidget(compressBtn);
    toolbar->addWidget(extractBtn);
    toolbar->addWidget(listBtn);
    toolbar->addStretch();
    mainLayout->addLayout(toolbar);
    
    // Mode selection
    QHBoxLayout* modeLayout = new QHBoxLayout();
    modeLayout->addWidget(new QLabel("Mode:", this));
    modeCombo = new QComboBox(this);
    modeCombo->addItem("🐇 Hare (Fast)");
    modeCombo->addItem("🐈 Cat (Balanced)");
    modeCombo->addItem("🐅 Tiger (Max)");
    modeLayout->addWidget(modeCombo);
    
    passwordCheck = new QCheckBox("Password", this);
    modeLayout->addWidget(passwordCheck);
    
    passwordInput = new QLineEdit(this);
    passwordInput->setEchoMode(QLineEdit::Password);
    passwordInput->setEnabled(false);
    modeLayout->addWidget(passwordInput);
    
    modeLayout->addStretch();
    mainLayout->addLayout(modeLayout);
    
    // File list
    fileList = new QListWidget(this);
    mainLayout->addWidget(fileList);
    
    // Progress
    progress = new QProgressBar(this);
    progress->setRange(0, 100);
    progress->setValue(0);
    mainLayout->addWidget(progress);
    
    // Status
    statusLabel = new QLabel("Ready", this);
    mainLayout->addWidget(statusLabel);
}

void MainWindow::connectSignals() {
    connect(compressBtn, &QPushButton::clicked, this, &MainWindow::onCompress);
    connect(extractBtn, &QPushButton::clicked, this, &MainWindow::onExtract);
    connect(listBtn, &QPushButton::clicked, this, &MainWindow::onList);
    connect(passwordCheck, &QCheckBox::toggled, passwordInput, &QLineEdit::setEnabled);
}

void MainWindow::onCompress() {
    QStringList files = QFileDialog::getOpenFileNames(this, "Select files to compress");
    if (files.isEmpty()) return;
    
    QString output = QFileDialog::getSaveFileName(this, "Save archive", "", "NekoArchive (*.nar)");
    if (output.isEmpty()) return;
    
    std::vector<std::string> fileList;
    for (const QString& f : files) {
        fileList.push_back(f.toStdString());
    }
    
    NekoArchive::CompressionMode mode = NekoArchive::CompressionMode::CAT;
    if (modeCombo->currentIndex() == 0) mode = NekoArchive::CompressionMode::HARE;
    else if (modeCombo->currentIndex() == 2) mode = NekoArchive::CompressionMode::TIGER;
    
    statusLabel->setText("Compressing...");
    
    NekoArchive::Archive archive;
    if (archive.create(output.toStdString(), fileList)) {
        statusLabel->setText("✅ Compression complete!");
        QMessageBox::information(this, "Success", "Archive created successfully!");
    } else {
        statusLabel->setText("❌ Compression failed");
        QMessageBox::critical(this, "Error", "Compression failed!");
    }
}

void MainWindow::onExtract() {
    QString archive = QFileDialog::getOpenFileName(this, "Select archive", "", "NekoArchive (*.nar)");
    if (archive.isEmpty()) return;
    
    QString dir = QFileDialog::getExistingDirectory(this, "Select extraction directory");
    if (dir.isEmpty()) return;
    
    statusLabel->setText("Extracting...");
    
    NekoArchive::Archive archiveObj;
    if (archiveObj.extract(archive.toStdString(), dir.toStdString())) {
        statusLabel->setText("✅ Extraction complete!");
        QMessageBox::information(this, "Success", "Files extracted successfully!");
    } else {
        statusLabel->setText("❌ Extraction failed");
        QMessageBox::critical(this, "Error", "Extraction failed!");
    }
}

void MainWindow::onList() {
    QString archive = QFileDialog::getOpenFileName(this, "Select archive", "", "NekoArchive (*.nar)");
    if (archive.isEmpty()) return;
    
    fileList->clear();
    
    NekoArchive::Archive archiveObj;
    std::vector<NekoArchive::FileEntry> entries;
    
    if (archiveObj.list(archive.toStdString(), entries)) {
        for (const auto& entry : entries) {
            QString size = QString::number(entry.original_size);
            QString ratio = QString::number((double)entry.compressed_size / entry.original_size * 100, 'f', 1) + "%";
            fileList->addItem(QString("%1  (%2 bytes, %3 compressed)").arg(entry.name.c_str()).arg(size).arg(ratio));
        }
        statusLabel->setText(QString("📦 %1 files in archive").arg(entries.size()));
    } else {
        statusLabel->setText("❌ Failed to read archive");
        QMessageBox::critical(this, "Error", "Failed to read archive!");
    }
}

#include "mainwindow.moc"
