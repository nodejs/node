#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>

extern unsigned long OPENSSL_s390xcap_P[];

static sigjmp_buf ill_jmp;
static void ill_handler (int sig) { siglongjmp(ill_jmp,sig); }

unsigned long OPENSSL_s390x_facilities(void);

void OPENSSL_cpuid_setup(void)
	{
	sigset_t oset;
	struct sigaction ill_act,oact;

	if (OPENSSL_s390xcap_P[0]) return;

	OPENSSL_s390xcap_P[0] = 1UL<<(8*sizeof(unsigned long)-1);

	memset(&ill_act,0,sizeof(ill_act));
	ill_act.sa_handler = ill_handler;
	sigfillset(&ill_act.sa_mask);
	sigdelset(&ill_act.sa_mask,SIGILL);
	sigdelset(&ill_act.sa_mask,SIGTRAP);
	sigprocmask(SIG_SETMASK,&ill_act.sa_mask,&oset);
	sigaction (SIGILL,&ill_act,&oact);

	/* protection against missing store-facility-list-extended */
	if (sigsetjmp(ill_jmp,1) == 0)
		OPENSSL_s390x_facilities();

	sigaction (SIGILL,&oact,NULL);
	sigprocmask(SIG_SETMASK,&oset,NULL);
	}
