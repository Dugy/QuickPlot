#include <fstream>
#include <sstream>
#include <iostream>
#include <QFont>
#include "quickplot.h"
#include "ui_quickplot.h"
#include "qcustomplot.h"

constexpr double LOG_IDENTIFYING_THRESHOLD = 10;
constexpr double POINTS_RELATIVE_THRESHOLD = 0.02;
constexpr int POINTS_MIN_THRESHOLD = 10;
constexpr int POINTS_MAX_THRESHOLD = 1000;

QuickPlot::QuickPlot(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::QuickPlot)
{
	ui->setupUi(this);
	connect(ui->actionExit, &QAction::triggered, this, &QuickPlot::on_exitButton_clicked);
}

QuickPlot::~QuickPlot()
{
	delete ui;
}

void QuickPlot::showEvent(QShowEvent* event) {
	int windowSize = 1;
	while (windowSize * windowSize < int(_graphs.size()))
		windowSize++;

	for (int i = 0; i < int(_graphs.size()); i++) {
		GraphData& data = _graphs[size_t(i)];
		QCustomPlot* plot = new QCustomPlot(this);
		plot->plotLayout()->insertRow(0);
		plot->plotLayout()->addElement(0, 0, new QCPTextElement(plot, QString::fromStdString(data.name), QFont("sans", 12, QFont::Bold)));
		ui->layout->addWidget(plot, i / windowSize, i % windowSize);
		plot->setInteraction(QCP::iRangeDrag, true);
		plot->setInteraction(QCP::iRangeZoom, true);
		if(_logarithmic) {
			plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
			QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
			plot->yAxis->setTicker(logTicker);
		}

		for (unsigned int i = 0; i < data.lines.size(); i++) {
			plot->addGraph();
			QCPGraph* graph = plot->graph(int(i));
			graph->setPen(QColor::fromHsv(int(360.0 / data.lines.size()) * int(i), 204, 255));
			int validPoints = data.lines[i].validPoints;
			if (validPoints < POINTS_MIN_THRESHOLD || (validPoints < POINTS_MAX_THRESHOLD && validPoints < _maxValidPoints * POINTS_RELATIVE_THRESHOLD)) {
				graph->setLineStyle(QCPGraph::lsNone);
				graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlus, 8));
			}
		}

		if (_named) {
			plot->xAxis->setLabel(QString::fromStdString(data.xName));
			for (int i = 0; i < int(data.lines.size()); i++) {
				plot->graph(i)->setName(QString::fromStdString(data.lines[size_t(i)].name));
			}
			plot->legend->setVisible(true);
		}

		for (unsigned int i = 0; i < data.lines.size(); i++) {
			QCPGraph* graph = plot->graph(int(i));
			for (unsigned int j = 0; j < data.valuesX.size(); j++) {
				double value = data.lines[i].valuesY[j];
				if (value == value) // NaN means missing value
					graph->addData(data.valuesX[j], value);
			}
		}
		plot->rescaleAxes();
		plot->replot();
	}
	QWidget::showEvent(event);
}

void QuickPlot::addFile(std::ifstream file, const std::string& fileName) {
	std::string firstLine;
	std::getline(file, firstLine, '\n');
	char separator = ' ';
	for (char c : firstLine) {
		if (c == '\t') separator = '\t';
		else if (c == ';' && separator != '\t') separator = ';';
		else if (c == ',' && separator != '\t' && separator != ';') separator = ',';
	}

	unsigned int position = 0;
	auto readOne = [&] (const std::string& source) -> std::string {
		std::string assembled;
		while (source[position] != separator && source[position] != '\n' && source[position] != '\r' && source[position]) {
			assembled.push_back(source[position]);
			position++;
		}
		while (source[position] == separator || source[position] == '\r')
			position++;
		return assembled;
	};

	auto isDescription = [] (const std::string& maybe) {
		auto canBeNumber = [] (char letter) {
			if (letter >= '0' && letter <= '9') return true;
			if (letter == '-' || letter == 'e' || letter == 'E' || letter == '+' || letter == ' ' || letter == '.' || letter == ',') return true;
			return false;
		};
		if (maybe.empty()) return false;
		int decimalPoints = 0;
		for (char c : maybe) {
			if (!canBeNumber(c)) return true;
			if (c == '.' || c == ',') {
				decimalPoints++;
				if (decimalPoints > 1)
					return true;
			}
		}

		return false;
	};
	GraphData graph;
	graph.name = fileName;
	while (firstLine[position] && firstLine[position] != '\n') {
		std::string part = readOne(firstLine);
		if (isDescription(part)) {
			_named = true;
		}
		graph.lines.emplace_back();
	}

	position = 0;
	if (_named) {
		graph.xName = readOne(firstLine);
		while (firstLine[position] && firstLine[position] != '\n') {
			graph.lines.back().name = readOne(firstLine);
		}
	}

	auto readNumber = [] (const std::string& asString) {
		if (asString.empty() || (asString[0] >= 'a' && asString[0] <= 'z') || (asString[0] >= 'A' && asString[0] <= 'Z'))
			return std::numeric_limits<double>::quiet_NaN();

		std::stringstream sstream(asString);
		double number;
		sstream >> number;
		return number;
	};

	std::vector<double> numbers;
	double sum = 0;
	auto addLine = [&] (const std::string& line) {
		position = 0;
		graph.valuesX.push_back(readNumber(readOne(line)));
		for (GraphLine& series : graph.lines) {
			double got = readNumber(readOne(line));
			series.valuesY.push_back(got);
			if (got == got) { // If a valid number
				numbers.push_back(got);
				sum += got;
				series.validPoints++;
			}
		}
	};

	std::string line;
	if (_named) {
		addLine(firstLine);
	}
	while (std::getline(file, line, '\n')) {
		addLine(line);
	}

	if (!numbers.empty()) {
		std::sort(numbers.begin(), numbers.end());
		double average = sum / numbers.size();
		double median = numbers[numbers.size() / 2];
		if (average > median * LOG_IDENTIFYING_THRESHOLD)
			_logarithmic = true;

		for (auto& it : graph.lines)
			_maxValidPoints = std::max(_maxValidPoints, it.validPoints);
	}

	_graphs.push_back(std::move(graph));
}

void QuickPlot::on_exitButton_clicked()
{
	QApplication::quit();
}
