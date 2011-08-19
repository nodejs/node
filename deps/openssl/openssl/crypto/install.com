$! INSTALL.COM -- Installs the files in a given directory tree
$!
$! Author: Richard Levitte <richard@levitte.org>
$! Time of creation: 22-MAY-1998 10:13
$!
$! Changes by Zoltan Arpadffy <zoli@polarhome.com>
$!
$! P1	root of the directory tree
$!
$	IF P1 .EQS. ""
$	THEN
$	    WRITE SYS$OUTPUT "First argument missing."
$	    WRITE SYS$OUTPUT -
		  "It should be the directory where you want things installed."
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
$	DEFINE/NOLOG WRK_SSLLIB WRK_SSLROOT:['ARCH'_LIB]
$	DEFINE/NOLOG WRK_SSLINCLUDE WRK_SSLROOT:[INCLUDE]
$
$	IF F$PARSE("WRK_SSLROOT:[000000]") .EQS. "" THEN -
	   CREATE/DIR/LOG WRK_SSLROOT:[000000]
$	IF F$PARSE("WRK_SSLLIB:") .EQS. "" THEN -
	   CREATE/DIR/LOG WRK_SSLLIB:
$	IF F$PARSE("WRK_SSLINCLUDE:") .EQS. "" THEN -
	   CREATE/DIR/LOG WRK_SSLINCLUDE:
$
$	SDIRS := ,-
		 OBJECTS,-
		 MD2,MD4,MD5,SHA,MDC2,HMAC,RIPEMD,-
		 DES,AES,RC2,RC4,RC5,IDEA,BF,CAST,CAMELLIA,SEED,-
		 BN,EC,RSA,DSA,ECDSA,DH,ECDH,DSO,ENGINE,-
		 BUFFER,BIO,STACK,LHASH,RAND,ERR,-
		 EVP,ASN1,PEM,X509,X509V3,CONF,TXT_DB,PKCS7,PKCS12,COMP,OCSP,-
		 UI,KRB5,-
		 STORE,PQUEUE,JPAKE
$	EXHEADER_ := crypto.h,tmdiff.h,opensslv.h,opensslconf.h,ebcdic.h,-
		symhacks.h,ossl_typ.h
$	EXHEADER_OBJECTS := objects.h,obj_mac.h
$	EXHEADER_MD2 := md2.h
$	EXHEADER_MD4 := md4.h
$	EXHEADER_MD5 := md5.h
$	EXHEADER_SHA := sha.h
$	EXHEADER_MDC2 := mdc2.h
$	EXHEADER_HMAC := hmac.h
$	EXHEADER_RIPEMD := ripemd.h
$	EXHEADER_DES := des.h,des_old.h
$	EXHEADER_AES := aes.h
$	EXHEADER_RC2 := rc2.h
$	EXHEADER_RC4 := rc4.h
$	EXHEADER_RC5 := rc5.h
$	EXHEADER_IDEA := idea.h
$	EXHEADER_BF := blowfish.h
$	EXHEADER_CAST := cast.h
$	EXHEADER_CAMELLIA := camellia.h
$	EXHEADER_SEED := seed.h
$	EXHEADER_BN := bn.h
$	EXHEADER_EC := ec.h
$	EXHEADER_RSA := rsa.h
$	EXHEADER_DSA := dsa.h
$	EXHEADER_ECDSA := ecdsa.h
$	EXHEADER_DH := dh.h
$	EXHEADER_ECDH := ecdh.h
$	EXHEADER_DSO := dso.h
$	EXHEADER_ENGINE := engine.h
$	EXHEADER_BUFFER := buffer.h
$	EXHEADER_BIO := bio.h
$	EXHEADER_STACK := stack.h,safestack.h
$	EXHEADER_LHASH := lhash.h
$	EXHEADER_RAND := rand.h
$	EXHEADER_ERR := err.h
$	EXHEADER_EVP := evp.h
$	EXHEADER_ASN1 := asn1.h,asn1_mac.h,asn1t.h
$	EXHEADER_PEM := pem.h,pem2.h
$	EXHEADER_X509 := x509.h,x509_vfy.h
$	EXHEADER_X509V3 := x509v3.h
$	EXHEADER_CONF := conf.h,conf_api.h
$	EXHEADER_TXT_DB := txt_db.h
$	EXHEADER_PKCS7 := pkcs7.h
$	EXHEADER_PKCS12 := pkcs12.h
$	EXHEADER_COMP := comp.h
$	EXHEADER_OCSP := ocsp.h
$	EXHEADER_UI := ui.h,ui_compat.h
$	EXHEADER_KRB5 := krb5_asn.h
$!	EXHEADER_STORE := store.h,str_compat.h
$	EXHEADER_STORE := store.h
$	EXHEADER_PQUEUE := pqueue.h,pq_compat.h
$	EXHEADER_JPAKE := jpake.h
$	LIBS := LIBCRYPTO
$
$	EXE_DIR := [-.'ARCH'.EXE.CRYPTO]
$
$	I = 0
$ LOOP_SDIRS: 
$	D = F$EDIT(F$ELEMENT(I, ",", SDIRS),"TRIM")
$	I = I + 1
$	IF D .EQS. "," THEN GOTO LOOP_SDIRS_END
$	tmp = EXHEADER_'D'
$	IF D .EQS. ""
$	THEN
$	  COPY 'tmp' WRK_SSLINCLUDE: /LOG
$	ELSE
$	  COPY [.'D']'tmp' WRK_SSLINCLUDE: /LOG
$	ENDIF
$	SET FILE/PROT=WORLD:RE WRK_SSLINCLUDE:'tmp'
$	GOTO LOOP_SDIRS
$ LOOP_SDIRS_END:
$
$	I = 0
$ LOOP_LIB: 
$	E = F$EDIT(F$ELEMENT(I, ",", LIBS),"TRIM")
$	I = I + 1
$	IF E .EQS. "," THEN GOTO LOOP_LIB_END
$	SET NOON
$	IF F$SEARCH(EXE_DIR+E+".OLB") .NES. ""
$	THEN
$	  COPY 'EXE_DIR''E'.OLB WRK_SSLLIB:'E'.OLB/log
$	  SET FILE/PROT=W:RE WRK_SSLLIB:'E'.OLB
$	ENDIF
$	! Preparing for the time when we have shareable images
$	IF F$SEARCH(EXE_DIR+E+".EXE") .NES. ""
$	THEN
$	  COPY 'EXE_DIR''E'.EXE WRK_SSLLIB:'E'.EXE/log
$	  SET FILE/PROT=W:RE WRK_SSLLIB:'E'.EXE
$	ENDIF
$	SET ON
$	GOTO LOOP_LIB
$ LOOP_LIB_END:
$
$	EXIT
