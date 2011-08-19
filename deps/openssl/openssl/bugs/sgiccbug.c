/* NOCW */
/* sgibug.c */
/* bug found by Eric Young (eay@mincom.oz.au) May 95 */

#include <stdio.h>

/* This compiler bug it present on IRIX 5.3, 5.1 and 4.0.5 (these are
 * the only versions of IRIX I have access to.
 * defining FIXBUG removes the bug.
 * (bug is still present in IRIX 6.3 according to
 * Gage <agage@forgetmenot.Mines.EDU>
 */
 
/* Compare the output from
 * cc sgiccbug.c; ./a.out
 * and
 * cc -O sgiccbug.c; ./a.out
 */

static unsigned long a[4]={0x01234567,0x89ABCDEF,0xFEDCBA98,0x76543210};
static unsigned long b[4]={0x89ABCDEF,0xFEDCBA98,0x76543210,0x01234567};
static unsigned long c[4]={0x77777778,0x8ACF1357,0x88888888,0x7530ECA9};

main()
	{
	unsigned long r[4];
	sub(r,a,b);
	fprintf(stderr,"input a= %08X %08X %08X %08X\n",a[3],a[2],a[1],a[0]);
	fprintf(stderr,"input b= %08X %08X %08X %08X\n",b[3],b[2],b[1],b[0]);
	fprintf(stderr,"output = %08X %08X %08X %08X\n",r[3],r[2],r[1],r[0]);
	fprintf(stderr,"correct= %08X %08X %08X %08X\n",c[3],c[2],c[1],c[0]);
	}

int sub(r,a,b)
unsigned long *r,*a,*b;
	{
	register unsigned long t1,t2,*ap,*bp,*rp;
	int i,carry;
#ifdef FIXBUG
	unsigned long dummy;
#endif

	ap=a;
	bp=b;
	rp=r;
	carry=0;
	for (i=0; i<4; i++)
		{
		t1= *(ap++);
		t2= *(bp++);
		t1=(t1-t2);
#ifdef FIXBUG
		dummy=t1;
#endif
		*(rp++)=t1&0xffffffff;
		}
	}
