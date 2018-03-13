#ifndef _SAMCODER_H_
#define _SAMCODER_H_

#include <codec2.h>
#include "fec.h"

#define MODE_1200	CODEC2_MODE_1200
#define MODE_2400	CODEC2_MODE_2400

extern  const unsigned char dpat_tab[4][6];

typedef struct samcoder{
	int 	      _codec2_mode;
	int 		  _fec_mode;
	size_t 		  _nfecc;
	struct CODEC2  *codec;
	struct fec 	   *fec;
	unsigned char  *bits;
	unsigned char  *fecc;
	unsigned char  *patfrm;
}samcoder_t;

struct samcoder* 
	 samcoder_create(int mode,...);
struct samcoder*
	 samcoder_clone(struct samcoder *coder);
void samcoder_destroy(struct samcoder *coder);

void samcoder_encode( struct samcoder *coder,short *samples,short *fecbits);
int  samcoder_decode( struct samcoder *coder,short *fecbits,short *samples);
void samcoder_encode_patdata(struct samcoder *coder,short *signs);
int  samcoder_decode_verbose(struct samcoder *coder,short *fecbits,short *samples,fecfrm_stat_t *stat);
int  samcoder_set_test_patdata(struct samcoder *coder, const unsigned char *data);
//����ÿ֡��Ҫ������Ƶ������ѹ����fec����
static inline int samcoder_samples_per_frame(struct samcoder *coder)
{
	return codec2_samples_per_frame(coder->codec);
}
//����ÿ֡�ж���fec��
static inline int samcoder_fecs_per_frame(struct samcoder *coder)
{
	return codec2_bits_per_frame(coder->codec) / infb_per_fecc(coder->fec);
}
//ÿ��fec��Ĵ�С
static inline int samcoder_size_per_fec(struct samcoder *coder)
{
	return size_per_fecc(coder->fec);
}
//ÿ֡����Ҫ���ٿռ����洢
static inline int samcoder_fecsize_per_frame(struct samcoder *coder)
{
	return size_per_fecc(coder->fec) * samcoder_fecs_per_frame(coder);
}
//ÿ֡�ж�����Чbit(���)���
static inline int samcoder_bits_per_frame(struct samcoder *coder)
{
	return bits_per_fecc(coder->fec) * samcoder_fecs_per_frame(coder);
}

static inline int samcoder_signsize_per_frame(struct samcoder *coder)
{
	return size_per_fecc(coder->fec) * samcoder_bits_per_frame(coder);
}

static inline int samcoder_verbose_check(const struct samcoder *coder)
{
	return (coder->patfrm != NULL);
}

#endif
