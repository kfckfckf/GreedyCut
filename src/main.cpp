#include <iostream>
#include "Auxiliary/OptionReader.h"
#include "MeshDefinition/MeshDefinition.h"
#include "MeshDefinition/MeshCache.h"
#include "PointSampling/PointSampling.h"
#include "MeshCut/MeshCut.h"
#include "KPNewton/KPNewton.h"
#include "PointFinding/PointFinding.h"
#include "GAP/GAP.h"
//MinGW和MSVC都需要包含这两个
#ifdef _WIN64
#include <direct.h>
#include <io.h>
#endif
//MINGW需要包含这些
#ifdef __MINGW64__
#define OM_STATIC_BUILD 1
#include <OpenMesh/Core/IO/reader/OBJReader.hh>
#include <OpenMesh/Core/IO/writer/OBJWriter.hh>
#endif


//主函数
int main(int argc, char* argv[])
{
	time_t st = clock();
	system("chcp 65001");
#ifdef __MINGW64__
	std::cout << "The Complier is MinGW" << std::endl;	
	OpenMesh::IO::_OBJReader_();
	OpenMesh::IO::_OBJWriter_();
#elif _MSC_VER
	std::cout << "The Complier is MSVC" << std::endl;
#endif // __GNUC

	//网格名称需要单独读取，为了批量化的测试。
	std::string ModelPath = argv[1];
	//读取配置文件，批量化测试的配置应当相同。
	Option opt(argv[2],ModelPath);
	std::cout << "start processing [" << opt.ModelName << "] ......" << std::endl;

	//创建保存运行结果的文件夹
#ifdef _WIN64
	if (_access(opt.OutputDir.c_str(),0)==-1)
		int i = _mkdir(opt.OutputDir.c_str());
#endif // _WIN64

	//读网格，新建备份类
	Mesh mesh;
	OpenMesh::IO::read_mesh(mesh, argv[1]);
	if (mesh.n_vertices() == 0)
	{
		std::cerr << "网格顶点数为0" << std::endl;
		return 0;
	}
	MeshCache MC(mesh);

	std::cout << "Find Cut 1" << std::endl;
	//生成第一条割缝，根据Option中的值来确定
	//三种最远点采样的方法
#ifdef _DEBUG
	MC.updataCapacity();
#endif // DEBUG
	//第一次找点是没有什么约束条件的
	PointSampling ps(MC);
	ps.Set(opt.SampleMethod,opt.SampleFirstPoint,2);//读设置
	std::vector<int> SamplePoints;
	ps.ComputeSamples(SamplePoints);
#ifdef _DEBUG
	MC.updataCapacity();
#endif // DEBUG
	//初始的一刀
	MeshCut MCut(mesh, MC);
	//传入采样点
	MCut.SetCondition(SamplePoints);
	//算Cut
	MCut.Connect();
#ifdef _DEBUG
	MC.updataCapacity();
#endif // DEBUG
	MCut.MakeSeam();
	Mesh cuted_mesh = MCut.GetCutedMesh();

	if (opt.MeshcutOutput == "Yes")
		OpenMesh::IO::write_mesh(cuted_mesh, opt.OutputDir + "\\initial_cut.obj");
	std::cout << "KPNewton1" << std::endl;
	//第一次kpnewton,KPN的部分以后搞懂了之后可以再修改，先保持
	KPNewton kpn(cuted_mesh);
	kpn.Tutte();
	kpn.PrepareDataFree();
	kpn.RunFree(KPNewton::MIPS);
	kpn.RunFree(KPNewton::AMIPS);
	kpn.UpdateMesh();
	kpn.ResultRect(mesh);
	if (kpn.EnergyNan())
	{
		std::cerr <<"fail in KPNewton!" << std::endl;
		return -1;
	}
	if (opt.KPNewtonOutput=="Yes")
		OpenMesh::IO::write_mesh(cuted_mesh, opt.OutputDir + "\\kpn1.obj",OpenMesh::IO::Options::Default,10);
	std::cout << "Kpnewton Finish" << std::endl;

	std::vector<int> firstcutResult;
	PointFinding PF(mesh, cuted_mesh,MC);
	PF.Set(opt.VertexPriorityMetric);
	//找局部最大值点，然后求他们的禁止区域
	PF.FindLocalMaximizer();
	
	MeshCut Mcut2(mesh, MC);
	//准备求禁止区域，参数是第一条割缝，局部最大值点，是否连线
	Mcut2.SetBanCondition(PF.GetLocalMaximizer(), MCut.GetCutvertex(),opt.BanAreaMethod);
	//计算禁止区域，参数是邻域长度，顶点扭曲度量，最大的连通区域面积阈值，邻域衰减系数
	Mcut2.CalcBanArea(opt.Dn,opt.VertexPriorityMetric,opt.Alpha,opt.DecayRate);
	std::vector<int> AllowedArea = Mcut2.GetMaxConnectedRegion();
	std::vector<int> cut2Edges, cut2Vertices;
	//从平滑区域选择第二条cut
	ps.ComputeSamplesFromSelectedRegion(opt.SampleMethod,AllowedArea, cut2Vertices,cut2Edges);
	Mcut2.SetCut(cut2Vertices, cut2Edges);
	Mcut2.MakeSeam();
	Mesh cuted_mesh2 = Mcut2.GetCutedMesh();
	if (opt.MeshcutOutput == "Yes")
		OpenMesh::IO::write_mesh(cuted_mesh2, opt.OutputDir + "\\twice_cut.obj");

	KPNewton kpn2(cuted_mesh2);
	kpn2.Tutte();
	kpn2.PrepareDataFree();
	kpn2.RunFree(KPNewton::MIPS);
	kpn2.RunFree(KPNewton::AMIPS);
	kpn2.UpdateMesh();
	kpn2.ResultRect(mesh);
	if (kpn2.EnergyNan())
	{
		std::cerr << "fail in KPNewton2!" << std::endl;
		return -1;
	}
	if (opt.KPNewtonOutput == "Yes")
		OpenMesh::IO::write_mesh(cuted_mesh2, opt.OutputDir + "\\kpn2.obj", OpenMesh::IO::Options::Default, 10);
	std::cout << "Kpnewton2 Finish" << std::endl;

	PointFinding PF2(mesh, cuted_mesh2, MC);
	PF2.Set(opt.VertexPriorityMetric);
	std::vector<std::pair<int, double>> Res;
	PF2.FindLocalMaximizer();
	std::vector<int> result = PF2.GetLocalMaximizer();
	PF2.Find(Res);

	MC.updataCapacity();

	std::cout << "GAP Stage ......" << std::endl;
	GAP gap(mesh, MC);
	gap.Set(Res,opt.VertexPriorityMetric,20,5,opt.filtering_rate);
	gap.GenFirstCut();
	gap.gradually_addp_pipeline();
	//执行过滤阶段





	time_t et = clock();
	std::cout << et - st << std::endl;










}