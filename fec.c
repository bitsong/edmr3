#include "fec.h"
#include "bitmap.h"

/*------------------------------------------------------------------*/
//FEC12_8
#define VCOFFSET 	16
static int sym_idx[16] = {
	[0]  = 0,
	[1]  = 1 + VCOFFSET,//bit1
	[2]  = 2 + VCOFFSET,//bit2
	[3]  = 5,
	[4]  = 3 + VCOFFSET,//bit3
	[5]  = -1,
	[6]  = 6,
	[7]  = 9,
	[8]  = 4 + VCOFFSET,//bit4
	[9]  = 8,
	[10] = -2,
	[11] = 10,
	[12] = 7,
	[13] = 11,
	[14] = 12,
	[15] = -3
};

static unsigned short enfec12_8(const unsigned char d)
{
	unsigned short tmp = 0;
	
	tmp = RS(d,6)^RS(d,5)^RS(d,4)^RS(d,3)^RS(d,2)^d^LS(d,1);
	tmp ^= (RS(d,6)^RS(d,5)) & 8;	//A3
	tmp ^= (RS(d,6)^RS(d,3)) & 4;	//A2
	tmp ^= (RS(d,5)^RS(d,2)) & 2;	//A1
	tmp ^= (RS(d,2)^LS(d,1)) & 1;	//A0
/*
	A3 = ( RS(d,4)^RS(d,3)^RS(d,2)^d^LS(d,1) ) & 8;
	A2 = ( RS(d,5)^RS(d,4)^RS(d,2)^d^LS(d,1) ) & 4;
	A1 = ( RS(d,6)^RS(d,4)^RS(d,3)^d^LS(d,1) ) & 2;
	A0 = ( RS(d,6)^RS(d,5)^RS(d,4)^RS(d,3)^d ) & 1;
*/	
	return ((unsigned short)d << 4 | (tmp & 0x0f));
}
/*
static int defec12_8(const unsigned short data)
{
	unsigned short idx;
	int sc;
	int ret;
	
	idx = enfec12_8((unsigned char)(data >> 4)) & 0x0f;
	idx = idx ^ data;
	sc = sym_idx[idx & 0x0f];

	if(sc < 0)
		return sc;
	else if(sc > 0 && sc < 13)
		return BCORRECT(data,sc - 1) >> 4;
	else 
		return data >> 4;
}*/

static int defec12_8(const unsigned short fecc,unsigned char *data)
{
	unsigned short idx;
	int sc;
	
	idx = enfec12_8((unsigned char)(fecc >> 4)) & 0x0f;
	idx = idx ^ fecc;
	sc = sym_idx[idx & 0x0f];
	
	if(sc > 0 && sc < 13){
		*data = BCORRECT(fecc,sc - 1) >> 4;
		return 1;
	}
	*data = fecc >> 4;
	return sc < 0 ? -1 : 0;
}

static int fec_encode12_8(void *const in,const size_t len,void *const out)
{
	int i;
	const unsigned char *idata = (unsigned char *)in;
	unsigned short *odata = (unsigned short*)out;
	
	for(i = 0; i < len; i++){
		odata[i] = enfec12_8(idata[i]);
	}
	
	return 0;
}

static int fec_decode12_8(void *const in,const size_t len,void *const out)
{
	int i;
	int ret = 0,rval;
	const unsigned short *idata = (unsigned short*)in;
	unsigned char  *odata = (unsigned char *)out;
	
	for(i = 0; i < len; i++){
		rval = defec12_8(idata[i],&odata[i]);
		if(rval < 0 && !ret)
			ret = rval;
	}
	
	return ret;
}

static int  fec_decode_verbose_12_8(void *const in,const size_t len,void *const out,fecfrm_stat_t *const stat)
{
	int i;
	int rval;
	const unsigned short *idata = (unsigned short*)in;
	unsigned char  *odata = (unsigned char *)out;
	unsigned char  errors = 0;
	
	for(i = 0; i < len; i++){
		rval = defec12_8(idata[i],&odata[i]);
		if(rval < 0)
			errors++;
		else{
			stat->efree += !rval;
			stat->fixed += rval;
		}
	}
	
	stat->error += errors;
	
	return errors;
}

/*
static void matrix_transpose(const short *in,const int rows,const int cols,short *out)
{
	int i,j,n = 0;
	
	for(i = 0; i < cols;i++){
		for(j = 0; j < rows;j++)
			out[n++] = in[j * cols + i];
	}
}

void fecs_map_signs12_8(const struct fec *fec,void *const fec_in,const size_t nfecs,void *const sign_out)
{
	int i,j,n = 0;
	int rows,cols;
	int bpfecc = fec->bits_per_fecc;
	const unsigned short *fecc = (const unsigned short*)fec_in;
	short sign[nfecs * bpfecc];
	//short *sign = (short*)sign_out;
	
	for(i = 0;i < nfecs; i++){
		for(j = bpfecc -1; j >= 0; j--){
			sign[n++] = (fecc[i] & (1 << j)) == 0 ? -1 : 1;
		}
	}
	
	rows = nfecs;
	cols = bpfecc;
	matrix_transpose(sign,rows,cols,sign_out);
}

void signs_map_fecs12_8(const struct fec *fec,void *const sign_in,const size_t nsigns,void *const fec_out)
{
	int i,n = 0,ns = 0;
	int bpfecc = fec->bits_per_fecc;
	unsigned short *fecc = (unsigned short *)fec_out;
	short sign[nsigns];
	//const short *sign = (const short *)sign_in;
	
	rows = bpfec;
	cols = nsigns / bpfec;
	matrix_transpose(sign_in,rows,cols,sign);
	
	while(ns < nsigns){
		fecc[n] = 0;
		for(i = 0; i < bpfecc && ns < nsigns; i++){
			fecc[n] <<= 1;
			if(sign[ns++] > 0)
				fecc[n] |= 1;
		}
		n++;
	}
	
	
}
*/

#define TRANSPOSE(r,c,rows,cols) ((c) * (rows) + (r))
#define POSE(r,c,rows,cols)		 ((r) * (cols) + (c))

void fecs_map_signs12_8(const struct fec *fec,void *const fec_in,const size_t nfecs,void *const sign_out)
{
	const unsigned short *const fecc = (const unsigned short *const)fec_in;
	short *const sign = (short *const)sign_out;
	int i,j;
	int cols = fec->bits_per_fecc;
	int rows = nfecs;
	
	for(i = 0; i < rows; i++){
		for(j = 0; j < cols; j++){
			sign[TRANSPOSE(i,j,rows,cols)] = (fecc[i] & (1 << j)) == 0 ? -1 : 1;
		}
	}
}

void signs_map_fecs12_8(const struct fec *fec,void *const sign_in,const size_t nsigns,void *const fec_out)
{
	int i,j,n = 0;
	int rows = fec->bits_per_fecc;
	int cols = nsigns / rows;
	unsigned short *const fecc = (unsigned short *const)fec_out;
	const short *const sign = (const short *const)sign_in;
	
	for(n = 0; n < rows;n++)
		fecc[n] = 0;
	
	for(i = 0; i < rows; i++){
		for(j = 0; j < cols; j++){
			if(sign[POSE(i,j,rows,cols)] > 0)
				fecc[j] |= 1 << i;
		}
	}
}

/*------------------------------------------------------------------*/


/*------------------------------------------------------------------*/
static int fec_mode_match(struct fec *fec,enum fec_mode mode)
{
	switch(mode){
		case FEC12_8:
			fec->bits_per_fecc = 12;
			fec->size_per_fecc = 2;//(10 + 7)/8
			fec->infb_per_fecc = 8;
			fec->size_per_sign = 2;
			fec->encode    = fec_encode12_8;
			fec->decode    = fec_decode12_8;
			fec->decode_verbose = fec_decode_verbose_12_8;
			fec->signmap   = fecs_map_signs12_8;
			fec->designmap = signs_map_fecs12_8;
			break;
		default:
			return -1;
	}
	
	return 0;
}

struct fec* fec_create(enum fec_mode mode)
{
	struct fec *fec = (struct fec*)malloc(sizeof(struct fec));
	if(!fec)
		return NULL;
	
	if(fec_mode_match(fec,mode) < 0){
		free(fec);
		return NULL;
	}
	
	return fec;
}

void fec_destroy(struct fec *fec)
{
	free(fec);
}
