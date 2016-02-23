//
// gettsc.inl
//
// gives access to the Pentium's (secret) cycle counter
//
// This software was written by Leonard Janke (janke@unixg.ubc.ca)
// in 1996-7 and is entered, by him, into the public domain.

#if defined(__WATCOMC__)
void GetTSC(unsigned long&);
#pragma aux GetTSC = 0x0f 0x31 "mov [edi], eax" parm [edi] modify [edx eax];
#elif defined(__GNUC__)
inline
void GetTSC(unsigned long& tsc)
{
  asm volatile(".byte 15, 49\n\t"
	       : "=eax" (tsc)
	       :
	       : "%edx", "%eax");
}
#elif defined(_MSC_VER)
inline
void GetTSC(unsigned long& tsc)
{
  unsigned long a;
  __asm _emit 0fh
  __asm _emit 31h
  __asm mov a, eax;
  tsc=a;
}
#endif      

#include <stdio.h>
#include <stdlib.h>
#include <openssl/ripemd.h>

#define ripemd160_block_x86 ripemd160_block_asm_host_order

extern "C" {
void ripemd160_block_x86(RIPEMD160_CTX *ctx, unsigned char *buffer,int num);
}

void main(int argc,char *argv[])
	{
	unsigned char buffer[64*256];
	RIPEMD160_CTX ctx;
	unsigned long s1,s2,e1,e2;
	unsigned char k[16];
	unsigned long data[2];
	unsigned char iv[8];
	int i,num=0,numm;
	int j=0;

	if (argc >= 2)
		num=atoi(argv[1]);

	if (num == 0) num=16;
	if (num > 250) num=16;
	numm=num+2;
#if 0
	num*=64;
	numm*=64;
#endif

	for (j=0; j<6; j++)
		{
		for (i=0; i<10; i++) /**/
			{
			ripemd160_block_x86(&ctx,buffer,numm);
			GetTSC(s1);
			ripemd160_block_x86(&ctx,buffer,numm);
			GetTSC(e1);
			GetTSC(s2);
			ripemd160_block_x86(&ctx,buffer,num);
			GetTSC(e2);
			ripemd160_block_x86(&ctx,buffer,num);
			}
		printf("ripemd160 (%d bytes) %d %d (%.2f)\n",num*64,
			e1-s1,e2-s2,(double)((e1-s1)-(e2-s2))/2);
		}
	}

