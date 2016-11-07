#include <windows.h>
#include "..\Win64\gdal1.11.0_x64\include\gdal_priv.h"


// TODO: 在此处引用程序需要的其他头文件
#include <tchar.h>
#include <WinBase.h>

#include "fftn.h"
#include <time.h>

int main()
{
	GDALAllRegister();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

	//char *pszInputFileName = "D:\\Data\\zy-3\\zy339-8-tlc.tif"; GF1_PMS1_E111.7_N28.6_20131119_L1A0000114370 - PAN1.tiff
	//char *pszInputFileName = "D:\\Data\\GF1_PMS1_E111.7_N28.6_20131119_L1A0000114370.tar\\GF1_PMS1_E111.7_N28.6_20131119_L1A0000114370-PAN1.tiff";
	char *pszInputFileName = "D:\\Data\\zy-3\\zy3_37362_28972.dat";
	char *pszOutputFileName = "D:\\Data\\TEMP\\big\\zy3_fft.dat";


	GDALDataset *pSrcDS = (GDALDataset *)GDALOpen(pszInputFileName, GA_ReadOnly);
	if (!pSrcDS)
	{
		//smartlog << "原始数据集pSrcDS为NULL!";
		return FALSE;
	}

	int bandNums = 1;
	__int64 srcSizeX = pSrcDS->GetRasterXSize();
	__int64 srcSizeY = pSrcDS->GetRasterYSize();
	__int64 imgSize = srcSizeX*srcSizeY;
	//GDALDataType srcDataType = pSrcDS->GetRasterBand(1)->GetRasterDataType();

	__int64 nBlockHeight = (1024 * 1024 * 4) / (srcSizeX * 4);// 4M
	__int64 nBlockNums = srcSizeY / nBlockHeight;
	__int64 nLeftRow = srcSizeY - nBlockHeight*nBlockNums;

	// 创建FFT结果文件
	GDALDriver *pDriver = (GDALDriver *)GDALGetDriverByName("ENVI");
	GDALDataset *pDstDS = pDriver->Create(pszOutputFileName, srcSizeX, srcSizeY, 2 * bandNums, GDT_Float32, NULL);
	if (!pDstDS)
	{
		//smartlog << "创建结果数据集pDstDS为NULL!";
		return FALSE;
	}

	// 设置FFT结果文件空间投影信息
	double adfGeoTransform[6] = { 0 };
	pSrcDS->GetGeoTransform(adfGeoTransform);
	pDstDS->SetGeoTransform(adfGeoTransform);
	pDstDS->SetProjection(pSrcDS->GetProjectionRef());

	// 进度条相关
	__int64 totalSize = imgSize*bandNums * 3;// 分3个阶段：行变换，列变换，中心化。

	DWORD imgBytes = imgSize * sizeof(float);
	DWORD rowBytes = srcSizeX * sizeof(float);
	DWORD colBytes = srcSizeY * sizeof(float);

	// 创建临时文件，用于内存映射文件
	HANDLE hTmpFile = CreateFile("D:\\Data\\TEMP\\big\\fft_tmp.dat",/*文件的绝对路径*/
		GENERIC_READ | GENERIC_WRITE,/*文件的操作方式*/
		0,/*共享模式, 0-本进程独占*/
		NULL,/*表示文件句柄的安全属性，该结构并不常用，NULL-表示不能继承本句柄*/
		CREATE_ALWAYS,/*表示操作模式*/
		FILE_ATTRIBUTE_NORMAL,/*FILE_FLAG_NO_BUFFERING,表示文件属性和文件标志*/
		NULL/*表示不适用模板文件*/);
	if (hTmpFile == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	// 创建指定大小的文件
	//long lDistance = imgSize*4*10;   
	//SetFilePointer (hTmpFile, lDistance, NULL, FILE_BEGIN) ;// 小于等于4G大小时使用
	//SetEndOfFile(hTmpFile);//必须调用SetEndOfFile，否则SetFilePointer的修改不生效  
	//CloseHandle(hFile); 

	// 创建一个指定大小的文件
	LARGE_INTEGER liDistance;
	liDistance.QuadPart = 2 * imgBytes;
	SetFilePointerEx(hTmpFile, liDistance, NULL, FILE_BEGIN);
	SetEndOfFile(hTmpFile);//必须调用SetEndOfFile，否则SetFilePointer的修改不生效

	HANDLE hTmpMap = CreateFileMapping(hTmpFile,/*物理文件句柄*/
		0/*安全设置*/,
		PAGE_READWRITE/*保护设置*/,
		0/*高位文件大小，对于小于4G的文件设置为0*/,
		0/*imgBytes*2*//*低位文件大小*/,
		0/*共享内存名称*/);//指定大小，文件将以0填
	if (hTmpMap == NULL)
	{
		return FALSE;
	}

	// 得到系统的分配粒度，一般为64KB
	/*SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	DWORD dwGran = sysInfo.dwAllocationGranularity;*/

	// 得到文件尺寸
	DWORD dwFileSizeHigh;
	__int64 qwFileSize = GetFileSize(hTmpFile, &dwFileSizeHigh);
	qwFileSize |= (((__int64)dwFileSizeHigh) << 32);
	CloseHandle(hTmpFile);

	// 偏移地址
	__int64 qwFileOffset = 0;
	//__int64 dwBlockBytes = imgBytes * 2;
	PFLOAT lpbMapAddress = (PFLOAT)MapViewOfFile(hTmpMap,/*一个打开的映射文件对象的句柄*/
		FILE_MAP_ALL_ACCESS,/*指定访问文件视图的类型*/
		(DWORD)(qwFileOffset >> 32), /*指定开始映射文件偏移量的高位*/
		(DWORD)(qwFileOffset & 0xFFFFFFFF),/*指定开始映射文件偏移量的低位*/
		0/*dwBlockBytes*//* 指定需要映射的文件的字节数量，如果dwNumberOfBytesToMap为0，映射整个的文件*/);
	if (lpbMapAddress == NULL)
	{
		//DWORD err = GetLastError();// 8--表示存储空间不足
		//goto LARGE_IMAGE_PROCESSING;
		//system("pause");
		return FALSE;
	}

	clock_t start, startAll;

	for (int b = 0; b < bandNums; b++)// 逐波段处理
	{
		/*if (NULL != info.m_pFun)
			info.m_pFun(float(b) / bandNums, _T("FFT变换-正变换功能正在处理中……"), info.m_pParams);*/
		printf("%.2lf%%\n", float(b) / bandNums*100);

		/////////////////////////////////////////////////////////////////////////////////////////////////
		//!1 FFT变换开始
		int i, j, dims[1];

		// 1.1 先对每一行数据进行一维FFT
		dims[0] = srcSizeX;
		float *tmpRe_Data = NULL, *tmpIm_Data = NULL, *tmpDataBuf = NULL;
		float *Re_Data;
		float *Im_Data;
		GDALRasterBand *pSrcBand = pSrcDS->GetRasterBand(1);
		__int64 nBlockBytes = rowBytes * nBlockHeight * 2;
		__int64 nHalfBlockBytes = rowBytes * nBlockHeight;
		__int64 nBlockSize = nBlockHeight*srcSizeX;
		__int64 nLeftRowsSize = nLeftRow*srcSizeX;
		tmpDataBuf = new float[nBlockSize * 2];// 分配实部和虚部
		tmpRe_Data = tmpDataBuf;
		tmpIm_Data = tmpDataBuf + nBlockSize;
		
		start = clock();
		startAll = start;
		for (__int64 block = 0; block < nBlockNums; block++)// 块处理
		{
			printf("%.2lf%%\n", float(b*imgSize * 3 + block*nBlockSize) / totalSize * 100);

			memset(tmpIm_Data, 0, nHalfBlockBytes);

			pSrcBand->RasterIO(GF_Read, 0, block*nBlockHeight, srcSizeX, nBlockHeight, tmpRe_Data, 
				srcSizeX, nBlockHeight, GDT_Float32, 0, 0);

			for (j = 0; j < nBlockHeight; j++)
			{
				if (fftnf(1, dims, tmpRe_Data + j*srcSizeX, tmpIm_Data + j*srcSizeX, 1, -1))
				{// fftnf执行成功返回0，执行错误返回-1
					fft_free();
					printf("行变换失败\n");
					system("pause");
					return FALSE;
				}
			}
			memcpy(lpbMapAddress + 2*block*nBlockHeight, tmpDataBuf, nBlockBytes);
		}
		if (tmpDataBuf)
		{
			delete[]tmpDataBuf;
			tmpDataBuf = NULL;
		}
		if (nLeftRow)
		{
			tmpDataBuf = new float[nLeftRow*srcSizeX*2];
			tmpRe_Data = tmpDataBuf;
			tmpIm_Data = tmpDataBuf + nLeftRow*srcSizeX;
			memset(tmpIm_Data, 0, nLeftRow*srcSizeX*sizeof(float));

			pSrcBand->RasterIO(GF_Read, 0, nBlockNums*nBlockHeight, srcSizeX, nLeftRow, tmpRe_Data, 
				srcSizeX, nLeftRow, GDT_Float32, 0, 0);
			
			for (j = 0; j < nLeftRow; j++)
			{
				if (fftnf(1, dims, tmpRe_Data + j*srcSizeX, tmpIm_Data + j*srcSizeX, 1, -1))
				{// fftnf执行成功返回0，执行错误返回-1
					fft_free();
					printf("行变换失败\n");
					system("pause");
					return FALSE;
				}
			}
			memcpy(lpbMapAddress + 2 * nBlockNums*nBlockHeight, tmpDataBuf, 2 * nLeftRowsSize*sizeof(float));
		}
		if (tmpDataBuf)
		{
			delete[]tmpDataBuf;
			tmpDataBuf = NULL;
		}
		printf("%.2lf%%\n", float((3 * b + 1)*imgSize) / totalSize * 100);
		printf("行变换时间：%d%%\n", clock() - start);

		// 1.2 在行FFT基础上对每一列进行一维FFT
		float *real = new float[srcSizeY];
		float *imag = new float[srcSizeY];

		dims[0] = srcSizeY;
		start = clock();
		for (i = 0; i < srcSizeX; i++)
		{
			printf("%.2lf%%\n", float((3 * b + 1)*imgSize + i*srcSizeY) / totalSize * 100);

			__int64 indexRe, tmpBlockSize, tmpIndex;
			for (__int64 block = 0; block < nBlockNums; block++)// 块处理
			{
				tmpBlockSize = 2 * block*nBlockHeight;
				for (j = 0; j < nBlockHeight; j++)
				{
					indexRe = tmpBlockSize + j * srcSizeX + i;
					tmpIndex = block*nBlockHeight + j;
					real[tmpIndex] = lpbMapAddress[indexRe];
					imag[tmpIndex] = lpbMapAddress[indexRe + nBlockSize];
				}
			}
			if (nLeftRow)
			{
				tmpBlockSize = 2*nBlockNums*nBlockHeight;
				for (j = 0; j < nLeftRow; j++)
				{
					indexRe = tmpBlockSize + j * srcSizeX + i;
					tmpIndex = nBlockNums*nBlockHeight + j;
					real[tmpIndex] = lpbMapAddress[indexRe];
					imag[tmpIndex] = lpbMapAddress[indexRe + nLeftRowsSize];
				}
			}

			/*for (j = 0; j < srcSizeY; j++)
			{
				__int64 indexRe = j * srcSizeX * 2 + i;

				real[j] = lpbMapAddress[indexRe];
				imag[j] = lpbMapAddress[indexRe + srcSizeX];
			}*/

			if (fftnf(1, dims, real, imag, 1, -1))
			{
				//smartlog << "可能系统内存不足，处理FFT变换失败。\n";
				if (real) delete[]real;
				if (imag) delete[]imag;
				real = NULL;
				imag = NULL;
				fft_free();
				printf("列变换失败\n");
				system("pause");
				return FALSE;
			}
			/*for (j = 0; j < srcSizeY; j++)
			{
				__int64 indexRe = j * srcSizeX * 2 + i;

				lpbMapAddress[indexRe] = real[j];
				lpbMapAddress[indexRe + srcSizeX] = -imag[j];
			}*/

			for (__int64 block = 0; block < nBlockNums; block++)// 块处理
			{
				tmpBlockSize = 2 * block*nBlockHeight;
				for (j = 0; j < nBlockHeight; j++)
				{
					indexRe = tmpBlockSize + j * srcSizeX + i;
					tmpIndex = block*nBlockHeight + j;

					lpbMapAddress[indexRe] = real[tmpIndex];
					lpbMapAddress[indexRe + nBlockSize] = -imag[tmpIndex];
				}
			}
			if (nLeftRow)
			{
				tmpBlockSize = 2 * nBlockNums*nBlockHeight;
				for (j = 0; j < nLeftRow; j++)
				{
					indexRe = tmpBlockSize + j * srcSizeX + i;
					tmpIndex = nBlockNums*nBlockHeight + j;

					lpbMapAddress[indexRe] = real[tmpIndex];
					lpbMapAddress[indexRe + nLeftRowsSize] = -imag[tmpIndex];
				}
			}
		}
		printf("列变换时间：%d%%\n", clock() - start);
		fft_free();
		if (real) delete[]real;
		if (imag) delete[]imag;
		real = NULL;
		imag = NULL;
		//!1 FFT变换结束
		/////////////////////////////////////////////////////////////////////////////////////////////////


		/////////////////////////////////////////////////////////////////////////////////////////////////
		//!2 对FFT变换结果进行中心化，并写入结果文件
		__int64 shiftYBy = (srcSizeY % 2 == 0) ? (srcSizeY / 2) : ((srcSizeY - 1) / 2);
		__int64 shiftXBy = (srcSizeX % 2 == 0) ? (srcSizeX / 2) : ((srcSizeX - 1) / 2);

		float *realTemp = new float[srcSizeX];
		float *imagTemp = new float[srcSizeX];

		// 每个输入波段对应二个输出波段，分别为实部和虚部
		GDALRasterBand *pDstBandRe = pDstDS->GetRasterBand(2 * b + 1);
		GDALRasterBand *pDstBandIm = pDstDS->GetRasterBand(2 * b + 2);

		// 设置输出波段描述信息，Real，Imaginary（Power）
		std::string strReal = "FFT Real (";
		std::string strImaginary = "FFT Imaginary (";
		const char * strSrcBandDes = pSrcBand->GetDescription();
		strReal += strSrcBandDes;
		strImaginary += strSrcBandDes;
		strReal += ")";
		strImaginary += ")";
		pDstBandRe->SetDescription(strReal.c_str());
		pDstBandIm->SetDescription(strImaginary.c_str());

		// 中心化核心代码
		start = clock();
		for (i = 0; i < srcSizeY; i++)
		{
			printf("%.2lf%%\n", float((3 * b + 2)*imgSize + i*srcSizeX) / totalSize * 100);

			__int64 ii = (i + shiftYBy) % srcSizeY;// ii代表行
			if (ii < 0)
				ii = srcSizeY + ii;

			for (j = 0; j < srcSizeX; j++)
			{
				__int64 jj = (j + shiftXBy) % srcSizeX;// jj 代表列
				if (jj < 0)
					jj = srcSizeX + jj;

				__int64 indexIn = i * srcSizeX * 2 + j;
				realTemp[jj] = lpbMapAddress[indexIn];
				imagTemp[jj] = lpbMapAddress[indexIn + srcSizeX];
			}

			pDstBandRe->RasterIO(GF_Write, 0, ii, srcSizeX, 1, realTemp, srcSizeX, 1, GDT_Float32, 0, 0);
			pDstBandIm->RasterIO(GF_Write, 0, ii, srcSizeX, 1, imagTemp, srcSizeX, 1, GDT_Float32, 0, 0);
		}

		__int64 tmpBlockSize, tmpIndex;
		for (__int64 block = 0; block < nBlockNums; block++)// 块处理
		{
			tmpBlockSize = 2 * block*nBlockHeight;
			for (i = 0; i < nBlockHeight; i++)
			{
				__int64 ii = (tmpBlockSize + i + shiftYBy) % srcSizeY;// ii代表行
				if (ii < 0)
					ii = srcSizeY + ii;

				for (j = 0; j < srcSizeX; j++)
				{
					__int64 jj = (j + shiftXBy) % srcSizeX;// jj 代表列
					if (jj < 0)
						jj = srcSizeX + jj;

					__int64 indexIn = tmpBlockSize + i * srcSizeX + j;
					realTemp[jj] = lpbMapAddress[indexIn];
					imagTemp[jj] = lpbMapAddress[indexIn + nBlockSize];
				}
				pDstBandRe->RasterIO(GF_Write, 0, ii, srcSizeX, 1, realTemp, srcSizeX, 1, GDT_Float32, 0, 0);
				pDstBandIm->RasterIO(GF_Write, 0, ii, srcSizeX, 1, imagTemp, srcSizeX, 1, GDT_Float32, 0, 0);
			}
		}
		if (tmpDataBuf)
		{
			delete[]tmpDataBuf;
			tmpDataBuf = NULL;
		}
		if (nLeftRow)
		{
			tmpBlockSize = 2 * nBlockNums*nBlockHeight;
			for (j = 0; j < nLeftRow; j++)
			{
				__int64 ii = (tmpBlockSize + i + shiftYBy) % srcSizeY;// ii代表行
				if (ii < 0)
					ii = srcSizeY + ii;

				for (j = 0; j < srcSizeX; j++)
				{
					__int64 jj = (j + shiftXBy) % srcSizeX;// jj 代表列
					if (jj < 0)
						jj = srcSizeX + jj;

					__int64 indexIn = tmpBlockSize + i * srcSizeX + j;
					realTemp[jj] = lpbMapAddress[indexIn];
					imagTemp[jj] = lpbMapAddress[indexIn + nBlockSize];
				}
				pDstBandRe->RasterIO(GF_Write, 0, ii, srcSizeX, 1, realTemp, srcSizeX, 1, GDT_Float32, 0, 0);
				pDstBandIm->RasterIO(GF_Write, 0, ii, srcSizeX, 1, imagTemp, srcSizeX, 1, GDT_Float32, 0, 0);
			}
		}

		printf("中心化时间：%d%%\n", clock() - start);
		UnmapViewOfFile(lpbMapAddress);
		CloseHandle(hTmpMap);

		// 每处理完一个波段，释放相应内存资源
		if (realTemp) delete[]realTemp;
		if (imagTemp) delete[]imagTemp;
		realTemp = NULL;
		imagTemp = NULL;
		//!2
		/////////////////////////////////////////////////////////////////////////////////////////////////		
	}// end for 波段循环
	remove("D:\\Data\\TEMP\\big\\fft_tmp.dat");
	printf("%d%%\n\n", 100);
	printf("总变换时间：%d%%\n", clock() - startAll);

	GDALClose((GDALDatasetH)pSrcDS);
	GDALClose((GDALDatasetH)pDstDS);

	system("pause");
	return 0;
}