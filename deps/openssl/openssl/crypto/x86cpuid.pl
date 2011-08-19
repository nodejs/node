#!/usr/bin/env perl

push(@INC,"perlasm");
require "x86asm.pl";

&asm_init($ARGV[0],"x86cpuid");

for (@ARGV) { $sse2=1 if (/-DOPENSSL_IA32_SSE2/); }

&function_begin("OPENSSL_ia32_cpuid");
	&xor	("edx","edx");
	&pushf	();
	&pop	("eax");
	&mov	("ecx","eax");
	&xor	("eax",1<<21);
	&push	("eax");
	&popf	();
	&pushf	();
	&pop	("eax");
	&xor	("ecx","eax");
	&bt	("ecx",21);
	&jnc	(&label("done"));
	&xor	("eax","eax");
	&cpuid	();
	&xor	("eax","eax");
	&cmp	("ebx",0x756e6547);	# "Genu"
	&data_byte(0x0f,0x95,0xc0);	#&setne	(&LB("eax"));
	&mov	("ebp","eax");
	&cmp	("edx",0x49656e69);	# "ineI"
	&data_byte(0x0f,0x95,0xc0);	#&setne	(&LB("eax"));
	&or	("ebp","eax");
	&cmp	("ecx",0x6c65746e);	# "ntel"
	&data_byte(0x0f,0x95,0xc0);	#&setne	(&LB("eax"));
	&or	("ebp","eax");
	&mov	("eax",1);
	&cpuid	();
	&cmp	("ebp",0);
	&jne	(&label("notP4"));
	&and	("eax",15<<8);		# familiy ID
	&cmp	("eax",15<<8);		# P4?
	&jne	(&label("notP4"));
	&or	("edx",1<<20);		# use reserved bit to engage RC4_CHAR
&set_label("notP4");
	&bt	("edx",28);		# test hyper-threading bit
	&jnc	(&label("done"));
	&shr	("ebx",16);
	&and	("ebx",0xff);
	&cmp	("ebx",1);		# see if cache is shared(*)
	&ja	(&label("done"));
	&and	("edx",0xefffffff);	# clear hyper-threading bit if not
&set_label("done");
	&mov	("eax","edx");
	&mov	("edx","ecx");
&function_end("OPENSSL_ia32_cpuid");
# (*)	on Core2 this value is set to 2 denoting the fact that L2
#	cache is shared between cores.

&external_label("OPENSSL_ia32cap_P");

&function_begin_B("OPENSSL_rdtsc","EXTRN\t_OPENSSL_ia32cap_P:DWORD");
	&xor	("eax","eax");
	&xor	("edx","edx");
	&picmeup("ecx","OPENSSL_ia32cap_P");
	&bt	(&DWP(0,"ecx"),4);
	&jnc	(&label("notsc"));
	&rdtsc	();
&set_label("notsc");
	&ret	();
&function_end_B("OPENSSL_rdtsc");

# This works in Ring 0 only [read DJGPP+MS-DOS+privileged DPMI host],
# but it's safe to call it on any [supported] 32-bit platform...
# Just check for [non-]zero return value...
&function_begin_B("OPENSSL_instrument_halt","EXTRN\t_OPENSSL_ia32cap_P:DWORD");
	&picmeup("ecx","OPENSSL_ia32cap_P");
	&bt	(&DWP(0,"ecx"),4);
	&jnc	(&label("nohalt"));	# no TSC

	&data_word(0x9058900e);		# push %cs; pop %eax
	&and	("eax",3);
	&jnz	(&label("nohalt"));	# not enough privileges

	&pushf	();
	&pop	("eax")
	&bt	("eax",9);
	&jnc	(&label("nohalt"));	# interrupts are disabled

	&rdtsc	();
	&push	("edx");
	&push	("eax");
	&halt	();
	&rdtsc	();

	&sub	("eax",&DWP(0,"esp"));
	&sbb	("edx",&DWP(4,"esp"));
	&add	("esp",8);
	&ret	();

&set_label("nohalt");
	&xor	("eax","eax");
	&xor	("edx","edx");
	&ret	();
&function_end_B("OPENSSL_instrument_halt");

# Essentially there is only one use for this function. Under DJGPP:
#
#	#include <go32.h>
#	...
#	i=OPENSSL_far_spin(_dos_ds,0x46c);
#	...
# to obtain the number of spins till closest timer interrupt.

&function_begin_B("OPENSSL_far_spin");
	&pushf	();
	&pop	("eax")
	&bt	("eax",9);
	&jnc	(&label("nospin"));	# interrupts are disabled

	&mov	("eax",&DWP(4,"esp"));
	&mov	("ecx",&DWP(8,"esp"));
	&data_word (0x90d88e1e);	# push %ds, mov %eax,%ds
	&xor	("eax","eax");
	&mov	("edx",&DWP(0,"ecx"));
	&jmp	(&label("spin"));

	&align	(16);
&set_label("spin");
	&inc	("eax");
	&cmp	("edx",&DWP(0,"ecx"));
	&je	(&label("spin"));

	&data_word (0x1f909090);	# pop	%ds
	&ret	();

&set_label("nospin");
	&xor	("eax","eax");
	&xor	("edx","edx");
	&ret	();
&function_end_B("OPENSSL_far_spin");

&function_begin_B("OPENSSL_wipe_cpu","EXTRN\t_OPENSSL_ia32cap_P:DWORD");
	&xor	("eax","eax");
	&xor	("edx","edx");
	&picmeup("ecx","OPENSSL_ia32cap_P");
	&mov	("ecx",&DWP(0,"ecx"));
	&bt	(&DWP(0,"ecx"),1);
	&jnc	(&label("no_x87"));
	if ($sse2) {
		&bt	(&DWP(0,"ecx"),26);
		&jnc	(&label("no_sse2"));
		&pxor	("xmm0","xmm0");
		&pxor	("xmm1","xmm1");
		&pxor	("xmm2","xmm2");
		&pxor	("xmm3","xmm3");
		&pxor	("xmm4","xmm4");
		&pxor	("xmm5","xmm5");
		&pxor	("xmm6","xmm6");
		&pxor	("xmm7","xmm7");
	&set_label("no_sse2");
	}
	# just a bunch of fldz to zap the fp/mm bank followed by finit...
	&data_word(0xeed9eed9,0xeed9eed9,0xeed9eed9,0xeed9eed9,0x90e3db9b);
&set_label("no_x87");
	&lea	("eax",&DWP(4,"esp"));
	&ret	();
&function_end_B("OPENSSL_wipe_cpu");

&function_begin_B("OPENSSL_atomic_add");
	&mov	("edx",&DWP(4,"esp"));	# fetch the pointer, 1st arg
	&mov	("ecx",&DWP(8,"esp"));	# fetch the increment, 2nd arg
	&push	("ebx");
	&nop	();
	&mov	("eax",&DWP(0,"edx"));
&set_label("spin");
	&lea	("ebx",&DWP(0,"eax","ecx"));
	&nop	();
	&data_word(0x1ab10ff0);	# lock;	cmpxchg	%ebx,(%edx)	# %eax is envolved and is always reloaded
	&jne	(&label("spin"));
	&mov	("eax","ebx");	# OpenSSL expects the new value
	&pop	("ebx");
	&ret	();
&function_end_B("OPENSSL_atomic_add");

# This function can become handy under Win32 in situations when
# we don't know which calling convention, __stdcall or __cdecl(*),
# indirect callee is using. In C it can be deployed as
#
#ifdef OPENSSL_CPUID_OBJ
#	type OPENSSL_indirect_call(void *f,...);
#	...
#	OPENSSL_indirect_call(func,[up to $max arguments]);
#endif
#
# (*)	it's designed to work even for __fastcall if number of
#	arguments is 1 or 2!
&function_begin_B("OPENSSL_indirect_call");
	{
	my $i,$max=7;		# $max has to be chosen as 4*n-1
				# in order to preserve eventual
				# stack alignment
	&push	("ebp");
	&mov	("ebp","esp");
	&sub	("esp",$max*4);
	&mov	("ecx",&DWP(12,"ebp"));
	&mov	(&DWP(0,"esp"),"ecx");
	&mov	("edx",&DWP(16,"ebp"));
	&mov	(&DWP(4,"esp"),"edx");
	for($i=2;$i<$max;$i++)
		{
		# Some copies will be redundant/bogus...
		&mov	("eax",&DWP(12+$i*4,"ebp"));
		&mov	(&DWP(0+$i*4,"esp"),"eax");
		}
	&call_ptr	(&DWP(8,"ebp"));# make the call...
	&mov	("esp","ebp");	# ... and just restore the stack pointer
				# without paying attention to what we called,
				# (__cdecl *func) or (__stdcall *one).
	&pop	("ebp");
	&ret	();
	}
&function_end_B("OPENSSL_indirect_call");

&initseg("OPENSSL_cpuid_setup");

&asm_finish();
