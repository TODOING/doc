#include <windows.h>
#include "..\Win64\gdal1.11.0_x64\include\gdal_priv.h"


// TODO: �ڴ˴����ó�����Ҫ������ͷ�ļ�
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
		//smartlog << "ԭʼ���ݼ�pSrcDSΪNULL!";
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

	// ����FFT����ļ�
	GDALDriver *pDriver = (GDALDriver *)GDALGetDriverByName("ENVI");
	GDALDataset *pDstDS = pDriver->Create(pszOutputFileName, srcSizeX, srcSizeY, 2 * bandNums, GDT_Float32, NULL);
	if (!pDstDS)
	{
		//smartlog << "����������ݼ�pDstDSΪNULL!";
		return FALSE;
	}

	// ����FFT����ļ��ռ�ͶӰ��Ϣ
	double adfGeoTransform[6] = { 0 };
	pSrcDS->GetGeoTransform(adfGeoTransform);
	pDstDS->SetGeoTransform(adfGeoTransform);
	pDstDS->SetProjection(pSrcDS->GetProjectionRef());

	// ���������
	__int64 totalSize = imgSize*bandNums * 3;// ��3���׶Σ��б任���б任�����Ļ���

	DWORD imgBytes = imgSize * sizeof(float);
	DWORD rowBytes = srcSizeX * sizeof(float);
	DWORD colBytes = srcSizeY * sizeof(float);

	// ������ʱ�ļ��������ڴ�ӳ���ļ�
	HANDLE hTmpFile = CreateFile("D:\\Data\\TEMP\\big\\fft_tmp.dat",/*�ļ��ľ���·��*/
		GENERIC_READ | GENERIC_WRITE,/*�ļ��Ĳ�����ʽ*/
		0,/*����ģʽ, 0-�����̶�ռ*/
		NULL,/*��ʾ�ļ�����İ�ȫ���ԣ��ýṹ�������ã�NULL-��ʾ���ܼ̳б����*/
		CREATE_ALWAYS,/*��ʾ����ģʽ*/
		FILE_ATTRIBUTE_NORMAL,/*FILE_FLAG_NO_BUFFERING,��ʾ�ļ����Ժ��ļ���־*/
		NULL/*��ʾ������ģ���ļ�*/);
	if (hTmpFile == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	// ����ָ����С���ļ�
	//long lDistance = imgSize*4*10;   
	//SetFilePointer (hTmpFile, lDistance, NULL, FILE_BEGIN) ;// С�ڵ���4G��Сʱʹ��
	//SetEndOfFile(hTmpFile);//�������SetEndOfFile������SetFilePointer���޸Ĳ���Ч  
	//CloseHandle(hFile); 

	// ����һ��ָ����С���ļ�
	LARGE_INTEGER liDistance;
	liDistance.QuadPart = 2 * imgBytes;
	SetFilePointerEx(hTmpFile, liDistance, NULL, FILE_BEGIN);
	SetEndOfFile(hTmpFile);//�������SetEndOfFile������SetFilePointer���޸Ĳ���Ч

	HANDLE hTmpMap = CreateFileMapping(hTmpFile,/*�����ļ����*/
		0/*��ȫ����*/,
		PAGE_READWRITE/*��������*/,
		0/*��λ�ļ���С������С��4G���ļ�����Ϊ0*/,
		0/*imgBytes*2*//*��λ�ļ���С*/,
		0/*�����ڴ�����*/);//ָ����С���ļ�����0��
	if (hTmpMap == NULL)
	{
		return FALSE;
	}

	// �õ�ϵͳ�ķ������ȣ�һ��Ϊ64KB
	/*SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	DWORD dwGran = sysInfo.dwAllocationGranularity;*/

	// �õ��ļ��ߴ�
	DWORD dwFileSizeHigh;
	__int64 qwFileSize = GetFileSize(hTmpFile, &dwFileSizeHigh);
	qwFileSize |= (((__int64)dwFileSizeHigh) << 32);
	CloseHandle(hTmpFile);

	// ƫ�Ƶ�ַ
	__int64 qwFileOffset = 0;
	//__int64 dwBlockBytes = imgBytes * 2;
	PFLOAT lpbMapAddress = (PFLOAT)MapViewOfFile(hTmpMap,/*һ���򿪵�ӳ���ļ�����ľ��*/
		FILE_MAP_ALL_ACCESS,/*ָ�������ļ���ͼ������*/
		(DWORD)(qwFileOffset >> 32), /*ָ����ʼӳ���ļ�ƫ�����ĸ�λ*/
		(DWORD)(qwFileOffset & 0xFFFFFFFF),/*ָ����ʼӳ���ļ�ƫ�����ĵ�λ*/
		0/*dwBlockBytes*//* ָ����Ҫӳ����ļ����ֽ����������dwNumberOfBytesToMapΪ0��ӳ���������ļ�*/);
	if (lpbMapAddress == NULL)
	{
		//DWORD err = GetLastError();// 8--��ʾ�洢�ռ䲻��
		//goto LARGE_IMAGE_PROCESSING;
		//system("pause");
		return FALSE;
	}

	clock_t start, startAll;

	for (int b = 0; b < bandNums; b++)// �𲨶δ���
	{
		/*if (NULL != info.m_pFun)
			info.m_pFun(float(b) / bandNums, _T("FFT�任-���任�������ڴ����С���"), info.m_pParams);*/
		printf("%.2lf%%\n", float(b) / bandNums*100);

		/////////////////////////////////////////////////////////////////////////////////////////////////
		//!1 FFT�任��ʼ
		int i, j, dims[1];

		// 1.1 �ȶ�ÿһ�����ݽ���һάFFT
		dims[0] = srcSizeX;
		float *tmpRe_Data = NULL, *tmpIm_Data = NULL, *tmpDataBuf = NULL;
		float *Re_Data;
		float *Im_Data;
		GDALRasterBand *pSrcBand = pSrcDS->GetRasterBand(1);
		__int64 nBlockBytes = rowBytes * nBlockHeight * 2;
		__int64 nHalfBlockBytes = rowBytes * nBlockHeight;
		__int64 nBlockSize = nBlockHeight*srcSizeX;
		__int64 nLeftRowsSize = nLeftRow*srcSizeX;
		tmpDataBuf = new float[nBlockSize * 2];// ����ʵ�����鲿
		tmpRe_Data = tmpDataBuf;
		tmpIm_Data = tmpDataBuf + nBlockSize;
		
		start = clock();
		startAll = start;
		for (__int64 block = 0; block < nBlockNums; block++)// �鴦��
		{
			printf("%.2lf%%\n", float(b*imgSize * 3 + block*nBlockSize) / totalSize * 100);

			memset(tmpIm_Data, 0, nHalfBlockBytes);

			pSrcBand->RasterIO(GF_Read, 0, block*nBlockHeight, srcSizeX, nBlockHeight, tmpRe_Data, 
				srcSizeX, nBlockHeight, GDT_Float32, 0, 0);

			for (j = 0; j < nBlockHeight; j++)
			{
				if (fftnf(1, dims, tmpRe_Data + j*srcSizeX, tmpIm_Data + j*srcSizeX, 1, -1))
				{// fftnfִ�гɹ�����0��ִ�д��󷵻�-1
					fft_free();
					printf("�б任ʧ��\n");
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
				{// fftnfִ�гɹ�����0��ִ�д��󷵻�-1
					fft_free();
					printf("�б任ʧ��\n");
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
		printf("�б任ʱ�䣺%d%%\n", clock() - start);

		// 1.2 ����FFT�����϶�ÿһ�н���һάFFT
		float *real = new float[srcSizeY];
		float *imag = new float[srcSizeY];

		dims[0] = srcSizeY;
		start = clock();
		for (i = 0; i < srcSizeX; i++)
		{
			printf("%.2lf%%\n", float((3 * b + 1)*imgSize + i*srcSizeY) / totalSize * 100);

			__int64 indexRe, tmpBlockSize, tmpIndex;
			for (__int64 block = 0; block < nBlockNums; block++)// �鴦��
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
				//smartlog << "����ϵͳ�ڴ治�㣬����FFT�任ʧ�ܡ�\n";
				if (real) delete[]real;
				if (imag) delete[]imag;
				real = NULL;
				imag = NULL;
				fft_free();
				printf("�б任ʧ��\n");
				system("pause");
				return FALSE;
			}
			/*for (j = 0; j < srcSizeY; j++)
			{
				__int64 indexRe = j * srcSizeX * 2 + i;

				lpbMapAddress[indexRe] = real[j];
				lpbMapAddress[indexRe + srcSizeX] = -imag[j];
			}*/

			for (__int64 block = 0; block < nBlockNums; block++)// �鴦��
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
		printf("�б任ʱ�䣺%d%%\n", clock() - start);
		fft_free();
		if (real) delete[]real;
		if (imag) delete[]imag;
		real = NULL;
		imag = NULL;
		//!1 FFT�任����
		/////////////////////////////////////////////////////////////////////////////////////////////////


		/////////////////////////////////////////////////////////////////////////////////////////////////
		//!2 ��FFT�任����������Ļ�����д�����ļ�
		__int64 shiftYBy = (srcSizeY % 2 == 0) ? (srcSizeY / 2) : ((srcSizeY - 1) / 2);
		__int64 shiftXBy = (srcSizeX % 2 == 0) ? (srcSizeX / 2) : ((srcSizeX - 1) / 2);

		float *realTemp = new float[srcSizeX];
		float *imagTemp = new float[srcSizeX];

		// ÿ�����벨�ζ�Ӧ����������Σ��ֱ�Ϊʵ�����鲿
		GDALRasterBand *pDstBandRe = pDstDS->GetRasterBand(2 * b + 1);
		GDALRasterBand *pDstBandIm = pDstDS->GetRasterBand(2 * b + 2);

		// �����������������Ϣ��Real��Imaginary��Power��
		std::string strReal = "FFT Real (";
		std::string strImaginary = "FFT Imaginary (";
		const char * strSrcBandDes = pSrcBand->GetDescription();
		strReal += strSrcBandDes;
		strImaginary += strSrcBandDes;
		strReal += ")";
		strImaginary += ")";
		pDstBandRe->SetDescription(strReal.c_str());
		pDstBandIm->SetDescription(strImaginary.c_str());

		// ���Ļ����Ĵ���
		start = clock();
		for (i = 0; i < srcSizeY; i++)
		{
			printf("%.2lf%%\n", float((3 * b + 2)*imgSize + i*srcSizeX) / totalSize * 100);

			__int64 ii = (i + shiftYBy) % srcSizeY;// ii������
			if (ii < 0)
				ii = srcSizeY + ii;

			for (j = 0; j < srcSizeX; j++)
			{
				__int64 jj = (j + shiftXBy) % srcSizeX;// jj ������
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
		for (__int64 block = 0; block < nBlockNums; block++)// �鴦��
		{
			tmpBlockSize = 2 * block*nBlockHeight;
			for (i = 0; i < nBlockHeight; i++)
			{
				__int64 ii = (tmpBlockSize + i + shiftYBy) % srcSizeY;// ii������
				if (ii < 0)
					ii = srcSizeY + ii;

				for (j = 0; j < srcSizeX; j++)
				{
					__int64 jj = (j + shiftXBy) % srcSizeX;// jj ������
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
				__int64 ii = (tmpBlockSize + i + shiftYBy) % srcSizeY;// ii������
				if (ii < 0)
					ii = srcSizeY + ii;

				for (j = 0; j < srcSizeX; j++)
				{
					__int64 jj = (j + shiftXBy) % srcSizeX;// jj ������
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

		printf("���Ļ�ʱ�䣺%d%%\n", clock() - start);
		UnmapViewOfFile(lpbMapAddress);
		CloseHandle(hTmpMap);

		// ÿ������һ�����Σ��ͷ���Ӧ�ڴ���Դ
		if (realTemp) delete[]realTemp;
		if (imagTemp) delete[]imagTemp;
		realTemp = NULL;
		imagTemp = NULL;
		//!2
		/////////////////////////////////////////////////////////////////////////////////////////////////		
	}// end for ����ѭ��
	remove("D:\\Data\\TEMP\\big\\fft_tmp.dat");
	printf("%d%%\n\n", 100);
	printf("�ܱ任ʱ�䣺%d%%\n", clock() - startAll);

	GDALClose((GDALDatasetH)pSrcDS);
	GDALClose((GDALDatasetH)pDstDS);

	system("pause");
	return 0;
}