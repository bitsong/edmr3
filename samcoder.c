#include <stdio.h>
#include <string.h>

#include "samcoder.h"
#include "bitmap.h"
#define GRAY

#ifdef GRAY
#define ISGRAY 1
#else
#define ISGRAY 0
#endif

const unsigned char dpat_tab[4][6] ={
	{0xac,0xbc,0xd2,0x11,0x4d,0xae},
	{0,0,0,0,0,0},
	{0,0,0,0,0,0},
	{0,0,0,0,0,0}
};

static int samcoder_construct(struct samcoder *coder,int mode)
{
	int bytes,fecsz,ret = 0;
	
	coder->codec = codec2_create(mode);
	if(!coder->codec){
		ret = -1;
		goto failed;
	}
	
	coder->fec = fec_create(FEC12_8);
	if(!coder->fec){
		ret = -2;
		goto failed;
	}

	bytes = (codec2_bits_per_frame(coder->codec) + 7) / 8;
	fecsz =  samcoder_fecsize_per_frame(coder);
	coder->bits = (unsigned char*)malloc(bytes);
	coder->fecc = (unsigned char*)malloc(fecsz);
	if(!coder->bits || !coder->fecc){
		ret = -3;
		goto failed;
	}
	
	coder->patfrm = NULL;
	coder->_codec2_mode = mode;
	coder->_fec_mode = FEC12_8;
	coder->_nfecc = samcoder_fecs_per_frame(coder);
	coder->patbits = NULL;
	codec2_set_natural_or_gray(coder->codec,ISGRAY);
	goto out;
	
failed:
	
	if(coder->bits)
		free(coder->bits);
	if(coder->fecc)
		free(coder->fecc);
	if(coder->fec)
		fec_destroy(coder->fec);
	if(coder->codec)
		codec2_destroy(coder->codec);
out:
	return ret;
}

static void samcoder_destruct(struct samcoder *coder)
{
	if(coder->bits){
		coder->bits = NULL;
		free(coder->bits);
	}
	if(coder->fecc){
		coder->bits = NULL;
		free(coder->fecc);
	}
	if(coder->fec){
		coder->fec = NULL;
		fec_destroy(coder->fec);
	}
	if(coder->codec){
		coder->codec = NULL;
		codec2_destroy(coder->codec);
	}
	coder->patbits = NULL;
}

struct samcoder* samcoder_create(int mode,...)
{
	struct samcoder *coder = (struct samcoder*)malloc(sizeof(struct samcoder));
	if(!coder)
		return NULL;
	
	if(samcoder_construct(coder,mode) < 0){
		free(coder);
		return NULL;
	}
	
	return coder;
}

void samcoder_destroy(struct samcoder *coder)
{
	if(coder){
		samcoder_destruct(coder);
		free(coder);
	}
}

struct samcoder* samcoder_clone(struct samcoder *coder)
{
	struct samcoder *codera = (struct samcoder*)malloc(sizeof(struct samcoder));
	if(!codera)
		return NULL;
	
	if(samcoder_construct(codera,coder->_codec2_mode) < 0){
		free(codera);
		return NULL;
	}
	return codera;
}


void samcoder_encode(struct samcoder *coder,short *samples,short *fecbits)
{
	codec2_encode(coder->codec,coder->bits,samples);
	coder->fec->encode(coder->bits,coder->_nfecc,coder->fecc);
	coder->fec->signmap(coder->fec,coder->fecc,coder->_nfecc,fecbits);
}

int  samcoder_decode(struct samcoder *coder,short *fecbits,short *samples)
{
	int rc;
	size_t signs;
	
	signs  =  coder->_nfecc * bits_per_fecc(coder->fec);
	
	coder->fec->designmap(coder->fec,fecbits,signs,coder->fecc);
	rc = coder->fec->decode(coder->fecc,coder->_nfecc,coder->bits);
	//if(rc < 0)
	//	return rc;
	codec2_decode(coder->codec,samples,coder->bits);
	return rc;
}

static int statistic_ebits_physical(const void *const realfrm,const size_t len,const void *const patfrm)
{
	int i;
	int cnt = 0;
	const unsigned short *hope = patfrm;
	const unsigned short *real = realfrm;
	
	for(i = 0; i < len; i++){
		cnt += bitones16(real[i] ^ hope[i]);
	}
	
	return cnt;
}

static int statistic_ebits_applayer(const void *const realbits,const size_t len,const void *const patbits)
{
	const unsigned char *hope = patbits;
	const unsigned char *real = realbits;
	int i,cnt = 0;
	
	for(i = 0; i < len; i++){
		cnt += bitones8(real[i] ^ hope[i]);
	}
	
	return cnt;
}

int  samcoder_set_test_patdata(struct samcoder *coder,const unsigned char *data)
{
	int fecsz;
	unsigned char *tmp;
	
	if(!coder->patfrm){
		fecsz =  samcoder_fecsize_per_frame(coder);
		tmp = (unsigned char*)malloc(fecsz);
		if(!tmp)
			return -1;
		coder->patfrm = tmp;
	}
	
	coder->patbits = data;
	coder->fec->encode(data,coder->_nfecc,coder->patfrm);
	return 0;
}

void samcoder_encode_patdata(struct samcoder *coder,short *signs)
{
	coder->fec->signmap(coder->fec,coder->patfrm,coder->_nfecc,signs);
}

int  samcoder_decode_verbose(struct samcoder *coder,short *fecbits,short *samples,fecfrm_stat_t *stat)
{
	int rc;
	size_t signs;
	struct fec *fec = coder->fec;
	
	fecfrm_stat_clear(stat);
	signs  =  coder->_nfecc * bits_per_fecc(fec);
	
	fec->designmap(fec,fecbits,signs,coder->fecc);
	stat->ebits = statistic_ebits_physical(coder->fecc,coder->_nfecc,coder->patfrm);
	rc = fec->decode_verbose(coder->fecc,coder->_nfecc,coder->bits,stat);
	if(rc < 0)
		return -2;
	
	stat->ebapp = statistic_ebits_applayer(coder->bits,coder->_nfecc,coder->patbits);
	
	codec2_decode(coder->codec,samples,coder->bits);
	return 0;
}
