/****************************************************************************/
/*                                                                          */
/*              data for LMX2571                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
#ifndef DATA2571_H_
#define DATA2571_H_

/****************************************************************************/
/*                                                                          */
/*              宏定义                                                      */
/*                                                                          */
/****************************************************************************/
//osc 16.8M
#define	PFDin		(42.0*1000)			// 鉴相频率（指定:84.0MHz）
#define	PFDin_ex	(4.2*1000)			// 鉴相频率（指定:28.0MHz）

#define FR_BAUD		24		// 120K
#define DEN_in			1048576  //1048576 //2^20		//16777216	//2^24
#define DEN_ex			1048576 //2^20		//16777216	//2^24
#define Prescaler	2


typedef unsigned short	u16;

#if 1
//F2 tx 	EXTERN VCO FOR RX:F1  fixed
unsigned int lmx_init[174]={
0x002000,
0x3CA000,
0x3A8C00,
0x357806,//
0x2F0000,
//0x2B4830,
0x2A0208,
0x290408,
0x28081C,
0x2711FB,
0x230C83,
0x221003,
0x210000,
0x200000,
0x1F0000,
0x1E0000,
0x1D0000,
0x1C0000,
0x1B0000,
0x1A0000,
0x190000,
0x18040E,
0x170EC4,//0x170E84
0x169585,
0x150201,
0x142034,
0x13FFFF,
0x121861,
0x110F06,
0x100000,
0x0F0000,
0x0E0000,
0x0D0000,
0x0C0000,
0x0B0000,
0x0A0000,
0x090000,
0x08006E,
0x070EC4,
0x068A81,
0x050401,
0x043012,
0x03FFFF,
0x020C30,
0x010F07,
0x0009c3,
};
#else
///pdf 42M	internal VCO FOR RX:F1
unsigned int lmx_init[174]={
0x002000,
0x3CA000,
0x3A8C00,
0x357806,//
0x2F0000,
//0x2B4830,
0x2A0210,
0x290810,
0x28101C,
0x2711FB,
0x230C83,
0x221003,
0x210000,
0x200000,
0x1F0000,
0x1E0000,
0x1D0000,
0x1C0000,
0x1B0000,
0x1A0000,
0x190000,
0x18040E,
0x170E44,//0x170E84
0x169085,
0x150201,
0x14203B,
0x13D085,
0x12094B,
0x110300,
0x100000,
0x0F0000,
0x0E0000,
0x0D0000,
0x0C0000,
0x0B0000,
0x0A0000,
0x090000,
0x08000E,
0x070E84,//0x070E44
0x069585,
0x050201,
0x042034,
0x03D087,
0x027402,
0x010301,
0x000983,
};
#endif

//���ջ�
volatile union{
	struct{
		u16 OUTBUF_TX_PWR_F2:5;		// output power at RFoutTx port.
		u16 EXTVCO_SEL_F2:	1;		// F2选择（1=内置/0=外置）(1=internal/0=external).
		u16 EXTVCO_CHDIV_F2:4;		// F2输出分频（）
		u16 FSK_EN_F2:		1;		// FSK Enables (1=Enable/0=Disable).
		u16 NULL_1:			5;		// set 0h.
	}CtrlBit;
	u16 Data;
}LMX2571_R24;
//=====================================================================// R23:F2���
volatile union{
	struct{
		u16 LF_R4_F2:		3;		// .
		u16 NULL_1:			3;		// .
		u16 OUTBUF_RX_EN_F2:1;		// .
		u16 OUTBUF_TX_EN_F2:1;		// .
		u16 OUTBUF_RX_PWR_F2:5;		// .
		u16 NULL_2:			3;		// .
	}CtrlBit;
	u16 Data;
}LMX2571_R23;
//=====================================================================// R22:F2���
volatile union{
	struct{
		u16 MULT_F2:		5;		// .
		u16 PFD_DELAY_F2:	3;		// .
		u16 CHDIV1_F2:		2;		// .
		u16 CHDIV2_F2:		3;		//
		u16 LF_R3_F2:		3;		//
	}CtrlBit;
	u16 Data;
}LMX2571_R22;
//=====================================================================// R21:F2Ƶ��
volatile union{
	struct{
		u16 PLL_R_PRE_F2:	8;		// OSCin buffer Pre-divider value.
		u16 PLL_R_F2:		8;		// OSCin buffer Post-divider value.
	}CtrlBit;
	u16 Data;
}LMX2571_R21;
//=====================================================================// R20:F2Ƶ��
volatile union{
	struct{
		//------------------------------------ data
		unsigned int	PLL_N_F2:		12;	// integer portion of the N-divider value.
		unsigned int	FRAC_ORDER_F2:	3;	// Delta Sigma modulator.
		unsigned int	PLL_N_PRE_F2:	1;	// Prescaler value.
	}CtrlBit;
	u16 Data;
}LMX2571_R20;
//=====================================================================// R19:F2Ƶ��
volatile union{
	struct{
		//------------------------------------ data
		unsigned int	LSB_PLL_DEN_F2:	16;	// LSB denominator of the N-divider.
	}CtrlBit;
	u16 Data;
}LMX2571_R19;
//=====================================================================// R18:F2Ƶ��
volatile union{
	struct{
		unsigned int	LSB_PLL_NUM_F2:	16;	// LSB numerator of the N-divide.
	}CtrlBit;
	u16 Data;
}LMX2571_R18;
//=====================================================================// R17:F2Ƶ��
volatile union{
	struct{
		unsigned int	MSB_PLL_NUM_F2:	8;	// MSB numerator of the N-divide.
		unsigned int	MSB_PLL_DEN_F2:	8;	// MSB denominator of the N-divider.
	}CtrlBit;
	u16 Data;
}LMX2571_R17;
//�����
//=====================================================================// R8:F1ģʽ
volatile union{
	struct{
		unsigned int	OUTBUF_TX_PWR_F1:5;	// output power at RFoutTx port.
		unsigned int	EXTVCO_SEL_F1:	1;	// VCO (1=internal/0=external).
		unsigned int	EXTVCO_CHDIV_F1:4;	// output divider.
		unsigned int	FSK_EN_F1:		1;	// FSK Enables (1=Enable/0=Disable).
		unsigned int	NULL_1:			5;	// set 0h.
	}CtrlBit;
	u16 Data;
}LMX2571_R08;
//=====================================================================// R7:F1���
volatile union{
	struct{
		unsigned int	LF_R4_F1:		3;	// .
		unsigned int	NULL_1:			3;	// .
		unsigned int	OUTBUF_RX_EN_F1:1;	// .
		unsigned int	OUTBUF_TX_EN_F1:1;	// .
		unsigned int	OUTBUF_RX_PWR_F1:5;	// .
		unsigned int	NULL_2:			3;	// .
	}CtrlBit;
	u16 Data;
}LMX2571_R07;
//=====================================================================// R6:F1���
volatile union{
	struct{
		unsigned int	MULT_F1:		5;	// .
		unsigned int	PFD_DELAY_F1:	3;	// .
		unsigned int	CHDIV1_F1:		2;	// .
		unsigned int	CHDIV2_F1:		3;	//
		unsigned int	LF_R3_F1:		3;	//
	}CtrlBit;
	u16 Data;
}LMX2571_R06;
//=====================================================================// R5:F1Ƶ��
volatile union{
	struct{
		unsigned int	PLL_R_PRE_F1:	8;	// OSCin buffer Pre-divider value.
		unsigned int	PLL_R_F1:		8;	// OSCin buffer Post-divider value.
	}CtrlBit;
	u16 Data;
}LMX2571_R05;
//=====================================================================// R4:F1Ƶ��
volatile union{
	struct{
		unsigned int	PLL_N_F1:		12;	// integer portion of the N-divider value.
		unsigned int	FRAC_ORDER_F1:	3;	// Delta Sigma modulator.
		unsigned int	PLL_N_PRE_F1:	1;	// Prescaler value.
	}CtrlBit;
	u16 Data;
}LMX2571_R04;
//=====================================================================// R3:F1Ƶ��
volatile union{
	struct{
		unsigned int	LSB_PLL_DEN_F1:	16;	// LSB denominator of the N-divider.
	}CtrlBit;
	u16 Data;
}LMX2571_R03;
//=====================================================================// R2:F1Ƶ��
volatile union{
	struct{
		unsigned int	LSB_PLL_NUM_F1:	16;	// 00-15: F1��16λ(LSB numerator of the N-divide.)
	}CtrlBit;
	u16 Data;
}LMX2571_R02;								// R02	��F1 of the N-divide
//=====================================================================// R1:F1Ƶ��
volatile union{
	struct{
		unsigned int	MSB_PLL_NUM_F1:	8;	// 00-07: F1���Ӹ߰�λ(MSB numerator of the N-divide.)
		unsigned int	MSB_PLL_DEN_F1:	8;	// 08-15: F1��ĸ�߰�λ(MSB denominator of the N-divider.)
	}CtrlBit;
	u16 Data;
}LMX2571_R01;								// R01	��F1 of the N-divide



#endif //end OF .H
