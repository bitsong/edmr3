/*
 * audio_loop.c
 *
 *  Created on: 2016-12-20
 *      Author: ws
 */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/timers/timer64/Timer.h>
#include <ti/sysbios/hal/Hwi.h>
#include <math.h>

#include "syslink_init.h"
#include "main.h"
#include "audio_queue.h"
#include "dsc_rx.h"
#include "amc_adf.h"
#include "samcoder.h"
#include "timer64.h"

#define SIZE1	72
#define SIZE2	720
#define DSC_TH	20
CSL_GpioRegsOvly     gpioRegs = (CSL_GpioRegsOvly)(CSL_GPIO_0_REGS);

//const Int BUFSIZE=3600;
extern float E_noise;
const float E_noise_th[10]={0.38,	0.31,	0.248,	0.185,	0.131,	0.085,	0.056,	0.036,  0.019,	0.013};
Timer_Handle 		timer;
const unsigned int dsc_ch[4]={221, 231, 236, 238};
unsigned int current_dscch, power_index;

#pragma DATA_ALIGN   (dsc_buf, 128)
unsigned char dsc_buf[4][DSC_BUF_SIZE ];
//short intersam[1200];
short send_buf[960];
float Buf10[720],Buf5[4800];
UInt	tx_flag, rx_flag, tx_submit, rx_submit;
Bool testing_tx, testing_rx;
Semaphore_Handle sem1,sem2,sem3,sem4;
unsigned char *buf_transmit;
struct rpe *prpe = &rpe[0];
rpe_attr_t attr=RPE_ATTR_INITIALIZER;
short buf_send[RPE_DATA_SIZE/2];
unsigned char buf_adc[BUFSIZE];
float eeprom_data[150];
short buf_de[320];
short buf_md[960];
Int RXSS_THRESHOLD =0,	TH_LEVEL=0;
float RSSI_db=0;
float channel_freq=0;
float transmit_power;
Short working_mode ,dsc_status;
int p_index0=0;
float error_rate[5];
//Short p_errorarray[256];
//Short p_arrayin[256][72];
double sys_time, exec_time,per_time;
int test_count0, test_count1,test_count2, test_count3,test_count4,test_count5;
short  ebapp,eflink,ebphy,flink;
extern void* bufRxPingPong[2];
extern void* bufTxPingPong[2];
extern int TxpingPongIndex,RxpingPongIndex;
extern unsigned int DSC_TEST_CNT[25];

unsigned char dsc_ch_flag, dsc_test_flag;
volatile unsigned int dsc_buf_index;

/* private functions */
Void smain(UArg arg0, UArg arg1);
Void task_enque(UArg arg0, UArg arg1);
Void task_io(UArg arg0, UArg arg1);
Void task_modulate(UArg arg0, UArg arg1);
Void hwiFxn(UArg arg);
Void clk0Fxn(UArg arg0);
Void DSCRxTask(UArg a0, UArg a1);
Void data_send(uint8_t *buf_16);
Void DSCScanningTask(UArg a0, UArg a1);

extern Void task_mcbsp(UArg arg0, UArg arg1);
extern int Rx_process(unsigned short* ad_data,	short* de_data);

void data_process(float *buf_in, unsigned char *buf_out, unsigned int size);
void eeprom_cache();
void sys_configure(void);
void dsp_logic();

/*
 *  ======== main =========
 */
Int main(Int argc, Char* argv[])
{
    Error_Block     eb;
    Task_Params     taskParams;

    log_init();
    tx_flag=0;
    rx_flag=1;
    Error_init(&eb);

    log_info("-->main:");
    /* create main thread (interrupts not enabled in main on BIOS) */
    Task_Params_init(&taskParams);
    taskParams.instance->name = "smain";
    taskParams.arg0 = (UArg)argc;
    taskParams.arg1 = (UArg)argv;
    taskParams.stackSize = 0x1000;
    taskParams.priority=3;
    Task_create(smain, &taskParams, &eb);
    if(Error_check(&eb)) {
        System_abort("main: failed to create application startup thread");
    }

    /* start scheduler, this never returns */
    BIOS_start();

    /* should never get here */
    log_info("<-- main:\n");
    return (0);
}


/*
 *  ======== smain ========
 */
Void smain(UArg arg0, UArg arg1)
{
    Error_Block		eb;
    Task_Params     taskParams;
    Int 			status=0;

    log_info("-->smain:");
    if((status=syslink_prepare())<0){
    	log_error("syslink prepare log_error status=%d",status);
       	return;
    }

    sem1=Semaphore_create(0,NULL,&eb);
    sem2=Semaphore_create(0,NULL,&eb);
    sem3=Semaphore_create(0,NULL,&eb);
    sem4=Semaphore_create(0,NULL,&eb);

    sys_configure();
    status=NO_ERR;
    Spi_dev_init();
    status=amc7823_init();
    if(status<0)
    	log_error("amc7823 initial error:%d",status);
    else
    	log_info("amc7823 initial done.");
    adf_init();
    current_dscch=231;
    eeprom_cache();
    adf_set_ch(231);
    dac_write(0, eeprom_data[24]+eeprom_data[25]);
    dac_write(3, 0);
    dac_write(1, 2.5);
    dac_write(2, 2.5);//test
    dac_write(4, 1.6);
    dac_write(5, 2.5);
    transmit_power=eeprom_data[39];
    buf_transmit=(unsigned char*)malloc(BUFSIZE);
    //task create
    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.instance->name = "task_mcbsp";
    taskParams.arg0 = (UArg)arg0;
    taskParams.arg1 = (UArg)arg1;
    taskParams.stackSize = 0x1000;
    taskParams.priority=2;
    Task_create(task_mcbsp, &taskParams, &eb);
    if(Error_check(&eb)) {
    	System_abort("main: failed to create application 1 thread");
    }

#if 1
    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.instance->name = "task_io";
    taskParams.arg0 = (UArg)arg0;
    taskParams.arg1 = (UArg)arg1;
    taskParams.stackSize = 0x8000;
    taskParams.priority=3;
    Task_create(task_io, &taskParams, &eb);
    if(Error_check(&eb)) {
        System_abort("main: failed to create application 0 thread");
    }

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.instance->name = "task_modulate";
    taskParams.arg0 = (UArg)arg0;
    taskParams.arg1 = (UArg)arg1;
    taskParams.stackSize = 0x6000;
    taskParams.priority=6;
    Task_create(task_modulate, &taskParams, &eb);
    if(Error_check(&eb)) {
        System_abort("main: failed to create application task_modulate thread");
    }
#endif

#if 1
    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.instance->name = "task_enque";
    taskParams.arg0 = (UArg)arg0;
    taskParams.arg1 = (UArg)arg1;
    taskParams.stackSize = 0x5000;
    taskParams.priority=6;
    Task_create(task_enque, &taskParams, &eb);
    if(Error_check(&eb)) {
    	System_abort("main: failed to create application 2 thread");
    }

#endif
//
    Task_Params_init(&taskParams);
	taskParams.priority = 2;
    taskParams.instance->name = "DSCScanningTask";
    taskParams.stackSize = 0x1000;
    Task_create(DSCScanningTask, &taskParams, NULL);
    if(Error_check(&eb)) {
           System_abort("main: failed to create application DSCScanningTask thread");
    }

    Task_Params_init(&taskParams);
	taskParams.priority = 1;
    taskParams.instance->name = "DSCRxTask";
    taskParams.stackSize = 0x5000;
    Task_create(DSCRxTask, &taskParams, NULL);
    if(Error_check(&eb)) {
           System_abort("main: failed to create application DSCRxTask thread");
    }


//    Timer_Params 		timerParams;
//    Error_init(&eb);
//    Timer_Params_init(&timerParams);
//    timerParams.startMode= Timer_StartMode_AUTO;
//    timerParams.period =1200000;
//    timer = Timer_create(1, clk0Fxn, &timerParams, &eb);
//    if (timer == NULL) {
//    	System_abort("Timer create failed");
//    }
//    else
//    	log_info("Timer create success");

    setup_TIMER(2, 119, hwiFxn);

    for(;;){
    	Task_sleep(20);

    	RSSI_db=-24.288+10*log10(RSSI)-eeprom_data[84];//120      -125_10
    	if(RSSI_db<-115.4)
    		// y = - 0.16365*x^{2} - 36.798*x - 2182.9
    		//RSSI_db=-0.16365*RSSI_db*RSSI_db-36.798*RSSI_db-2182.9;
    		//fitting : y = 0.071842*x^{3} + 25.165*x^{2} + 2939.6*x + 1.144e+05
    		RSSI_db=0.071842*powi(RSSI_db,3)+ 25.165*powi(RSSI_db,2)+ 2939.6*RSSI_db + 114395;
    	if(RSSI_db<-135)
    		RSSI_db=-135;
    	if(dsc_buf_index>18000)
    		dsc_buf_index=0;
    	dsp_logic();
    }

    //cleanup
//        sync_send(SYNC_CODE_MSGOUT);
//        sync_send(SYNC_CODE_NTFOUT);
//        sync_send(SYNC_CODE_RPEOUT);
//        status=syslink_cleanup();
//        if(status<0){
//        	log_error("syslink cleanup failed!");
//        }
}


/* Here on dsc_timer interrupt */
Void hwiFxn(UArg arg)
{
	dsc_buf[current_dscch][dsc_buf_index++]=CSL_FEXT(gpioRegs->BANK[1].IN_DATA,GPIO_IN_DATA_IN1);
}

/*
 *  ======== clk0Fxn =======
 */
Void clk0Fxn(UArg arg0)
{
//	UInt key;
    static int count=2;
    UInt32 timeout, timecount;

    timecount = count++%4;
#if 0
    if(0==timecount){
    	timeout=1200000;
    	count=1;
		CSL_FINS(gpioRegs->BANK[4].OUT_DATA,GPIO_OUT_DATA_OUT13,1);
    }
    else{
		CSL_FINS(gpioRegs->BANK[4].OUT_DATA,GPIO_OUT_DATA_OUT13,0);
    	timeout=1200000;
    }
#else

    timeout=1200000;
    //bypass channel 0:ask
    if(0==timecount){
    	timecount=1;
    	count=2;
    }

#endif
    adf_set_ch(dsc_ch[timecount]);
    current_dscch=timecount;
    dsc_buf_index=0;
    dsc_ch_flag=current_dscch+1;
//    key=Hwi_disable();
    Timer_stop(timer);
    Timer_setPeriodMicroSecs(timer, timeout);
    Timer_start(timer);
//    Hwi_restore(key);
//    add_timer(2, timeout);
}

void clock_add(Timer_Handle handle, UInt32 ticks_us)
{
	UInt key;

    key=Hwi_disable();
    Timer_setPeriodMicroSecs(handle, ticks_us);
    Timer_start(handle);
    Hwi_restore(key);
}
/*
 *  ======== DSCRxTask ========
 */
Void DSCRxTask(UArg a0, UArg a1)
{
	DSC_RX();
}


/****************************
***       HPF函数  and energy   **********
*****************************/
void DSC_Dete_HPF(unsigned char *inBuf,float *outBuf)
{
	register short i = 0;
	static float temp[436] = {0};
	float coffe0=0.752144544489035,coffe1=-0.222634817517790, coffe2=-0.156529751247228,coffe3=-0.073745158087348,
		  coffe4=-0.002078714878018,coffe5= 0.039022173303044, coffe6=0.045410806593024,coffe7=0.027304738142922,coffe8=0.001888797891960;
	for(i = 0; i < 16; i++)
		temp[i] = temp[420 + i];
	for(i=0; i<420;i++)
		temp[i+16] = inBuf[i];
	for(i = 0; i < 420; i++)
	{
		outBuf[i] = temp[i+8]*coffe0+(temp[i+7]+temp[i+9])*coffe1+(temp[i+6]+temp[i+10])*coffe2
		+(temp[i+5]+temp[i+11])*coffe3 + (temp[i+4]+temp[i+12])*coffe4+(temp[i+3]+temp[i+13])*coffe5
		+(temp[i+2]+temp[i+14])*coffe6 + (temp[i+1]+temp[i+15])*coffe7 + (temp[i+0]+temp[i+16])*coffe8;
	}
}
/*****************************
***   求平方和函数  **********
******************************/
float En_Value(float *inBuf)
{
	static float energy_avr[DSC_AVR];
	static int j;
	int i=0;
	float sum=0;
	float sum_sum=0;

	for(i=0;i<420;i++)
	{
		sum += inBuf[i]*inBuf[i];
	}
	for(i=0; i< DSC_AVR-1; i++)
		energy_avr[i]=energy_avr[i+1];
	energy_avr[i]=sum;

	for(i=0; i<DSC_AVR; i++){
		sum_sum +=energy_avr[i];
	}
	j++;
	if(j>DSC_AVR)
		j=DSC_AVR;
	return sum_sum/j;
}

float energy_dsc;
Void DSCScanningTask(UArg a0, UArg a1)
{
    static int count=1;
    UInt32  timecount;
    unsigned char *ptr_scanning;
    float temp_buf[SCAN_LEN];
//    float energy_dsc;

    while(1){
		ptr_scanning=(unsigned char *)dsc_buf;
		timecount = count++%4;
		//bypass channel 0:ask
		if(0==timecount){
			 timecount=1;
			 count=2;
			 CSL_FINS(gpioRegs->BANK[1].OUT_DATA,GPIO_OUT_DATA_OUT29,1);
		}
		else if(2==timecount){
			CSL_FINS(gpioRegs->BANK[1].OUT_DATA,GPIO_OUT_DATA_OUT29,0);
		}
		dsc_buf_index=0;
		if(dsc_test_flag!=1){
			current_dscch=timecount;
			adf_set_ch(dsc_ch[timecount]);
		}
		Task_sleep(150);
		ptr_scanning+=timecount*DSC_BUF_SIZE+840;
		DSC_Dete_HPF(ptr_scanning, temp_buf);
		energy_dsc=En_Value(temp_buf);

		if(energy_dsc>DSC_TH){
			continue;
		}
		else{
			dsc_buf_index=0;
			Task_sleep(890);
			dsc_ch_flag=current_dscch+1;
		}
	}
}
/*
 *功能：信号发送时低通滤波
 *参数：inBuf:输入为960个数据的指针; outBuf:输出为低通滤波后数据指针; len:输入数据的长度（默认960）
 */
void LP_Filter0(short *inBuf,short *outBuf)
{
	register short i = 0;
	static float temp[992] = {0};
	float coffe0=0.271241480518137,coffe1=0.239449787069302, coffe2= 0.157378145257887,coffe3=  0.058305139382982 ,
	coffe4=-0.020800133712440,coffe5=-0.056459523840367,coffe6=-0.047850447855385,coffe7=-0.013826369480974  ,
	coffe8=0.019517316087605, coffe9=  0.033169629000253,coffe10=0.023579105912583, coffe11=0.001367819251319  ,
	coffe12=-0.017511867274160 , coffe13=-0.022085670223589 ,coffe14=-0.012012239345358 ,coffe15=0.003998245857162,
	coffe16=0.014966385905624;

	for(i = 0; i < 32; ++i)
		temp[i] = temp[960 + i];
	for(i=0; i<960;i++)
		temp[i+32] = inBuf[i];
	//
	for(i = 0; i < 960; ++i){
		outBuf[i] = temp[i+16]*coffe0
		+(temp[i+17]+temp[i+15])*coffe1 + (temp[i+18]+temp[i+14])*coffe2  + (temp[i+19]+temp[i+13])*coffe3  + (temp[i+20]+temp[i+12])*coffe4
		+(temp[i+21]+temp[i+11])*coffe5 + (temp[i+22]+temp[i+10])*coffe6  + (temp[i+23]+temp[i+9])*coffe7   + (temp[i+24]+temp[i+8])*coffe8
		+(temp[i+25]+temp[i+7])*coffe9  + (temp[i+26]+temp[i+6])*coffe10  + (temp[i+27]+temp[i+5])*coffe11  + (temp[i+28]+temp[i+4])*coffe12
		+(temp[i+29]+temp[i+3])*coffe13 + (temp[i+30]+temp[i+2])*coffe14  + (temp[i+31]+temp[i+1])*coffe15  + (temp[i+32]+temp[i])*coffe16;
	}
}
void LP_Filter(float *inBuf,float *outBuf)
{
	register short i = 0;
	static float temp[992] = {0};
	float coffe0=0.271241480518137,coffe1=0.239449787069302, coffe2= 0.157378145257887,coffe3=  0.058305139382982 ,
	coffe4=-0.020800133712440,coffe5=-0.056459523840367,coffe6=-0.047850447855385,coffe7=-0.013826369480974  ,
	coffe8=0.019517316087605, coffe9=  0.033169629000253,coffe10=0.023579105912583, coffe11=0.001367819251319  ,
	coffe12=-0.017511867274160 , coffe13=-0.022085670223589 ,coffe14=-0.012012239345358 ,coffe15=0.003998245857162,
	coffe16=0.014966385905624;

	for(i = 0; i < 32; ++i)
		temp[i] = temp[960 + i];
	for(i=0; i<960;i++)
		temp[i+32] = inBuf[i];
	//
	for(i = 0; i < 960; ++i){
		outBuf[i] = temp[i+16]*coffe0
		+(temp[i+17]+temp[i+15])*coffe1 + (temp[i+18]+temp[i+14])*coffe2  + (temp[i+19]+temp[i+13])*coffe3  + (temp[i+20]+temp[i+12])*coffe4
		+(temp[i+21]+temp[i+11])*coffe5 + (temp[i+22]+temp[i+10])*coffe6  + (temp[i+23]+temp[i+9])*coffe7   + (temp[i+24]+temp[i+8])*coffe8
		+(temp[i+25]+temp[i+7])*coffe9  + (temp[i+26]+temp[i+6])*coffe10  + (temp[i+27]+temp[i+5])*coffe11  + (temp[i+28]+temp[i+4])*coffe12
		+(temp[i+29]+temp[i+3])*coffe13 + (temp[i+30]+temp[i+2])*coffe14  + (temp[i+31]+temp[i+1])*coffe15  + (temp[i+32]+temp[i])*coffe16;
	}
}
/*
 *预加重
 */
void sendPreEmphasis(short *inBuf,float *outBuf,short len)
{
	register short i = 0;
	static short firstData = 0;
	outBuf[0] = inBuf[0] - (0.91*firstData);
	for(i=1;i<len;i++)
		outBuf[i] = inBuf[i] - (0.91*inBuf[i-1]);
	firstData = inBuf[len-1];
}

/*
 *功能：24k采样转换120k
 *参数：inBuf:输入为960个数据的指针; outBuf:输出为4800个数据的指针; len:输入数据的长度（默认960）
 */
void from24To120(float *inBuf,float *outBuf,short len)
{
	short i;
	static float tempBuf[967] = {0};
	float	coffe0=0.154830207920112,	coffe1=0.147009839249942,	coffe2=0.125337104718526,	coffe3=0.094608021712450,
			coffe4=0.061139668733940,	coffe5=0.030930241474728,	coffe6=0.008127364075467,	coffe7=-0.005711418466492,
			coffe8=-0.011436697663118,  coffe9=-0.011435901979223,	coffe10=-0.008517719961896, coffe11=-0.004993842265637,
			coffe12=-0.002219513516004, coffe13=-0.000610415386896, coffe14=0.000034684251061,	coffe15=0.000140032519269;

	for(i=0;i<7;i++)
		tempBuf[i] = tempBuf[960+i];
	for(i=0;i<960;i++)
		tempBuf[i+7] = inBuf[i];

		for(i=0;i<len;i++)
		{
			outBuf[5*i]   = coffe0*tempBuf[i+3] + (coffe5*tempBuf[i+2]) + (coffe5*tempBuf[i+4]) + (coffe10*tempBuf[i+1]) + (coffe10*tempBuf[i+5]) + (coffe15*tempBuf[i]) + (coffe15*tempBuf[i+6]);
			outBuf[5*i+1] = coffe1*tempBuf[i+3] + (coffe6*tempBuf[i+2]) + (coffe4*tempBuf[i+4]) + (coffe11*tempBuf[i+1]) + (coffe9*tempBuf[i+5]) + (coffe14*tempBuf[i+6]);
			outBuf[5*i+2] = coffe2*tempBuf[i+3] + (coffe7*tempBuf[i+2]) + (coffe3*tempBuf[i+4]) + (coffe12*tempBuf[i+1]) + (coffe8*tempBuf[i+5]) + (coffe13*tempBuf[i+6]);
			outBuf[5*i+3] = coffe3*tempBuf[i+3] + (coffe8*tempBuf[i+2]) + (coffe2*tempBuf[i+4]) + (coffe13*tempBuf[i+1]) + (coffe7*tempBuf[i+5]) + (coffe12*tempBuf[i+6]);
			outBuf[5*i+4] = coffe4*tempBuf[i+3] + (coffe9*tempBuf[i+2]) + (coffe1*tempBuf[i+4]) + (coffe14*tempBuf[i+1]) + (coffe6*tempBuf[i+5]) + (coffe11*tempBuf[i+6]);
		}
}
/*
 *功能：数据发送去直流
 *参数：inBuf:输入为960个数据的指针，并带回960个去直流后数据; len:输入数据的长度（默认960）
 */
//void delDc(short *inbuf,short len)
//{
//	short i;
//	int sun=0;
//	short average;
//	for(i=0;i<len;i++)
//		sun = sun + inbuf[i];
//	average = sun/len;
//	for(i=0;i<len;i++){
//		inbuf[i] -= average;
//	}
//
//}

void scopeLimit(float *inBuf,short len)
{
	short i = 0;
	for(i=0;i<len;i++)
	{
		if(inBuf[i]>16000.0)
			inBuf[i] = 16000.0;
		else if(inBuf[i]<-16000.0)
			inBuf[i] = -16000.0;
	}
}

void hpFilter(short *inBuf,short *outBuf)
{
	short i = 0;
	static float x[962] = {0};
	static float y[962] = {0};

	x[0] = x[960];
	x[1] = x[961];

	y[0] = y[960];
	y[1] = y[961];

	for(i=0;i<960;i++)
		x[i+2] = inBuf[i];

	for(i=0;i<960;i++)
		y[i+2] = 0.937260390269893*(x[i+2]-2*x[i+1]+x[i]) + 1.870580640735279*y[i+1] - 0.878460920344291*y[i];

	for(i=0;i<960;i++)
		outBuf[i] = y[i+2];
}


/*
 *功能：数据发送端数据处理。（64阶低通滤波、预加重、24k->120k处理）
 *参数：inBuf:输入为960个数据的指针; outBuf:输出为4800个数据的指针; len:输入数据的长度（默认960）
 */
void dataFilterAndTrans(short *inBuf,float *outBuf,short len)
{
//	delDc(inBuf,len);
	hpFilter(inBuf, inBuf);
	sendPreEmphasis(inBuf,outBuf,len);		//input:outBuf,output:inBuf(inBuf as a temp buffer)
	scopeLimit(outBuf,len);
	LP_Filter(outBuf,outBuf);
	from24To120(outBuf,outBuf,len);
}


/*
function: insert data 1:5
inBuf size:720
outBuf size:3600
*/
void from24To120d(float *inBuf,float *outBuf)
{
	short i;
	static float temp[SIZE2+7] = {0};
	float	coef0=0.154830207920112,	coef1=0.147009839249942,	coef2=0.125337104718526,	coef3=0.094608021712450,
			coef4=0.061139668733940,	coef5=0.030930241474728,	coef6=0.008127364075467,	coef7=-0.005711418466492,
			coef8=-0.011436697663118,  coef9=-0.011435901979223,	coef10=-0.008517719961896, coef11=-0.004993842265637,
			coef12=-0.002219513516004, coef13=-0.000610415386896, coef14=0.000034684251061,	coef15=0.000140032519269;

	for(i=0;i<7;i++)
		temp[i] = temp[SIZE2+i];
	for(i=0;i<SIZE2;i++)
		temp[i+7] = inBuf[i];

		for(i=0;i<SIZE2;i++)
		{
			outBuf[5*i]   = coef0*temp[i+3] + (coef5*temp[i+2]) + (coef5*temp[i+4]) + (coef10*temp[i+1]) + (coef10*temp[i+5]) + (coef15*temp[i]) + (coef15*temp[i+6]);
			outBuf[5*i+1] = coef1*temp[i+3] + (coef6*temp[i+2]) + (coef4*temp[i+4]) + (coef11*temp[i+1]) + (coef9*temp[i+5]) + (coef14*temp[i+6]);
			outBuf[5*i+2] = coef2*temp[i+3] + (coef7*temp[i+2]) + (coef3*temp[i+4]) + (coef12*temp[i+1]) + (coef8*temp[i+5]) + (coef13*temp[i+6]);
			outBuf[5*i+3] = coef3*temp[i+3] + (coef8*temp[i+2]) + (coef2*temp[i+4]) + (coef13*temp[i+1]) + (coef7*temp[i+5]) + (coef12*temp[i+6]);
			outBuf[5*i+4] = coef4*temp[i+3] + (coef9*temp[i+2]) + (coef1*temp[i+4]) + (coef14*temp[i+1]) + (coef6*temp[i+5]) + (coef11*temp[i+6]);
		}
}
/*
function: insert data 1:10
inBuf size:72
outBuf size:720
*/
#if 0
void rrcFilter(short *inBuf,float *outBuf)
{
	short i = 0;
	static float temp[SIZE1+4] = {0};

	const float coef0= 0.100000000000000, coef1= 0.096112668720556,
		 coef2= 0.085202893267106, coef3= 0.069328801981117,coef4= 0.051318575730875, coef5= 0.034057202788830,
		 coef6= 0.019804789400712, coef7= 0.009738296381340,coef8= 0.003832472937066, coef9= 0.001086839573313,
		 coef10= 0.000000000000000,
		coef11=0.000000000000000, coef12=0.000000000000000, coef13=0.000000000000000, coef14=0.000000000000000,
		coef15=0.000000000000000,coef16=0.000000000000000;

	for(i=0;i<4;i++)
		temp[i] = temp[i+SIZE1];
	for(i=0;i<SIZE1;i++)
		temp[i+4] = inBuf[i];
	for(i=0;i<SIZE1;i++)
	{
		outBuf[10*i]  = temp[i+1]*coef5  + temp[i+2]*coef5 + temp[i+3]*coef15+temp[i]*coef15;
		outBuf[10*i+1]= temp[i+1]*coef6  + temp[i+2]*coef4 + temp[i+3]*coef14;
		outBuf[10*i+2]= temp[i+1]*coef7  + temp[i+2]*coef3 + temp[i+3]*coef13;
		outBuf[10*i+3]= temp[i+1]*coef8  + temp[i+2]*coef2 + temp[i+3]*coef12;
		outBuf[10*i+4]= temp[i+1]*coef9  + temp[i+2]*coef1 + temp[i+3]*coef11;
		outBuf[10*i+5]= temp[i+1]*coef10 + temp[i+2]*coef0 + temp[i+3]*coef10;
		outBuf[10*i+6]= temp[i+1]*coef11 + temp[i+2]*coef1 + temp[i+3]*coef9;
		outBuf[10*i+7]= temp[i+1]*coef12 + temp[i+2]*coef2 + temp[i+3]*coef8;
		outBuf[10*i+8]= temp[i+1]*coef13 + temp[i+2]*coef3 + temp[i+3]*coef7;
		outBuf[10*i+9]= temp[i+1]*coef14 + temp[i+2]*coef4 + temp[i+3]*coef6;
	}
}
#else
void rrcFilter(short *inBuf,float *outBuf)
{
	short i = 0;
	static float temp[SIZE1+1] = {0};

	//coef0= 0.000000000000000,
	float  coef1= 0.000049374050532, coef2 = 0.000731373552360, coef3= 0.003301607415752,
		   coef4= 0.009578085548176, coef5= 0.021321677321237, coef6 = 0.039068021702553, coef7= 0.061044810757830,
		   coef8= 0.083002555065670, coef9= 0.099385268589141, coef10= 0.105464790894703;

	for(i=0;i<1;i++)
		temp[i] = temp[i+SIZE1];
	for(i=0;i<SIZE1;i++)
		temp[i+1] = inBuf[i];
	for(i=0;i<SIZE1;i++)
	{
		outBuf[10*i]  = temp[i]*coef10;
		outBuf[10*i+1]= temp[i]*coef9  + temp[i+1]*coef1;
		outBuf[10*i+2]= temp[i]*coef8  + temp[i+1]*coef2;
		outBuf[10*i+3]= temp[i]*coef7  + temp[i+1]*coef3;
		outBuf[10*i+4]= temp[i]*coef6  + temp[i+1]*coef4;
		outBuf[10*i+5]= temp[i]*coef5  + temp[i+1]*coef5;
		outBuf[10*i+6]= temp[i]*coef4  + temp[i+1]*coef6;
		outBuf[10*i+7]= temp[i]*coef3  + temp[i+1]*coef7;
		outBuf[10*i+8]= temp[i]*coef2  + temp[i+1]*coef8;
		outBuf[10*i+9]= temp[i]*coef1  + temp[i+1]*coef9;
	}
}
#endif


/* 函数说明：IIR二阶滤波器
 *           2 Order
 *
 */
void iirFilter_AfterCodec(short *inBuf,short *outBuf,short len)
{
	short i = 0;
	static short tempIn[322]  = {0};
	static short tempOut[322] = {0};
	for(i=0;i<2;i++)
	{
		tempIn[i] = tempIn[len+i];
		tempOut[i]= tempOut[len+i];
	}
	for(i=0;i<len;i++)
		tempIn[i+2] = inBuf[i];
	for(i=0;i<len;i++)
		tempOut[i+2] = 0.7305032*(tempIn[i+2] - tempIn[i])+0.1830361*tempOut[i+1]+0.4610063*tempOut[i];
	for(i=0;i<len;i++)
		outBuf[i] = tempOut[i+2];
}

float p_statics(short *pbufp, int index0)
{
	int i,j,csum;
	float p_error_rate=0;	//frame BER
    short data[4]={0},cdata[4]={0};
	csum=0;

	for(j=0;j<8;j++){		//bin2dec
		for(i=0;i<4;i++){
			if(-1==pbufp[35-j-8*i])
				pbufp[35-j-8*i]=0;
				data[i]+=pbufp[35-j-8*i]<<j;
		}
	}
	for(i=0;i<4;i++){
		cdata[i]=data[i];
		data[i]=0;
	}
//	p_errorarray[index0]=cdata[0];
	for(i=0;i<3;i++)		//everyone compare with each other
		for(j=i+1;j<4;j++){
			if(cdata[i]==cdata[j]){
				csum++;
//				p_errorarray[index0]=cdata[i];
			}
		}
//	memset(cdata, 0, 8);
	if(6==csum)			//the sum
		p_error_rate=0;
	else if(3==csum)
		p_error_rate=0.25;
	else if(2==csum||1==csum)
		p_error_rate=0.5;
    else
    	p_error_rate=1;

	csum=0;
	return p_error_rate;
}

Void statics_fec(fecfrm_stat_t fec_state)
{
	ebapp += fec_state.ebapp;
	flink += (fec_state.efree + fec_state.fixed);
	eflink+= fec_state.error;
	ebphy += fec_state.ebits;
}
Void caculate_ber(short total_frame)
{
	//APP  /bit
	error_rate[0]=(double)ebapp/(flink*8*18);
	//LINK /FEC frame
//	error_rate[1]=(double)eflink/(total_frame*18);
	error_rate[1]=(double)total_frame/1000;
	//PHY /bit
	error_rate[2]=(double)ebphy/(total_frame*216);
}

void adjust_amp(short *input)
{
	int i;
	short *p=input;

	for(i=0;i<RPE_DATA_SIZE;i++){
		*p=1<<(*p);
		p++;
	}
}
#if 1
Void task_io(UArg arg0, UArg arg1)
{
    Error_Block     eb;
	uint8_t 		*buf = NULL;
    uint32_t		size;
    Int status=0,p2sum=0;
    Bool sp_init=FALSE;
    static short sp_count, total_frames, rev_count;
	short *pbuf, *pbuf2;
	short i, j;
	static short p_index;
    samcoder_t *dcoder0=samcoder_create(MODE_1200);
	short frame0[72];
	short outbuf[320];
	const short syn_p0[72]={
			 1, -1, -1,  1,  1, -1, -1,  1,  1, -1,  1, -1,
			-1,  1,  1, -1, -1,  1 ,-1 , 1 ,-1,  1, -1,  1,
			-1,  1,  1, -1,  1, -1, -1,  1, -1,  1,  1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	};
	short syn_p[72];
	int nsamples=samcoder_samples_per_frame(dcoder0);
	int nbits=samcoder_bits_per_frame(dcoder0);
	int nsize=samcoder_fecsize_per_frame(dcoder0);
	int nfecs=samcoder_fecs_per_frame(dcoder0);

	memcpy(syn_p, syn_p0, 144);
    log_info("-->task_receive:");
    Error_init(&eb);
    fecfrm_stat_t fec_state;
    samcoder_set_test_patdata(dcoder0, dpat_tab[0]);

    while(1){
    	if(1==rx_flag){
    		sp_init=FALSE;
    		if(FALSE==Semaphore_pend(sem4,45))
    			continue;
    		test_count2++;
    		exec_time=system_time()-sys_time;
    		sys_time=system_time();
    		if(working_mode==DIGITAL_MODE){
        		pbuf=buf_de;
//        		memcpy(p_arrayin[p_index0],pbuf,72*2);//
        		pbuf+=36;
        		for(j=0; j<36; j++){
        			if(pbuf[j]==-1)
        				pbuf[j]=0;
        			p2sum +=pbuf[j];
        		}
//        		p_errorarray[p_index0++]=p2sum;
        		if(p2sum>32){
        			testing_rx=TRUE;
        			total_frames++;
        		}
        		//test finish,submit result,test quit
        		if(p2sum<10 && testing_rx==TRUE){
        			caculate_ber(total_frames);
        			p_index0=total_frames=0;
//            		memset(p_errorarray, 0, 512);
            		ebapp=eflink=ebphy=flink=0;
        			rx_submit=1;
        			rx_flag=0;
        			continue;
        		}
        		p2sum=0;
    			pbuf+=36;

    			pbuf=buf_de+72;
    		}
    		else
    			pbuf2=buf_md;

    		for(i=0;i<3;i++){
    			if(testing_rx==TRUE && working_mode==DIGITAL_MODE){
    				samcoder_decode_verbose(dcoder0, pbuf, buf_send, &fec_state);
    				statics_fec(fec_state);
    				pbuf+=72;
    			}else if(working_mode==DIGITAL_MODE){
    				samcoder_decode( dcoder0, pbuf, buf_send);
//    				adjust_amp(buf_send);
    				data_send((uint8_t*)buf_send);
    				pbuf+=72;
    				test_count1++;
    			}else{
    				data_send((uint8_t*)pbuf2);
    				pbuf2+=320;
    			}
    		}
    		sp_count=0;
    	}
    	else  if(1==tx_flag){
       		if(dsc_status==0 && working_mode==DIGITAL_MODE)
        			rev_count=1;
        		else
        			rev_count=3;
    		if(FALSE==Semaphore_pend(sem3,45))
    			continue;
    		while(rev_count-->0){
    			//BER test
    			if(testing_tx==TRUE && working_mode==DIGITAL_MODE){
    				if((sp_count==3||sp_init==FALSE) && p_index!=252){
    					if(0==sp_count)
    						sp_init=TRUE;
    					for(j=0; j<36; j++){			//set p2
    						if(p_index<250)
    							syn_p[36+j]=1;
    						else
    							syn_p[36+j]=-1;
    					}
//            			memcpy(p_arrayin[p_index],syn_p,72*2);
            			sp_count=0;
            			p_index++;
            			test_count1++;
            		    rrcFilter(syn_p, Buf10);
            		    from24To120d(Buf10,(float*)Buf5);
    				}else if(sp_count==3 && p_index==252){
        				p_index=sp_count=0;
        				sp_init=FALSE;
        				memcpy(syn_p, syn_p0, 144);
            			rpe_flush(prpe,RPE_ENDPOINT_READER,TRUE,&attr);//返回 flush的数据量
            			dac_write(3, 0);
            			CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT6,0); //TX_SW
            			CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT7,1);//RX_SW
            			CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT13,0); //R:F1
            			rx_submit=1;
        				tx_flag=0;
        				memset(buf_transmit,0x88,BUFSIZE);
//            			rx_flag=1;
            			continue;
    				}else if(sp_count!=3){
    					samcoder_encode_patdata(dcoder0, frame0);
    					sp_count++;

            		    rrcFilter(frame0, Buf10);
            		    from24To120d(Buf10,(float*)Buf5);
    				}
    			//speech send
    			}else if(dsc_status==0 && working_mode==DIGITAL_MODE && (sp_count==3 || sp_init==FALSE)){    	    //send syn_p
    				if(0==sp_count)
    					sp_init=TRUE;
        			test_count3++;
        		    rrcFilter(syn_p, Buf10);
        		    from24To120d(Buf10,(float*)Buf5);
        		    sp_count=0;
        		}
//    			else if(working_mode!=DIGITAL_MODE || sp_init==TRUE || sp_count!=3){
    			else
    			{
        			size = RPE_DATA_SIZE;
        			status = rpe_acquire_reader(prpe,(rpe_buf_t*)&buf,&size);
        			if(status == ERPE_B_PENDINGATTRIBUTE){
        				status = rpe_get_attribute(prpe,&attr,RPE_ATTR_FIXED);
        				if(!status && attr.type == RPE_DATAFLOW_END){
        					memset(buf_transmit,0x88,BUFSIZE);
        					tx_flag=0;
        					dac_write(3, 0);
        					CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT6,0); //TX_SW
        					CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT7,1);//RX_SW
        					CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT13,0); //R:F1
        					rx_submit=1;
        					continue;
        				}
    				}else if(status < 0){
    					Task_sleep(1);
    					continue;
    				}
        			if(dsc_status==0 && working_mode==DIGITAL_MODE){
        				if(size!=RPE_DATA_SIZE)
        					log_warn("data not enough,size=%d",size);
        				iirFilter_AfterCodec((short *)buf, outbuf, 320);
        				samcoder_encode( dcoder0, outbuf, frame0);
        				rrcFilter(frame0,Buf10);
        				from24To120d(Buf10,(float*)Buf5);
        				sp_count++;
        			}
        			else{
        				memcpy((uint8_t*)send_buf+(2-rev_count)*640, buf, size);
        				if(0==rev_count){
        					dataFilterAndTrans(send_buf,Buf5,960);
        				}
        			}
    				status = rpe_release_reader_safe(prpe,buf,size);
    				if(status < 0){
    					log_warn("rpe release writer end failed!");
    					continue;
    				}
        		}
    		}

    	}
    	else
    		Task_sleep(2);
    }
}
#else
void date_rev(short *iobuf)
{
	uint8_t 		*buf = NULL;
    uint32_t		size;
    Int status=0;

    while(1==tx_flag){
    	size = RPE_DATA_SIZE;
    	status = rpe_acquire_reader(prpe,(rpe_buf_t*)&buf,&size);
    	if(status == ERPE_B_PENDINGATTRIBUTE){
    		status = rpe_get_attribute(prpe,&attr,RPE_ATTR_FIXED);
    		if(!status && attr.type == RPE_DATAFLOW_END){
    			memset(buf_transmit,0x88,RPE_DATA_SIZE/2*15);
    			tx_flag=0;
    			dac_write(3, 0);
    			CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT6,0); //TX_SW
    			CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT7,1);//RX_SW
    			CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT13,0); //R:F1
    			rx_submit=1;
    			test_count5++;
    			continue;
    		}
    	}else if(status < 0){
    		Task_sleep(1);
    		continue;
    	}
    	memcpy((uint8_t *)iobuf, buf, size);
    	status = rpe_release_reader_safe(prpe,buf,size);
    	if(status < 0){
    		log_warn("rpe release writer end failed!");
    		continue;
    	}else
    		break;
    }
    return;
}

Void task_io(UArg arg0, UArg arg1)
{
    int p2sum=0;
    Bool sp_init=FALSE;
    static short sp_count, total_frames, rev_count;
	short *pbuf, *pbuf2;
	short i, j;
	static short p_index;
    samcoder_t *dcoder0=samcoder_create(MODE_1200);
	short frame0[72];
	short inbuf[320],outbuf[320];
	const short syn_p0[72]={
			 1, -1, -1,  1,  1, -1, -1,  1,  1, -1,  1, -1,
			-1,  1,  1, -1, -1,  1 ,-1 , 1 ,-1,  1, -1,  1,
			-1,  1,  1, -1,  1, -1, -1,  1, -1,  1,  1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	};
	short syn_p[72];
	int nsamples=samcoder_samples_per_frame(dcoder0);
	int nbits=samcoder_bits_per_frame(dcoder0);
	int nsize=samcoder_fecsize_per_frame(dcoder0);
	int nfecs=samcoder_fecs_per_frame(dcoder0);

	memcpy(syn_p, syn_p0, 144);
    log_info("-->task_io:");

    fecfrm_stat_t fec_state;
    samcoder_set_test_patdata(dcoder0, dpat_tab[0]);

    while(1){
    	if(1==rx_flag){
    		sp_init=FALSE;
    		if(FALSE==Semaphore_pend(sem4,45))
    			continue;
    		test_count2++;
    		exec_time=system_time()-sys_time;
    		sys_time=system_time();
    		if(working_mode==DIGITAL_MODE){
        		pbuf=buf_de;
        		memcpy(p_arrayin[p_index0],pbuf,72*2);//
        		pbuf+=36;
        		for(j=0; j<36; j++){
        			if(pbuf[j]==-1)
        				pbuf[j]=0;
        			p2sum +=pbuf[j];
        		}
        		p_errorarray[p_index0++]=p2sum;
        		if(p2sum>32){
//        			testing_rx=TRUE;
        			total_frames++;
        		}
        		//test finish,submit result,test quit
        		if(p2sum<10 && testing_rx==TRUE){
        			caculate_ber(total_frames);
        			p_index0=total_frames=0;
            		memset(p_errorarray, 0, 512);
            		ebapp=eflink=ebphy=flink=0;
        			rx_submit=1;
        			rx_flag=0;
        			continue;
        		}
        		p2sum=0;
    			pbuf+=36;
    		}
    		else
    			pbuf2=buf_md;

    		for(i=0;i<3;i++){
    			if(testing_rx==TRUE && working_mode==DIGITAL_MODE){
    				samcoder_decode_verbose(dcoder0, pbuf, buf_send, &fec_state);
    				statics_fec(fec_state);
    				pbuf+=72;
    			}else if(working_mode==DIGITAL_MODE){
    				samcoder_decode( dcoder0, pbuf, buf_send);
    				data_send((uint8_t*)buf_send);
    				pbuf+=72;
//    				test_count1++;
    			}else{
    				data_send((uint8_t*)pbuf2);
    				pbuf2+=320;
    			}
    		}
    		sp_count=0;
    	}
    	else  if(1==tx_flag){
    		if(FALSE==Semaphore_pend(sem3,45))
    			continue;

       		if(dsc_status==0 && working_mode==DIGITAL_MODE){
        			//BER test
        			if(testing_tx==TRUE){
        				if((sp_count==3||sp_init==FALSE) && p_index!=252){
        					if(0==sp_count)
        						sp_init=TRUE;
        					for(j=0; j<36; j++){			//set p2
        						if(p_index<250)
        							syn_p[36+j]=1;
        						else
        							syn_p[36+j]=-1;
        					}
                			memcpy(p_arrayin[p_index],syn_p,72*2);
                			sp_count=0;
                			p_index++;
                			test_count1++;
                		    rrcFilter(syn_p, Buf10);
                		    from24To120d(Buf10,(float*)Buf5);
        				}else if(sp_count==3 && p_index==252){
            				p_index=sp_count=0;
            				sp_init=FALSE;
            				memcpy(syn_p, syn_p0, 144);
                			rpe_flush(prpe,RPE_ENDPOINT_READER,TRUE,&attr);//返回 flush的数据量
                			dac_write(3, 0);
                			CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT6,0); //TX_SW
                			CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT7,1);//RX_SW
                			CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT13,0); //R:F1
                			rx_submit=1;
            				tx_flag=0;
            				memset(buf_transmit,0x88,RPE_DATA_SIZE/2*15);
    //            			rx_flag=1;
                			continue;
        				}else if(sp_count!=3){
        					samcoder_encode_patdata(dcoder0, frame0);
        					sp_count++;

                		    rrcFilter(frame0, Buf10);
                		    from24To120d(Buf10,(float*)Buf5);
        				}
        			}
        			//speech send
        			else{
        				//syn_p
        				if(dsc_status==0 && working_mode==DIGITAL_MODE && (sp_count==3 || sp_init==FALSE)){
							if(0==sp_count)
								sp_init=TRUE;
							test_count3++;
							rrcFilter(syn_p, Buf10);
							from24To120d(Buf10,(float*)Buf5);
							sp_count=0;
        				}
        				//speech
        				date_rev(inbuf);
        				if(dsc_status==0 ){
							iirFilter_AfterCodec(inbuf, outbuf, 320);
							samcoder_encode( dcoder0, outbuf, frame0);
							rrcFilter(frame0,Buf10);
							from24To120d(Buf10,(float*)Buf5);
							sp_count++;
        				}
        			}
       		}
       		else{
    			date_rev(inbuf);
    			memcpy((uint8_t*)send_buf+rev_count*640, (uint8_t*)inbuf, 640);
				if(2==rev_count++){
					dataFilterAndTrans(send_buf,Buf5,960);
					rev_count=0;
				}
       		}
    	}
    	else
    		Task_sleep(1);
    }
}
#endif

Void task_modulate(UArg arg0, UArg arg1)
{
    int total_count, transmit_count;
    static float * pBuf5;
    float buf5_cp[4800];

	do{
		while(1==tx_flag){
			if(dsc_status==0 && working_mode==DIGITAL_MODE){
				total_count=3;
			}
			else{
				total_count=4;
			}
			memcpy(buf5_cp, Buf5, total_count*1200*sizeof(float));

			Semaphore_post(sem3);
			pBuf5=buf5_cp;
			transmit_count=0;
    		while(transmit_count++<total_count){
    			Semaphore_pend(sem2, BIOS_WAIT_FOREVER);
//    			exec_time=system_time()-sys_time;
//    			sys_time=system_time();
	    		data_process(pBuf5,buf_transmit,BUFSIZE/3);
	    		pBuf5+=1200;
    		}
		}
		Task_sleep(3);
	}while(1);

}

void data_process(float *buf_in, unsigned char *buf_out, unsigned int size)
{
	uint32_t tempdata=0;
	uint32_t tempCount;
	reg_16 reg16;
	float k;
	static float factor;

	if(dsc_status==0 && working_mode==DIGITAL_MODE)
		k=160000;
	else
		k=5.8;
	factor=FSK_FAST_SPI_calc();

    for(tempCount=0;tempCount<size;tempCount++)
    {
//    	intersam[tempCount]=buf_in[tempCount]*k;
    	reg16.all=(unsigned short)(factor*(buf_in[tempCount]*k));
    	((uint8_t *)buf_out)[tempdata++] 	 	 = reg16.dataBit.data0;
    	((uint8_t *)buf_out)[tempdata++] 	 	 = reg16.dataBit.data1;
    	((uint8_t *)buf_out)[tempdata++] 	     = (uint8_t)33;
    }
}


/*
 *  ======== data_send ========
 */
Void data_send(uint8_t *buf_16)
{
//    struct rpe *prpe = NULL;
//    prpe=&rpe[0];
	uint8_t 		*buf = NULL;
    uint32_t		size;
    Int status=0;
    static uint32_t count1,count2;
    static unsigned char silence;
    //data send
    size=RPE_DATA_SIZE;

    while(rx_flag){
		status=rpe_acquire_writer(prpe,(rpe_buf_t*)&buf,&size);
		if((status&&status!=ERPE_B_BUFWRAP)||size!=RPE_DATA_SIZE||status<0){
	        rpe_release_writer(prpe,0);
	        size=RPE_DATA_SIZE;
	        continue;
		}
		//silence
		if(dsc_status==0 && working_mode==DIGITAL_MODE){
			memcpy(buf,(unsigned char*)buf_16,size);	//buf_16: data to be sent
		}else{
		    if(RSSI_db<RXSS_THRESHOLD || E_noise > E_noise_th[TH_LEVEL]){
		    	if(++count1==3){
		    		silence=0;
		    		count1--;
		    		count2=0;
		    	}
		    }
		    else if(RSSI_db>RXSS_THRESHOLD+3 && E_noise< E_noise_th[TH_LEVEL]-0.03){
		    	if(++count2==3){
		    		silence=1;
			    	count2--;
			    	count1=0;
		    	}
		    }

		    if(0==silence && RXSS_THRESHOLD!=-250)
		    	memset(buf,0,size);
		    else
		    	memcpy(buf,(unsigned char*)buf_16,size);	//buf_16: data to be sent
		}

        status=rpe_release_writer_safe(prpe,buf,size);
        if(status<0){
        	log_warn("rpe release writer end failed!");
        	continue;
        }else
        	break;
    }
    return;
}


//24bit-->16bit
void data_extract(unsigned char *input, unsigned char *output)
{
	unsigned int i;

	for(i=0;i<BUFSIZE/3;i++){
		*output++=*input++;
		*output++=*input++;
		input++;
	}
	return;
}

/*
 *  ======== task_enque ========
 */
#if 0
Void task_enque(UArg arg0, UArg arg1)
{
	extern audioQueue audioQ;
	extern AudioQueue_DATA_TYPE audioQueueRxBuf[];
	unsigned short buf_16[REC_BUFSIZE/3] = {0};
	static short pingpongflag;

	do{
		Semaphore_pend(sem1,BIOS_WAIT_FOREVER);

		pingpongflag=RxpingPongIndex?0:1;
		data_extract((unsigned char*)bufRxPingPong[pingpongflag],(unsigned char*)buf_16);
		Rx_process(buf_16,buf_de);
		data_send((uint8_t*)buf_de);
	}while(1);
}
#else

Void task_enque(UArg arg0, UArg arg1)
{
	extern audioQueue audioQ;
	extern AudioQueue_DATA_TYPE audioQueueRxBuf[];
	unsigned short buf_16[1200] = {0};
	static short pingpongflag, send_count;
	static short *pmd;
	int status;

	pmd=buf_md;
	do{
		Semaphore_pend(sem1,BIOS_WAIT_FOREVER);
		test_count0++;
		pingpongflag=RxpingPongIndex?0:1;
		data_extract((unsigned char*)bufRxPingPong[pingpongflag],(unsigned char*)buf_16);
		status=Rx_process(buf_16,buf_de);
		if(1==status)
			Semaphore_post(sem4);
		else if(0==status){
			memcpy(pmd, buf_de, 480);	//240 samples
			if(send_count++<4){
				pmd+=240;
//				test_count3++;
			}
			if(send_count==4){
				pmd=buf_md;
				Semaphore_post(sem4);
				send_count=0;
			}
		}
	}while(1);
}

#endif

void sys_configure(void)
{
    CSL_SyscfgRegsOvly syscfgRegs = (CSL_SyscfgRegsOvly)CSL_SYSCFG_0_REGS;

    //Select PLL0_SYSCLK2
//    syscfgRegs->CFGCHIP3 &= ~CSL_SYSCFG_CFGCHIP3_ASYNC3_CLKSRC_MASK;
//    syscfgRegs->CFGCHIP3 |= ((CSL_SYSCFG_CFGCHIP3_ASYNC3_CLKSRC_PLL0)
//        						  <<(CSL_SYSCFG_CFGCHIP3_ASYNC3_CLKSRC_SHIFT));
    //mcbsp1
    syscfgRegs->PINMUX1 &= ~(CSL_SYSCFG_PINMUX1_PINMUX1_7_4_MASK |
                             CSL_SYSCFG_PINMUX1_PINMUX1_11_8_MASK |
                             CSL_SYSCFG_PINMUX1_PINMUX1_15_12_MASK |
                             CSL_SYSCFG_PINMUX1_PINMUX1_19_16_MASK |
                             CSL_SYSCFG_PINMUX1_PINMUX1_23_20_MASK |
                             CSL_SYSCFG_PINMUX1_PINMUX1_27_24_MASK |
                             CSL_SYSCFG_PINMUX1_PINMUX1_31_28_MASK);
    syscfgRegs->PINMUX1   = 0x22222220;
    //spi1
    syscfgRegs->PINMUX5 &= ~(CSL_SYSCFG_PINMUX5_PINMUX5_3_0_MASK |
    						 CSL_SYSCFG_PINMUX5_PINMUX5_7_4_MASK |
                             CSL_SYSCFG_PINMUX5_PINMUX5_11_8_MASK |
                             CSL_SYSCFG_PINMUX5_PINMUX5_19_16_MASK |
                             CSL_SYSCFG_PINMUX5_PINMUX5_23_20_MASK );
    syscfgRegs->PINMUX5   |= 0x00110111;

    syscfgRegs->PINMUX6 &= ~(CSL_SYSCFG_PINMUX6_PINMUX6_15_12_MASK|
    						CSL_SYSCFG_PINMUX6_PINMUX6_23_20_MASK|
    						CSL_SYSCFG_PINMUX6_PINMUX6_27_24_MASK);
    syscfgRegs->PINMUX6  |= 0x08808000;

    syscfgRegs->PINMUX7  &= ~(CSL_SYSCFG_PINMUX7_PINMUX7_11_8_MASK|
    						CSL_SYSCFG_PINMUX7_PINMUX7_15_12_MASK);
    syscfgRegs->PINMUX7  |= 0X00008800;

    syscfgRegs->PINMUX13 &= ~(CSL_SYSCFG_PINMUX13_PINMUX13_11_8_MASK|
    						CSL_SYSCFG_PINMUX13_PINMUX13_15_12_MASK);
    syscfgRegs->PINMUX13 |= 0x00008800;
    syscfgRegs->PINMUX14 &= ~(CSL_SYSCFG_PINMUX14_PINMUX14_3_0_MASK|
    						CSL_SYSCFG_PINMUX14_PINMUX14_7_4_MASK);
    syscfgRegs->PINMUX14 |= 0x00000088;

    syscfgRegs->PINMUX18 &=~(CSL_SYSCFG_PINMUX18_PINMUX18_19_16_MASK);
    syscfgRegs->PINMUX18 |= 0x00080000;

    syscfgRegs->PINMUX19 &=~(CSL_SYSCFG_PINMUX19_PINMUX19_27_24_MASK);
    syscfgRegs->PINMUX19 |= 0x08000000;

	 /* Configure GPIO2_1 (GPIO2_1_PIN) as an input                            */
	 CSL_FINS(gpioRegs->BANK[1].DIR,GPIO_DIR_DIR1,1);

    //GP2[2]	 NO USE
    CSL_FINS(gpioRegs->BANK[GP2].DIR,GPIO_DIR_DIR2,0);
//    CSL_FINS(gpioRegs->BANK[1].OUT_DATA,GPIO_OUT_DATA_OUT2,0);
    CSL_FINS(gpioRegs->BANK[GP2].OUT_DATA,GPIO_OUT_DATA_OUT2,1);
    //GP2[4]	 DSC_LOCK
    CSL_FINS(gpioRegs->BANK[GP2].DIR,GPIO_DIR_DIR4,1);

    //GP3[12] LNA_ATT_EN
    CSL_FINS(gpioRegs->BANK[1].DIR,GPIO_DIR_DIR28,0);
    CSL_FINS(gpioRegs->BANK[1].OUT_DATA,GPIO_OUT_DATA_OUT28,0);
    //GP3[13] IF_AGC_CTL
    CSL_FINS(gpioRegs->BANK[1].DIR,GPIO_DIR_DIR29,0);
    CSL_FINS(gpioRegs->BANK[1].OUT_DATA,GPIO_OUT_DATA_OUT29,0);

    //GP6[0]
    CSL_FINS(gpioRegs->BANK[3].DIR,GPIO_DIR_DIR0,0);
    CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT0,1);


    CSL_FINS(gpioRegs->BANK[3].DIR,GPIO_DIR_DIR12,1);

    //GP6[13]
    CSL_FINS(gpioRegs->BANK[3].DIR,GPIO_DIR_DIR13,0);
    CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT13,0);
//    CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT13,1); //T:F2
    //GP6[6] GP6[7]	TX_SW RX_SW
    CSL_FINS(gpioRegs->BANK[3].DIR,GPIO_DIR_DIR6,0);//TX_SW
    CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT6,0);
    CSL_FINS(gpioRegs->BANK[3].DIR,GPIO_DIR_DIR7,0);//RX_SW
    CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT7,1);
    //GP8[13]  DSC_SEL  msk:0
    CSL_FINS(gpioRegs->BANK[4].DIR,GPIO_DIR_DIR13,0);//DSC_SEL
    CSL_FINS(gpioRegs->BANK[4].OUT_DATA,GPIO_OUT_DATA_OUT13,0);
}

void ch_chPara(){
	reg_24 reg_24data;
	Int tempCount=0;

//	for (tempCount = 0; tempCount < 42; tempCount++){
//	    reg_24data.all=lmx_init[45+tempCount/3];
//	    buf_transmit[tempCount++] = reg_24data.dataBit.data0;
//	    buf_transmit[tempCount++] = reg_24data.dataBit.data1;
//	    buf_transmit[tempCount]   = reg_24data.dataBit.data2;
//	}

    for (tempCount = 0; tempCount < BUFSIZE; tempCount++){
    	buf_transmit[tempCount++]  =0x88;
    	buf_transmit[tempCount++]  =0x88;
    	buf_transmit[tempCount]    = 0x88;
    }
    for (tempCount = BUFSIZE/2-42; tempCount < BUFSIZE/2; tempCount++){
    		reg_24data.all=lmx_init[45+(42-BUFSIZE/2+tempCount)/3];
    		buf_transmit[tempCount++]  = reg_24data.dataBit.data0;
    		buf_transmit[tempCount++]  = reg_24data.dataBit.data1;
    		buf_transmit[tempCount]   = reg_24data.dataBit.data2;
    }

}

void ch_init(){
	reg_24 reg_24data;
	Int tempCount=0;

    for (tempCount = 0; tempCount < BUFSIZE-135; tempCount++){
    	buf_transmit[tempCount++] =0x88;
    	buf_transmit[tempCount++] =0x88;
    	buf_transmit[tempCount]  = 0x88;
    }
    for (tempCount = BUFSIZE-135; tempCount < BUFSIZE; tempCount++){
    		reg_24data.all=lmx_init[(135-BUFSIZE+tempCount)/3];
    		buf_transmit[tempCount++] = reg_24data.dataBit.data0;
    		buf_transmit[tempCount++] = reg_24data.dataBit.data1;
    		buf_transmit[tempCount]   = reg_24data.dataBit.data2;
    }
}

void power_ctrl(float freq)
{
	unsigned int freq_index=0;
	float freq_section;

	freq_index=(freq-27.5)/2;
	if(freq_index>=6)
	{
		log_error("bad frequency");
		return;
	}

	freq_section=27.5+2*freq_index;
#if 1
	if(0==power_index)
		freq_index=freq_index+60;
	else
		freq_index=freq_index+67;
#else
	freq_index=freq_index+53+power_index*14;
#endif
	if(freq-freq_section<0.001){
		transmit_power = eeprom_data[freq_index];
	}
	else{
		transmit_power = (eeprom_data[freq_index+1]-eeprom_data[freq_index])*(freq-freq_section)/2 + eeprom_data[freq_index];
	}
}


void dsp_logic()
{
	int status=NO_ERR;
	message_t msg_temp;
	int ad_ch;
	float ad_value;
	char str_temp[32];
	char* pstr=NULL;
	char* poffs=NULL;
    message_t *msg =NULL, *msg_send=NULL;
//    static int unlock_count;

    if(1==rx_submit){
    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	if(!msg_send){
    	    log_warn("msgq out of memmory");
    	    goto out;
    	}
    	if(testing_tx==TRUE){
    		msg_send->type=TEST_TX;
    		testing_tx=FALSE;
    		msg_send->data.d[0]='\0';
    	}
    	else if(testing_rx==TRUE){
    		msg_send->type=TEST_RX;
    		testing_rx=FALSE;
    		sprintf(msg_send->data.d,"%.6f",error_rate[0]);
    		sprintf(msg_send->data.d+8,"%.6f",error_rate[1]);
    		sprintf(msg_send->data.d+16,"%.6f",error_rate[2]);
//    		memcpy(msg_send->data.d,(char *)error_rate,4);
//    		memcpy(msg_send->data.d+4,(char *)(error_rate+1),4);
//    		memcpy(msg_send->data.d+8,(char *)(error_rate+4),4);

    	}
    	else{
    		msg_send->type=DATA_END;
    		msg_send->data.d[0]='\0';
    	}

		status=messageq_send_safe(&msgq[0],msg_send,0,0,0);
		if(status<0){
			log_error("message send error");
			message_free(msg_send);
		}
    	rx_flag=1;
    	rx_submit=0;
    }
    //
    status=messageq_receive(&msgq[0],&msg,0);
    if (status>=0){
    	msg_temp.type=msg->type;
    	memcpy(msg_temp.data.d,msg->data.d,100);
    	message_free(msg);
    }
    if(status>=0){
    		switch (msg_temp.type){
    		case LMX2571_TX:
    		case TX_ON:
//    			rpe_flush(prpe,RPE_ENDPOINT_WRITER,TRUE,&attr);//返回 flush的数据量
    			CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT7,0);//RX_SW
    			CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT6,1); //TX_SW
    			CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT13,1); //T:F2
    		    //TX_DAC
    			power_ctrl(channel_freq);
    		    dac_write(3, transmit_power*0.5);
    		    Task_sleep(5);
    		    dac_write(3, transmit_power);
    			rx_flag=0;
    			tx_flag=1;
    			test_count5=rpe_flush(prpe,RPE_ENDPOINT_READER,TRUE,&attr);
    			break;
    		case LMX2571_RX:
    		case RX_ON:
//    			rpe_flush(prpe,RPE_ENDPOINT_READER,TRUE,&attr);//返回 flush的数据量
    			dac_write(3, 0);
    			CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT6,0); //TX_SW
    			CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT7,1);//RX_SW
    			CSL_FINS(gpioRegs->BANK[3].OUT_DATA,GPIO_OUT_DATA_OUT13,0); //R:F1
    			tx_submit=0;
				tx_flag=0;
				memset(buf_transmit,0x88,BUFSIZE);
    			rx_flag=1;
    			test_count4=rpe_flush(prpe,RPE_ENDPOINT_WRITER,TRUE,&attr);
    			break;
    		case RSSI_RD:
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=RSSI_RD;
    			sprintf(msg_send->data.d,"%f",RSSI_db);
    			status=messageq_send_safe(&msgq[0],msg_send,0,0,0);
    			if(status<0){
    				log_error("message send error");
    				message_free(msg_send);
    			}
    			break;
    		case IF_AGC_ON:
    			CSL_FINS(gpioRegs->BANK[1].OUT_DATA,GPIO_OUT_DATA_OUT29,1);
    			break;
    		case IF_AGC_OFF:
    		    CSL_FINS(gpioRegs->BANK[1].OUT_DATA,GPIO_OUT_DATA_OUT29,0);
    		    break;
    		case LNA_ATT_ON:
    			CSL_FINS(gpioRegs->BANK[1].OUT_DATA,GPIO_OUT_DATA_OUT28,1);
    			break;
    		case LNA_ATT_OFF:
    		    CSL_FINS(gpioRegs->BANK[1].OUT_DATA,GPIO_OUT_DATA_OUT28,0);
    		    break;
    		case LMX2571_LD:
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=LMX2571_LD;
    			sprintf(msg_send->data.d,"%d",CSL_FEXT(gpioRegs->BANK[3].IN_DATA,GPIO_IN_DATA_IN12));
    			log_info("lmx2571_ld is %s",msg_send->data.d);
    			status=messageq_send_safe(&msgq[0],msg_send,0,0,0);
    			if(status<0){
    				log_error("message send error");
    				message_free(msg_send);
    			}
    			break;
    		case TX_CH:
    		case TX_CHF:
//    			log_info("tx_ch:%f",atof(msg_temp.data.d));
    			channel_freq=atof(msg_temp.data.d);
    			LMX2571_FM_CAL(1,atof(msg_temp.data.d), 1);
    			lmx_init[55]=0xBC3;
    			lmx_init[56]=lmx_init[57]=0x9C3;
    			lmx_init[58]=0x9C3;
    			ch_chPara();
    			Task_sleep(15);
    			memset(buf_transmit,0x88,BUFSIZE);
    			break;
    		case RX_CH:
    		case RX_CHF:
//    			log_info("rx_ch:%f",49.95+atof(msg_temp.data.d));
    			dac_write(1, 2.5+(atof(msg_temp.data.d)-27.5)*0.1667);
    			LMX2571_FM_CAL(0,49.95+atof(msg_temp.data.d), 0);
    			lmx_init[55]=0xB83;
    			lmx_init[56]=lmx_init[57]=0x983;
    			lmx_init[58]=0x983;
    			ch_chPara();
    			Task_sleep(15);
    			memset(buf_transmit,0x88,BUFSIZE);
    			break;
    		case AMC7823_AD:
    			ad_ch=atoi(msg_temp.data.d);
    			if(ad_ch>8||ad_ch<0)
    				log_error("error ad_ch parameter %d",ad_ch);
    			ad_value=adc_read(ad_ch);
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=AMC7823_AD;
    			sprintf(msg_send->data.d,"%f",ad_value);
    			status=messageq_send_safe(&msgq[0],msg_send,0,0,0);
    			if(status<0){
    			    log_error("message send error");
    			    message_free(msg_send);
    			}
    			break;
    		case AMC7823_DA:
    			poffs=strstr(msg_temp.data.d,":");
    			strncpy(str_temp,msg_temp.data.d,poffs-msg_temp.data.d);
    			str_temp[poffs-msg_temp.data.d]='\0';
    			ad_ch=atoi(str_temp);
    			if(ad_ch>8||ad_ch<0)
    				log_error("error ad_ch parameter %d",ad_ch);
    			pstr=poffs+1;
//    			poffs=strstr(pstr,":");
//    			strncpy(str_temp,pstr,poffs-pstr);
    			strcpy(str_temp,pstr);
//    			str_temp[poffs-pstr]='\0';
    			ad_value=atof(str_temp);
    			dac_write(ad_ch,ad_value);
    			break;
    		case P_TEMP2:
    		case PA_TEMP:
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
//    			ad_value=temperature_read();
    	    	ad_value=adc_read(4);
    	    	ad_value=(ad_value*1000-500)*0.1;
    		    msg_send->type=msg_temp.type;
    		    sprintf(msg_send->data.d,"%f",ad_value);
    		    status=messageq_send_safe(&msgq[0],msg_send,0,0,0);
    		    if(status<0){
    		    	log_error("message send error");
    		    	message_free(msg_send);
    		    }
    		    break;
    		case H_TX:
    			power_index=1;
//    			transmit_power=eeprom_data[28];
    			break;
    		case L_TX:
    			power_index=0;
//    			transmit_power=eeprom_data[39];
    			break;
    		case RSSTH:
    			TH_LEVEL=atoi(msg_temp.data.d);
    			RXSS_THRESHOLD=2*TH_LEVEL-125;
    			if(0==TH_LEVEL)
    				RXSS_THRESHOLD=-250;
    			break;
    		case PA_CURRENT:
    		case P_CURRENT2:
    			ad_value=adc_read(2)/0.4;
    			if(ad_value>6.0)
    				dac_write(3, transmit_power*0.75);
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=msg_temp.type;
    			sprintf(msg_send->data.d,"%f",ad_value);
    			status=messageq_send(&msgq[0],(messageq_msg_t)msg_send,0,0,0);
    			if(status<0){
    				log_error("message send error");
    				message_free(msg_send);
    			}
    			break;
    		case VSWR:
    		case VSWR2:
    			ad_value=adc_read(0);
    			if(ad_value<=eeprom_data[105])
    				ad_ch=15;
    			else if(eeprom_data[105]<ad_value&&ad_value<=eeprom_data[108])
    				ad_ch=3-0.1*ad_value;
    			else if(eeprom_data[108]<ad_value&&ad_value<=eeprom_data[111])
    				ad_ch=3.25-0.125*ad_value;
    			else if(eeprom_data[111]<ad_value&&ad_value<=eeprom_data[114])
    				ad_ch=3.5-0.167*ad_value;
    			else if(ad_value>=eeprom_data[114])
    				ad_ch=3;
    			//send
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=msg_temp.type;
    			sprintf(msg_send->data.d,"%d",ad_ch);
    			status=messageq_send(&msgq[0],(messageq_msg_t)msg_send,0,0,0);
    			if(status<0){
    				log_error("message send error");
    				message_free(msg_send);
    			}
    			break;
    		case RSSI2:
    		case RXSSI:
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=msg_temp.type;
    			sprintf(msg_send->data.d,"%f",RSSI_db);
    			status=messageq_send(&msgq[0],(messageq_msg_t)msg_send,0,0,0);
    			if(status<0){
    				log_error("message send error");
    				message_free(msg_send);
    			}
    			break;
    		case TXPI:
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=TXPI;
    			sprintf(msg_send->data.d,"%f",transmit_power*10);
    			status=messageq_send(&msgq[0],(messageq_msg_t)msg_send,0,0,0);
    			if(status<0){
    			    log_error("message send error");
    			    message_free(msg_send);
    			}
    			break;
    		case V138:
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=V138;
    			ad_value=adc_read(3);
    			ad_value=ad_value*8;
    			sprintf(msg_send->data.d,"%f",ad_value);
    			status=messageq_send(&msgq[0],(messageq_msg_t)msg_send,0,0,0);
    			if(status<0){
    			    log_error("message send error");
    			    message_free(msg_send);
    			}
    			break;
    		case V6:
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=V6;
    			ad_value=6;
    			sprintf(msg_send->data.d,"%f",ad_value);
    			status=messageq_send(&msgq[0],(messageq_msg_t)msg_send,0,0,0);
    			if(status<0){
    			    log_error("message send error");
    			    message_free(msg_send);
    			}
    			break;
    		case ADJVCO:
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=ADJVCO;
    			sprintf(msg_send->data.d,"%f",eeprom_data[25]);
    			status=messageq_send(&msgq[0],(messageq_msg_t)msg_send,0,0,0);
    			if(status<0){
    			    log_error("message send error");
    			    message_free(msg_send);
    			}
    			break;
    		case VPWR25:
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=VPWR25;
    			sprintf(msg_send->data.d,"%f",eeprom_data[28]);
    			status=messageq_send(&msgq[0],(messageq_msg_t)msg_send,0,0,0);
    			if(status<0){
    			    log_error("message send error");
    			    message_free(msg_send);
    			}
    			break;
    		case VPWR14:
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=VPWR14;
    			sprintf(msg_send->data.d,"%f",eeprom_data[34]);
    			status=messageq_send(&msgq[0],(messageq_msg_t)msg_send,0,0,0);
    			if(status<0){
    			    log_error("message send error");
    			    message_free(msg_send);
    			}
    			break;
    		case VPWR1:
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=VPWR1;
    			sprintf(msg_send->data.d,"%f",eeprom_data[39]);
    			status=messageq_send(&msgq[0],(messageq_msg_t)msg_send,0,0,0);
    			if(status<0){
    			    log_error("message send error");
    			    message_free(msg_send);
    			}
    			break;
    		case ADJRSSI:
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=ADJRSSI;
    			sprintf(msg_send->data.d,"%f",eeprom_data[84]);
    			status=messageq_send(&msgq[0],(messageq_msg_t)msg_send,0,0,0);
    			if(status<0){
    			    log_error("message send error");
    			    message_free(msg_send);
    			}
    			break;
    		case ADJ_VCO:
    			eeprom_data[25]=atof(msg_temp.data.d);
    			dac_write(0,eeprom_data[24]+eeprom_data[25]);
    			break;
    		case VOL_25W:
    			if(transmit_power==eeprom_data[28])
    				transmit_power=atof(msg_temp.data.d);
    			eeprom_data[28]=atof(msg_temp.data.d);
    			break;
    		case VOL_14W:
    			if(transmit_power==eeprom_data[34])
    				transmit_power=atof(msg_temp.data.d);
    			eeprom_data[34]=atof(msg_temp.data.d);
    			break;
    		case VOL_1W:
    			if(transmit_power==eeprom_data[39])
    				transmit_power=atof(msg_temp.data.d);
    			eeprom_data[39]=atof(msg_temp.data.d);
    			break;
    		case ADJ_RSSI:
    			eeprom_data[84]=atoi(msg_temp.data.d);
    			break;
    		case WORK_MODE:
    			working_mode=atoi(msg_temp.data.d);
    			testing_tx=testing_rx=FALSE;
    			break;
    		case DSC_MODE:
    			dsc_status=atoi(msg_temp.data.d);
    			break;
    		case TEST_TX:
    			if(working_mode==DIGITAL_MODE)
    				testing_tx=TRUE;
    			break;
    		case DSC_CNT:
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=DSC_CNT;
    			memcpy(msg_send->data.d, (char *)DSC_TEST_CNT,100);
    			status=messageq_send(&msgq[0],(messageq_msg_t)msg_send,0,0,0);
    			if(status<0){
    			    log_error("message send error");
    			    message_free(msg_send);
    			}
    			break;
    		case DSC_CH:
    			dsc_test_flag=1;
    			status = atoi(msg_temp.data.d);
    			adf_set_ch(status);
    			if (status == 221)
    				current_dscch=0;
    			else if(status == 231)
    				current_dscch=1;
    			else if(status == 236)
    				current_dscch=2;
    			else if(status == 238)
    				current_dscch=3;
    			break;
    		case ADF4002_LD:
    	    	msg_send=(message_t *)message_alloc(msgbuf[0],sizeof(message_t));
    	    	if(!msg_send){
    	    	    log_warn("msgq out of memmory");
    	    	    goto go_break;
    	    	}
    			msg_send->type=ADF4002_LD;
    			sprintf(msg_send->data.d,"%d",CSL_FEXT(gpioRegs->BANK[1].IN_DATA,GPIO_IN_DATA_IN4));
    			status=messageq_send_safe(&msgq[0],msg_send,0,0,0);
    			if(status<0){
    				log_error("message send error");
    				message_free(msg_send);
    			}
    			break;
    		case DSC:  //DSC_SEL
    		    CSL_FINS(gpioRegs->BANK[4].OUT_DATA,GPIO_OUT_DATA_OUT13, atoi(msg_temp.data.d));
    			break;
    		default:
    			log_error("unknown message  type is %d", msg_temp.type);
go_break:
				break;
    		}
    }

//    if(0==CSL_FEXT(gpioRegs->BANK[3].IN_DATA,GPIO_IN_DATA_IN12) && 1==rx_flag){
//    	if(unlock_count++>5){
//    		unlock_count=0;
//    		LMX2571_INIT_CAL(channel_freq);
//        	ch_init();
//    		Task_sleep(10);
//    		memset(buf_transmit,0x88,BUFSIZE);
//    	}
//    }
//    else if(unlock_count>0 && CSL_FEXT(gpioRegs->BANK[3].IN_DATA,GPIO_IN_DATA_IN12) > 0 ){
//    	unlock_count--;
//    }

out:
	return;
}

void eeprom_cache()
{
	  static short count0;
	  int status=NO_ERR;
	  message_t *msg =NULL;

	  while(count0<101){
	    status=messageq_receive(&msgq[0],&msg,0);
	    if (status>=0&&msg->type==EEPROM){
			memcpy(eeprom_data+count0,msg->data.d,100);
			count0+=25;
	    	message_free(msg);
	    }
	  }
}
