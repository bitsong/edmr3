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

extern unsigned int current_dscch;
//clk0; ticks  time(ms)
#if 0
extern Clock_Handle clk0;
extern void clock_add(Clock_Handle handle, int ticks);
#else
extern Timer_Handle 		timer;
extern void clock_add(Timer_Handle handle, UInt32 ticks_us);
#endif

static UShort frameBuf[DSC_FRAME_LENGTH] = {0}; //
UChar dscInformationLen = 0;	//
int DSC_TEST_CNT[25] = {0};
static UShort *DSCInformation = NULL;			//???????????????
static UShort *DSCInformationCopy = NULL;
static UChar  DSCSendBuf[DSC_SEND_DATA_LEN] = {0};
static const UChar checkErrorTable[128] = {
	7, 3, 3, 5, 3, 5, 5, 1, 3, 5, 5, 1, 5, 1, 1, 6, 3, 5, 5, 1,
	5, 1, 1, 6, 5, 1, 1, 6, 1, 6, 6, 2, 3, 5, 5, 1, 5, 1, 1, 6,
	5, 1, 1, 6, 1, 6, 6, 2, 5, 1, 1, 6, 1, 6, 6, 2, 1, 6, 6, 2,
	6, 2, 2, 4, 3, 5, 5, 1, 5, 1, 1, 6, 5, 1, 1, 6, 1, 6, 6, 2,
	5, 1, 1, 6, 1, 6, 6, 2, 1, 6, 6, 2, 6, 2, 2, 4, 5, 1, 1, 6,
	1, 6, 6, 2, 1, 6, 6, 2, 6, 2, 2, 4, 1, 6, 6, 2, 6, 2, 2, 4,
	6, 2, 2, 4, 2, 4, 4, 0};

static QUEUE_DATA_TYPE   ASK_carelessHeadMat[CARELESS_HEAD_LEN+8] ={
	1,0,1,0,1,0,1,0,
	1,0,1,0,1,0,1,0,
	1,0,1,0,1,0,1,0,
	1,0,1,0,1,0,1,0,
	1,0,1,0,1,0,1,0,
	1,0,1,1,1,1,0,1};
	
static QUEUE_DATA_TYPE   MSK_carelessHeadMat[CARELESS_HEAD_LEN] ={
	1,0,1,1,1,1,1,0,0,1,
	1,1,0,1,0,1,1,0,1,0,
	1,0,1,1,1,1,1,0,0,1,
	0,1,0,1,0,1,1,0,1,1};
static UShort MSK_carelessMatch(Queue *q, QUEUE_DATA_TYPE * matchFrameHead, UShort startPosition,UShort len){
	UShort i = 0;
	UShort matchResult_g = 0;
	QUEUE_DATA_TYPE temp = 0;
	matchResult_g = 0;
	for(i = 0; i < len; i++){
        temp = q->buf[(i*7 + startPosition)& DSC_RX_BUF_LEN_1];//
		if(temp == matchFrameHead[i])
			matchResult_g++;
	}
	return matchResult_g;
}

static UShort ASK_carelessMatch(Queue *q, QUEUE_DATA_TYPE * matchFrameHead, UShort startPosition,UShort len){
	UShort i = 0;
	UShort matchResult_g = 0;
	QUEUE_DATA_TYPE temp = 0;
	matchResult_g = 0;
	for(i = 0; i < len; i++){
        temp = q->buf[(i*7*4 + startPosition)& DSC_RX_BUF_LEN_1];//
		if(temp == matchFrameHead[i])
			matchResult_g++;
	}
	return matchResult_g;
}
//ϸɨ��ƥ��֡
static QUEUE_DATA_TYPE   MSK_dscFrameHeadMat[MATCH_FRAME_HEAD_LEN] ={
	1,1,1,1,1,1,1,   //若改变，直接注释掉
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
	0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,
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
	0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,
	1,1,1,1,1,1,1};

static QUEUE_DATA_TYPE   ASK_dscFrameHeadMat[4*MATCH_FRAME_HEAD_LEN+224] ={
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
	};
	
static UShort oneMatch(Queue *q, QUEUE_DATA_TYPE * matchFrameHead, UShort startPosition,UShort len){
	UShort i = 0;
	UShort matchResult = 0;
	QUEUE_DATA_TYPE temp = 0;
	for(i = 0; i < len; i++){
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

static UShort ASK_MatchHead(Queue *q, QUEUE_DATA_TYPE * matchFrameHead,UShort TH1,UShort TH2){
	UShort len = queueLength(q);
	float  matchLen = 0;
	short carelessMatchRate=0,carefulMatchRate=0;
	short maxMatchRate = 0;
	short i=0,j=0,k=0;
	char temp_j = 0;
	UInt32 time =0;
	
	if(len >= 4*30*8*7){ //队列里面接收的数据达到一定长度后才会进入函数  共计28个字节，每个字节8个bit。每个bit来自于7个流数据  28->60
		
		time=Clock_getTicks();
		matchLen = (len - 4*6*8*7-4*7)/(3*4+1);  //一开始值写的太大
		for(i=0;i < matchLen;i++)
		{
			carelessMatchRate = ASK_carelessMatch(q,ASK_carelessHeadMat,q->front+(3*4+1),CARELESS_HEAD_LEN+8);
			if(carelessMatchRate >= TH1)//48 has matched 40
			{
				DSC_TEST_CNT[0]++;  //统计粗扫描次数
				DSC_TEST_CNT[1] = DSC_TEST_CNT[1] + (carelessMatchRate-TH1); //粗扫描累加值ֵ
				for(j=-(3*4+1);j<=(3*4+1);j++)
				{
					carefulMatchRate = oneMatch(q, matchFrameHead, q->front+(3*4+1)+j, 4*MATCH_FRAME_HEAD_LEN+224);  //细扫描共计要扫描48*7 = 336次
					if(carefulMatchRate >= TH2){
						if(carefulMatchRate > maxMatchRate){
							maxMatchRate = carefulMatchRate;
							temp_j = j;
						}
					}
				}
				if(maxMatchRate >= TH2){
					DSC_TEST_CNT[2]++;//统计细扫描次数
					DSC_TEST_CNT[3] = DSC_TEST_CNT[3] + (maxMatchRate-TH2);////细扫描累加值
					for(k=0;k<(3*4+1)+temp_j;k++)
						deQueue(q);
					return OK;
				}
				else{
					for(j=0;j<(3*4+1);j++)
						deQueue(q);
				}
			}
			else{
				for(j=0;j<(3*4+1);j++)
					deQueue(q);
			}
		}
		time = Clock_getTicks() - time;
		return FAIL;
	}
	else{
		Task_sleep(20);
		return FAIL;
	}
}

static UShort MSK_MatchHead(Queue *q, QUEUE_DATA_TYPE * matchFrameHead,UShort TH1,UShort TH2){
	UShort len = queueLength(q);
	float  matchLen = 0;
	short carelessMatchRate=0,carefulMatchRate;
	short maxMatchRate = 0;
	short i=0,j=0,k=0;
	char temp_j = 0;
	UInt32 time =0;
	if(len >= 350){ //����������յ���ݴﵽһ�����Ⱥ�Ż���뺯��  DSC_RX_BUF_LEN_HALF
		time=Clock_getTicks();
		matchLen = (len - MATCH_FRAME_HEAD_LEN-7)/3;
		for(i=0;i < matchLen;i++){
			carelessMatchRate = MSK_carelessMatch(q,MSK_carelessHeadMat,q->front+3,CARELESS_HEAD_LEN); //60
			if(carelessMatchRate >= TH1){	//40 has matched 35
				DSC_TEST_CNT[0]++;  //ͳ�ƴ�ɨ�����
				DSC_TEST_CNT[1] = DSC_TEST_CNT[1] + (carelessMatchRate-TH1); //��ɨ���ۼ�ֵ
				for(j=-3;j<=3;j++){
					carefulMatchRate = oneMatch(q, matchFrameHead, q->front+3+j, MATCH_FRAME_HEAD_LEN);//420
					if(carefulMatchRate >= TH2){ //420 -> 225
						if(carefulMatchRate > maxMatchRate){
							maxMatchRate = carefulMatchRate;
							temp_j = j;
						}
					}
				}
				if(maxMatchRate >= TH2){
					DSC_TEST_CNT[2]++;//ͳ��ϸɨ�����
					DSC_TEST_CNT[3] = DSC_TEST_CNT[3] + (maxMatchRate-TH2);//ϸɨ���ۼ�ֵ
					for(k=0;k<3+temp_j;k++)
						deQueue(q);
					return OK;
				}
				else{
					for(j=0;j<3;j++)
						deQueue(q);
				}
			}
			else{
				for(j=0;j<3;j++)
					deQueue(q);
			}
		}
		time = Clock_getTicks() - time;
		return FAIL;
	}
	else{
		Task_sleep(20);
		return FAIL;
	}
}

static void ASK_FillFrame_Queue(Queue *q, UShort *des, UShort startPosition){
	UShort len = 4*30*8*7;   //长度要调整
	UShort count1 = 0;
	UShort count2 = 0;
	UShort i = 0;
	UShort temp1 = 0;
	QUEUE_DATA_TYPE temp2 = 0;
	UShort bitCount = 0;
	UShort result = 0;
	UShort start = 0;//这个位置要确定  start = 336 直接从数据位开始读取
	while(len > 0){
		len--;
		count1++;
		temp2 = q->buf[(q->front+start++)&DSC_RX_BUF_LEN_1];
		bitCount += temp2;
		if(count1 == 4*DSC_SAMPLE_SCALE){  //7bit
			count1 = 0;
			temp1 = (bitCount >= 4*BIT_THREAD) ? 0 : 1;
			bitCount = 0; 		//
			result = result | (temp1 << count2);  //这个地方以后要改正
			count2++;
			if(count2 == SLOT_BIT_NUM - 2){ //8bit
				des[i + startPosition] = result;
				result = 0;
				i++;
				count2 = 0;
			}
		}
	}
}
#if 1
static void MSK_FillFrame_Queue(Queue *q, UShort *des, UShort startPosition){
	UShort len = DSC_RX_BUF_LEN_HALF;  //����Ҫ����
	UShort count1 = 0;
	UShort count2 = 0;
	UShort i = 0;
	UShort temp1 = 0;
	QUEUE_DATA_TYPE temp2 = 0;
	UShort bitCount = 0;
	UShort result = 0;
	UShort start = 0;//���λ��Ҫȷ��  start = 336 ֱ�Ӵ����λ��ʼ��ȡ
	while(len > 0){
		len--;
		count1++;
		temp2 = q->buf[(q->front+start++)&DSC_RX_BUF_LEN_1];
		bitCount += temp2;
		if(count1 == DSC_SAMPLE_SCALE){  //7bit
			count1 = 0;
			temp1 = (bitCount >= BIT_THREAD) ? 1 : 0;
			bitCount = 0; 		//
			result = result | (temp1 << count2);
			count2++;
			if(count2 == SLOT_BIT_NUM ){ //7bit
				des[i + startPosition] = result;
				result = 0;
				i++;
				count2 = 0;
			}
		}
	}
}
#else
static void MSK_FillFrame_Queue(Queue *q, UShort *des, UShort startPosition){
	UShort len = DSC_RX_BUF_LEN_HALF;  //����Ҫ����
	UShort count2 = 0;
	UShort i = 0;
	UShort temp1 = 0;
	QUEUE_DATA_TYPE temp2 = 0;
	UShort result = 0;
	int start = 3;//���λ��Ҫȷ��  start = 336 ֱ�Ӵ����λ��ʼ��ȡ
	while(len > 0){
		len = len-7;
		temp2 = q->buf[(q->front+start)&DSC_RX_BUF_LEN_1];
		start = start + 7;
		temp1 = (temp2 == 1) ? 1 : 0;
		result = result | (temp1 << count2);
		count2++;
		if(count2 == SLOT_BIT_NUM ){ //10bit
			des[i + startPosition] = result;
			result = 0;
			i++;
			count2 = 0;
		}
	}
}
#endif

//由当前位置组一个字符数据，并判断其是否符合ECC校验
static UShort MSK_Fill_One_Frame(Queue *q, UShort Star)
{
	UShort count2 = 0;
	UShort temp1  = 0;
	QUEUE_DATA_TYPE temp2 = 0;
	UShort i=0;
	UShort result = 0;

	for(i =Star;i<Star+70;i=i+7)
	{
		temp1 = q->buf[(q->front+i)&DSC_RX_BUF_LEN_1];
		temp2 = (temp1 == 1) ? 1 : 0;
		result = result | (temp2 << count2);
		count2++;
		if(count2 == SLOT_BIT_NUM )//10bit
		{
			count2 = 0;
			return result;
		}
	}
	return result;
}
//如果组帧后失败,则向前向后各寻找2位
#if  0  //这个是不对的 以后要删除
static void MSK_Find_One_Frame(Queue *q,UShort *frameBuf)
{
	int   i = 0;
	char  j =0;
	UChar k =0;
	UShort temp_locat = 0;
	UShort inital_Locat0 = 0;
	UShort temp_frameBuf =0;
	UShort len = DSC_RX_BUF_LEN_HALF;  //70*10*10

	char index1 = 0;
	char zeroCountBit1 = 0;
	for(i=len;i>0;i=i-inital_Locat0)
	{
		DSC_TEST_CNT[4]++;
		for(j=-2;j<=2;j++)
		{
			DSC_TEST_CNT[4]++;
			temp_frameBuf = MSK_Fill_One_Frame(q,j+3+inital_Locat0);  //1-2-3-4-5
			index1 = temp_frameBuf & 0x7f;  		    //取出数值位
			zeroCountBit1 = (UChar)(temp_frameBuf >> 7);//取出校验位
//			DSC_TEST_CNT[7] = temp_frameBuf;
			if(zeroCountBit1 == checkErrorTable[index1])//校验通过直接跳出循环
			{
				DSC_TEST_CNT[5]++;
				frameBuf[k++] = temp_frameBuf;
				temp_locat    = inital_Locat0+j+3;  //记录当前位置
				break;
			}
			else    //校验未通过默认从 第4位开始组帧
			{
				DSC_TEST_CNT[6]++;
				if( j== 0) //这个地方调整最后不符合的开始位置
				{
					frameBuf[k++] = temp_frameBuf;
					temp_locat    = inital_Locat0+j+3;  //记录当前位置
				}
			}
		}
		inital_Locat0 = temp_locat+70;
	}
}
#else
static void MSK_Find_One_Frame(Queue *q,UShort *frameBuf)
{
	int   i = 0;
	char  j =0;
	char  TH = 2;
	UChar k =0;
	UShort temp_locat = 0;
	UShort inital_Locat0 = 0;
	UShort inital_Locat  = 0;
	UShort temp_frameBuf =0;
	UShort len = 4060;  //70*10*10  58个字节

	char index1 = 0;
	char zeroCountBit1 = 0;
	for(i=len;i>0;i=i-inital_Locat0)
	{
//		DSC_TEST_CNT[4]++;
		temp_frameBuf = MSK_Fill_One_Frame(q,j+3+inital_Locat);  //1-2-3-4-5
		index1 = temp_frameBuf & 0x7f;  		    //取出数值位
		zeroCountBit1 = (UChar)(temp_frameBuf >> 7);//取出校验位
		if(zeroCountBit1 == checkErrorTable[index1])//校验通过
		{
//			DSC_TEST_CNT[5]++;
			frameBuf[k++] = temp_frameBuf;
			temp_locat    = j;  //记录当前位置
		}
		else  //校验未通过，进入滑动模式
		{
			for(j=-TH;j<=TH+1;j++)
			{
				temp_frameBuf = MSK_Fill_One_Frame(q,j+3+inital_Locat);  //1-2-3-4-5
				index1 = temp_frameBuf & 0x7f;  		    //取出数值位
				zeroCountBit1 = (UChar)(temp_frameBuf >> 7);//取出校验位
				if(zeroCountBit1 == checkErrorTable[index1])//校验通过直接跳出循环
				{
//					DSC_TEST_CNT[6]++; 					//通过滑动 找到 符合的个数
					frameBuf[k++] = temp_frameBuf;
					temp_locat    = j;  //记录当前位置
					break;
				}
				else    //校验未通过默认从 第4位开始组帧
				{
					if( j== TH+1) //这个地方调整最后不符合的开始位置
					{
//						DSC_TEST_CNT[7]++;  //通过滑动仍 未找到 符合的个数
						j = 0;
						frameBuf[k++] = temp_frameBuf;
						temp_locat    = j;  //记录当前位置
						break;
					}
				}
				if(j == -1) //这个位置在开始处已经检查过，现在直接过掉
					j=0;
			}
		}
		j =0;
		inital_Locat0 = temp_locat+70;               //这个参数为每次减去的数值
		inital_Locat = inital_Locat + inital_Locat0; //这个用来记录每次移动的位置
	}
}
#endif
static UShort ASK_GetNormalFrame(Queue *q, UShort *frameBuf){
	UShort startPosition = 0;
	UShort len = queueLength(q);
	if(len >= 4*(6*8*7+30*7*8)){  //这里面的长度要进行修改
		ASK_FillFrame_Queue(q, frameBuf, startPosition);

		return 1;
	}
	return 0;
}

static UShort MSK_GetNormalFrame(Queue *q, UShort *frameBuf){
	UShort startPosition = 0;
	UShort len = queueLength(q);
	if(len > 4060){  //������ĳ���Ҫ�����޸�  58个字节
//		MSK_FillFrame_Queue(q, frameBuf, startPosition);   //非滑动寻找
		MSK_Find_One_Frame(q,frameBuf);                      //相向字节，滑动寻找
//		DSC_TEST_CNT[10]++;
		return 1;
	}
	return 0;
}

UChar  ASK_GetInformationLen(UShort *frameBuf) ////这里极有可能会再次修改
{
	if(frameBuf[6]==0x86) //呼叫类型：求救
	{
		return 22;
	}
	else if( frameBuf[6]==0xE2 || frameBuf[6]==0xEA ) //呼叫类型：全呼，气象
	{
		return 13;
	}
	else	//呼叫类型：选呼，快呼，群呼，呼叫应答
	{
		return 13; //22
	}
}
static int bits_error(UShort value,UShort Temp_Eos){
	int i = 0;
	int count = 0;
	unsigned short temp = value ^ Temp_Eos;
	for(i = 0; i < 10; i ++){
		if(((temp >> i) & 0x01) == 0)
			count ++;
		else
			count --;
	}
	return count;
}
static int find_thread(unsigned short *frameBuf, int i,UShort Temp_Eos){
	unsigned short a = frameBuf[i];
	unsigned short b = frameBuf[i+4];
	unsigned short c = frameBuf[i+6];
	int count1 = 0;
	int count2 = 0;
	int count3 = 0;
	count1 = bits_error(a,Temp_Eos);
	count2 = bits_error(b,Temp_Eos);
	count3 = bits_error(c,Temp_Eos);
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

	unsigned short  start_temp1 = 4;
	unsigned short  start_temp2 = 6;
	unsigned short  start_temp3 = 9;
	unsigned short  start_temp4 = 11;
	frameBuf[eos_i]   = eos_temp;
//  以下 两行 可以不用
//	frameBuf[eos_i+4] = eos_temp;
//	frameBuf[eos_i+6] = eos_temp;

	//01  求救呼叫  格式符 110   主叫地区码 + 主叫自识别码 + GPS   无需应答   转223通话
	if((frameBuf[start_temp1]&0x7f)==110 || (frameBuf[start_temp2]&0x7f)==110 || (frameBuf[start_temp3]&0x7f)==110 || (frameBuf[start_temp4]&0x7f)==110)
	{
		len_flag = 0;
	}
	//02  气象呼叫  格式符 119   主叫地区码 + 主叫自识别码   无需应答   转225通话
	if((frameBuf[start_temp1]&0x7f)==119 || (frameBuf[start_temp2]&0x7f)==119 || (frameBuf[start_temp3]&0x7f)==119 || (frameBuf[start_temp4]&0x7f)==119)
	{
		len_flag = 0;
	}
	//03  全呼呼叫  格式符 116   通信信道+主叫地区码 + 主叫自识别码   无需应答   转D1D2通话
	if((frameBuf[start_temp1]&0x7f)==116 || (frameBuf[start_temp2]&0x7f)==116 || (frameBuf[start_temp3]&0x7f)==116 || (frameBuf[start_temp4]&0x7f)==116)
	{
		len_flag = 0;
	}
	//04  海呼呼叫  格式符 102   被叫地区码+通信信道+主叫地区码 + 主叫自识别码   需应答   转D1D2通话
	if((frameBuf[start_temp1]&0x7f)==102 || (frameBuf[start_temp2]&0x7f)==102 || (frameBuf[start_temp3]&0x7f)==102 || (frameBuf[start_temp4]&0x7f)==102)
	{
		len_flag = 0;
	}
	//05  群呼呼叫  格式符 114   被叫地区码+通信信道+主叫地区码 + 主叫自识别码   无需应答   转D1D2通话
	if((frameBuf[start_temp1]&0x7f)==114 || (frameBuf[start_temp2]&0x7f)==114 || (frameBuf[start_temp3]&0x7f)==114 || (frameBuf[start_temp4]&0x7f)==114)
	{
		len_flag = 0;
	}
	//06  选呼呼叫1  格式符 120   被叫地区码+被叫自识别码+通信信道+主叫地区码 + 主叫自识别码 +GPS  需应答   转D1D2通话
	if((frameBuf[start_temp1]&0x7f)==120 || (frameBuf[start_temp2]&0x7f)==120 || (frameBuf[start_temp3]&0x7f)==120 || (frameBuf[start_temp4]&0x7f)==120)
	{
		len_flag = 0;
	}
	//07  选呼呼叫2  格式符 121   被叫地区码+被叫自识别码+通信信道+主叫地区码 + 主叫自识别码 +GPS  需应答   转D1D2通话
	if((frameBuf[start_temp1]&0x7f)==121 || (frameBuf[start_temp2]&0x7f)==121 || (frameBuf[start_temp3]&0x7f)==121 || (frameBuf[start_temp4]&0x7f)==121)
	{
		len_flag = 0;
	}
	//08  船位呼呼叫  格式符 101   被叫地区码+被叫自识别码+主叫地区码 + 主叫自识别码  需应答   转D1D2通话
	if((frameBuf[start_temp1]&0x7f)==101 || (frameBuf[start_temp2]&0x7f)==101 || (frameBuf[start_temp3]&0x7f)==101 || (frameBuf[start_temp4]&0x7f)==101)
	{
		len_flag = 0;
	}

	if(len_flag == 1){
//		frameBuf[eos_i+18] = eos_temp;
		len_temp = (eos_i - start_temp1)/2+2+9;
	}
	else{
		len_temp = (eos_i - start_temp1)/2+2;
	}
	return len_temp;
}
UChar  MSK_GetInformationLen(UShort *frameBuf){
	int i = 0;
	int ret1 = 0;
	int ret2 = 0;
	int ret3 = 0;
	int start  = 16;  //由原来的14变为现在的16，因为增加了两个点阵字符
	unsigned short eos_temp = 0;
	unsigned short eos_i    = 0;
	unsigned char  len      = 0;
#if  1
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
		else   //如果没有找到帧尾
		{
			continue;
		}
	}
#else
	for(i = start; i < 100; i = i+2)
	{
		ret1 = find_thread(frameBuf, i, 0x7f);
		if(ret1 >= 24)
		{
			eos_temp = 0x7f;
			eos_i = i;
			break;
		}
	}
	if (ret1 < 24)
	{
		for(i = start; i < 100; i = i+2)
		{
			ret2 = find_thread(frameBuf, i, 0x17a);
			if(ret2 >= 24)
			{
				eos_temp = 0x17a;
				eos_i = i;
				break;
			}
		}
	}
	else if ( (ret1 < 24) && (ret2 < 24) )
	{
		for(i = start; i < 100; i = i+2)
		{
			ret3 = find_thread(frameBuf, i, 0x175);
			if(ret3 >= 24)
			{
				eos_temp = 0x175;
				eos_i = i;
				break;
			}
		}
	}
#endif
	switch(eos_temp)
	{
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
//					printf("error occur\n");
					len = 0;
					break;
	}
	return len;
}
void abandonQueue(Queue *q,UShort size){
	UShort i;
	for(i=0;i<size;i++)
		deQueue(q);
}

static void ASK_SeperateInf(UShort *src, UShort *des1){
	UShort i = 0;
	for(i=0;i<dscInformationLen;i++)  //从呼叫类型位 开始读取数据
		des1[i] = src[i+6];
}

static UShort ASK_Error(UShort *infBuf1){
	UShort i=0;
	UShort Temp_Data=0;
	UShort XY_Data =0;

	for(i = 0; i < 11; i++)
	{
		Temp_Data = infBuf1[i]/16 + infBuf1[i]%16;//
		XY_Data += Temp_Data;
	}
		Temp_Data = infBuf1[11] >> 4;//累加值校验位
		Temp_Data |= (infBuf1[11] <<4) &0xff;
		if(Temp_Data == XY_Data)
			return 1;
		else
			return 0;
}	

static UShort NE_Add(UShort *P_Data){
	return P_Data[1] + P_Data[2] + P_Data[5] + P_Data[6] - P_Data[0] - P_Data[3] - P_Data[4] + 0x37;
}


static UShort ASK_ErrorFixup(UShort *infBuf1){
	UShort ASK_Flag=0;
	if(dscInformationLen == 13){//简单的检错不包含GPS
			ASK_Flag = ASK_Error(infBuf1);
			return ASK_Flag;
	}
	else
	{//除去数据检错还有GPS检错
		ASK_Flag = ASK_Error(infBuf1);
		if( (NE_Add(&infBuf1[14]) == infBuf1[21])&&  ASK_Flag == 1)
		{   DSC_TEST_CNT[8]++;
			return 1;
		}
		else
		{
			DSC_TEST_CNT[9]++;
			return 0;
		}
	}
}

static void MSK_SeperateInf(UShort *src, UShort *des1, UShort *des2){
	UShort start1 = 4;     //第一A开始
	UShort start2 = 9;     //
	UShort index = start1;
	UShort i = 0;
	UShort count = 0;
	for(count = 0; count < dscInformationLen; count++){
		des1[i++] = src[index];
		index += 2;
	}
	i = 0;
	index = start2;
	for(count = 0; count < dscInformationLen; count++){
		des2[i++] = src[index];
		index += 2;
	}
}
static UShort I_check(UShort *infBuf1, UShort *infBuf2){
	char zeroCountBit1 = 0;
	char index1 = 0;
	char zeroCountBit2 = 0;
	char index2 = 0;
	index1 = infBuf1[dscInformationLen-1] & 0x007F;
	zeroCountBit1 = (UChar)(infBuf1[dscInformationLen-1] >> 7);  //
	if(zeroCountBit1 != checkErrorTable[index1]){
		index2 = infBuf2[dscInformationLen-1] & 0x007F;
		zeroCountBit2 = (UChar)(infBuf2[dscInformationLen-1] >> 7);
		if(zeroCountBit2 == checkErrorTable[index2]){
			infBuf1[dscInformationLen-1] = infBuf2[dscInformationLen-1];
		}
		else{
			if(index2 == index1)
				return 1;
			else if (zeroCountBit2 == checkErrorTable[index1])
				return 1;
			else if(zeroCountBit1 == checkErrorTable[index2]){
				infBuf1[dscInformationLen-1] = infBuf2[dscInformationLen-1];
				return 1;
			}
			else
				return 0;
		}
	}
	return 1;
}
static void oneBitCorrect(UShort *infBuf1,errorInf *rowError,errorInf *columError){
	char i = rowError->location;
	char j = columError->location;
	char bitFlag = 0;
	UShort value = infBuf1[i];
	bitFlag = (1 << j) & value;
	if(bitFlag == 0)
		infBuf1[i] = (1 << j) | value;
	else
		infBuf1[i] = (~(1 << j)) & (value);
}
static UShort mode2Adder(UShort num1, UShort num2, UShort addBitsNum){
	UShort addResult = 0;
	UShort i = 0;
	UShort mask = 0;
	UShort temp1 = 0;
	UShort temp2 = 0;
	for(i = 0; i < addBitsNum; i++){
		mask = 1 << i;
		temp1 = num1 & mask;
		temp2 = num2 & mask;
		addResult += ((temp1 + temp2) & mask);
		mask = 0;
	}
	return addResult;
}
static void checkEcc(const UShort *informationBuf,char len,errorInf * colum){
	UShort i = 0,k = 0;
	UShort temp1 = 0;
	UShort temp2 = 0;
	UShort mode2AddResult = 0;
	for(i = 0; i < len; ++i)
		mode2AddResult = mode2Adder(mode2AddResult, informationBuf[i], 7); //
	for(i = 0; i < 7; ++i){
		temp1 = ((informationBuf[len] >> i) & 0x0001);
		temp2 = ((mode2AddResult >> i) & 0x0001);
		if(temp1 != temp2){
			colum->count   += 1;
			colum->location = i;
			colum->test[k++]  = i;
		}
	}
}
static UShort MSK_ErrorFixup(UShort *infBuf1, UShort *infBuf2){
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
	if(flag == 1){
		for(i = 0; i < dscInformationLen; i++ )
		{
			index1 = infBuf1[i] & 0x007F;
			zeroCountBit1 = (UChar)(infBuf1[i] >> 7);
			if(zeroCountBit1 != checkErrorTable[index1])
			{
				index2 = infBuf2[i] & 0x007F;
				zeroCountBit2 = (UChar)(infBuf2[i] >> 7);
				if(zeroCountBit2 == checkErrorTable[index2]){
					infBuf1[i] = infBuf2[i];
				}
				else{
					if(index1 == index2)
						continue;
					else if(zeroCountBit2 == checkErrorTable[index1])
						continue;
					else if(zeroCountBit1 == checkErrorTable[index2]){
						infBuf1[i] = infBuf2[i];
						continue;
					}
					else{
						rowError.location = i;
						rowError.count += 1;
					}
				}
			}
		}

//		for(i=0;i<18;i++)
//		{
//			DSC_TEST_CNT[7+i] = infBuf1[i];
//		}

		if(rowError.count == 0){
				return 1;
		}
		else if(rowError.count == 1){
			checkEcc(infBuf1,dscInformationLen-1,&columError);//???????��????????2??????��??
			if(columError.count == 1){
				bitTemp = (1 << (columError.location));
				infBuf1[rowError.location] = mode2Adder(infBuf1[rowError.location], bitTemp, 7);
				return 1;
			}
			else{
				for(i = 0;i < columError.count; i = i + 1){
					columError.location = columError.test[i];
					oneBitCorrect(infBuf1, &rowError, &columError);
				}
				return 1;
			}
		}
		else{
			return 0;
		}
	}
	else
		return 0;
}
void MSK_package(const UShort *in, UChar *out){
	UShort  i = 0;
	for(i = 0; i < dscInformationLen; i++)
		out[i] = (UChar)(in[i] & 0x7F);
}
void ASK_package(const UShort *in, UChar *out){
	UShort  i = 0;
	for(i = 0; i < dscInformationLen; i++)
		out[i] = (UChar)(in[i] & 0xFF);
}
extern Timer_Handle timer;
extern Void hwiFxn(UArg arg);
//dsc_message_t *msg_dsc_se;
msg_t *dscmsg;
int sendcount1,fixcount;

void DSC_RX(Queue *q){
	UShort matchFrameHeadFlag = FAIL; //匹配帧头标志位
	UShort getFrameComFlag = 0;       //是否得到正常的队列数据
	UShort status_DSC = 0;
	int status1 = 0;
	DSCInformation = (UShort*)malloc(sizeof(UShort));
	DSCInformationCopy = (UShort*)malloc(sizeof(UShort));
	int i=0;
	unsigned int current_dscch_temp;
	unsigned int current_dscch_send;
//	char TEST_CNT_FLAG = 0;
	while(1)
	{
		if(current_dscch == 221)
		{//ASK专用信道
			current_dscch_temp = current_dscch;
			DSC_TEST_CNT[4] = current_dscch_temp;

			if(matchFrameHeadFlag == FAIL)
			{
				matchFrameHeadFlag = ASK_MatchHead(q, ASK_dscFrameHeadMat,48,4*180);//匹配帧头 42/44/46/47 180
//				if(matchFrameHeadFlag == 1)
//				{
//					if(current_dscch_temp == current_dscch)
//						clock_add(timer,1200000);            //找到帧头后，延长Time1 = 1200 ms；
//				}
			}
			else
			{
				getFrameComFlag = ASK_GetNormalFrame(q, frameBuf);
				if(getFrameComFlag == 1)
				{	
				    getFrameComFlag = 0;
				    matchFrameHeadFlag = FAIL;
				    dscInformationLen = ASK_GetInformationLen(frameBuf);
					abandonQueue(q,(dscInformationLen+6) * 4 * DSC_SAMPLE_SCALE * (SLOT_BIT_NUM-2)); //delete the information data
					DSCInformation = (UShort*)realloc(DSCInformation,dscInformationLen*sizeof(UShort)); //申请动态内存
					ASK_SeperateInf(frameBuf, DSCInformation);//把数据从frameBuf里面，转移到新开辟的内存里面
					status_DSC = ASK_ErrorFixup(DSCInformation);
					if(status_DSC == 1)
					{
						status_DSC = 0;

				        //清除DSCSendBuf里面的数据 避免出现错误
						for(i=0;i<DSC_SEND_DATA_LEN;i++)
						{
							DSCSendBuf[i] = 0;
						}

						ASK_package(DSCInformation, DSCSendBuf);
						for(i=0;i<20;i++)
						{
							DSC_TEST_CNT[5+i] = DSCSendBuf[i];
						}
//						TEST_CNT_FLAG = 1;   //测试专用
						dscmsg = (msg_t *)message_alloc(msgbuf[1], sizeof(msg_t));
						if(!dscmsg)
						{
							log_error("msg_dsc_se malloc fail");
						}
						memcpy(dscmsg->data.dsc_msg, DSCSendBuf, 46);
						dscmsg->data.mid=67;
						dscmsg->data.dsc_len = dscInformationLen;
					   	dscmsg->data.chn = current_dscch_send;    		//新添加数据
						status1 = messageq_send_safe(&msgq[1],dscmsg,0,0,0);

						DSC_TEST_CNT[6] = 2;
						if(status1 >= 0)
							sendcount1++;
						else
							log_error("send1 error");
					}
					else
					{  //
						fixcount ++;
						status_DSC = 0;
					}
				}
				else
				{
					Task_sleep(10);
				}
			}
		}
		else if( (current_dscch == 231) ||
		(current_dscch == 236) || (current_dscch == 238) )
		{//MSK专用信道
			current_dscch_temp = current_dscch;
//			DSC_TEST_CNT[4] = current_dscch_temp;     //记录当前频道号码


			if(matchFrameHeadFlag == FAIL)            //首先进来检测是否找到帧头位置
			{
				matchFrameHeadFlag = MSK_MatchHead(q, MSK_dscFrameHeadMat,38,200 );//匹配帧头  全部匹配 TH1 = 60，TH2 = 420
				if(current_dscch_temp == current_dscch)
				{
				   	DSC_TEST_CNT[4]  = current_dscch_temp;
				   	DSC_TEST_CNT[5] ++;
				   	if(matchFrameHeadFlag == 1)
				   	{
				   		if(current_dscch_temp == current_dscch)
				   		clock_add(timer,450000);   //找到帧头后延长  54个字节
				   		current_dscch_send = current_dscch_temp;
				   	}
				}
				else
				{
					matchFrameHeadFlag = FAIL; //如果找到帧头后发现频道号码已经切换，那么立马抛弃当前的值
					for(i=0;i<70;i++)
						deQueue(q);
					DSC_TEST_CNT[6]  = current_dscch;
					DSC_TEST_CNT[7] ++;
				}

			}
			else
			{
				getFrameComFlag = MSK_GetNormalFrame(q, frameBuf); //修改组帧方法

				for(i=0;i<15;i++)
				{
					DSC_TEST_CNT[10+i] = frameBuf[i]&0x7f;
				}
//				for(i=0;i<21;i++)
//				{
//					DSC_TEST_CNT[4+i] = frameBuf[2+2*i]&0x7f;
//				}

				if(getFrameComFlag == 1)
				{
				   DSC_TEST_CNT[8]  = current_dscch;
				   DSC_TEST_CNT[9]  = current_dscch_temp;
				   getFrameComFlag = 0;
				   matchFrameHeadFlag = FAIL;//
				   dscInformationLen = MSK_GetInformationLen(frameBuf);//这个长度的方法还没有确定，先放在这里
//				   DSC_TEST_CNT[4] = dscInformationLen;
				   abandonQueue(q,(dscInformationLen+4) * 2 * DSC_SAMPLE_SCALE * SLOT_BIT_NUM); //delete the information data
				   DSCInformation = (UShort*)realloc(DSCInformation,dscInformationLen*sizeof(UShort));//申请动态内存
				   DSCInformationCopy = (UShort*)realloc(DSCInformationCopy,dscInformationLen*sizeof(UShort));
				   MSK_SeperateInf(frameBuf, DSCInformation, DSCInformationCopy);
//
//				   //把数据从frameBuf里面，转移到新开辟的内存里面
				   status_DSC = MSK_ErrorFixup(DSCInformation, DSCInformationCopy);
				   if(dscInformationLen == 0)
					   status_DSC = 0;
				   if(status_DSC == 1)
				   {
					   status_DSC = 0;
					   MSK_package(DSCInformation, DSCSendBuf);

					   dscmsg = (msg_t *)message_alloc(msgbuf[1], sizeof(msg_t));

					   	if(!dscmsg){
					   		log_error("dscmsg malloc fail!!!");
					   	}
					   	memcpy(dscmsg->data.dsc_msg, DSCSendBuf, 46);//46个字节到底够不够用以后再说
					   	dscmsg->data.mid = 67;
					   	dscmsg->data.dsc_len = dscInformationLen;
					   	dscmsg->data.chn = current_dscch_send;    		//新添加数据
					   	status1 = messageq_send_safe(&msgq[1],dscmsg,0,0,0); //最新修改

					   	if(status1 >= 0)
				   		sendcount1++;
					   	else
					   		log_error("send1 error");
					 }
					 else{
						fixcount++;
					   	status_DSC = 0;
					 }
				 }
				else
				{
					Task_sleep(10);
				}
			}
		}
		else
		{
			printf("ERROR!!!\n");
		}
	}
}
