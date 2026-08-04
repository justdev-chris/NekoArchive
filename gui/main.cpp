#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    app.setApplicationName("NekoArchive");
    app.setOrganizationName("NekoArchive");
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
