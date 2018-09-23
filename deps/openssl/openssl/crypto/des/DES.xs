#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "des.h"

#define deschar	char
static STRLEN len;

static int
not_here(s)
char *s;
{
    croak("%s not implemented on this architecture", s);
    return -1;
}

MODULE = DES	PACKAGE = DES	PREFIX = des_

char *
des_crypt(buf,salt)
	char *	buf
	char *	salt

void
des_set_odd_parity(key)
	des_cblock *	key
PPCODE:
	{
	SV *s;

	s=sv_newmortal();
	sv_setpvn(s,(char *)key,8);
	des_set_odd_parity((des_cblock *)SvPV(s,na));
	PUSHs(s);
	}

int
des_is_weak_key(key)
	des_cblock *	key

des_key_schedule
des_set_key(key)
	des_cblock *	key
CODE:
	des_set_key(key,RETVAL);
OUTPUT:
RETVAL

des_cblock
des_ecb_encrypt(input,ks,encrypt)
	des_cblock *	input
	des_key_schedule *	ks
	int	encrypt
CODE:
	des_ecb_encrypt(input,&RETVAL,*ks,encrypt);
OUTPUT:
RETVAL

void
des_cbc_encrypt(input,ks,ivec,encrypt)
	char *	input
	des_key_schedule *	ks
	des_cblock *	ivec
	int	encrypt
PPCODE:
	{
	SV *s;
	STRLEN len,l;
	char *c;

	l=SvCUR(ST(0));
	len=((((unsigned long)l)+7)/8)*8;
	s=sv_newmortal();
	sv_setpvn(s,"",0);
	SvGROW(s,len);
	SvCUR_set(s,len);
	c=(char *)SvPV(s,na);
	des_cbc_encrypt((des_cblock *)input,(des_cblock *)c,
		l,*ks,ivec,encrypt);
	sv_setpvn(ST(2),(char *)c[len-8],8);
	PUSHs(s);
	}

void
des_cbc3_encrypt(input,ks1,ks2,ivec1,ivec2,encrypt)
	char *	input
	des_key_schedule *	ks1
	des_key_schedule *	ks2
	des_cblock *	ivec1
	des_cblock *	ivec2
	int	encrypt
PPCODE:
	{
	SV *s;
	STRLEN len,l;

	l=SvCUR(ST(0));
	len=((((unsigned long)l)+7)/8)*8;
	s=sv_newmortal();
	sv_setpvn(s,"",0);
	SvGROW(s,len);
	SvCUR_set(s,len);
	des_3cbc_encrypt((des_cblock *)input,(des_cblock *)SvPV(s,na),
		l,*ks1,*ks2,ivec1,ivec2,encrypt);
	sv_setpvn(ST(3),(char *)ivec1,8);
	sv_setpvn(ST(4),(char *)ivec2,8);
	PUSHs(s);
	}

void
des_cbc_cksum(input,ks,ivec)
	char *	input
	des_key_schedule *	ks
	des_cblock *	ivec
PPCODE:
	{
	SV *s1,*s2;
	STRLEN len,l;
	des_cblock c;
	unsigned long i1,i2;

	s1=sv_newmortal();
	s2=sv_newmortal();
	l=SvCUR(ST(0));
	des_cbc_cksum((des_cblock *)input,(des_cblock *)c,
		l,*ks,ivec);
	i1=c[4]|(c[5]<<8)|(c[6]<<16)|(c[7]<<24);
	i2=c[0]|(c[1]<<8)|(c[2]<<16)|(c[3]<<24);
	sv_setiv(s1,i1);
	sv_setiv(s2,i2);
	sv_setpvn(ST(2),(char *)c,8);
	PUSHs(s1);
	PUSHs(s2);
	}

void
des_cfb_encrypt(input,numbits,ks,ivec,encrypt)
	char *	input
	int	numbits
	des_key_schedule *	ks
	des_cblock *	ivec
	int	encrypt
PPCODE:
	{
	SV *s;
	STRLEN len;
	char *c;

	len=SvCUR(ST(0));
	s=sv_newmortal();
	sv_setpvn(s,"",0);
	SvGROW(s,len);
	SvCUR_set(s,len);
	c=(char *)SvPV(s,na);
	des_cfb_encrypt((unsigned char *)input,(unsigned char *)c,
		(int)numbits,(long)len,*ks,ivec,encrypt);
	sv_setpvn(ST(3),(char *)ivec,8);
	PUSHs(s);
	}

des_cblock *
des_ecb3_encrypt(input,ks1,ks2,encrypt)
	des_cblock *	input
	des_key_schedule *	ks1
	des_key_schedule *	ks2
	int	encrypt
CODE:
	{
	des_cblock c;

	des_ecb3_encrypt((des_cblock *)input,(des_cblock *)&c,
		*ks1,*ks2,encrypt);
	RETVAL= &c;
	}
OUTPUT:
RETVAL

void
des_ofb_encrypt(input,numbits,ks,ivec)
	unsigned char *	input
	int	numbits
	des_key_schedule *	ks
	des_cblock *	ivec
PPCODE:
	{
	SV *s;
	STRLEN len,l;
	unsigned char *c;

	len=SvCUR(ST(0));
	s=sv_newmortal();
	sv_setpvn(s,"",0);
	SvGROW(s,len);
	SvCUR_set(s,len);
	c=(unsigned char *)SvPV(s,na);
	des_ofb_encrypt((unsigned char *)input,(unsigned char *)c,
		numbits,len,*ks,ivec);
	sv_setpvn(ST(3),(char *)ivec,8);
	PUSHs(s);
	}

void
des_pcbc_encrypt(input,ks,ivec,encrypt)
	char *	input
	des_key_schedule *	ks
	des_cblock *	ivec
	int	encrypt
PPCODE:
	{
	SV *s;
	STRLEN len,l;
	char *c;

	l=SvCUR(ST(0));
	len=((((unsigned long)l)+7)/8)*8;
	s=sv_newmortal();
	sv_setpvn(s,"",0);
	SvGROW(s,len);
	SvCUR_set(s,len);
	c=(char *)SvPV(s,na);
	des_pcbc_encrypt((des_cblock *)input,(des_cblock *)c,
		l,*ks,ivec,encrypt);
	sv_setpvn(ST(2),(char *)c[len-8],8);
	PUSHs(s);
	}

des_cblock *
des_random_key()
CODE:
	{
	des_cblock c;

	des_random_key(c);
	RETVAL=&c;
	}
OUTPUT:
RETVAL

des_cblock *
des_string_to_key(str)
char *	str
CODE:
	{
	des_cblock c;

	des_string_to_key(str,&c);
	RETVAL=&c;
	}
OUTPUT:
RETVAL

void
des_string_to_2keys(str)
char *	str
PPCODE:
	{
	des_cblock c1,c2;
	SV *s1,*s2;

	des_string_to_2keys(str,&c1,&c2);
	EXTEND(sp,2);
	s1=sv_newmortal();
	sv_setpvn(s1,(char *)c1,8);
	s2=sv_newmortal();
	sv_setpvn(s2,(char *)c2,8);
	PUSHs(s1);
	PUSHs(s2);
	}
