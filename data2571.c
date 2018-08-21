/****************************************************************************/
/*                                                                          */
/*              McBSP1 							                              */
/*                                                                          */
/*	auther:wangsong															*/
/*	date:20160831															*/
/*	Right:shanghai haotong													*/

#include "data2571.h"
#include <math.h>

#define Pi			3.1415926f
#define FR_BAUD		24		// 120K
#define DEN_in			1048576  //1048576 //2^20		//16777216	//2^24
#define DEN_ex			1048576 //2^20		//16777216	//2^24
#define Prescaler	2
#define	PFDin_ex	(4.2*1000)

/****************************************************************************/
/*                                                                          */
/*              鍑芥暟澹版槑                                                    */
/*                                                                          */
/****************************************************************************/
//osc 25.2M
float	PFDin	=	63.0*1000;

double	VCO_FM;							// VCO棰戠巼
double	PLL_N_CAL;						// PLL鎺у埗鏁版嵁锛圢鍊硷級
int	PLL_NUM;						// PLL鎺у埗鏁版嵁锛堝皬鏁帮級
unsigned char	CHDIV1,CHDIV2;					// 鍐呯疆VCO杈撳嚭鍒嗛鍊�
unsigned short	    PLL_N;							// PLL鎺у埗鏁版嵁锛堟暣鏁帮級
unsigned short		PLL_NUM_H8;						// PLL鎺у埗鏁版嵁楂樺叓浣嶏紙灏忔暟锛�
unsigned short		PLL_NUM_L16;					// PLL鎺у埗鏁版嵁浣�6浣嶏紙灏忔暟锛�
unsigned short	    PLL_DEN_H8;
unsigned short	    PLL_DEN_L16;

unsigned char	CHDIV1_DATA[]	= {4,5,6,7};				// 鍐呯疆VCO杈撳嚭鍒嗛1
unsigned char	CHDIV2_DATA[]	= {1,2,4,8,16,32,64};		// 鍐呯疆VCO杈撳嚭鍒嗛2
unsigned int 	DEN;

void FSK_FAST_SPI(double FMout)
{
	unsigned int i2,FSK_steps;
	double Freq_dev;

	FSK_steps=0;
#if 0
	for(i2=0; i2<FR_BAUD; i2++){
		Freq_dev=FMout*sin(2*Pi*i2*1/FR_BAUD);
//		Freq_dev=3;
		if(Freq_dev>=0)
			FSK_steps=((Freq_dev*DEN/PFDin)*(CHDIV1_DATA[CHDIV1]*CHDIV2_DATA[CHDIV2]/Prescaler));
		else
			FSK_steps=65535+((Freq_dev*DEN/PFDin)*(CHDIV1_DATA[CHDIV1]*CHDIV2_DATA[CHDIV2]/Prescaler))+1;

		lmx_init[54+i2]=0x210000|(unsigned int)(FSK_steps);

	}

#else
	for(i2=0; i2<FR_BAUD; i2++){
					Freq_dev=FMout*sin(2*Pi*i2*1/FR_BAUD);
//					Freq_dev=0;
					FSK_steps=(unsigned int)((Freq_dev*DEN/PFDin)*(CHDIV1_DATA[CHDIV1]*CHDIV2_DATA[CHDIV2]/Prescaler));
					lmx_init[54+i2]=0x210000|(FSK_steps&0x00FFFF);
			}
#endif
}


float  FSK_FAST_SPI_calc(void)
{
	float factor;

	factor=DEN/(float)(PFDin*1000)*(CHDIV1_DATA[CHDIV1]*CHDIV2_DATA[CHDIV2]/Prescaler);
//	FSK_steps=(unsigned int)((float)Freq_dev*factor);
//	FSK_steps=(unsigned int)(((double)Freq_dev*DEN/PFDin/32000)*(CHDIV1_DATA[CHDIV1]*CHDIV2_DATA[CHDIV2]/Prescaler));
//	FSK_steps=0x210000|(FSK_steps&0x00FFFF);
//	return FSK_steps;
	return	factor;
}


#if 0
//================================================================ 璁＄畻杈撳嚭鍒嗛鍊�
void VCO_FM_CAL(double FMout)
{
	int i,j;
	double fm;

	double	VCO_Range[] 			= {4200000000,5520000000};	// VCO 鍏佽鑼冨洿(4993.6MHz,5184MHz)
	//------------------------------------------------------------
	for (j=0; j<7; j++)
	{
		for (i=0; i<4; i++)
		{
			fm =  CHDIV1_DATA[i]*CHDIV2_DATA[j]*FMout*1000000;

			if (fm > VCO_Range[0] & fm < VCO_Range[1])
			{
				VCO_FM = fm;
				CHDIV1 = i;	//Correspond to CHDIV1_DATA[i] in register;
				CHDIV2 = j;	//Correspond to CHDIV2_DATA[j] in register;
				return;
			}
		}
	}
}
#else

//================================================================ 璁＄畻杈撳嚭鍒嗛鍊�
void VCO_FM_CAL(unsigned short vco, double FMout)
{
	int i,j;
	double fm;

	double	VCO_Range[] 			= {4300000000,5376000000};	// VCO 鍏佽鑼冨洿(4993.6MHz,5184MHz)
	//------------------------------------------------------------

	if(1==vco){

#if 0
	for (j=6; j>-1; j--)
	{
		for (i=3; i>-1; i--)
		{
			fm =  CHDIV1_DATA[i]*CHDIV2_DATA[j]*FMout*1000000;

			if (fm > VCO_Range[0] & fm < VCO_Range[1])
			{
				VCO_FM = fm;
				CHDIV1 = i;	//Correspond to CHDIV1_DATA[i] in register;
				CHDIV2 = j;	//Correspond to CHDIV2_DATA[j] in register;
				return;
			}
		}
	}
#else
	for (j=0; j<7; j++)
	{
		for (i=0; i<4; i++)
		{
			fm =  CHDIV1_DATA[i]*CHDIV2_DATA[j]*FMout*1000000;

			if (fm > VCO_Range[0] & fm < VCO_Range[1])
			{
				VCO_FM = fm;
				CHDIV1 = i;	//Correspond to CHDIV1_DATA[i] in register;
				CHDIV2 = j;	//Correspond to CHDIV2_DATA[j] in register;
				return;
			}
		}
	}

#endif


	}
	else{

		VCO_FM=FMout*1000000;
	}
}
#endif
#if 0
//============================================================ 璁＄畻N鍊�
void VCO_N_CAL(double FMout)
{
	double	fm;
	double	Fm;
	//-------------------------------------------- N
	Fm = PFDin;
	Fm = Fm*1000;

	PLL_N_CAL = VCO_FM/2;
	PLL_N_CAL = PLL_N_CAL/Fm;
	//-------------------------------------------- N鏁存暟
	PLL_N = (int) PLL_N_CAL;
	//-------------------------------------------- N灏忔暟
	fm = PLL_N_CAL - PLL_N;
	Fm = fm * 16777215;
	PLL_NUM = (int) Fm;
	PLL_NUM_H8 = PLL_NUM/65536;					// N灏忔暟锛堥珮鍏綅锛�
	PLL_NUM_L16 = PLL_NUM - PLL_NUM_H8*65536;	// N灏忔暟锛堜綆16浣嶏級

	PLL_DEN_H8=255;
	PLL_DEN_L16=65535;
//	PLL_DEN_H8=0;
//	PLL_DEN_L16=0;
}

#else
//============================================================ 璁＄畻N鍊�
void VCO_N_CAL(unsigned char vco, double FMout)
{
	double	fm;
	double	Fm;
	//-------------------------------------------- N



	if(1==vco){
		Fm = PFDin;
		Fm = Fm*1000;
		PLL_N_CAL = VCO_FM/Prescaler;
	}
	else{
		Fm = PFDin_ex;
		Fm = Fm*1000;
		PLL_N_CAL = VCO_FM;
	}


	PLL_N_CAL = PLL_N_CAL/Fm;
	//-------------------------------------------- N鏁存暟
	PLL_N = (int) PLL_N_CAL;
	//-------------------------------------------- N灏忔暟
	fm = PLL_N_CAL - PLL_N;
	Fm = fm * (DEN);
	PLL_NUM = (int) Fm;
	PLL_NUM_H8 = PLL_NUM/65536;					// N灏忔暟锛堥珮鍏綅锛�
	PLL_NUM_L16 = PLL_NUM - PLL_NUM_H8*65536;	// N灏忔暟锛堜綆16浣嶏級

//	PLL_DEN_H8=15;
//	PLL_DEN_L16=65535;
//	PLL_DEN_H8=0;
//	PLL_DEN_L16=DEN-1;
	if(DEN<65536){
		PLL_DEN_H8=0;
		PLL_DEN_L16=DEN;
	}else{
		PLL_DEN_H8=DEN/65536-1;
		PLL_DEN_L16=65535;
	}
}
#endif
/*
 *	CH		FREQ	FPD		CURRENT	(R40.R41)			M R22
 *	1		27500	63.0	1250	(8)					5
 *
 *	35		28350	100.8	781.3 	(5)					8
 *	98		29925	100.8	781.3	(5)					8
 *	161		31500	75.6	1093.8	(7)					6
 *	431		38250	75.6	1093.8	(7)					6
 *	476		39375	75.6	1093.8	(7)					6
 */
//internal vco:vco=1 external vco:vco=0
void LMX2571_FM_CAL(unsigned short ch, double fm, unsigned char vco)
{
	int channel_no;

	if( 1==vco){

		LMX2571_R40.Data=lmx_init[7]&0xffff;
		LMX2571_R41.Data=lmx_init[6]&0xffff;
		LMX2571_R22.Data=lmx_init[22]&0xffff;
		channel_no=(int)(fm*1000);
		switch (channel_no){
		case 28350:
		case 29925:
			PFDin=100.8*1000;
			LMX2571_R41.CtrlBit.CP_IDN=LMX2571_R40.CtrlBit.CP_IUP=5;
			LMX2571_R22.CtrlBit.MULT_F2=8;
			break;
		case 31500:
		case 38250:
		case 39375:
			PFDin=75.6*1000;
			LMX2571_R41.CtrlBit.CP_IDN=LMX2571_R40.CtrlBit.CP_IUP=7;
			LMX2571_R22.CtrlBit.MULT_F2=6;
			break;
		default:
			PFDin=63.0*1000;
			LMX2571_R41.CtrlBit.CP_IDN=LMX2571_R40.CtrlBit.CP_IUP=8;
			LMX2571_R22.CtrlBit.MULT_F2=5;
			break;
		}

		DEN=DEN_in;
	}

	else
		DEN=DEN_ex;

	VCO_FM_CAL(vco, fm);
	VCO_N_CAL( vco, fm);


	if (ch==0)
	{	//---------------------------------------- 銆恈h=0銆戝彂灏勬満锛團1锛�
		LMX2571_R06.Data=lmx_init[38]&0xffff;
		LMX2571_R06.CtrlBit.CHDIV1_F1 = CHDIV1;
		LMX2571_R06.CtrlBit.CHDIV2_F1 = CHDIV2;

		LMX2571_R04.Data=lmx_init[40]&0xffff;
		LMX2571_R04.CtrlBit.PLL_N_F1 = PLL_N;

		LMX2571_R03.Data=lmx_init[41]&0xffff;
		LMX2571_R03.CtrlBit.LSB_PLL_DEN_F1 = PLL_DEN_L16;

		LMX2571_R02.Data=lmx_init[42]&0xffff;
		LMX2571_R02.CtrlBit.LSB_PLL_NUM_F1 = PLL_NUM_L16;

		LMX2571_R01.Data=lmx_init[43]&0xffff;
		LMX2571_R01.CtrlBit.MSB_PLL_NUM_F1 = PLL_NUM_H8;
		LMX2571_R01.CtrlBit.MSB_PLL_DEN_F1 = PLL_DEN_H8;

		lmx_init[45]=lmx_init[36];
		lmx_init[46]=lmx_init[37];
		lmx_init[47]=LMX2571_R06.Data|0x060000;
		lmx_init[48]=lmx_init[39];
		lmx_init[49]=LMX2571_R04.Data|0x040000;
		lmx_init[50]=LMX2571_R03.Data|0x030000;
		lmx_init[51]=LMX2571_R02.Data|0x020000;
		lmx_init[52]=LMX2571_R01.Data|0x010000;//R1
		lmx_init[53]=LMX2571_R40.Data|0x280000;
		lmx_init[54]=LMX2571_R41.Data|0x290000;
	}
	else if (ch==1)
	{	//---------------------------------------- 銆恈h=1銆戞帴鏀舵満锛團2锛�
//		LMX2571_R22.Data=lmx_init[22]&0xffff;
		LMX2571_R22.CtrlBit.CHDIV1_F2 = CHDIV1;
		LMX2571_R22.CtrlBit.CHDIV2_F2 = CHDIV2;

		LMX2571_R20.Data=lmx_init[24]&0xffff;
		LMX2571_R20.CtrlBit.PLL_N_F2 = PLL_N;

		LMX2571_R19.Data=lmx_init[25]&0xffff;
		LMX2571_R19.CtrlBit.LSB_PLL_DEN_F2 = PLL_DEN_L16;

		LMX2571_R18.Data=lmx_init[26]&0xffff;
		LMX2571_R18.CtrlBit.LSB_PLL_NUM_F2 = PLL_NUM_L16;

		LMX2571_R17.Data=lmx_init[27]&0xffff;
		LMX2571_R17.CtrlBit.MSB_PLL_NUM_F2 = PLL_NUM_H8;
		LMX2571_R17.CtrlBit.MSB_PLL_DEN_F2 = PLL_DEN_H8;

		lmx_init[45]=lmx_init[20];
		lmx_init[46]=lmx_init[21];
		lmx_init[47]=LMX2571_R22.Data|0x160000;
		lmx_init[48]=lmx_init[23];
		lmx_init[49]=LMX2571_R20.Data|0x140000;
		lmx_init[50]=LMX2571_R19.Data|0x130000;
		lmx_init[51]=LMX2571_R18.Data|0x120000;
		lmx_init[52]=LMX2571_R17.Data|0x110000;//R17
		lmx_init[53]=LMX2571_R40.Data|0x280000;
		lmx_init[54]=LMX2571_R41.Data|0x290000;
	}
	else
	{	//---------------------------------------- 銆恈h=鍏跺畠銆戞棤鏁�
	}
	lmx_init[55]=lmx_init[44];
}

void LMX2571_INIT_CAL(double fm)
{
	int channel_no;


	DEN=DEN_ex;
	VCO_FM_CAL(0, fm);
	VCO_N_CAL( 0, fm);
	{
		LMX2571_R06.Data=lmx_init[38]&0xffff;
		LMX2571_R06.CtrlBit.CHDIV1_F1 = CHDIV1;
		LMX2571_R06.CtrlBit.CHDIV2_F1 = CHDIV2;

		LMX2571_R04.Data=lmx_init[40]&0xffff;
		LMX2571_R04.CtrlBit.PLL_N_F1 = PLL_N;

		LMX2571_R03.Data=lmx_init[41]&0xffff;
		LMX2571_R03.CtrlBit.LSB_PLL_DEN_F1 = PLL_DEN_L16;

		LMX2571_R02.Data=lmx_init[42]&0xffff;
		LMX2571_R02.CtrlBit.LSB_PLL_NUM_F1 = PLL_NUM_L16;

		LMX2571_R01.Data=lmx_init[43]&0xffff;
		LMX2571_R01.CtrlBit.MSB_PLL_NUM_F1 = PLL_NUM_H8;
		LMX2571_R01.CtrlBit.MSB_PLL_DEN_F1 = PLL_DEN_H8;

		lmx_init[38]=LMX2571_R06.Data|0x060000;

		lmx_init[40]=LMX2571_R04.Data|0x040000;
		lmx_init[41]=LMX2571_R03.Data|0x030000;
		lmx_init[42]=LMX2571_R02.Data|0x020000;
		lmx_init[43]=LMX2571_R01.Data|0x010000;//R1
	}

	LMX2571_R40.Data=lmx_init[7]&0xffff;
	LMX2571_R41.Data=lmx_init[6]&0xffff;
	LMX2571_R22.Data=lmx_init[22]&0xffff;
	channel_no=(int)(fm*1000);
	switch (channel_no){
	case 28350:
	case 29925:
		PFDin=100.8*1000;
		LMX2571_R41.CtrlBit.CP_IDN=LMX2571_R40.CtrlBit.CP_IUP=5;
		LMX2571_R22.CtrlBit.MULT_F2=8;
		break;
	case 31500:
	case 38250:
	case 39375:
		PFDin=75.6*1000;
		LMX2571_R41.CtrlBit.CP_IDN=LMX2571_R40.CtrlBit.CP_IUP=7;
		LMX2571_R22.CtrlBit.MULT_F2=6;
		break;
	default:
		PFDin=63.0*1000;
		LMX2571_R41.CtrlBit.CP_IDN=LMX2571_R40.CtrlBit.CP_IUP=8;
		LMX2571_R22.CtrlBit.MULT_F2=5;
		break;
	}

	DEN=DEN_in;
	VCO_FM_CAL(1, fm);
	VCO_N_CAL( 1, fm);
	{
//		LMX2571_R22.Data=lmx_init[22]&0xffff;
		LMX2571_R22.CtrlBit.CHDIV1_F2 = CHDIV1;
		LMX2571_R22.CtrlBit.CHDIV2_F2 = CHDIV2;

		LMX2571_R20.Data=lmx_init[24]&0xffff;
		LMX2571_R20.CtrlBit.PLL_N_F2 = PLL_N;

		LMX2571_R19.Data=lmx_init[25]&0xffff;
		LMX2571_R19.CtrlBit.LSB_PLL_DEN_F2 = PLL_DEN_L16;

		LMX2571_R18.Data=lmx_init[26]&0xffff;
		LMX2571_R18.CtrlBit.LSB_PLL_NUM_F2 = PLL_NUM_L16;

		LMX2571_R17.Data=lmx_init[27]&0xffff;
		LMX2571_R17.CtrlBit.MSB_PLL_NUM_F2 = PLL_NUM_H8;
		LMX2571_R17.CtrlBit.MSB_PLL_DEN_F2 = PLL_DEN_H8;

		lmx_init[22]=LMX2571_R22.Data|0x160000;

		lmx_init[24]=LMX2571_R20.Data|0x140000;
		lmx_init[25]=LMX2571_R19.Data|0x130000;
		lmx_init[26]=LMX2571_R18.Data|0x120000;
		lmx_init[27]=LMX2571_R17.Data|0x110000;//R17

		lmx_init[7]=LMX2571_R40.Data|0x280000;
		lmx_init[6]=LMX2571_R41.Data|0x290000;
	}


}
