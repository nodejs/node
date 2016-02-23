#!/usr/bin/env perl
#
# For Microsoft CL this is implemented as inline assembler. So that
# even though this script can generate even Win32 code, we'll be
# using it primarily to generate Win64 modules. Both IA-64 and AMD64
# are supported...

# pull APPLINK_MAX value from applink.c...
$applink_c=$0;
$applink_c=~s|[^/\\]+$||g;
$applink_c.="applink.c";
open(INPUT,$applink_c) || die "can't open $applink_c: $!";
@max=grep {/APPLINK_MAX\s+(\d+)/} <INPUT>;
close(INPUT);
($#max==0) or die "can't find APPLINK_MAX in $applink_c";

$max[0]=~/APPLINK_MAX\s+(\d+)/;
$N=$1;	# number of entries in OPENSSL_UplinkTable not including
	# OPENSSL_UplinkTable[0], which contains this value...

# Idea is to fill the OPENSSL_UplinkTable with pointers to stubs
# which invoke 'void OPENSSL_Uplink (ULONG_PTR *table,int index)';
# and then dereference themselves. Latter shall result in endless
# loop *unless* OPENSSL_Uplink does not replace 'table[index]' with
# something else, e.g. as 'table[index]=unimplemented;'...

$arg = shift;
#( defined shift || open STDOUT,">$arg" ) || die "can't open $arg: $!";

if ($arg =~ /win32n/)	{ ia32nasm();  }
elsif ($arg =~ /win32/)	{ ia32masm();  }
elsif ($arg =~ /coff/)	{ ia32gas();   }
elsif ($arg =~ /win64i/ or $arg =~ /ia64/)	{ ia64ias();   }
elsif ($arg =~ /win64a/ or $arg =~ /amd64/)	{ amd64masm(); }
else	{ die "nonsense $arg"; }

sub ia32gas() {
print <<___;
.text
___
for ($i=1;$i<=$N;$i++) {
print <<___;
.def	.Lazy$i;	.scl	3;	.type	32;	.endef
.align	4
.Lazy$i:
	pushl	\$$i
	pushl	\$_OPENSSL_UplinkTable
	call	_OPENSSL_Uplink
	addl	\$8,%esp
	jmp	*(_OPENSSL_UplinkTable+4*$i)
___
}
print <<___;
.data
.align	4
.globl  _OPENSSL_UplinkTable
_OPENSSL_UplinkTable:
	.long	$N
___
for ($i=1;$i<=$N;$i++) {   print "	.long	.Lazy$i\n";   }
}

sub ia32masm() {
print <<___;
.386P
.model	FLAT

_DATA	SEGMENT
PUBLIC	_OPENSSL_UplinkTable
_OPENSSL_UplinkTable	DD	$N	; amount of following entries
___
for ($i=1;$i<=$N;$i++) {   print "	DD	FLAT:\$lazy$i\n";   }
print <<___;
_DATA	ENDS

_TEXT	SEGMENT
EXTRN	_OPENSSL_Uplink:NEAR
___
for ($i=1;$i<=$N;$i++) {
print <<___;
ALIGN	4
\$lazy$i	PROC NEAR
	push	$i
	push	OFFSET FLAT:_OPENSSL_UplinkTable
	call	_OPENSSL_Uplink
	add	esp,8
	jmp	DWORD PTR _OPENSSL_UplinkTable+4*$i
\$lazy$i	ENDP
___
}
print <<___;
ALIGN	4
_TEXT	ENDS
END
___
}

sub ia32nasm() {
print <<___;
SEGMENT	.data
GLOBAL	_OPENSSL_UplinkTable
_OPENSSL_UplinkTable	DD	$N	; amount of following entries
___
for ($i=1;$i<=$N;$i++) {   print "	DD	\$lazy$i\n";   }
print <<___;

SEGMENT	.text
EXTERN	_OPENSSL_Uplink
___
for ($i=1;$i<=$N;$i++) {
print <<___;
ALIGN	4
\$lazy$i:
	push	$i
	push	_OPENSSL_UplinkTable
	call	_OPENSSL_Uplink
	add	esp,8
	jmp	[_OPENSSL_UplinkTable+4*$i]
___
}
print <<___;
ALIGN	4
END
___
}

sub ia64ias () {
local $V=8;	# max number of args uplink functions may accept...
print <<___;
.data
.global	OPENSSL_UplinkTable#
OPENSSL_UplinkTable:	data8	$N	// amount of following entries
___
for ($i=1;$i<=$N;$i++) {   print "	data8	\@fptr(lazy$i#)\n";   }
print <<___;
.size	OPENSSL_UplinkTable,.-OPENSSL_UplinkTable#

.text
.global	OPENSSL_Uplink#
.type	OPENSSL_Uplink#,\@function
___
for ($i=1;$i<=$N;$i++) {
print <<___;
.proc	lazy$i
lazy$i:
{ .mii;	alloc	loc0=ar.pfs,$V,3,2,0
	mov	loc1=b0
	addl	loc2=\@ltoff(OPENSSL_UplinkTable#),gp	};;
{ .mmi;	ld8	out0=[loc2]
	mov	out1=$i					};;
{ .mib;	adds	loc2=8*$i,out0
	br.call.sptk.many	b0=OPENSSL_Uplink#	};;
{ .mmi;	ld8	r31=[loc2];;
	ld8	r30=[r31],8				};;
{ .mii;	ld8	gp=[r31]
	mov	b6=r30
	mov	b0=loc1					};;
{ .mib; mov	ar.pfs=loc0
	br.many	b6					};;
.endp	lazy$i#
___
}
}

sub amd64masm() {
print <<___;
_DATA	SEGMENT
PUBLIC	OPENSSL_UplinkTable
OPENSSL_UplinkTable	DQ	$N
___
for ($i=1;$i<=$N;$i++) {   print "	DQ	\$lazy$i\n";   }
print <<___;
_DATA	ENDS

_TEXT	SEGMENT
EXTERN	OPENSSL_Uplink:PROC
___
for ($i=1;$i<=$N;$i++) {
print <<___;
ALIGN	4
\$lazy$i	PROC
	push	r9
	push	r8
	push	rdx
	push	rcx
	sub	rsp,40
	lea	rcx,OFFSET OPENSSL_UplinkTable
	mov	rdx,$i
	call	OPENSSL_Uplink
	add	rsp,40
	pop	rcx
	pop	rdx
	pop	r8
	pop	r9
	jmp	QWORD PTR OPENSSL_UplinkTable+8*$i
\$lazy$i	ENDP
___
}
print <<___;
_TEXT	ENDS
END
___
}

