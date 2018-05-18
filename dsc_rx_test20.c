#include <xdc/runtime/System.h>
#include <ti/sysbios/hal/Timer.h>
#include <ti/sysbios/knl/clock.h>
#include <ti/sysbios/hal/Hwi.h>
#include "dsc_rx.h"
#include "audio_queue.h"
#include "syslink.h"
#include "main.h"
#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include "syslink_init.h"

UChar dscInformationLen = 0;//定义DX/RX序列长度
int DSC_TEST_CNT[11] = {0};//用来测试，后期直接删掉
static QUEUE_DATA_TYPE   carelessHeadMat[CARELESS_HEAD_LEN] = {
	1,0,1,1,1,1,1,0,0,1,
	1,1,1,1,0,1,1,0,0,1,
	1,0,1,1,1,1,1,0,0,1,
	0,1,1,1,0,1,1,0,1,0, 
};
static QUEUE_DATA_TYPE   dscFrameHeadMat[MATCH_FRAME_HEAD_LEN] = {
		 1,1,1,1,1,1,1,
		 0,0,0,0,0,0,0,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 0,0,0,0,0,0,0,
		 0,0,0,0,0,0,0,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 0,0,0,0,0,0,0,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 0,0,0,0,0,0,0,
		 0,0,0,0,0,0,0,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 0,0,0,0,0,0,0,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 0,0,0,0,0,0,0,
		 0,0,0,0,0,0,0,
		 1,1,1,1,1,1,1,
		 0,0,0,0,0,0,0,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 0,0,0,0,0,0,0,
		 1,1,1,1,1,1,1,
		 1,1,1,1,1,1,1,
		 0,0,0,0,0,0,0,
		 1,1,1,1,1,1,1,
		 0,0,0,0,0,0,0};
static UShort frameBuf[DSC_FRAME_LENGTH] = {0}; //用于存放组帧后的混合字符
static const UChar checkErrorTable[128] = {
	7, 3, 3, 5, 3, 5, 5, 1, 3, 5, 5, 1, 5, 1, 1, 6, 3, 5, 5, 1,
	5, 1, 1, 6, 5, 1, 1, 6, 1, 6, 6, 2, 3, 5, 5, 1, 5, 1, 1, 6,
	5, 1, 1, 6, 1, 6, 6, 2, 5, 1, 1, 6, 1, 6, 6, 2, 1, 6, 6, 2,
	6, 2, 2, 4, 3, 5, 5, 1, 5, 1, 1, 6, 5, 1, 1, 6, 1, 6, 6, 2,
	5, 1, 1, 6, 1, 6, 6, 2, 1, 6, 6, 2, 6, 2, 2, 4, 5, 1, 1, 6,
	1, 6, 6, 2, 1, 6, 6, 2, 6, 2, 2, 4, 1, 6, 6, 2, 6, 2, 2, 4,
	6, 2, 2, 4, 2, 4, 4, 0 };
static UShort *DSCInformation = NULL;
static UShort *DSCInformationCopy = NULL;
static UChar  DSCSendBuf[DSC_SEND_DATA_LEN] = {0};

/***********************************************************************************
 **** 函数的名称：carelessMatch();
 **** 函数的作用：统计粗扫描计数结果
 **** 函数返回值：粗扫描匹配值 matchResult_g ；
 ***********************************************************************************/
static UShort carelessMatch(Queue *q, QUEUE_DATA_TYPE * matchFrameHead, UShort startPosition,UShort len){
	UShort i = 0;
	UShort matchResult_g = 0;
	QUEUE_DATA_TYPE temp = 0;
	matchResult_g = 0;
	for(i = 0; i < len; i++){
        temp = q->buf[(i*7 + startPosition) & DSC_RX_BUF_LEN_1];
		if(temp == matchFrameHead[i])
			matchResult_g++;
	}
	return matchResult_g;
}

/***********************************************************************************
 **** 函数的名称：oneMatch();
 **** 函数的作用：统计细扫描计数结果
 **** 函数返回值：细扫细匹配值 matchResult ；
 ***********************************************************************************/
static UShort oneMatch(Queue *q, QUEUE_DATA_TYPE * matchFrameHead, UShort startPosition,UShort len){
	UShort i = 0;
	UShort matchResult = 0;
	QUEUE_DATA_TYPE temp = 0;
	for(i = 0; i < MATCH_FRAME_HEAD_LEN; ++i){
        temp = q->buf[(i + startPosition) & DSC_RX_BUF_LEN_1];
		if(temp == matchFrameHead[i])
			matchResult++;
		else{
			if(matchResult == 0)
				matchResult = 0;
			else
				matchResult--;
		}
	}
	return matchResult;
}

 
/***********************************************************************************
 **** 函数的名称：matchHead();
 **** 函数的作用：统计粗扫描计数结果
 **** 函数返回值：粗扫描匹配值 matchResult_g ；
 ***********************************************************************************/
static UShort matchHead(Queue *q, QUEUE_DATA_TYPE * matchFrameHead)
{
	UShort len = queueLength(q);
	float  matchLen = 0;
	short carelessMatchRate=0,carefulMatchRate;
	short maxMatchRate = 0;
	short i=0,j=0,k=0;
	char temp_j = 0;
	UInt32 time =0;

	if(len >= DSC_RX_BUF_LEN_HALF)															// wait until enough length;  7000 bits
	{
		time=Clock_getTicks();
		matchLen = (len - MATCH_FRAME_HEAD_LEN-7)/3;			//(7000-280-7)/3 = 2237

		for(i=0;i < matchLen; i++)																//detect 0~(MATCH_FRAME_HEAD_LEN/7+1)
		{
			carelessMatchRate = carelessMatch(q,carelessHeadMat,q->front+3,CARELESS_HEAD_LEN);	//start position is q->front+3(middle sample : 3,10,17...)
			if(carelessMatchRate >= 35)												//40 has matched 35
			{
				//DSC_TEST_CNT[0]  ++;                                         //
				//DSC_TEST_CNT[1] = DSC_TEST_CNT[1] + (carelessMatchRate-35);  //
				for(j=-3;j<=3;j ++)													//find max value  2-1-6
				{	carefulMatchRate = oneMatch(q, matchFrameHead, q->front+3+j, MATCH_FRAME_HEAD_LEN);
					if(carefulMatchRate >= MATCH_FRAME_HEAD_LEN - 130)
					{
						if(carefulMatchRate > maxMatchRate)
						{
							maxMatchRate = carefulMatchRate;
							temp_j = j;
						}
					}
				}
				if(maxMatchRate >= carefulMatchRate){
					//DSC_TEST_CNT[2]++;                                  //???????????
					//DSC_TEST_CNT[3] = DSC_TEST_CNT[3] + (maxMatchRate-150);//??????????????????????29?????????
					for(k=0;k<3+temp_j;k++)									//if matched success,delete queue size
						deQueue(q);
					return OK;												//match success
				}
				else{
					for(j=0;j<3;j++)										//careless success,but careful unsuccess
						deQueue(q);
				}
			}
			else{
				for(j=0;j<3;j++)											//if matched unsuccessfully,delete queue size
					deQueue(q);
			}
		}
		time = Clock_getTicks() - time;
		return FAIL;														//match unsuccessful
	}
	else{
		Task_sleep(20);														//no enough length,need delay 32ms,wait 40*0.8333;
		return FAIL;
	}
}

/***********************************************************************************
 **** 函数的名称：fillFrame_Queue() ;
 **** 函数的作用：将二进制比特流数据填充(转换)成一帧DSC数据;
 **** 函数返回值：无返回值
 ***********************************************************************************/
static void fillFrame_Queue(Queue *q, UShort *des, UShort startPosition)
{
	UShort len = DSC_RX_BUF_LEN_HALF;// - MATCH_FRAME_HEAD_SLOT_LEN; //??Queue??????????????
	//UShort len = DSC_RX_BUF_LEN_HALF;
	UShort count1 = 0;
	UShort count2 = 0;
	UShort i = 0;
	UShort temp1 = 0;
	QUEUE_DATA_TYPE temp2 = 0;
	UShort bitCount = 0;
	UShort result = 0;
	UShort start = 0;
	while(len > 0)
	{
		len--;
		count1++;
		temp2 = q->buf[(q->front+start++)&DSC_RX_BUF_LEN_1];		//lao added
		bitCount += temp2;
		if(count1 == DSC_SAMPLE_SCALE){  //7bit
			count1 = 0;
			temp1 = (bitCount >= BIT_THREAD) ? 1 : 0;
			bitCount = 0; 		//???bitCount ?
			result = result | (temp1 << count2);
			++count2;
			if(count2 == SLOT_BIT_NUM){ //10bit
				des[i + startPosition] = result;
				result = 0;
				i++;
				count2 = 0;
			}
		}
	}
}

/***********************************************************************************
 **** 函数的名称：getNormalFrame();
 **** 函数的作用：获得一帧DSC数据
 **** 函数返回值：成功返回 1，错误返回 0；
 ***********************************************************************************/
static UShort getNormalFrame(Queue *q, UShort *frameBuf)
{
	UShort startPosition = 0;
	UShort len = queueLength(q);
	if(len > DSC_RX_BUF_LEN_HALF + MATCH_FRAME_HEAD_LEN)
	{
		fillFrame_Queue(q, frameBuf, startPosition);
		return 1; //?????
	}
	return 0; //??????
}

/***********************************************************************************
 **** 函数的名称：seperateInf();
 **** 函数的作用：将一帧原始DSC数据分解成DX、RX两列DSC数据；
 **** 函数返回值：无返回值；
 ***********************************************************************************/
static void seperateInf(UShort *src, UShort *des1, UShort *des2)
{ 
	UShort start1 = 14;     
	UShort start2 = 19;     
	UShort index = start1;
	UShort i = 0;
	UShort count = 0;
	for(count = 0; count < dscInformationLen; ++count)
	{
		des1[i++] = src[index];
		index += 2;
	}
	i = 0;
	index = start2;
	for(count = 0; count < dscInformationLen; ++count)
	{
		des2[i++] = src[index];
		index += 2;
	}
}

//************************* find eos *******************************************
/***********************************************************************************
 **** 函数的名称：bits_error();
 **** 函数的作用：统计找到的结束符EOS的正确比特数；
 **** 函数返回值：返回正确的比特个数；
 ***********************************************************************************/
static int bits_error(unsigned short value, unsigned short eos_value)
{	
	int i = 0;
	int count = 0;
	unsigned short temp = 0;
	
	switch(eos_value){
		case 0x7f:
				temp = value ^ 0x7f;
				break;
		case 0x17a:
				temp = value ^ 0x17a;
				break;
		case 0x175:
				temp = value ^ 0x175;
				break;
		default:
				break;
	}
	for(i = 0; i < 10; i++)
	{
		if(((temp >> i) & 0x01) == 0)
			count ++;
		else
			count --;
	}
	return count;
}

/***********************************************************************************
 **** 函数的名称： find_thread();
 **** 函数的作用：找到该帧DSC序列的结束符EOS组合的正确比特位数(0-30之间)；
 **** 函数返回值：返回计算所得的正确比特位数；
 ***********************************************************************************/
static int find_thread(unsigned short *frameBuf, int i,unsigned short eos_value)
{
	unsigned short a = frameBuf[i];
	unsigned short b = frameBuf[i+4];
	unsigned short c = frameBuf[i+6];
	
	int count1 = 0;
	int count2 = 0;
	int count3 = 0;
	
	count1 = bits_error(a, eos_value);
	count2 = bits_error(b, eos_value);
	count3 = bits_error(c, eos_value);
	
	return (count1 + count2 + count3);
}

/***********************************************************************************
 **** 函数的名称： singleFrameLen()
 **** 函数的作用：获取一帧DSC的长度(DX或RX序列)；
 **** 函数返回值：返回该帧DSC的长度；
 ***********************************************************************************/
static unsigned short singleFrameLen(unsigned short *frameBuf, unsigned short eos_temp, unsigned short eos_i)
{
	unsigned short len_temp = 0;
	unsigned short len_flag = 0;
//	printf("******************find eos,  eos_temp = %d, eos_i = %d\n",eos_temp, eos_i);
	frameBuf[eos_i]   = eos_temp;
	frameBuf[eos_i+4] = eos_temp;
	frameBuf[eos_i+6] = eos_temp;

	if((frameBuf[12]&0x7f)==112 || (frameBuf[14]&0x7f)==112 || (frameBuf[17]&0x7f)==112 || (frameBuf[19]&0x7f)==112)
	{
		len_flag = 1;
	}
	else if((frameBuf[26]&0x7f)==112 || (frameBuf[38]&0x7f)==112 || (frameBuf[31]&0x7f)==112 || (frameBuf[43]&0x7f)==112)
	{
		len_flag = 1;
	}
	else if((frameBuf[16]&0x7f)==112 || (frameBuf[28]&0x7f)==112 || (frameBuf[21]&0x7f)==112 || (frameBuf[31]&0x7f)==112)
	{
		len_flag = 1;
	}
	else if((frameBuf[16]&0x7f)==112 || (frameBuf[21]&0x7f)==112)
	{
		len_flag = 1;
	}
	else if((frameBuf[12]&0x7f)==120 || (frameBuf[14]&0x7f)==120 || (frameBuf[17]&0x7f)==120 || (frameBuf[19]&0x7f)==120)
	{
		if((frameBuf[26]&0x7f)==108 || (frameBuf[31]&0x7f)==108)
		{
			if((frameBuf[38]&0x7f)==121 || (frameBuf[43]&0x7f)==121)
			{
				if((frameBuf[58]&0x7f)==122 || (frameBuf[63]&0x7f)==122 || (frameBuf[62]&0x7f)==122 || (frameBuf[64]&0x7f)==122)
				{
					len_flag = 1;
				}
			}
		}
	}

	if(len_flag == 1){
		frameBuf[eos_i+18] = eos_temp;
		len_temp = (eos_i - 14)/2+2+9;
	}
	else{
		len_temp = (eos_i - 14)/2+2;
	}
	return len_temp;
}

/***********************************************************************************
 **** 函数的名称：getInformationLen();
 **** 函数的作用：找到正确的结束符EOS，获取一帧DSC的长度；
 **** 函数返回值：返回一帧DSC数据的长度；
 ***********************************************************************************/
UChar  getInformationLen(UShort *frameBuf)
{
	int i = 0;
	int ret1 = 0;
	int ret2 = 0;
	int ret3 = 0;
	int start  = 14;
	unsigned short eos_temp = 0;
	unsigned short eos_i    = 0;
	unsigned char  len      = 0;

	for(i = start; i < 100; i = i+2)
	{
		ret1 = find_thread(frameBuf, i, 0x7f);
		ret2 = find_thread(frameBuf, i, 0x17a);
		ret3 = find_thread(frameBuf, i, 0x175);

		if(ret1 >= 24){
			eos_temp = 0x7f;
			eos_i = i;
			break;
		}
		else if(ret2 >= 24){
			eos_temp = 0x17a;
			eos_i = i;
			break;
		}
		else if(ret3 >= 24){
			eos_temp = 0x175;
			eos_i = i;
			break;
		}
		else
			continue;
	}

	switch(eos_temp){
		case 0x7f:
			len = singleFrameLen(frameBuf, eos_temp, eos_i);
			break;
		case 0x17a:
			len = singleFrameLen(frameBuf, eos_temp, eos_i);
			break;
		case 0x175:
			len = singleFrameLen(frameBuf, eos_temp, eos_i);
			break;
		default:
			printf("error occur\n");
			break;
	}
	return len;
} 

/***********************************************************************************
 **** 函数的名称：abandonQueue();
 **** 函数的作用：丢掉二进制比特流数据，个数是size个；
 **** 函数返回值：无返回值
 ***********************************************************************************/
void abandonQueue(Queue *q,UShort size)
{
	UShort i;
	for(i=0;i<size;i++)
		deQueue(q);

}

/***********************************************************************************
 **** 函数的名称：oneBitCorrect();
 **** 函数的作用：纠正一个bit错误
 **** 函数返回值：无返回值
 ***********************************************************************************/
static void oneBitCorrect(UShort *infBuf1,errorInf *rowError,errorInf *columError)
{
	char i = rowError->location;
	char j = columError->location;
	char bitFlag = 0;

	UShort value = infBuf1[i];
	bitFlag = (1 << j) & value;

	if(bitFlag == 0)
	{
		infBuf1[i] = (1 << j) | value;
	}
	else
	{
		infBuf1[i] = (~(1 << j)) & (value);
	}
}

//**********************************new error corection********************************************
/***********************************************************************************
 **** 函数的名称：I_check();
 **** 函数的作用：检查接收到的一帧数据的ECC是否正确
 **** 函数返回值：正确返回1，错误返回0
 ***********************************************************************************/
static UShort I_check(UShort *infBuf1, UShort *infBuf2)
{
	char zeroCountBit1 = 0;
	char index1 = 0;
	char zeroCountBit2 = 0;
	char index2 = 0;
	index1 = infBuf1[dscInformationLen-1] & 0x007F;
	zeroCountBit1 = (UChar)(infBuf1[dscInformationLen-1] >> 7);  //???????
	if(zeroCountBit1 != checkErrorTable[index1])
	{
		index2 = infBuf2[dscInformationLen-1] & 0x007F;
		zeroCountBit2 = (UChar)(infBuf2[dscInformationLen-1] >> 7);  //
		if(zeroCountBit2 == checkErrorTable[index2])
		{
			infBuf1[dscInformationLen-1] = infBuf2[dscInformationLen-1];
		}
		else
		{
			if(index2 == index1)
			{
				return 1;
			}
			else if (zeroCountBit2 == checkErrorTable[index1])
			{
				return 1;
			}
			else if(zeroCountBit1 == checkErrorTable[index2])
			{
				infBuf1[dscInformationLen-1] = infBuf2[dscInformationLen-1];
				return 1;
			}
			else
				return 0;
		}
	}
	return 1;
}

/***********************************************************************************
 **** 函数的名称：mode2Adder();
 **** 函数的作用：进行奇偶校验
 **** 函数返回值：返回校验结果；
 ***********************************************************************************/
static UShort mode2Adder(UShort num1, UShort num2, UShort addBitsNum)
{
	UShort addResult = 0;
	UShort i = 0;
	UShort mask = 0;
	UShort temp1 = 0;
	UShort temp2 = 0;

	for(i = 0; i < addBitsNum; ++i){
		mask = 1 << i;
		temp1 = num1 & mask;
		temp2 = num2 & mask;
		addResult += ((temp1 + temp2) & mask);
		mask = 0;
	}
	return addResult;
}

/***********************************************************************************
 **** 函数的名称：checkEcc();
 **** 函数的作用：检查校验字符ECC
 **** 函数返回值：无返回值；
 ***********************************************************************************/
static void checkEcc(const UShort *informationBuf,char len, errorInf *colum)
{
	UShort i = 0,k = 0;
	UShort temp1 = 0;
	UShort temp2 = 0;
	UShort mode2AddResult = 0;
  
	for(i = 0; i < len; ++i)
	{
		mode2AddResult = mode2Adder(mode2AddResult, informationBuf[i], 7);
	}
	
	for(i = 0; i < 7; ++i)
	{
		temp1 = ((informationBuf[len] >> i) & 0x0001);
		temp2 = ((mode2AddResult >> i) & 0x0001);
		if(temp1 != temp2)
		{
			colum->count     += 1;
			colum->location   = i;
			colum->test[k++]  = i;
		}
	}
}

/***********************************************************************************
 **** 函数的名称：errorFixup();
 **** 函数的作用：检错纠错函数；
 **** 函数返回值：纠错成功返回1，纠错失败返回0；
 ***********************************************************************************/
static UShort errorFixup(UShort *infBuf1, UShort *infBuf2)
{
	char zeroCountBit1 = 0;
	char zeroCountBit2 = 0;
	char i = 0;
	char index1 = 0;
	char index2 = 0;
	char flag  = 0;
	char bitTemp  = 0;

	errorInf columError;
	errorInf rowError;

	columError.count = 0;
	columError.location = 0;
	rowError.count = 0;
	rowError.location = 0;

	flag = I_check(infBuf1, infBuf2);

	if(flag == 1)
	{
		for(i = 0; i < dscInformationLen; i++ )
		{
			index1 = infBuf1[i] & 0x007F;
			zeroCountBit1 = (UChar)(infBuf1[i] >> 7);
			if(zeroCountBit1 != checkErrorTable[index1])
			{
				index2 = infBuf2[i] & 0x007F;
				zeroCountBit2 = (UChar)(infBuf2[i] >> 7);
				if(zeroCountBit2 == checkErrorTable[index2])
				{
					infBuf1[i] = infBuf2[i];
				}
				else
				{
					if(index1 == index2)
					{
						continue;
					}
					else if(zeroCountBit2 == checkErrorTable[index1])//????????��?????
					{
						continue;
					}
					else if(zeroCountBit1 == checkErrorTable[index2])//????????��?????
					{
						infBuf1[i] = infBuf2[i];
						continue;
					}
					else
					{
						rowError.location = i;
						rowError.count += 1;
					}
				}
			}
		}
		if(rowError.count == 0)
		{
//			DSC_TEST_CNT[8]++;
				return 1;
		}
		else if(rowError.count == 1)
		{
//			DSC_TEST_CNT[0]++;
			checkEcc(infBuf1,dscInformationLen-1,&columError);		//ֻ����һ�д��������²Ž�����У��
			if(columError.count == 1) 
			{
				bitTemp = (1 << (columError.location));
				infBuf1[rowError.location] = mode2Adder(infBuf1[rowError.location], bitTemp, 7);  
//				DSC_TEST_CNT[1]++;
				return 1;
			}
			else
			{
//				DSC_TEST_CNT[2]++;
				for(i = 0;i < columError.count; i = i + 1)
				{
					columError.location = columError.test[i];
					oneBitCorrect(infBuf1, &rowError, &columError);
				}
				return 1;
			}
		}
		else	
		{
//			DSC_TEST_CNT[3]++;
			return 0;
		}
	}
	else
	{
		return 0;
	}
}


/*
	##&&TABBBCCCDDEEEFFFGHHHHHHHHHHHIIIIIIIIIIJ$$$
*/
/***********************************************************************************
 **** 函数的名称：package();
 **** 函数的作用：打包数据：将10bit数据打包成7bit数据(去掉3bit校验位)
 **** 函数返回值：无返回值
 ***********************************************************************************/
void package(const UShort *in, UChar *out)
{
	UShort  i = 0;
	UShort  j = 0;
	for(j = 0; j < dscInformationLen; ++j)
	{
		out[i++] = (UChar)(in[j] & 0x007F);
	}
}

extern Timer_Handle timer;
extern Void hwiFxn(UArg arg);
//dsc_message_t *msg_dsc_se;

int sendcount1,sendcount2,fixcount;

/***********************************************************************************
 **** 函数的名称：DSC_RX();
 **** 函数的作用：DSC接收模块的主函数，负责DSC接收的流程控制
 **** 函数返回值：无返回值
 ***********************************************************************************/
void DSC_RX(Queue *q)
{
	UShort matchFrameHeadFlag = FAIL;
	UShort getFrameComFlag = 0;
	UShort status = 0;
	int status1 = 0;

	DSCInformation = (UShort*)malloc(sizeof(UShort));
	DSCInformationCopy = (UShort*)malloc(sizeof(UShort));
	while(1)
	{

		if(matchFrameHeadFlag == FAIL)
		{
			matchFrameHeadFlag = matchHead(q, dscFrameHeadMat);
		}
		else
		{
			/******/
			getFrameComFlag = getNormalFrame(q, frameBuf);
			if(getFrameComFlag == 1)
			{
				    getFrameComFlag = 0;
				    matchFrameHeadFlag = FAIL;


				    dscInformationLen = getInformationLen(frameBuf);

					abandonQueue(q,(dscInformationLen+9) * 2 * DSC_SAMPLE_SCALE * SLOT_BIT_NUM); //delete the information data

					DSCInformation = (UShort*)realloc(DSCInformation,dscInformationLen*sizeof(UShort));
					DSCInformationCopy = (UShort*)realloc(DSCInformationCopy,dscInformationLen*sizeof(UShort));


					seperateInf(frameBuf, DSCInformation, DSCInformationCopy);

					status = errorFixup(DSCInformation, DSCInformationCopy);

					//DSCInformation
					if(status == 1)
					{
						status = 0;
						package(DSCInformation, DSCSendBuf);
						
//						msg_dsc_se = (dsc_message_t *)message_alloc(msgbuf[1], sizeof(dsc_message_t));
//						if(!msg_dsc_se)
//						{
//							log_error("msg_dsc_se malloc fail");
//						}
//						memcpy(msg_dsc_se->data.dsc_msg, DSCSendBuf, 46);
//						msg_dsc_se->data.mid=67;
//						msg_dsc_se->data.dsc_len = dscInformationLen;
//						status1 = messageq_send(&msgq[1],(messageq_msg_t)msg_dsc_se,0,0,0);
//						if(status1 >= 0)
//							sendcount1++;
//						else
//							log_error("send1 error");
					}
					else
					{  //
						fixcount ++;
						status = 0;
					}
			}
			else
			{
				Task_sleep(10);					//sleep
			}
		}
	}
}






