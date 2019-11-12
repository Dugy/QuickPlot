#ifndef QUICKPLOT_H
#define QUICKPLOT_H

#include <QMainWindow>
#include <string>
#include <vector>

QT_BEGIN_NAMESPACE
namespace Ui { class QuickPlot; }
QT_END_NAMESPACE

class QuickPlot : public QMainWindow
{
	Q_OBJECT

public:
	QuickPlot(QWidget *parent = nullptr);
	~QuickPlot();
	void addFile(const std::string& fileName);
	void showEvent(QShowEvent* event);

private slots:
	void on_exitButton_clicked();

private:
	Ui::QuickPlot *ui;

	struct GraphLine {
		std::string name;
		std::vector<double> valuesY;
		int validPoints = 0;
	};

	struct GraphData {
		std::string name;
		std::string xName;
		std::vector<double> valuesX;
		std::vector<GraphLine> lines;
	};

	std::vector<GraphData> _graphs;
	bool _named = false;
	bool _logarithmic = false;
	int _maxValidPoints = 0;
};
#endif // QUICKPLOT_H
