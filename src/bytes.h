#ifndef _BYTES_H
#define _BYTES_H

#if 0
// TODO: Big Endian platform defines here
#else
// Little Endian

#  if 0
// TODO: 64-bit assumptions here:
#  else
// 32-bit assumptions here:
//   sizeof(unsigned long long) = 8
//   sizeof(unsigned long) = 4
//   sizeof(unsigned short) = 2
//   sizeof(unsigned char) = 1
#    define LSETLWORD(x,y)		( *((unsigned long long*)(x)) = (y) )
#    define LSETDWORD(x,y)		( *((unsigned long*)(x)) = (y) )
#    define LSETWORD(x,y)		( *((unsigned short*)(x)) = (y) )
#    define LSETBYTE(x,y)		( *((unsigned char*)(x)) = (y) )

#    define LGETLWORD(x)		( *((unsigned long long*)(x)) )
#    define LGETDWORD(x)		( *((unsigned long*)(x)) )
#    define LGETWORD(x)			( *((unsigned short*)(x)) )
#    define LGETBYTE(x)			( *((unsigned char*)(x)) )
#  endif

#endif

#endif //_BYTES_H

