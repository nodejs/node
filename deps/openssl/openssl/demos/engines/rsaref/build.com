$! BUILD.COM -- Building procedure for the RSAref engine
$
$	if f$search("source.dir") .eqs. "" -
	   .or. f$search("install.dir") .eqs. ""
$	then
$	    write sys$error "RSAref 2.0 hasn't been properly extracted."
$	    exit
$	endif
$
$	if (f$getsyi("cpu").lt.128)
$	then
$	    arch := vax
$	else
$	    arch = f$edit( f$getsyi( "ARCH_NAME"), "UPCASE")
$	    if (arch .eqs. "") then arch = "UNK"
$	endif
$
$	_save_default = f$environment("default")
$	set default [.install]
$	files := desc,digit,md2c,md5c,nn,prime,-
		rsa,r_encode,r_dh,r_enhanc,r_keygen,r_random,-
		r_stdlib
$	delete rsaref.olb;*
$	library/create/object rsaref.olb
$	files_i = 0
$ rsaref_loop:
$	files_e = f$edit(f$element(files_i,",",files),"trim")
$	files_i = files_i + 1
$	if files_e .eqs. "," then goto rsaref_loop_end
$	cc/include=([-.source],[])/define=PROTOTYPES=1/object=[]'files_e'.obj -
		[-.source]'files_e'.c
$	library/replace/object rsaref.olb 'files_e'.obj
$	goto rsaref_loop
$ rsaref_loop_end:
$
$	set default [-]
$	define/user openssl [---.include.openssl]
$	cc/define=ENGINE_DYNAMIC_SUPPORT rsaref.c
$
$	if arch .eqs. "VAX"
$	then
$	    macro/object=rsaref_vec.obj sys$input:
;
; Transfer vector for VAX shareable image
;
	.TITLE librsaref
;
; Define macro to assist in building transfer vector entries.  Each entry
; should take no more than 8 bytes.
;
	.MACRO FTRANSFER_ENTRY routine
	.ALIGN QUAD
	.TRANSFER routine
	.MASK	routine
	JMP	routine+2
	.ENDM FTRANSFER_ENTRY
;
; Place entries in own program section.
;
	.PSECT $$LIBRSAREF,QUAD,PIC,USR,CON,REL,LCL,SHR,EXE,RD,NOWRT

LIBRSAREF_xfer:
	FTRANSFER_ENTRY bind_engine
	FTRANSFER_ENTRY v_check

;
; Allocate extra storage at end of vector to allow for expansion.
;
	.BLKB 512-<.-LIBRSAREF_xfer>	; 1 page.
	.END
$	    link/share=librsaref.exe sys$input:/option
!
! Ensure transfer vector is at beginning of image
!
CLUSTER=FIRST
COLLECT=FIRST,$$LIBRSAREF
!
! make psects nonshareable so image can be installed.
!
PSECT_ATTR=$CHAR_STRING_CONSTANTS,NOWRT
[]rsaref_vec.obj
[]rsaref.obj
[.install]rsaref.olb/lib
[---.vax.exe.crypto]libcrypto.olb/lib
$	else
$	    if arch_name .eqs. "ALPHA"
$	    then
$		link/share=librsaref.exe sys$input:/option
[]rsaref.obj
[.install]rsaref.olb/lib
[---.alpha.exe.crypto]libcrypto.olb/lib
symbol_vector=(bind_engine=procedure,v_check=procedure)
$	    else
$		if arch_name .eqs. "IA64"
$		then
$		    link /shareable=librsaref.exe sys$input: /options
[]rsaref.obj
[.install]rsaref.olb/lib
[---.ia64.exe.crypto]libcrypto.olb/lib
symbol_vector=(bind_engine=procedure,v_check=procedure)
$		endif
$	    endif
$	endif
$
$	set default '_save_default'
