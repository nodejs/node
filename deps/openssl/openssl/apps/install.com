$! INSTALL.COM -- Installs the files in a given directory tree
$!
$! Author: Richard Levitte <richard@levitte.org>
$! Time of creation: 22-MAY-1998 10:13
$!
$! P1	root of the directory tree
$!
$
$	IF P1 .EQS. ""
$	THEN
$	    WRITE SYS$OUTPUT "First argument missing."
$	    WRITE SYS$OUTPUT -
		  "Should be the directory where you want things installed."
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
$	DEFINE/NOLOG WRK_SSLEXE WRK_SSLROOT:['ARCH'_EXE]
$
$	IF F$PARSE("WRK_SSLROOT:[000000]") .EQS. "" THEN -
	   CREATE/DIR/LOG WRK_SSLROOT:[000000]
$	IF F$PARSE("WRK_SSLEXE:") .EQS. "" THEN -
	   CREATE/DIR/LOG WRK_SSLEXE:
$
$	EXE := openssl
$
$	EXE_DIR := [-.'ARCH'.EXE.APPS]
$
$	I = 0
$ LOOP_EXE: 
$	E = F$EDIT(F$ELEMENT(I, ",", EXE),"TRIM")
$	I = I + 1
$	IF E .EQS. "," THEN GOTO LOOP_EXE_END
$	SET NOON
$	IF F$SEARCH(EXE_DIR+E+".EXE") .NES. ""
$	THEN
$	  COPY 'EXE_DIR''E'.EXE WRK_SSLEXE:'E'.EXE/log
$	  SET FILE/PROT=W:RE WRK_SSLEXE:'E'.EXE
$	ENDIF
$	SET ON
$	GOTO LOOP_EXE
$ LOOP_EXE_END:
$
$	SET NOON
$	COPY CA.COM WRK_SSLEXE:CA.COM/LOG
$	SET FILE/PROT=W:RE WRK_SSLEXE:CA.COM
$	COPY OPENSSL-VMS.CNF WRK_SSLROOT:[000000]OPENSSL.CNF/LOG
$	SET FILE/PROT=W:R WRK_SSLROOT:[000000]OPENSSL.CNF
$	SET ON
$
$	EXIT
