#ifndef _FEC_H_
#define _FEC_H_

#include <stdlib.h>

//d����λbitλ
#define LS(d,bit)		((d) << (bit))

//d����λbitλ
#define RS(d,bit)		((d) >> (bit))

//����d��bitλ
#define BCORRECT(d,bit) ((d) ^ ((1UL) << bit))

#define FECFRM_STAT_INITIALIZER {0,0,0,0}

typedef struct fec_frame_statistics{
	unsigned int efree;
	unsigned int fixed;
	unsigned int error;
	unsigned int ebits;
}fecfrm_stat_t;

struct fec;

typedef int  (*fec_func_t)(void *const in,const size_t len,void *const out);
typedef int  (*fec_func_verbose_t)(void *const in,const size_t len,void *const out,fecfrm_stat_t *const stat);
typedef void (*fec_signmap_t)(const struct fec *fec,void *const in,const size_t len,void *const out);



struct fec{
	int size_per_fecc;	//ÿ��fec֡��Ҫ�����ֽڴ洢
	int bits_per_fecc;	//ÿ��fec֡�ж��ٸ���Чbit
	int infb_per_fecc;	//ÿ��fec֡�ж��ٸ���Ϣλ��Ҫ��8�ı���
	int size_per_sign;		//ÿ��fec֡�е�bitӳ��Ϊ���ʱ���洢һ�������Ҫ�ڴ��С(��λ�ֽ�)
	fec_func_t    encode;
	fec_func_t    decode;
	fec_signmap_t signmap;
	fec_signmap_t designmap;
	fec_func_verbose_t decode_verbose;
};

enum fec_mode{
	FEC12_8 = 1
	//...
};

struct fec* 
     fec_create(enum fec_mode mode);
void fec_destroy(struct fec *fec);

void fecs_map_signs12_8(const struct fec *fec,void * const fec_in,const size_t nfecs,void * const sign_out);
void signs_map_fecs12_8(const struct fec *fec,void * const sign_in,const size_t nsigns,void * const fec_out);

//ÿ��fec������Ҫ�����ֽڴ洢
static inline size_per_fecc(const struct fec *fec)
{
	return fec->size_per_fecc;
}
//ÿ��fec�������ٸ���Чbit
static inline bits_per_fecc(const struct fec *fec)
{
	return fec->bits_per_fecc;
}
//ÿ��fec��������λ��Ϣbit
static inline infb_per_fecc(const struct fec *fec)
{
	//return fec->bits_per_fecc - fec->fixb_per_fecfrm;
	return fec->infb_per_fecc;
}
//�����������һ��fec֡����Ϣbit��Ҫ���ٸ��ֽڴ洢
static inline info_byte_per_fecfrm(const struct fec *fec)
{
	return (infb_per_fecc(fec)+7) / 8;
}
//ÿ�������Ҫ�����ֽڴ洢
static inline size_per_sign(const struct fec *fec)
{
	return fec->size_per_sign;
}

#endif
