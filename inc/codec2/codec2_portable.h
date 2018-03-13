#ifndef __CODEC2_PORTABLE_H__
#define __CODEC2_PORTABLE_H__


//portable for sysbios
/***********************************************************************/

#define M_PI       3.14159265358979323846

static inline long int lroundf(float x)
{
	return (long int)(x>=0?(x+0.5):(x-0.5));
}

/***********************************************************************/

#endif

