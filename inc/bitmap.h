#ifndef __BITMAP__H
#define __BITMAP__H

#include <stdint.h>

#if defined(__cplusplus)
extern "C"{
#endif

#define BIT(n)			(1 << (n))
#define LONGBIT(n)		((1LL) << (n))

#define MASK(start,len)	((((1LL) << (len)) - 1) << (start))

#define BITMAPINITIALIZER 0

typedef long long int bitmap;

static inline void bitmap_set(bitmap *bmap,const int bit)
{
	long long int mask = (1 << bit);
	*bmap |= mask;
}

static inline void bitmap_set2(bitmap *bmap,const long long bit)
{
	*bmap |= bit;
}

static inline int bitmap_test(const bitmap *bmap,const int bit)
{
	long long int mask = 1 << bit;
	return ((*bmap & mask) == mask);
}

static inline int bitmap_test2(const bitmap *bmap,const long long bitv)
{
	return (*bmap & bitv == bitv);
}

static inline void bitmap_sets(bitmap *bmap,long long mask,long long value)
{
	*bmap |= value & mask;
}

static inline void bitmap_sets2(bitmap *bmap,int start,int len,long long value)
{

	*bmap |= (value << start) & MASK(start,len);
}

static inline void bitmap_clear(bitmap *bmap)
{
	*bmap = 0;
}

/*
	计算一个数所含的1的个数。
	bitone8计算8个bit位中1的个数。
	bitone16计算16个bit位中1的个数。
	bitone32计算32个bit位中1的个数。
*/
static inline uint32_t _bitones8(uint32_t num)
{
	num = ((num & 0xaaaaaaaa) >> 1) + (num & 0x55555555);
	num = ((num & 0xcccccccc) >> 2) + (num & 0x33333333);
	num = ((num & 0xf0f0f0f0) >> 4) + (num & 0x0f0f0f0f);
	
	return num;
}

static inline uint32_t _bitones16(uint32_t num)
{
	num = _bitones8(num);
	num = ((num & 0xff00ff00) >> 8) + (num & 0x00ff00ff);
	
	return num;
}

static inline uint32_t _bitones32(uint32_t num)
{
	num = _bitones16(num);
	num = ((num & 0xffff0000) >> 16) + (num & 0x0000ffff);
	
	return num;
}

//计算0的个数
#define bitones7(num)   _bitones8( (num) & 0x0000007f)
#define bitones8(num)	_bitones8( (num) & 0x000000ff)
#define bitones16(num)	_bitones16((num) & 0x0000ffff)
#define bitones32(num)	_bitones32(num)

#define bitzeros8(num) 	( 8 - bitones8(num) )
#define bitzeros16(num)	(16 - bitones16(num))
#define bitzeros32(num)	(32 - bitones32(num))
#define bitzeros7(num)	( 7 - bitones7(num) )


#if defined(__cplusplus)
}
#endif

#endif
