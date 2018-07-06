#ifndef __MAIN_H__
#define __MAIN_H__

#include <ti/csl/cslr_syscfg0_OMAPL138.h>
#include <ti/csl/cslr_gpio.h>
#include <ti/csl/cslr_device.h>
#include "syslink_init.h"
#include "system_time.h"
//debug command

#define VHF_DSC 		1
#define VHF_DSC_ON		2
#define VHF_DSC_OFF		3
#define	PWB_STATUS		4
#define PWB_ON			5
#define PWB_OFF			6
#define	DSC				7
#define DSC_CH			8
#define ADF4002_LD		9
#define RSSI_RD			10
#define IF_AGC_ON		11
#define IF_AGC_OFF		12
#define LNA_ATT_ON		13
#define LNA_ATT_OFF		14
#define TX_ON			15
#define TX_OFF			16
#define RX_ON			17
#define RX_OFF			18
#define LMX2571_TX		19
#define LMX2571_RX		20
#define LMX2571_LD		21
#define TX_CH			22
#define RX_CH			23
#define TX_DAC			24
#define TX_PWR_SET		25
#define	RX_TUNE			26
#define BAND_0			27
#define BAND_1			28
#define ANT_NET_SEL		29
#define DAC_SET_16M8	30
#define PA_CURRENT		31
#define PA_TEMP			32
#define TX_POWER		33
#define REV_POWER		34
#define VSWR			35
#define AMC7823_AD		36
#define AMC7823_DA		37
#define AMC7823_AD_SET	38
#define AMC7823_DA_SET	39
#define I2C_W			40
#define IWC_R			41

#define TX_CHF			100
#define RX_CHF			101
#define H_TX			110
#define L_TX			111
#define RSSTH			120
#define RXSSI			150
#define TXPI			151

#define EEPROM			200

#define V138			152
#define V6				153
#define ADJVCO			154
#define VPWR25			155
#define VPWR14			156
#define VPWR1			157

#define RSSI2			180
#define P_CURRENT2		181
#define P_TEMP2			182
#define VSWR2			183
#define ADJRSSI			184

#define ADJ_VCO			160
#define VOL_25W			161
#define VOL_14W			162
#define VOL_1W			163
#define ADJ_RSSI		164

#define DATA_END		218
#define TEST_TX			300
#define TEST_RX			301
#define DSC_CNT			302

#define WORK_MODE		190
#define DSC_MODE		191
#define NORMAL_MODE		0
#define ENHANCE_MODE	1
#define DIGITAL_MODE	2
#define Pi			3.1415926f
//#define P_LEN		10000

#define SYNC_CODE_PREPARE	100
#define RPE_DATAFLOW_END	101
#define RPE_DATA_SIZE		640
#define REC_BUFSIZE	 		3600

typedef volatile union reg_24bit{
	struct reg24_2571{
		unsigned char	data0;
		unsigned char	data1;
		unsigned char	data2;
		unsigned char	data3;
	}dataBit;
	unsigned int all;
}reg_24;

typedef volatile union reg_16bit{
	struct reg16_2571{
		unsigned char	data0;
		unsigned char	data1;
	}dataBit;
	unsigned short all;
}reg_16;

typedef volatile union reg2_16bit{
	struct reg2_16_2571{
		unsigned char	data0;
		unsigned char	data1;
	}dataBit;
	short all;
}reg2_16;

extern float RSSI;

struct mydata{
    char d[100];
};

declare_message_type(message_t,struct mydata);


struct dsc_fmt{
	short int ch;
	short int res;
	union{
		short 	chnl[5];
		short 	code;
		uint8_t msg[50];
	}un_dsc;
};

struct dsc_msg_body{
	unsigned char   mid;
	unsigned short  seq;
	unsigned int 	len;
	union{
		char byte[64];
		struct dsc_fmt mb_dsc;
		//other type...
	}un;
	#define chn		un.mb_dsc.ch
	#define reserve	un.mb_dsc.res
	#define channel	un.mb_dsc.un_dsc.chnl
	#define rcode	un.mb_dsc.un_dsc.code
	#define dsc_msg un.mb_dsc.un_dsc.msg
	#define dsc_len un.mb_dsc.res
};

declare_message_type(msg_t,struct dsc_msg_body);

extern unsigned int lmx_init[];

extern void LMX2571_FM_CAL(unsigned short ch, double fm, unsigned char vco);
extern float  FSK_FAST_SPI_calc(void);
extern void LMX2571_INIT_CAL(double fm);

#endif	//__MAIN_H__
