#include <xdc/std.h>
#include "main.h"
#include "syslink_common.h"
#include "audioDetectFrequency_rx.h"
#include "audio_queue.h"

extern Short working_mode;

#define SAMPLE_SIZE		1200
#define SEND_SIZE		240
#define ORDER_IF		64
#define ORDER_LF		32
#define ORDER_3K		32


#define TH1	1.5f
#define TH2 1.65f

float tempIn_1[244] = {0};
float tempOut_1[244] = {0};

float th1_data, th2_data=0.91;
float P_rrcpf[36];  //GSW 用来存放帧头

float dcValue  =0;
float RSSI= 0;
float E_noise = 0;
float deDcBufOut[SAMPLE_SIZE] = {0};
float ifBufOut[SAMPLE_SIZE] = {0};
float lfBufOut[SEND_SIZE] = {0};		//lao
float deEmBufOut[SEND_SIZE] = {0};
float reg_len[SAMPLE_SIZE+ORDER_IF] = { 0 };		//中频滤波buf
float reg_test[SEND_SIZE+32] = { 0 };


/* 函数说明：在寻找P帧之前，匹配滤波
 *           20 Order
 *
 */
void rrcFilter_Before_Psyn(float *inBuf,float *outBuf,short index,unsigned short start) // GSW17/12/22
{
	register short i = 0;
	unsigned short j = 0;
    static float temp[960]={0};  //定义一个中间变量数组, 这里应该是383
	float  coef0= 0.000000000000000, coef1= 0.000049374050532, coef2 = 0.000731373552360, coef3= 0.003301607415752,
		   coef4= 0.009578085548176, coef5= 0.021321677321237, coef6 = 0.039068021702553, coef7= 0.061044810757830,
		   coef8= 0.083002555065670, coef9= 0.099385268589141, coef10= 0.105464790894703;

	for(i=0;i<10;i++)
		temp[i] = temp[index+i];  //把临时数组的后20位赋值到前20位
	for(i=0;i<index;i++)
		temp[i+10] = inBuf[i];  //从临时数组的第20位开始，把输入数组的360位全部赋值到里面
	for(i = 0;i<36;i++)        //产生36个数据，放在outbuf里面
	{
		j = 10*i + start;     //此处为起始位置
		outBuf[i]   = temp[j+10]*coef10
				    + (temp[j+11]+temp[j+9]) *coef9  + (temp[j+12]+temp[j+8])*coef8  + (temp[j+13]+temp[j+7])*coef7
				    + (temp[j+14]+temp[j+6])*coef6   + (temp[j+15]+temp[j+5])*coef5  + (temp[j+16]+temp[j+4])*coef4
				    + (temp[j+17]+temp[j+3])*coef3   + (temp[j+18]+temp[j+2])*coef2  + (temp[j+19]+temp[j+1])*coef1
				    + (temp[j+20]+temp[j+0])*coef0;
	}
}

/*
 * 功能：计算直流分量
 */
float calculateDcValue(unsigned short *dataP)
{
	int i = 0;
	float dcVal = 0;

	for(i = 0; i < SAMPLE_SIZE; i++)
	{
		dcVal += *dataP++;
	}

	return dcVal/SAMPLE_SIZE;
}
/********************************************** 鍙橀噺瀹氫箟 **********************************************/
/*
 * 功能：计算载波幅度(从语言接收循环队列中取出10ms数据，去直流，并计算幅值)
 * 返回值： 10ms数据近似幅值(10ms SAMPLE_SIZE个数据)
 */

float deDc(unsigned short* ad_data,float* deDcBufOut)
{
	Int i = 0;

	register Int temp = 0;
	register Float squareCal = 0; //骞虫柟绉垎
//鍙栦箣鍓嶇殑鏈�悗64涓暟鎹ˉ鍒板墠64浣�
	for(i = 0; i < ORDER_IF; ++i)
		reg_len[i] = reg_len[SAMPLE_SIZE + i];
	//浠�audioQueue涓彇鍑�0ms鏁版嵁锛屽苟鍘荤洿娴佸垎閲忥紝鍚屾椂10ms鏁版嵁鐨勫箙鍊煎钩鏂�
	//鍙�200涓暟鎹埌32~1231
	for(i = 0; i < SAMPLE_SIZE; ++i)
	{
		temp = *ad_data++;
		reg_len[i+ORDER_IF] = temp - dcValue;
		squareCal += reg_len[i+ORDER_IF] * reg_len[i+ORDER_IF];
	}
	return 0;
}

float ScaleCal(float* reg_len)
{
	Int i = 0;
	register Float squareCal = 0; //平方积分

	//从 audioQueue中取出10ms数据，并去直流分量，同时10ms数据的幅值平方
	//取1200个数据到32~1231
	for(i = 0; i < SAMPLE_SIZE; ++i)
		squareCal += reg_len[i+32] * reg_len[i+32];

	squareCal = squareCal * 2 / SAMPLE_SIZE;

	return 1/squareCal;
}
//=====================================================================================================
/*
 * 功能：中频带通滤波器
 */
//注意：在进行卷积前，需要对原数据去直流分量
//总共1264个数据，拷贝SAMPLE_SIZE个数据到reg后1200位，前64位为0；后64位浪费，----应将后64位续接到后一序列reg的前64位，
//中频带通滤波
#if 0
void IF_Filter(float* deDcBufOut,float* ifBufOut)
{
	register short i = 0;

	float coffe0=0.210031529431718,	coffe1=-0.000000000000000, 	coffe2=-0.194460944270102,	coffe3= 0.000000000000000,
	coffe4=0.152085284332050,		coffe5=-0.000000000000000,	coffe6=-0.094416947881908, 	coffe7=0.000000000000000,
	coffe8=0.036237969414126,  	coffe9= 0.000000000000002,	coffe10=0.009218305454725, 	coffe11=-0.000000000000001,
	coffe12=-0.034203417428227, 		coffe13=-0.000000000000001,	coffe14=0.038147914559184,	coffe15=0.000000000000001,
	coffe16=-0.026706102147916, 		coffe17=-0.000000000000001, coffe18=0.008793352791185,  coffe19=-0.000000000000001,
	coffe20=0.007037018500247, 	coffe21=0.000000000000001, 	coffe22=-0.015434542897116,  coffe23=-0.000000000000000 ,
	coffe24=0.015365105757340 , 		coffe25=-0.000000000000000,	coffe26=-0.009374216116240,	coffe27= 0.000000000000000,
	coffe28=0.001639546004580, 		coffe29=-0.000000000000000,	coffe30=0.004162330393914,  coffe31=-0.000000000000000, coffe32=-0.006257822882328;

	for(i = 0; i < SAMPLE_SIZE; ++i){
		ifBufOut[i] = reg_len[i+32]*coffe0
				+(reg_len[i+33]+reg_len[i+31])*coffe1  + (reg_len[i+34]+reg_len[i+30])*coffe2  + (reg_len[i+35]+reg_len[i+29])*coffe3  + (reg_len[i+36]+reg_len[i+28])*coffe4
				+(reg_len[i+37]+reg_len[i+27])*coffe5  + (reg_len[i+38]+reg_len[i+26])*coffe6  + (reg_len[i+39]+reg_len[i+25])*coffe7  + (reg_len[i+40]+reg_len[i+24])*coffe8
				+(reg_len[i+41]+reg_len[i+23])*coffe9  + (reg_len[i+42]+reg_len[i+22])*coffe10 + (reg_len[i+43]+reg_len[i+21])*coffe11 + (reg_len[i+44]+reg_len[i+20])*coffe12
				+(reg_len[i+45]+reg_len[i+19])*coffe13 + (reg_len[i+46]+reg_len[i+18])*coffe14 + (reg_len[i+47]+reg_len[i+17])*coffe15 + (reg_len[i+48]+reg_len[i+16])*coffe16
				+(reg_len[i+49]+reg_len[i+15])*coffe17 + (reg_len[i+50]+reg_len[i+14])*coffe18 + (reg_len[i+51]+reg_len[i+13])*coffe19 + (reg_len[i+52]+reg_len[i+12])*coffe20
				+(reg_len[i+53]+reg_len[i+11])*coffe21 + (reg_len[i+54]+reg_len[i+10])*coffe22 + (reg_len[i+55]+reg_len[i+9])*coffe23  + (reg_len[i+56]+reg_len[i+8])*coffe24
				+(reg_len[i+57]+reg_len[i+7])*coffe25  + (reg_len[i+58]+reg_len[i+6])*coffe26  + (reg_len[i+59]+reg_len[i+5])*coffe27  + (reg_len[i+60]+reg_len[i+4])*coffe28
				+(reg_len[i+61]+reg_len[i+3])*coffe29  + (reg_len[i+62]+reg_len[i+2])*coffe30  + (reg_len[i+63]+reg_len[i+1])*coffe31  + (reg_len[i+64]+reg_len[i])*coffe32;
	}
}
#else
void IF_Filter(float* deDcBufOut,float* ifBufOut)
{
	register short i = 0;
	float coffe0=0.210031529431718, 	coffe2=-0.194460944270102,	coffe4=0.152085284332050,	coffe6=-0.094416947881908,
	coffe8=0.036237969414126,  	coffe10=0.009218305454725, 	coffe12=-0.034203417428227, 		coffe14=0.038147914559184,
	coffe16=-0.026706102147916,  coffe18=0.008793352791185, 	coffe20=0.007037018500247, 	coffe22=-0.015434542897116,
	coffe24=0.015365105757340 , 	coffe26=-0.009374216116240,	coffe28=0.001639546004580,
	coffe30=0.004162330393914,  coffe32=-0.006257822882328;

	//
	for(i = 0; i < SAMPLE_SIZE; i++){
		//result = 0;
		ifBufOut[i] = reg_len[i+32]*coffe0
				 + (reg_len[i+34]+reg_len[i+30])*coffe2  +  (reg_len[i+36]+reg_len[i+28])*coffe4
				 + (reg_len[i+38]+reg_len[i+26])*coffe6  + (reg_len[i+40]+reg_len[i+24])*coffe8
				 + (reg_len[i+42]+reg_len[i+22])*coffe10 + (reg_len[i+44]+reg_len[i+20])*coffe12
				 + (reg_len[i+46]+reg_len[i+18])*coffe14 + (reg_len[i+48]+reg_len[i+16])*coffe16
				 + (reg_len[i+50]+reg_len[i+14])*coffe18 + (reg_len[i+52]+reg_len[i+12])*coffe20
				 + (reg_len[i+54]+reg_len[i+10])*coffe22 + (reg_len[i+56]+reg_len[i+8])*coffe24
				 + (reg_len[i+58]+reg_len[i+6])*coffe26  + (reg_len[i+60]+reg_len[i+4])*coffe28
				 + (reg_len[i+62]+reg_len[i+2])*coffe30  + (reg_len[i+64]+reg_len[i])*coffe32;
	}
}
#endif

void detectFreq(float *inBuf,float *outBuf)
{
	short i = 0;
	static float temp[1207] = {0};
	float reverseRSSI = 1/RSSI;
	for(i=0;i<7;i++)
		temp[i] = temp[SAMPLE_SIZE+i];
	for(i=0;i<SAMPLE_SIZE;i++)
		temp[i+7] = inBuf[i];

	for(i=0;i<SEND_SIZE;i++)
	{
		reverseRSSI = temp[3+5*i]*temp[3+5*i] + (0.0625*(9*(temp[4+5*i]-temp[2+5*i])+temp[6+5*i]-temp[i*5])) * (0.0625*(9*(temp[4+5*i]-temp[2+5*i])+temp[6+5*i]-temp[i*5]));
		reverseRSSI *=2;
		if(reverseRSSI<0.000001&&reverseRSSI>-0.000001)
			continue;
		outBuf[i] = (temp[3+5*i] * temp[4+5*i] + 0.00390625 * (9*temp[4+5*i] - 9*temp[2+5*i] + temp[6+5*i] - temp[5*i]) * (9*temp[5+5*i] - 9*temp[3+5*i] + temp[7+5*i] - temp[1+5*i]))/reverseRSSI;
		if(outBuf[i]>0.18 )
			outBuf[i] = 0.18;
		else if(outBuf[i]<-0.18)
			outBuf[i] = -0.18;
	}
}

void ddetectFreq(float *inBuf,float *outBuf)
{
	short i = 0;
	static float temp[1207] = {0};
	float reverseRSSI = 1/RSSI;
	for(i=0;i<7;i++)
		temp[i] = temp[SAMPLE_SIZE+i];
	for(i=0;i<SAMPLE_SIZE;i++)
		temp[i+7] = inBuf[i];

	for(i=0;i<SEND_SIZE;i++)
	{
		reverseRSSI = temp[3+5*i]*temp[3+5*i] + (0.0625*(9*(temp[4+5*i]-temp[2+5*i])+temp[6+5*i]-temp[i*5])) * (0.0625*(9*(temp[4+5*i]-temp[2+5*i])+temp[6+5*i]-temp[i*5]));
		if(reverseRSSI<0.000001&&reverseRSSI>-0.000001)
			continue;
		outBuf[i] = (temp[3+5*i] * temp[4+5*i] + 0.00390625 * (9*temp[4+5*i] - 9*temp[2+5*i] + temp[6+5*i] - temp[5*i]) * (9*temp[5+5*i] - 9*temp[3+5*i] + temp[7+5*i] - temp[1+5*i]))/reverseRSSI;
		if(outBuf[i]>0.3 )
			outBuf[i] = 0.3;
		else if(outBuf[i]<-0.3)
			outBuf[i] = -0.3;
	}
}

void detectFreqEnhance(float *inBuf,float *outBuf)
{
	short i = 0;
	static float temp[1203] = {0};
	float m0,m1,m2,m3,m4;
	float rssi0,rssi1,rssi2,rssi3,rssi4;

	for(i=0;i<3;i++)
		temp[i] = temp[SAMPLE_SIZE+i];
	for(i=0;i<SAMPLE_SIZE;i++)
		temp[i+3] = inBuf[i];

	for(i=0;i<SEND_SIZE;i++)
	{
		m0 = temp[5*i+1]*temp[5*i+2] + 0.25*(temp[5*i+2]-temp[5*i])*(temp[5*i+3]-temp[5*i+1]);
		m1 = temp[5*i+2]*temp[5*i+3] + 0.25*(temp[5*i+3]-temp[5*i+1])*(temp[5*i+4]-temp[5*i+2]);
		m2 = temp[5*i+3]*temp[5*i+4] + 0.25*(temp[5*i+4]-temp[5*i+2])*(temp[5*i+5]-temp[5*i+3]);
		m3 = temp[5*i+4]*temp[5*i+5] + 0.25*(temp[5*i+5]-temp[5*i+3])*(temp[5*i+6]-temp[5*i+4]);
		m4 = temp[5*i+5]*temp[5*i+6] + 0.25*(temp[5*i+6]-temp[5*i+4])*(temp[5*i+7]-temp[5*i+5]);

		rssi0 = temp[5*i+1]*temp[5*i+1] + 0.25*(temp[5*i+2]-temp[5*i])*(temp[5*i+2]-temp[5*i]);
		rssi1 = temp[5*i+2]*temp[5*i+2] + 0.25*(temp[5*i+3]-temp[5*i+1])*(temp[5*i+3]-temp[5*i+1]);
		rssi2 = temp[5*i+3]*temp[5*i+3] + 0.25*(temp[5*i+4]-temp[5*i+2])*(temp[5*i+4]-temp[5*i+2]);
		rssi3 = temp[5*i+4]*temp[5*i+4] + 0.25*(temp[5*i+5]-temp[5*i+3])*(temp[5*i+5]-temp[5*i+3]);
		rssi4 = temp[5*i+5]*temp[5*i+5] + 0.25*(temp[5*i+6]-temp[5*i+4])*(temp[5*i+6]-temp[5*i+4]);
		outBuf[i] = (m0+m1+m2+m3+m4)/(rssi0+rssi1+rssi2+rssi3+rssi4)*0.5;
	}
}

void delDcAfterPhaseDetector(float *inBuf,float *outBuf)
{
	short i;
	short len = 240;
	float sum = 0;
	float dcValue1=0;

	for(i = 0; i < len; ++i)		//求1200个数据的和
		sum += inBuf[i];
	dcValue1 = sum/SEND_SIZE;		//求数据平均值
	for(i = 0; i < len; ++i)		//求1200个数据的去直流值
		outBuf[i] = inBuf[i] - dcValue1;
}


void iirFilterAfterDetectFreq(float *inBuf,float *outBuf)
{
	short i = 0;
	static float tempIn[244] = {0};
	static float tempOut[244] = {0};
	for(i=0;i<4;i++)
	{
		tempIn[i] = tempIn[SEND_SIZE+i];
		tempOut[i]= tempOut[SEND_SIZE+i];
	}
	for(i=0;i<SEND_SIZE;i++)
		tempIn[i+4] = inBuf[i];
	for(i=0;i<SEND_SIZE;i++)
		tempOut[i+4] = 0.92862377785650 * (tempIn[4+i] - 2*tempIn[2+i] + tempIn[i]) + 1.85214648539593*tempOut[i+2] - 0.86234862603008*tempOut[i];
	for(i=0;i<SEND_SIZE;i++)
		outBuf[i] = tempOut[i+4];
}


//=====================================================================================================
/*
 * 功能：抽样、去加重
 * y[n]=x[n]+0.91*y[n-1]-dcValue
 *
 */
void deEmphasis(float* lfBufOut ,float* deEmBufOut)
{
	short i=0;
	static float RegDeEm=0;

	//去加重
	for(i=0;i<SEND_SIZE;i++){
		deEmBufOut[i]=lfBufOut[i]+0.91*RegDeEm;
		RegDeEm=deEmBufOut[i];
	}
}
void Fir_3K(float* lfBufOut,float* OutBufOut)
{
	register short i = 0;

	float coffe0=0.267134554588261  , coffe1=0.236643824211526, coffe2=0.157647210529892,coffe3=0.061468973796616,
	coffe4=-0.016754810589437,coffe5=-0.054091931650658,coffe6=-0.048639712874851, coffe7=-0.017244364825457,
	coffe8=0.015651002365617,  coffe9=0.031290227905308,coffe10=0.024836757789552, coffe11=0.004923060761584,
	coffe12=-0.013925270914956, coffe13=-0.020709569041141, coffe14=-0.013658269443140, coffe15=0.000424545729958, coffe16=0.011734619292740;


	for(i = 0; i < SEND_SIZE; ++i){
		reg_test[i+ORDER_3K] = lfBufOut[i];
	}

	//卷积（从reg[0:1199]）
	for(i = 0; i < SEND_SIZE; ++i){
		//result = 0;
		OutBufOut[i] = reg_test[i+16]*coffe0
		+(reg_test[i+17]+reg_test[i+15])*coffe1 + (reg_test[i+18]+reg_test[i+14])*coffe2  + (reg_test[i+19]+reg_test[i+13])*coffe3  + (reg_test[i+20]+reg_test[i+12])*coffe4
		+(reg_test[i+21]+reg_test[i+11])*coffe5 + (reg_test[i+22]+reg_test[i+10])*coffe6  + (reg_test[i+23]+reg_test[i+9])*coffe7   + (reg_test[i+24]+reg_test[i+8])*coffe8
		+(reg_test[i+25]+reg_test[i+7])*coffe9  + (reg_test[i+26]+reg_test[i+6])*coffe10  + (reg_test[i+27]+reg_test[i+5])*coffe11  + (reg_test[i+28]+reg_test[i+4])*coffe12
		+(reg_test[i+29]+reg_test[i+3])*coffe13 + (reg_test[i+30]+reg_test[i+2])*coffe14  + (reg_test[i+31]+reg_test[i+1])*coffe15  + (reg_test[i+32]+reg_test[i])*coffe16;
	}

	for(i = 0; i < ORDER_3K; ++i)
		reg_test[i] = reg_test[SEND_SIZE + i];


}

float getRSSI(float* lfBufOut)
{
	short i = 0;
	Float squareCal = 0;
	for(i = 0; i < SAMPLE_SIZE; ++i)
		squareCal += lfBufOut[i] * lfBufOut[i];
	return squareCal / SAMPLE_SIZE;
}

float getRssiAvr(float* lfBufOut)
{

	static float arrRSSI[RSSI_AVR] = {0};
	static char count;
	char i=0;
	float sun = 0;

	if(count>=RSSI_AVR)
		count =0;
	arrRSSI[count++%RSSI_AVR] = getRSSI(lfBufOut);
	for(i=0;i<RSSI_AVR;i++)
		sun += arrRSSI[i];
	return sun/RSSI_AVR;
}

void bandstopfilter(float *inBuf,float *outBuf)
{
	short i = 0;
	static float tempIn[242] = {0};
	static float tempOut[242] = {0};
	for(i=0;i<2;i++)
	{
		tempIn[i] = tempIn[SEND_SIZE+i];
		tempOut[i]= tempOut[SEND_SIZE+i];
	}
	for(i=0;i<SEND_SIZE;i++)
		tempIn[i+2] = inBuf[i];
	for(i=0;i<SEND_SIZE;i++)
		tempOut[i+2] = 0.9354535*(tempIn[i+2]-1.7326445*tempIn[i+1]+tempIn[i]) + 1.6208085*tempOut[i+1] - 0.8709071*tempOut[i];
	for(i=0;i<SEND_SIZE;i++)
		outBuf[i] = tempOut[i+2];
}


void bandstopfilter3k(float *inBuf,float *outBuf)
{
	short i = 0;

	for(i=0;i<4;i++)
	{
		tempIn_1[i] = tempIn_1[SEND_SIZE+i];
		tempOut_1[i]= tempOut_1[SEND_SIZE+i];
	}

	for(i=0;i<SEND_SIZE;i++)
		tempIn_1[i+4] = inBuf[i];
	for(i=0;i<SEND_SIZE;i++)
		tempOut_1[i+4] = 0.0742598*(tempIn_1[i+4]-0.5097106*tempIn_1[i+3]+1.4051935*tempIn_1[i+2]-0.5097106*tempIn_1[i+1]+tempIn_1[i]) +
					2.0471970*tempOut_1[i+3]-1.8715882*tempOut_1[i+2]+0.7856784*tempOut_1[i+1]-0.1384506*tempOut_1[i];
	for(i=0;i<SEND_SIZE;i++)
		outBuf[i] = tempOut_1[i+4];
}

void toShort(float *inBuf,short *outBuf)
{
	short i=0;
	for(i=0;i<SEND_SIZE;i++)
	{
		//lfBufOut[i] = lfBufOut[i] + ((lfBufOut[i]*lfBufOut[i]*lfBufOut[i])*0.1666667+((lfBufOut[i]*lfBufOut[i]*lfBufOut[i]*lfBufOut[i]*lfBufOut[i])*0.075));//����   y+1/6*y^3;
		if(inBuf[i]>0.85)
			inBuf[i]=0.85;
		else if(inBuf[i]<-0.85)
			inBuf[i]=-0.85;

		outBuf[i] = (short)(inBuf[i]*0x7fff*1.35);
	}
}


void deDcAfterDetectFreq(float *inBuf,float *outBuf)
{
	short i = 0;
	static float x[242] = {0};
	static float y[242] = {0};

	x[0] = x[240];
	x[1] = x[241];

	y[0] = y[240];
	y[1] = y[241];

	for(i=0;i<SEND_SIZE;i++)
		x[i+2] = inBuf[i];

	for(i=0;i<SEND_SIZE;i++)
		y[i+2] = 0.954774*(x[i+2]-2*x[i+1]+x[i]) + 1.9075016*y[i+1] - 0.9115945*y[i];

	for(i=0;i<SEND_SIZE;i++)
		outBuf[i] = y[i+2];

}

void rx_HPFilter(float *inBuf,float *outBuf)
{
	short i = 0;
	static float x[242] = {0};
	static float y[242] = {0};

	x[0] = x[240];
	x[1] = x[241];

	y[0] = y[240];
	y[1] = y[241];

	for(i=0;i<240;i++)
		x[i+2] = inBuf[i];

	for(i=0;i<240;i++)
		y[i+2] = 0.9816583*(x[i+2]-2*x[i+1]+x[i]) + 1.9629801*y[i+1] - 0.9636530*y[i];

	for(i=0;i<240;i++)
		outBuf[i] = y[i+2];
}


void rx_LPFilter(float *inBuf,float *outBuf)
{
	register short i = 0;
	static float temp[272] = {0};
	float coffe0=0.299496059427060,	coffe1=0.256986632618021, 	coffe2=0.150967710628479,	coffe3= 0.032995877067145,
	coffe4=-0.045687558189818,		coffe5=-0.062214363053361,	coffe6=-0.030565720303875 , 	coffe7=0.012959340065669 ,
	coffe8=0.035504661411791,  	coffe9=  0.026815438823179,	coffe10=0.000482815488849, 	coffe11=-0.020603625992256,
	coffe12=-0.022150036648907, 		coffe13=-0.006883093026563,	coffe14=0.010689881755938,	coffe15=0.017054011686632,
	coffe16= 0.009462869502823;

	for(i = 0; i < 32; ++i)
		temp[i] = temp[240 + i];
	for(i=0; i<240;i++)
		temp[i+32] = inBuf[i];
	//
	for(i = 0; i < 240; ++i){
		outBuf[i] = temp[i+16]*coffe0
		+(temp[i+17]+temp[i+15])*coffe1 + (temp[i+18]+temp[i+14])*coffe2  + (temp[i+19]+temp[i+13])*coffe3  + (temp[i+20]+temp[i+12])*coffe4
		+(temp[i+21]+temp[i+11])*coffe5 + (temp[i+22]+temp[i+10])*coffe6  + (temp[i+23]+temp[i+9])*coffe7   + (temp[i+24]+temp[i+8])*coffe8
		+(temp[i+25]+temp[i+7])*coffe9  + (temp[i+26]+temp[i+6])*coffe10  + (temp[i+27]+temp[i+5])*coffe11  + (temp[i+28]+temp[i+4])*coffe12
		+(temp[i+29]+temp[i+3])*coffe13 + (temp[i+30]+temp[i+2])*coffe14  + (temp[i+31]+temp[i+1])*coffe15  + (temp[i+32]+temp[i])*coffe16;
	}
}

static  float getMatchRate_TH1(float *inBuf,unsigned short start,unsigned short len)
{
	unsigned short i = 0;
	float matchRate = 0.0;
	char syn[36] = {
			 1, -1, -1,  1,  1, -1, -1,  1,  1, -1,  1, -1,
			-1,  1,  1, -1, -1,  1 ,-1 , 1 ,-1,  1, -1,  1,
			-1,  1,  1, -1,  1, -1, -1,  1, -1,  1,  1, -1
	};
	for(i = 0; i < len; ++i)
	{
		matchRate += inBuf[start+(10*i)]*syn[i];
	}
	return matchRate;
}
  //add by GSW
static  float getMatchRate_TH2(float *inBuf,unsigned short start,unsigned short len,short index )
{
	unsigned short i = 0;
	float matchRate = 0.0;
//	float P_rrcpf[36];
	char syn[36] = {
			 1, -1, -1,  1,  1, -1, -1,  1,  1, -1,  1, -1,
			-1,  1,  1, -1, -1,  1 ,-1 , 1 ,-1,  1, -1,  1,
			-1,  1,  1, -1,  1, -1, -1,  1, -1,  1,  1, -1
	};
    //把接收到用于检测帧头的值，全部滤波处理,然后抽样出来36个数据
	rrcFilter_Before_Psyn(inBuf,P_rrcpf,index,start);

	for(i = 0; i < len; i++)
	{
		matchRate += P_rrcpf[i]*syn[i];
	}

	return matchRate;
}


/*
>0 匹配成功,并返回匹配成功其实指针处到末尾的长度（都是有效的数据）
<0 match unsuccess
*/

#if 0
static short matchSYN(float *inBuf,float *outBuf,short len)
{
	float carelessMatchRate=0,carefulMatchRate;
	float maxMatchRate = -1000;
	short i=0,j=0,k=0,p=0;
	char flag = 0;
	short indexTemp = 0;
	static float tempBuf[960] = {0};
	static short index = 0;				//累积三次，找一次syn，syn一次需要360个数据，三次720个数据，相当于syn 2倍
	//short preDataIndex = 0;
	short preLen = 0;

	for(i=0;i<len;i++)
		tempBuf[index++] = inBuf[i];
	if(index >= 720)					//大于720个数据才找syn
	{
		for(i=0;i<=118;i++)																//(720-360)/3
		{//q->buf
			carelessMatchRate = getMatchRate_TH1(tempBuf,3*i,36);	//start position is q->front+3(middle sample : 3,10,17...)
			if(carelessMatchRate >= TH1)									//40 has matched 35
			{
				if(i==0)
					j = 0;
				else if(i==1)
					j = -3;
				else
					j = -6;
				for(;j<=6;j++)															//find max value  6-1-6
				{
					carefulMatchRate = getMatchRate_TH2(tempBuf,3*i+j,36);
					if(carefulMatchRate>=TH2)											//细扫描
					{
						flag = 1;
						if(carefulMatchRate>maxMatchRate)
						{
							maxMatchRate = carefulMatchRate;
						}
						else
							break;
					}
					else if(flag == 1)
					{
						break;
					}
				}
				if(maxMatchRate>TH2)
				{
					for(k=3*i+j-1;k<index;k++)
						outBuf[p++] = 2*tempBuf[k];    //multiply 2
					indexTemp = index;
					index = 0;
					return indexTemp-3*i-j+1;						//match success
				}
			}
		}
		preLen = 6+index-360;//要复制的长度
		index = 0;
		for(i=0;i<preLen;i++)			//保存最后未处理的数据，下一次处理
			tempBuf[index++] = tempBuf[354+i];
	}
			return 0;
}

#else
static short matchSYN(float *inBuf,float *outBuf,short len)
{
	float carelessMatchRate=0,carefulMatchRate;
	int  Fine_Sca_Count = 0;      //add by gsw
	float maxMatchRate = -1000;
	short i=0,j=0,k=0,p=0;
	short indexTemp = 0;
	static float tempBuf[960] = {0};
	static short index = 0;
	short preLen = 0;

	for(i=0;i<len;i++)
		tempBuf[index++] = inBuf[i];	//累积三次，找一次syn，syn一次需要360个数据，三次720个数据，相当于syn 2倍
	if(index >= 720)					//大于720个数据才找syn
	{
		for(i=0;i<=118;i++)								//现在这个118是明白了
		{
			carelessMatchRate = getMatchRate_TH1(tempBuf,3*i,36);
			if(carelessMatchRate >= TH1)
			{
				th1_data=carelessMatchRate;
				if(i==0)
					j = 0;
				else if(i==1)
					j = -3;
				else
					j = -6;
				for(;j<=6;j++)
				{
					carefulMatchRate = getMatchRate_TH2(tempBuf,3*i+j,36,index);
					if(carefulMatchRate>=TH2)
					{
						if(carefulMatchRate>=maxMatchRate)
						{
							maxMatchRate = carefulMatchRate;
							Fine_Sca_Count = 3*i+j;
							th2_data=maxMatchRate;
						}
						else
							continue;
					}
					else
							{
								if(j == 6)
									break;
								else
									continue;
							}
				}
				if(maxMatchRate>=TH2)
				{
					for(k=Fine_Sca_Count;k<index;k++)
						outBuf[p++] = tempBuf[k];   //1205   tempBuf[k]
					indexTemp = index;
					index = 0;
					return indexTemp-Fine_Sca_Count;						//match success
				}
			}
		}
		preLen = 6+index-360;//要复制的长度
		index = 0;
		for(i=0;i<preLen;i++)			//保存最后未处理的数据，下一次处理
			tempBuf[index++] = tempBuf[354+i];
	}
			return 0;
}
#endif


void rrcFilterReceive(float *inBuf,float *outBuf)
{
	register short i = 0;
	unsigned short j = 0;//计算后三个子帧的匹配滤波（288-72）
	float  coef0= 0.000000000000000, coef1= 0.000049374050532, coef2 = 0.000731373552360, coef3= 0.003301607415752,
		   coef4= 0.009578085548176, coef5= 0.021321677321237, coef6 = 0.039068021702553, coef7= 0.061044810757830,
		   coef8= 0.083002555065670, coef9= 0.099385268589141, coef10= 0.105464790894703;
	for(i = 0; i < 252; i++)
		{
			j = 350+10*i;  //550 - 200 = 350
			outBuf[i]   =inBuf[j+10]*coef10
			+(inBuf[j+11]+inBuf[j+9])*coef9 + (inBuf[j+12]+inBuf[j+8])*coef8  + (inBuf[j+13]+inBuf[j+7])*coef7
			+ (inBuf[j+14]+inBuf[j+6])*coef6 + (inBuf[j+15]+inBuf[j+5])*coef5 + (inBuf[j+16]+inBuf[j+4])*coef4
			+ (inBuf[j+17]+inBuf[j+3])*coef3+ (inBuf[j+18]+inBuf[j+2])*coef2 +(inBuf[j+19]+inBuf[j+1])*coef1
			+ (inBuf[j+20]+inBuf[j+0])*coef0;
		}
}

void judgeData(float *inBuf1,float *inBuf2,short *outBuf)
{
	short i=0;
	float average = 0.0;

	for(i=0;i<288;i++)
	{
		outBuf[i] = (inBuf2[i]-average)>0?1:0;
	}
}

/*
function: get 288bit value
return:
0:don't have 288 bit
1:had 288 bit
*/
char getFrame(float *inBuf,short *outBuf)
{
	short i = 0;
	static float tempBuf[3120] = {0};
	static short index = 0;
	float temp[252] = {0};                      //这个数组用来保存252个数据
	float P_Value[288] = {0};
	if(index==0)
	{
		index = matchSYN(inBuf,tempBuf,240);
		return 0;
	}
	else														//等待2880个完整数据（实际为2886，因为滤波原因，最后需要额外添加16个数据）
	{
		for(i=0;i<240;i++)
			tempBuf[index++] = inBuf[i];
		if(index >= 2886)
		{
			rrcFilterReceive(tempBuf,temp);				//产生252个数据
			for(i=0;i<36;i++)
				P_Value[i] = P_rrcpf[i];
			for(i=0;i<252;i++)
				P_Value[i+36] = temp[i];
			judgeData(tempBuf,P_Value,outBuf);			//P帧找判决门限，并把后288个数据转换1，0形式
			matchSYN(&tempBuf[2880],tempBuf,index-2880);//把多余2880的数据提前保存到帧头判断的数组中，用于下一次帧头判断
			index =0;
			return 1;
		}
		return 0;
	}
}

/****************************
***       HPF函数 vs em2  **********
*****************************/
void After_Dete_HPF(float *inBuf,float *outBuf)
{
	register short i = 0;
	static float temp[256] = {0};

	float coffe0=0.646398059180787,coffe1=-0.281133004870598, coffe2=-0.119452946823753,coffe3=0.017614117423981,
		  coffe4=0.060384694619964,coffe5= 0.028855619639291, coffe6=-0.010360344006374,coffe7=-0.025030955237859,coffe8=-0.003770433380231;
	for(i = 0; i < 16; i++)
		temp[i] = temp[240 + i];
	for(i=0; i<240;i++)
		temp[i+16] = inBuf[i];
	for(i = 0; i < 240; i++)
	{
		outBuf[i] = temp[i+8]*coffe0+(temp[i+7]+temp[i+9])*coffe1+(temp[i+6]+temp[i+10])*coffe2
		+(temp[i+5]+temp[i+11])*coffe3 + (temp[i+4]+temp[i+12])*coffe4+(temp[i+3]+temp[i+13])*coffe5
		+(temp[i+2]+temp[i+14])*coffe6 + (temp[i+1]+temp[i+15])*coffe7 + (temp[i+0]+temp[i+16])*coffe8;
	}
}

/*****************************
***   求平方和函数  **********
******************************/
float Em2_Value(float *inBuf)
{
	static float noise_avr[RSSI_AVR];
	int i=0;
	float sum=0;
	float sum_sum=0;

	for(i=0;i<240;i++)
	{
		sum += inBuf[i]*inBuf[i];
	}
	for(i=0; i< RSSI_AVR-1; i++)
		noise_avr[i]=noise_avr[i+1];
	noise_avr[i]=sum;

	for(i=0; i<RSSI_AVR; i++){
		sum_sum +=noise_avr[i];
	}

	return sum_sum/RSSI_AVR;
}


int Rx_process(unsigned short* ad_data,	short* de_data)
{
		dcValue = calculateDcValue(ad_data);
		deDc(ad_data, deDcBufOut);
		IF_Filter(deDcBufOut, ifBufOut);
		RSSI = getRssiAvr(ifBufOut);

		if(working_mode==NORMAL_MODE)
			detectFreq(ifBufOut,lfBufOut);
		else if(working_mode==ENHANCE_MODE)
			detectFreqEnhance(ifBufOut,lfBufOut);
		else if(working_mode==DIGITAL_MODE){
			ddetectFreq(ifBufOut,lfBufOut);
			if(getFrame(lfBufOut, de_data)==1){
				return 1;
			}
			else
				return -1;
		}
		//E_noise
		After_Dete_HPF(lfBufOut, deEmBufOut);
		E_noise=Em2_Value(deEmBufOut);
		//speech filter
		rx_LPFilter(lfBufOut, lfBufOut);
		deEmphasis(lfBufOut ,deEmBufOut);
		rx_HPFilter(deEmBufOut, deEmBufOut);
		toShort(deEmBufOut,de_data);
		return 0;
}
