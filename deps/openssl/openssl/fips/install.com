$! INSTALL.COM -- Installs the files in a given directory tree
$!
$! Author: Richard Levitte <richard@levitte.org>
$! Time of creation: 27-MAY-2004 11:47
$!
$! P1	root of the directory tree
$!
$	IF P1 .EQS. ""
$	THEN
$	    WRITE SYS$OUTPUT "First argument missing."
$	    WRITE SYS$OUTPUT "Should be the directory where you want things installed."
$	    EXIT
$	ENDIF
$
$	IF (F$GETSYI("CPU").LT.128)
$	THEN
$	    ARCH := VAX
$	ELSE
$	    ARCH = F$EDIT( F$GETSYI( "ARCH_NAME"), "UPCASE")
$	    IF (ARCH .EQS. "") THEN ARCH = "UNK"
$	ENDIF
$
$	ROOT = F$PARSE(P1,"[]A.;0",,,"SYNTAX_ONLY,NO_CONCEAL") - "A.;0"
$	ROOT_DEV = F$PARSE(ROOT,,,"DEVICE","SYNTAX_ONLY")
$	ROOT_DIR = F$PARSE(ROOT,,,"DIRECTORY","SYNTAX_ONLY") -
		   - "[000000." - "][" - "[" - "]"
$	ROOT = ROOT_DEV + "[" + ROOT_DIR
$
$	DEFINE/NOLOG WRK_SSLROOT 'ROOT'.] /TRANS=CONC
$	DEFINE/NOLOG WRK_SSLINCLUDE WRK_SSLROOT:[INCLUDE]
$
$	IF F$PARSE("WRK_SSLROOT:[000000]") .EQS. "" THEN -
	   CREATE/DIR/LOG WRK_SSLROOT:[000000]
$	IF F$PARSE("WRK_SSLINCLUDE:") .EQS. "" THEN -
	   CREATE/DIR/LOG WRK_SSLINCLUDE:
$
$	FDIRS := ,RAND,SHA,DES,AES,DSA,RSA,DH,HMAC
$	EXHEADER_ := fips.h
$	EXHEADER_SHA := fips_sha.h
$	EXHEADER_RAND := fips_rand.h
$	EXHEADER_DES :=
$	EXHEADER_AES :=
$	EXHEADER_DSA :=
$	EXHEADER_RSA :=
$	EXHEADER_DH :=
$	EXHEADER_HMAC :=
$
$	I = 0
$ LOOP_FDIRS: 
$	D = F$EDIT(F$ELEMENT(I, ",", FDIRS),"TRIM")
$	I = I + 1
$	IF D .EQS. "," THEN GOTO LOOP_FDIRS_END
$	tmp = EXHEADER_'D'
$	IF tmp .EQS. "" THEN GOTO LOOP_FDIRS
$	IF D .EQS. ""
$	THEN
$	  COPY 'tmp' WRK_SSLINCLUDE: /LOG
$	ELSE
$	  COPY [.'D']'tmp' WRK_SSLINCLUDE: /LOG
$	ENDIF
$	SET FILE/PROT=WORLD:RE WRK_SSLINCLUDE:'tmp'
$	GOTO LOOP_FDIRS
$ LOOP_FDIRS_END:
$
$	EXIT
