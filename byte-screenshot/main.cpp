#include <QApplication>
#include "MainWindow.h"


int main(int argc, char* argv[]) {
	QApplication app(argc, argv);
  /*  testUIAOnce();*/
	MainWindow w;
	w.show();

	return app.exec();
}

