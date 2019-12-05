#pragma once
#include <string>
#include <fstream>

class Option
{
public:
	//根据网格路径和option文件的名字来获得相应的路径名字
	Option(std::string, std::string);

	//输出目录
	std::string ModelName;
	std::string ModelPath;

	std::string OutputDir = "../Output/";
	std::string SampleMethod = "Dijkstra";
	std::string SampleFirstPoint = "Random";
	std::string IntermediateResultOutput = "Yes";
	std::string VertexPriorityMetric = "RealDis";
	int Dn = 10;
	double Alpha = 0.1;
	double DecayRate = 0.9;
	std::string BanAreaMethod = "NonConnect";
	int FixThreshold = 20;
	int GAPForbiddenRadius = 5;
	double GAPFilteringRate = 0.01;
	int GAPParrCount = 6;
	int AAPParrCount = 6;
	double AAPTrimmingRate = 0.01;
	int AAPMaxAddingCount = 100;

	double filtering_rate;
	double trimming_rate;
};
