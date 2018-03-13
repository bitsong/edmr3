
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

/************************************************** �������� **************************************************/
UChar dscInformationLen = 0;//dscInformationLen        22  //��ֵҲ����Ϊ21������ECC�����޸�----------------------------------------
//ƥ��֡ͷbuf
static QUEUE_DATA_TYPE   carelessHeadMat[CARELESS_HEAD_LEN] =
{
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
static UShort frameBuf[DSC_FRAME_LENGTH] = {0}; //�����DSC֡buf
static const UChar checkErrorTable[128] = {
	7, 3, 3, 5, 3, 5, 5, 1, 3, 5, 5, 1, 5, 1, 1, 6, 3, 5, 5, 1,
	5, 1, 1, 6, 5, 1, 1, 6, 1, 6, 6, 2, 3, 5, 5, 1, 5, 1, 1, 6,
	5, 1, 1, 6, 1, 6, 6, 2, 5, 1, 1, 6, 1, 6, 6, 2, 1, 6, 6, 2,
	6, 2, 2, 4, 3, 5, 5, 1, 5, 1, 1, 6, 5, 1, 1, 6, 1, 6, 6, 2,
	5, 1, 1, 6, 1, 6, 6, 2, 1, 6, 6, 2, 6, 2, 2, 4, 5, 1, 1, 6,
	1, 6, 6, 2, 1, 6, 6, 2, 6, 2, 2, 4, 1, 6, 6, 2, 6, 2, 2, 4,
	6, 2, 2, 4, 2, 4, 4, 0 };

static UShort *DSCInformation = NULL;			//�������·����С
static UShort *DSCInformationCopy = NULL;
static UChar  DSCSendBuf[DSC_SEND_DATA_LEN] = {0};

/*
 * ����:7:1����ƥ��
 */
static UShort carelessMatch(Queue *q, QUEUE_DATA_TYPE * matchFrameHead, UShort startPosition,UShort len)
{
	UShort i = 0;
	UShort matchResult_g = 0;
	QUEUE_DATA_TYPE temp = 0;
	matchResult_g = 0;
	for(i = 0; i < len; ++i){
        temp = q->buf[(i*7 + startPosition)   & DSC_RX_BUF_LEN_1];  //ֱ�ӷ��ʶ����е�Ԫ��
		if(temp == matchFrameHead[i])
			++matchResult_g;
	}
	return matchResult_g;
}
/*
 * ����:һ��ƥ��
 */
static UShort oneMatch(Queue *q, QUEUE_DATA_TYPE * matchFrameHead, UShort startPosition,UShort len)
{
	UShort i = 0;
	UShort matchResult = 0;
	QUEUE_DATA_TYPE temp = 0;

	for(i = 0; i < MATCH_FRAME_HEAD_LEN; ++i){
        temp = q->buf[(i + startPosition)   & DSC_RX_BUF_LEN_1];  //ֱ�ӷ��ʶ����е�Ԫ��
		if(temp == matchFrameHead[i])
			++matchResult;
		else {  //����ͬ��1
			if(matchResult == 0)
				matchResult = 0;
			else
				--matchResult;
		}
	}
	return matchResult;
}
/*
 * ����:ƥ��֡ͷ
 */
static UShort matchHead(Queue *q, QUEUE_DATA_TYPE * matchFrameHead)
{
	UShort len = queueLength(q);
	float  matchLen = 0;
	short carelessMatchRate=0,carefulMatchRate;
	short maxMatchRate = 0;
	short i=0,j=0,k=0;
	char temp_j = 0;
	UInt32 time =0;
	
	if(len >= DSC_RX_BUF_LEN_HALF)																//wait until enough length;
	{ //�����л�ȡ�ı��س��ȴ���2����  MATCH_FRAME_HEAD_LEN
		time=Clock_getTicks();
		matchLen = (len - MATCH_FRAME_HEAD_LEN-7)/3;
		for(i=0;i<matchLen;i++)																//detect 0~(MATCH_FRAME_HEAD_LEN/7+1)
		{//q->buf
			carelessMatchRate = carelessMatch(q,carelessHeadMat,q->front+3,CARELESS_HEAD_LEN);	//start position is q->front+3(middle sample : 3,10,17...)
			// if(carelessMatchRate >= CARELESS_HAD_MATCH_LEN)									    //40 has matched 35
			
			if(carelessMatchRate >= 28)								    //40 has matched 35
			{
				for(j=-3;j<=3;j++)																//find max value  2-1-6
				{
					carefulMatchRate = oneMatch(q,matchFrameHead,q->front+3+j,MATCH_FRAME_HEAD_LEN);
					if(carefulMatchRate >= MATCH_FRAME_HEAD_LEN-100)
					{
						if(carefulMatchRate > maxMatchRate)
						{
							maxMatchRate = carefulMatchRate;
							temp_j = j;
						}
						else
							break;
					}
				}

				if(maxMatchRate>0)
				{
					for(k=0;k<3+temp_j;k++)										//if matched success,delete queue size
						deQueue(q);
					return OK;														//match success
				}
				else
					for(j=0;j<3;j++)										//careless success,but careful unsuccess
						deQueue(q);
			}
			else
				for(j=0;j<3;j++)										//if matched unsuccessfully,delete queue size
					deQueue(q);
		}
		time = Clock_getTicks()-time;
		return FAIL;												//match unsuccessful
	}
	else
	{
		Task_sleep(20);												//no enough length,need delay 32ms,wait 40*0.8333;
		return FAIL;
	}
}
/*
 *���ܣ�������Զ����е�֡��Ϣ
 */
static void fillFrame_Queue(Queue *q, UShort *des, UShort startPosition)
{
	UShort len = DSC_RX_BUF_LEN_HALF;// - MATCH_FRAME_HEAD_SLOT_LEN; //��Queue��ȡ�ı���λ����
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
		--len;
		++count1;
		temp2 = q->buf[(q->front+start++)&DSC_RX_BUF_LEN_1];		//lao added
		bitCount += temp2;
		if(count1 == DSC_SAMPLE_SCALE){  //7bit
			count1 = 0;
			temp1 = (bitCount >= BIT_THREAD) ? 1 : 0;
			bitCount = 0; //���bitCount ֵ
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
/*
 *���ܣ���ȡDSC_FRAME_LENGTH�ֽڵı�׼DSC֡
 */
int testcount;
static UShort getNormalFrame(Queue *q, UShort *frameBuf)
{
	UShort startPosition = 0;
	UShort len = queueLength(q);
	if(len > DSC_RX_BUF_LEN_HALF + MATCH_FRAME_HEAD_LEN)
	{
		fillFrame_Queue(q, frameBuf, startPosition);
		return 1; //���ɹ�
	}
	return 0; //û�����
}
/*
 * ���ܣ������յ�������֡�����ظ�����Ϣ�ֿ�
 * src����Ҫ���ֿ���ԭʼ��Ϣ
 * des1��ԭʼ֡�ֳ��ĵ�һ����Ϣ
 * des2��ԭʼ֡�ֳ��ĵڶ�����Ϣ
 */
static void seperateInf(UShort *src, UShort *des1, UShort *des2)
{
	//UShort start1 = 12;   //��һ����Ϣ��ȡ��ʼ��ַ
	UShort start1 = 14;     //��ֵҲ�����14��Ҫ���ECC���㷽����ȷ��
	//UShort start2 = 17;   //�ڶ�����Ϣ��ȡ��ʼ��ַ
	UShort start2 = 19;     //��ֵҲ�����19��Ҫ���ECC���㷽����ȷ��
	UShort index = start1;
	UShort i = 0;
	UShort count = 0;
	//��ȡ��һ����Ϣ(A--I,22����Ϣ��)
	for(count = 0; count < dscInformationLen; ++count){		//hihh
		des1[i++] = src[index];
		index += 2;
	}
	//��ȡ�ڶ�����Ϣ(A--I,21����Ϣ��)
	i = 0;
	index = start2;
	for(count = 0; count < dscInformationLen; ++count){
		des2[i++] = src[index];
		index += 2;
	}
}

UChar  getInformationLen(const UShort *frameBuf)
{
	char i;
	char start  = 14;

	for(i = start; i < DSC_FRAME_LENGTH; i ++)		// 100
	{
		if((frameBuf[i]&0x7f)==117 || (frameBuf[i]&0x7f)==122 || (frameBuf[i]&0x7f)==127)
		{
			//�ж��Ƿ�Ϊ���գ���������Ҫ����չ����
			if((frameBuf[i] == frameBuf[i+4]) && (frameBuf[i] == frameBuf[i+6]))
			{
				if((frameBuf[12]&0x7f)==112 ||(frameBuf[14]&0x7f)==112 || (frameBuf[19]&0x7f)==112)
				{
					if((frameBuf[i+18]&0x7f)==117 || (frameBuf[i+18]&0x7f)==122 || (frameBuf[i+18]&0x7f)==127)
					{
						return (i-start)/2+2+9;
					}
					else
					{
						return 0;
					}
				}
				else
				{
					return (i-start)/2+2;
				}
			}
		}
	}

	return 0;
}


/*
 * ���ܣ�ȥ����������
 * ����size:ȥ����ݳ���
 */
void abandonQueue(Queue *q,UShort size)
{
	UShort i;
	for(i=0;i<size;i++)
		deQueue(q);
}

//************************************************************************************
/*Ŀ�ģ������У���ֵ�Ƿ���ȷ
 *      ��ȷ���� 1  ���򷵻� 0
 */
static UShort I_check(UShort *infBuf1, UShort *infBuf2)
{
	char zeroCountBit = 0;
	char index = 0;
	index = infBuf1[dscInformationLen-1] & 0x007F;
	zeroCountBit = (UChar)(infBuf1[dscInformationLen-1] >> 7);  //��ȡ�ල��
	if(zeroCountBit != checkErrorTable[index])
	{
		index = infBuf2[dscInformationLen-1] & 0x007F;
		zeroCountBit = (UChar)(infBuf2[dscInformationLen-1] >> 7);  //��ȡ�ල��
		if(zeroCountBit == checkErrorTable[index])
		{
			infBuf1[dscInformationLen-1] = infBuf2[dscInformationLen-1];
		}
		else
			return 0;
	}
	return 1;
}

/*
 *���ܣ�ģ�����
 */
static UShort mode2Adder(UShort num1, UShort num2, UShort addBitsNum)
{
	UShort addResult = 0;
	UShort i = 0;
	UShort mask = 0;
	UShort temp1 = 0;
	UShort temp2 = 0;

	for(i = 0; i < addBitsNum; ++i)		//7��
	{
		mask = 1 << i;
		temp1 = num1 & mask;
		temp2 = num2 & mask;
		addResult += ((temp1 + temp2) & mask);
		mask = 0;
	}
	return addResult;
}
static void checkEcc(const UShort *informationBuf,char len,errorInf *colum)
{
	UShort i = 0;
	UShort temp1 = 0;
	UShort temp2 = 0;
	UShort mode2AddResult = 0;
    //�����ֵ���������һ����ֵ
	for(i = 0; i < len; ++i)
	{
		mode2AddResult = mode2Adder(mode2AddResult, informationBuf[i], 7); //�ô���7��ʾ������֤7λ��Ϣ����
	}
	//ͳ��ģ����ӽ����ECC��ͬλ�ĸ���
	for(i = 0; i < 7; ++i)
	{
		temp1 = ((informationBuf[len] >> i) & 0x0001);
		temp2 = ((mode2AddResult >> i) & 0x0001);
		if(temp1 != temp2)
		{
			colum->count += 1;
		}
	}
}

/*
 * ���ܣ��м��
 */
static void checkColum(const UShort *infBuf, errorInf *colum)
{
	//������գ����ж�ǰecc�����ж���չ����ecc
	if((frameBuf[12]&0x7f)==112 || (frameBuf[16]&0x7f)==112 || (frameBuf[26]&0x7f)==112)	//�ж��Ƿ�Ϊ���գ���������Ҫ����չ����
	{
		//��ȥ����ecc����չ����
		checkEcc(infBuf,dscInformationLen-10,colum);  //����û�а�����չ����У��
		checkEcc(infBuf,dscInformationLen-1,colum);
	}
	else
		checkEcc(infBuf,dscInformationLen-1,colum);
}



/*
 * ���ܣ���?����
 * ����ֵ��0--�����޸���1--infBuf1 �޸��ɹ���2--infBuf2 �޸��ɹ�
 */
 
static UShort errorFixup(UShort *infBuf1, UShort *infBuf2)
{
	char zeroCountBit = 0;
	char i = 0;
	char index = 0;
	char flag   = 0;
	
	errorInf columError;
	columError.count = 0;	
	flag = I_check(infBuf1, infBuf2);
	
	if(flag == 1)
	{
		for(i = 0; i < dscInformationLen; ++i )
		{
			index = infBuf1[i] & 0x007F;
			zeroCountBit = (UChar)(infBuf1[i] >> 7);  //��ȡ�ල��
			if(zeroCountBit != checkErrorTable[index])
			{
				index = infBuf2[i] & 0x007F;
				zeroCountBit = (UChar)(infBuf2[i] >> 7);  //��ȡ�ල��
				if(zeroCountBit == checkErrorTable[index])
				{
					infBuf1[i] = infBuf2[i];
				}
				else
					return 0;
			}
		}

		//����У���λ��
		checkColum(infBuf1,&columError);
		if(columError.count == 0)
			return 1;
		else
			return 0;

	}
	else
	{
		return 0;
	}

}


/*
 * ���ܣ����
 * ������ݸ�ʽ��##&&TABBBCCCDDEEEFFFGHHHHHHHHHHHIIIIIIIIIIJ$$$
 */
void package(const UShort *in, UChar *out)
{
	UShort  i = 0;
	UShort  j = 0;
	for(j = 0; j < dscInformationLen; ++j)
	{
		out[i++] = (UChar)(in[j] & 0x007F);
	}
}
//=======================================================================================================
void printBuf(UShort *buf, int len)
{
	int i = 0;

	printf("The complete dsc frame is:\n");
	for(i = 0; i < len; ++i){
		printf("%x��",buf[i]);
	}
	printf("\n");
}
//=======================================================================================================
void printInf(unsigned char *buf)
{
	int i = 0;
	for(i = 0; i < 46; ++i)
	{
	}
}
extern Timer_Handle timer;
extern Void hwiFxn(UArg arg);
dsc_message_t *msg_dsc_se;
/*
 *���ܣ�DSC����������;
 */
int sendcount1,sendcount2,fixcount;
void DSC_RX(Queue *q)
{
	UShort matchFrameHeadFlag = FAIL;  //֡ͷƥ��ɹ���־
	UShort getFrameComFlag = 0;        //֡��Ϣ��ȡ��ɱ�־
	UShort status = 0;                 //��?�����־
	int status1 = 0;
	int eos_flag = 0;
	//����ƥ��֡ͷ
	DSCInformation = (UShort*)malloc(sizeof(UShort));
	DSCInformationCopy = (UShort*)malloc(sizeof(UShort));
	while(1)
	{
		/****��ƥ��֡ͷ  ****/
		if(matchFrameHeadFlag == FAIL)
		{
			matchFrameHeadFlag = matchHead(q, dscFrameHeadMat);
		}
		else
		{
			/****   ��ȡDSC����֡��Ϣ  ****/
			getFrameComFlag = getNormalFrame(q, frameBuf);
			if(getFrameComFlag == 1)
			{
				testcount++;
				getFrameComFlag = 0;
				matchFrameHeadFlag = FAIL;
				
				//��ȡ��Ч��Ϣ���ȣ�����ݸó����޸��ڴ�
				dscInformationLen = getInformationLen(frameBuf);

				abandonQueue(q,(dscInformationLen+9) * 2 * DSC_SAMPLE_SCALE * SLOT_BIT_NUM);	//delete the information data
				DSCInformation = (UShort*)realloc(DSCInformation,dscInformationLen*sizeof(UShort));
				DSCInformationCopy = (UShort*)realloc(DSCInformationCopy,dscInformationLen*sizeof(UShort));

				/****  �ֿ���Ϣ  ****/
				seperateInf(frameBuf, DSCInformation, DSCInformationCopy);
				/****  ��?����  ****/
				status = errorFixup(DSCInformation, DSCInformationCopy);
			    /****  �����  ****/
				if (DSCInformation[dscInformationLen - 2] == 127)
				{
					eos_flag = 1;
				}
				
				//���DSCInformation
				if((status == 1) && (eos_flag == 1))
				{
					status = 0;
					eos_flag = 0;
					
					package(DSCInformation, DSCSendBuf);
					msg_dsc_se = (dsc_message_t *)message_alloc(msgbuf[1], sizeof(dsc_message_t));
					if(!msg_dsc_se)
					{
						log_error("msg_dsc_se malloc fail");
					}
					memcpy(msg_dsc_se->data.dsc_msg, DSCSendBuf, 46);
					msg_dsc_se->data.mid=67;
					msg_dsc_se->data.dsc_len = dscInformationLen;
					status1 = messageq_send(&msgq[1],(messageq_msg_t)msg_dsc_se,0,0,0);
					if(status1 >= 0)
						sendcount1++;
					else
						log_error("send1 error");
					printInf(DSCSendBuf);
				}
				else
				{  //��Ϣ���?���޷��޸�
					fixcount++;
					status = 0;
				}
			}
			else
			{
				Task_sleep(10);//sleep
			}
		}
	}
}

