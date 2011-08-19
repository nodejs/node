$!
$! MAKEVMS.COM
$! Original Author:  UNKNOWN
$! Rewritten By:  Robert Byer
$!                Vice-President
$!                A-Com Computing, Inc.
$!                byer@mail.all-net.net
$!
$! Changes by Richard Levitte <richard@levitte.org>
$!	      Zoltan Arpadffy <zoli@polarhome.com>
$!
$! This procedure creates the SSL libraries of "[.xxx.EXE.CRYPTO]LIBCRYPTO.OLB"
$! "[.xxx.EXE.SSL]LIBSSL.OLB"
$! The "xxx" denotes the machine architecture of ALPHA, IA64 or VAX.
$!
$! This procedures accepts two command line options listed below.
$!
$! Specify one of the following build options for P1.
$!
$!      ALL       Just build "everything".
$!      CONFIG    Just build the "[.CRYPTO._<ARCH>]OPENSSLCONF.H" file.
$!      BUILDINF  Just build the "[.CRYPTO._<ARCH>]BUILDINF.H" file.
$!      SOFTLINKS Just fix the Unix soft links.
$!      BUILDALL  Same as ALL, except CONFIG, BUILDINF and SOFTILNKS aren't done.
$!      CRYPTO    Just build the "[.xxx.EXE.CRYPTO]LIBCRYPTO.OLB" library.
$!      CRYPTO/x  Just build the x part of the
$!                "[.xxx.EXE.CRYPTO]LIBCRYPTO.OLB" library.
$!      SSL       Just build the "[.xxx.EXE.SSL]LIBSSL.OLB" library.
$!      SSL_TASK  Just build the "[.xxx.EXE.SSL]SSL_TASK.EXE" program.
$!      TEST      Just build the "[.xxx.EXE.TEST]" test programs for OpenSSL.
$!      APPS      Just build the "[.xxx.EXE.APPS]" application programs for OpenSSL.
$!      ENGINES   Just build the "[.xxx.EXE.ENGINES]" application programs for OpenSSL.
$!
$!
$! P2 is ignored (it was used to denote if RSAref should be used or not,
$! and is simply kept so surrounding scripts don't get confused)
$!
$! Speficy DEBUG or NODEBUG as P3 to compile with or without debugging
$! information.
$!
$! Specify which compiler at P4 to try to compile under.
$!
$!	  VAXC	 For VAX C.
$!	  DECC	 For DEC C.
$!	  GNUC	 For GNU C.
$!	  LINK   To only link the programs from existing object files.
$!               (not yet implemented)
$!
$! If you don't speficy a compiler, it will try to determine which
$! "C" compiler to use.
$!
$! P5, if defined, sets a TCP/IP library to use, through one of the following
$! keywords:
$!
$!	UCX		for UCX or UCX emulation
$!	TCPIP		for TCP/IP Services or TCP/IP Services emulation
$!			(this is prefered over UCX)
$!	SOCKETSHR	for SOCKETSHR+NETLIB
$!	NONE		to avoid specifying which TCP/IP implementation to
$!			use at build time (this works with DEC C).  This is
$!			the default.
$!
$! P6, if defined, sets a compiler thread NOT needed on OpenVMS 7.1 (and up)
$!
$!
$ DEF_ORIG = F$ENVIRONMENT( "DEFAULT")
$ ON ERROR THEN GOTO TIDY
$ ON CONTROL_C THEN GOTO TIDY
$!
$! Check if we're in a batch job, and make sure we get to 
$! the directory this script is in
$!
$ IF F$MODE() .EQS. "BATCH"
$ THEN
$   COMNAME=F$ENVIRONMENT("PROCEDURE")
$   COMPATH=F$PARSE("A.;",COMNAME) - "A.;"
$   SET DEF 'COMPATH'
$ ENDIF
$!
$! Check What Architecture We Are Using.
$!
$ IF (F$GETSYI("CPU").LT.128)
$ THEN
$!
$!  The Architecture Is VAX.
$!
$   ARCH := VAX
$!
$! Else...
$!
$ ELSE
$!
$!  The Architecture Is Alpha, IA64 or whatever comes in the future.
$!
$   ARCH = F$EDIT( F$GETSYI( "ARCH_NAME"), "UPCASE")
$   IF (ARCH .EQS. "") THEN ARCH = "UNK"
$!
$! End The Architecture Check.
$!
$ ENDIF
$!
$! Check To Make Sure We Have Valid Command Line Parameters.
$!
$ GOSUB CHECK_OPTIONS
$!
$! Check To See What We Are To Do.
$!
$ IF (BUILDCOMMAND.EQS."ALL")
$ THEN
$!
$!  Start with building the OpenSSL configuration file.
$!
$   GOSUB CONFIG
$!
$!  Create The "BUILDINF.H" Include File.
$!
$   GOSUB BUILDINF
$!
$!  Fix The Unix Softlinks.
$!
$   GOSUB SOFTLINKS
$!
$ ENDIF
$!
$ IF (BUILDCOMMAND.EQS."ALL".OR.BUILDCOMMAND.EQS."BUILDALL")
$ THEN
$!
$!  Build The [.xxx.EXE.CRYPTO]LIBCRYPTO.OLB Library.
$!
$   GOSUB CRYPTO
$!
$!  Build The [.xxx.EXE.SSL]LIBSSL.OLB Library.
$!
$   GOSUB SSL
$!
$!  Build The [.xxx.EXE.SSL]SSL_TASK.EXE DECNet SSL Engine.
$!
$   GOSUB SSL_TASK
$!
$!  Build The [.xxx.EXE.TEST] OpenSSL Test Utilities.
$!
$   GOSUB TEST
$!
$!  Build The [.xxx.EXE.APPS] OpenSSL Application Utilities.
$!
$   GOSUB APPS
$!
$!  Build The [.xxx.EXE.ENGINES] OpenSSL Shareable Engines.
$!
$   GOSUB ENGINES
$!
$! Else...
$!
$ ELSE
$!
$!    Build Just What The User Wants Us To Build.
$!
$     GOSUB 'BUILDCOMMAND'
$!
$ ENDIF
$!
$! Time To EXIT.
$!
$ GOTO TIDY
$!
$! Rebuild The "[.CRYPTO._''ARCH']OPENSSLCONF.H" file.
$!
$ CONFIG:
$!
$! Tell The User We Are Creating The [.CRYPTO._<ARCH>]OPENSSLCONF.H File.
$!
$ WRITE SYS$OUTPUT "Creating [.CRYPTO._''ARCH']OPENSSLCONF.H Include File."
$!
$! First, make sure the directory exists.
$!
$ IF F$PARSE("SYS$DISK:[.CRYPTO._''ARCH']") .EQS. "" THEN -
     CREATE/DIRECTORY SYS$DISK:[.CRYPTO._'ARCH']
$!
$! Create The [.CRYPTO._<ARCH>]OPENSSLCONF.H File.
$! Make sure it has the right format.
$!
$ OSCH_NAME = "SYS$DISK:[.CRYPTO._''ARCH']OPENSSLCONF.H"
$ CREATE /FDL=SYS$INPUT: 'OSCH_NAME'
RECORD
        FORMAT stream_lf
$ OPEN /APPEND H_FILE 'OSCH_NAME'
$!
$! Write The [.CRYPTO._<ARCH>]OPENSSLCONF.H File.
$!
$ WRITE H_FILE "/* This file was automatically built using makevms.com */"
$ WRITE H_FILE "/* and [.CRYPTO._''ARCH']OPENSSLCONF.H_IN */"
$!
$! Write a few macros that indicate how this system was built.
$!
$ WRITE H_FILE ""
$ WRITE H_FILE "#ifndef OPENSSL_SYS_VMS"
$ WRITE H_FILE "# define OPENSSL_SYS_VMS"
$ WRITE H_FILE "#endif"
$ CONFIG_LOGICALS := NO_ASM,NO_RSA,NO_DSA,NO_DH,NO_MD2,NO_MD5,NO_RIPEMD,-
	NO_SHA,NO_SHA0,NO_SHA1,NO_DES/NO_MDC2;NO_MDC2,NO_RC2,NO_RC4,NO_RC5,-
	NO_IDEA,NO_BF,NO_CAST,NO_CAMELLIA,NO_SEED,NO_HMAC,NO_SSL2
$ CONFIG_LOG_I = 0
$ CONFIG_LOG_LOOP:
$   CONFIG_LOG_E1 = F$ELEMENT(CONFIG_LOG_I,",",CONFIG_LOGICALS)
$   CONFIG_LOG_I = CONFIG_LOG_I + 1
$   IF CONFIG_LOG_E1 .EQS. "" THEN GOTO CONFIG_LOG_LOOP
$   IF CONFIG_LOG_E1 .EQS. "," THEN GOTO CONFIG_LOG_LOOP_END
$   CONFIG_LOG_E2 = F$EDIT(CONFIG_LOG_E1,"TRIM")
$   CONFIG_LOG_E1 = F$ELEMENT(0,";",CONFIG_LOG_E2)
$   CONFIG_LOG_E2 = F$ELEMENT(1,";",CONFIG_LOG_E2)
$   CONFIG_LOG_E0 = F$ELEMENT(0,"/",CONFIG_LOG_E1)
$   CONFIG_LOG_E1 = F$ELEMENT(1,"/",CONFIG_LOG_E1)
$   IF F$TRNLNM("OPENSSL_"+CONFIG_LOG_E0)
$   THEN
$     WRITE H_FILE "#ifndef OPENSSL_",CONFIG_LOG_E0
$     WRITE H_FILE "# define OPENSSL_",CONFIG_LOG_E0
$     WRITE H_FILE "#endif"
$     IF CONFIG_LOG_E1 .NES. "/"
$     THEN
$       WRITE H_FILE "#ifndef OPENSSL_",CONFIG_LOG_E1
$       WRITE H_FILE "# define OPENSSL_",CONFIG_LOG_E1
$       WRITE H_FILE "#endif"
$     ENDIF
$   ELSE
$     IF CONFIG_LOG_E2 .NES. ";"
$     THEN
$       IF F$TRNLNM("OPENSSL_"+CONFIG_LOG_E2)
$       THEN
$         WRITE H_FILE "#ifndef OPENSSL_",CONFIG_LOG_E2
$         WRITE H_FILE "# define OPENSSL_",CONFIG_LOG_E2
$         WRITE H_FILE "#endif"
$       ENDIF
$     ENDIF
$   ENDIF
$   GOTO CONFIG_LOG_LOOP
$ CONFIG_LOG_LOOP_END:
$ WRITE H_FILE "#ifndef OPENSSL_NO_STATIC_ENGINE"
$ WRITE H_FILE "# define OPENSSL_NO_STATIC_ENGINE"
$ WRITE H_FILE "#endif"
$ WRITE H_FILE "#ifndef OPENSSL_THREADS"
$ WRITE H_FILE "# define OPENSSL_THREADS"
$ WRITE H_FILE "#endif"
$ WRITE H_FILE "#ifndef OPENSSL_NO_KRB5"
$ WRITE H_FILE "# define OPENSSL_NO_KRB5"
$ WRITE H_FILE "#endif"
$ WRITE H_FILE ""
$!
$! Different tar version may have named the file differently
$ IF F$SEARCH("[.CRYPTO]OPENSSLCONF.H_IN") .NES. ""
$ THEN
$   TYPE [.CRYPTO]OPENSSLCONF.H_IN /OUTPUT=H_FILE:
$ ELSE
$   IF F$SEARCH("[.CRYPTO]OPENSSLCONF_H.IN") .NES. ""
$   THEN
$     TYPE [.CRYPTO]OPENSSLCONF_H.IN /OUTPUT=H_FILE:
$   ELSE
$     ! For ODS-5
$     IF F$SEARCH("[.CRYPTO]OPENSSLCONF.H.IN") .NES. ""
$     THEN
$       TYPE [.CRYPTO]OPENSSLCONF.H.IN /OUTPUT=H_FILE:
$     ELSE
$       WRITE SYS$ERROR "Couldn't find a [.CRYPTO]OPENSSLCONF.H_IN.  Exiting!"
$       $STATUS = %X00018294 ! "%RMS-F-FNF, file not found".
$       GOTO TIDY
$     ENDIF
$   ENDIF
$ ENDIF
$ IF ARCH .NES. "VAX"
$ THEN
$!
$!  Write the Alpha specific data
$!
$   WRITE H_FILE "#if defined(HEADER_RC4_H)"
$   WRITE H_FILE "#undef RC4_INT"
$   WRITE H_FILE "#define RC4_INT unsigned int"
$   WRITE H_FILE "#undef RC4_CHUNK"
$   WRITE H_FILE "#define RC4_CHUNK unsigned long long"
$   WRITE H_FILE "#endif"
$!
$   WRITE H_FILE "#if defined(HEADER_DES_LOCL_H)"
$   WRITE H_FILE "#undef DES_LONG"
$   WRITE H_FILE "#define DES_LONG unsigned int"
$   WRITE H_FILE "#undef DES_PTR"
$   WRITE H_FILE "#define DES_PTR"
$   WRITE H_FILE "#undef DES_RISC1"
$   WRITE H_FILE "#undef DES_RISC2"
$   WRITE H_FILE "#define DES_RISC1"
$   WRITE H_FILE "#undef DES_UNROLL"
$   WRITE H_FILE "#define DES_UNROLL"
$   WRITE H_FILE "#endif"
$!
$   WRITE H_FILE "#if defined(HEADER_BN_H)"
$   WRITE H_FILE "#undef BN_LLONG"	! Never define with SIXTY_FOUR_BIT
$   WRITE H_FILE "#undef SIXTY_FOUR_BIT_LONG"
$   WRITE H_FILE "#undef SIXTY_FOUR_BIT"
$   WRITE H_FILE "#define SIXTY_FOUR_BIT"
$   WRITE H_FILE "#undef THIRTY_TWO_BIT"
$   WRITE H_FILE "#undef SIXTEEN_BIT"
$   WRITE H_FILE "#undef EIGHT_BIT"
$   WRITE H_FILE "#endif"
$
$   WRITE H_FILE "#undef OPENSSL_EXPORT_VAR_AS_FUNCTION"
$!
$!  Else...
$!
$ ELSE
$!
$!  Write the VAX specific data
$!
$   WRITE H_FILE "#if defined(HEADER_RC4_H)"
$   WRITE H_FILE "#undef RC4_INT"
$   WRITE H_FILE "#define RC4_INT unsigned char"
$   WRITE H_FILE "#undef RC4_CHUNK"
$   WRITE H_FILE "#define RC4_CHUNK unsigned long"
$   WRITE H_FILE "#endif"
$!
$   WRITE H_FILE "#if defined(HEADER_DES_LOCL_H)"
$   WRITE H_FILE "#undef DES_LONG"
$   WRITE H_FILE "#define DES_LONG unsigned long"
$   WRITE H_FILE "#undef DES_PTR"
$   WRITE H_FILE "#define DES_PTR"
$   WRITE H_FILE "#undef DES_RISC1"
$   WRITE H_FILE "#undef DES_RISC2"
$   WRITE H_FILE "#undef DES_UNROLL"
$   WRITE H_FILE "#endif"
$!
$   WRITE H_FILE "#if defined(HEADER_BN_H)"
$   WRITE H_FILE "#undef BN_LLONG"	! VAX C/DEC C doesn't have long long
$   WRITE H_FILE "#undef SIXTY_FOUR_BIT_LONG"
$   WRITE H_FILE "#undef SIXTY_FOUR_BIT"
$   WRITE H_FILE "#undef THIRTY_TWO_BIT"
$   WRITE H_FILE "#define THIRTY_TWO_BIT"
$   WRITE H_FILE "#undef SIXTEEN_BIT"
$   WRITE H_FILE "#undef EIGHT_BIT"
$   WRITE H_FILE "#endif"
$!
$! Oddly enough, the following symbol is tested in crypto/sha/sha512.c
$! before sha.h gets included (and HEADER_SHA_H defined), so we will not
$! protect this one...
$   WRITE H_FILE "#undef OPENSSL_NO_SHA512"
$   WRITE H_FILE "#define OPENSSL_NO_SHA512"
$!
$   WRITE H_FILE "#undef OPENSSL_EXPORT_VAR_AS_FUNCTION"
$   WRITE H_FILE "#define OPENSSL_EXPORT_VAR_AS_FUNCTION"
$!
$!  End
$!
$ ENDIF
$!
$! There are many places where this is needed.
$!
$!!! WRITE H_FILE "#define _XOPEN_SOURCE_EXTENDED"
$!
$! Close the [.CRYPTO._<ARCH>]OPENSSLCONF.H file
$!
$ CLOSE H_FILE
$!
$! Purge The [.CRYPTO._<ARCH>]OPENSSLCONF.H file
$!
$ PURGE SYS$DISK:[.CRYPTO._'ARCH']OPENSSLCONF.H
$!
$! That's All, Time To RETURN.
$!
$ RETURN
$!
$! Rebuild The "[.CRYPTO._<ARCH>]BUILDINF.H" file.
$!
$ BUILDINF:
$!
$! Tell The User We Are Creating The [.CRYPTO._<ARCH>]BUILDINF.H File.
$!
$ WRITE SYS$OUTPUT "Creating [.CRYPTO._''ARCH']BUILDINF.H Include File."
$!
$! Create The [.CRYPTO._<ARCH>]BUILDINF.H File.
$!
$ BIH_NAME = "SYS$DISK:[.CRYPTO._''ARCH']BUILDINF.H"
$ CREATE /FDL=SYS$INPUT: 'BIH_NAME'
RECORD
        FORMAT stream_lf
$!
$ OPEN /APPEND H_FILE 'bih_name'
$!
$! Get The Current Date & Time.
$!
$ TIME = F$TIME()
$!
$! Write The [.CRYPTO._<ARCH>]BUILDINF.H File.
$!
$ WRITE H_FILE "#define CFLAGS """" /* Not filled in for now */"
$ WRITE H_FILE "#define PLATFORM ""VMS ''ARCH' ''VMS_VER'"""
$ WRITE H_FILE "#define DATE ""''TIME'"" "
$!
$! Close The [.CRYPTO._<ARCH>]BUILDINF.H File.
$!
$ CLOSE H_FILE
$!
$! Purge The [.CRYPTO._<ARCH>]BUILDINF.H File.
$!
$ PURGE SYS$DISK:[.CRYPTO._'ARCH']BUILDINF.H
$!
$! That's All, Time To RETURN.
$!
$ RETURN
$!
$! Copy a lot of files around.
$!
$ SOFTLINKS: 
$!
$! Tell The User We Are Partly Rebuilding The [.APPS] Directory.
$!
$ WRITE SYS$OUTPUT "Rebuilding The '[.APPS]MD4.C' File."
$!
$ DELETE SYS$DISK:[.APPS]MD4.C;*
$!
$! Copy MD4.C from [.CRYPTO.MD4] into [.APPS]
$!
$ COPY SYS$DISK:[.CRYPTO.MD4]MD4.C SYS$DISK:[.APPS]
$!
$! Tell The User We Are Rebuilding The [.INCLUDE.OPENSSL] Directory.
$!
$ WRITE SYS$OUTPUT "Rebuilding The '[.INCLUDE.OPENSSL]' Directory."
$!
$! First, make sure the directory exists
$!
$ IF F$PARSE("SYS$DISK:[.INCLUDE.OPENSSL]") .EQS. "" THEN -
     CREATE/DIRECTORY SYS$DISK:[.INCLUDE.OPENSSL]
$!
$! Copy All The ".H" Files From The Main Directory.
$!
$ EXHEADER := e_os2.h
$ COPY 'EXHEADER' SYS$DISK:[.INCLUDE.OPENSSL]
$!
$! Copy All The ".H" Files From The [.CRYPTO] Directory Tree.
$!
$ SDIRS := ,-
   _'ARCH',-
   OBJECTS,-
   MD2,MD4,MD5,SHA,MDC2,HMAC,RIPEMD,-
   DES,AES,RC2,RC4,RC5,IDEA,BF,CAST,CAMELLIA,SEED,-
   BN,EC,RSA,DSA,ECDSA,DH,ECDH,DSO,ENGINE,-
   BUFFER,BIO,STACK,LHASH,RAND,ERR,-
   EVP,ASN1,PEM,X509,X509V3,CONF,TXT_DB,PKCS7,PKCS12,COMP,OCSP,UI,KRB5,-
   STORE,CMS,PQUEUE,JPAKE
$ EXHEADER_ := crypto.h,tmdiff.h,opensslv.h,ebcdic.h,symhacks.h,-
		ossl_typ.h
$ EXHEADER__'ARCH' := opensslconf.h
$ EXHEADER_OBJECTS := objects.h,obj_mac.h
$ EXHEADER_MD2 := md2.h
$ EXHEADER_MD4 := md4.h
$ EXHEADER_MD5 := md5.h
$ EXHEADER_SHA := sha.h
$ EXHEADER_MDC2 := mdc2.h
$ EXHEADER_HMAC := hmac.h
$ EXHEADER_RIPEMD := ripemd.h
$ EXHEADER_DES := des.h,des_old.h
$ EXHEADER_AES := aes.h
$ EXHEADER_RC2 := rc2.h
$ EXHEADER_RC4 := rc4.h
$ EXHEADER_RC5 := rc5.h
$ EXHEADER_IDEA := idea.h
$ EXHEADER_BF := blowfish.h
$ EXHEADER_CAST := cast.h
$ EXHEADER_CAMELLIA := camellia.h
$ EXHEADER_SEED := seed.h
$ EXHEADER_BN := bn.h
$ EXHEADER_EC := ec.h
$ EXHEADER_RSA := rsa.h
$ EXHEADER_DSA := dsa.h
$ EXHEADER_ECDSA := ecdsa.h
$ EXHEADER_DH := dh.h
$ EXHEADER_ECDH := ecdh.h
$ EXHEADER_DSO := dso.h
$ EXHEADER_ENGINE := engine.h
$ EXHEADER_BUFFER := buffer.h
$ EXHEADER_BIO := bio.h
$ EXHEADER_STACK := stack.h,safestack.h
$ EXHEADER_LHASH := lhash.h
$ EXHEADER_RAND := rand.h
$ EXHEADER_ERR := err.h
$ EXHEADER_EVP := evp.h
$ EXHEADER_ASN1 := asn1.h,asn1_mac.h,asn1t.h
$ EXHEADER_PEM := pem.h,pem2.h
$ EXHEADER_X509 := x509.h,x509_vfy.h
$ EXHEADER_X509V3 := x509v3.h
$ EXHEADER_CONF := conf.h,conf_api.h
$ EXHEADER_TXT_DB := txt_db.h
$ EXHEADER_PKCS7 := pkcs7.h
$ EXHEADER_PKCS12 := pkcs12.h
$ EXHEADER_COMP := comp.h
$ EXHEADER_OCSP := ocsp.h
$ EXHEADER_UI := ui.h,ui_compat.h
$ EXHEADER_KRB5 := krb5_asn.h
$!EXHEADER_STORE := store.h,str_compat.h
$ EXHEADER_STORE := store.h
$ EXHEADER_CMS := cms.h
$ EXHEADER_PQUEUE := pqueue.h,pq_compat.h
$ EXHEADER_JPAKE := jpake.h
$
$ I = 0
$ LOOP_SDIRS: 
$ D = F$EDIT(F$ELEMENT(I, ",", SDIRS),"TRIM")
$ I = I + 1
$ IF D .EQS. "," THEN GOTO LOOP_SDIRS_END
$ tmp = EXHEADER_'D'
$ IF D .EQS. ""
$ THEN
$   COPY [.CRYPTO]'tmp' SYS$DISK:[.INCLUDE.OPENSSL] !/LOG
$ ELSE
$   COPY [.CRYPTO.'D']'tmp' SYS$DISK:[.INCLUDE.OPENSSL] !/LOG
$ ENDIF
$ GOTO LOOP_SDIRS
$ LOOP_SDIRS_END:
$!
$! Copy All The ".H" Files From The [.SSL] Directory.
$!
$ EXHEADER := ssl.h,ssl2.h,ssl3.h,ssl23.h,tls1.h,dtls1.h,kssl.h
$ COPY SYS$DISK:[.SSL]'EXHEADER' SYS$DISK:[.INCLUDE.OPENSSL]
$!
$! Purge all doubles
$!
$ PURGE SYS$DISK:[.INCLUDE.OPENSSL]*.H
$!
$! That's All, Time To RETURN.
$!
$ RETURN
$!
$! Build The "[.xxx.EXE.CRYPTO]LIBCRYPTO.OLB" Library.
$!
$ CRYPTO:
$!
$! Tell The User What We Are Doing.
$!
$ WRITE SYS$OUTPUT ""
$ WRITE SYS$OUTPUT "Building The [.",ARCH,".EXE.CRYPTO]LIBCRYPTO.OLB Library."
$!
$! Go To The [.CRYPTO] Directory.
$!
$ SET DEFAULT SYS$DISK:[.CRYPTO]
$!
$! Build The [.xxx.EXE.CRYPTO]LIBCRYPTO.OLB Library.
$!  
$ @CRYPTO-LIB LIBRARY 'DEBUGGER' "''COMPILER'" "''TCPIP_TYPE'" "''ISSEVEN'" "''BUILDPART'"
$!
$! Build The [.xxx.EXE.CRYPTO]*.EXE Test Applications.
$!  
$ @CRYPTO-LIB APPS 'DEBUGGER' "''COMPILER'" "''TCPIP_TYPE'" 'ISSEVEN' "''BUILDPART'"
$!
$! Go Back To The Main Directory.
$!
$ SET DEFAULT [-]
$!
$! Time To RETURN.
$!
$ RETURN
$!
$! Build The "[.xxx.EXE.SSL]LIBSSL.OLB" Library.
$!
$ SSL:
$!
$! Tell The User What We Are Doing.
$!
$ WRITE SYS$OUTPUT ""
$ WRITE SYS$OUTPUT "Building The [.",ARCH,".EXE.SSL]LIBSSL.OLB Library."
$!
$! Go To The [.SSL] Directory.
$!
$ SET DEFAULT SYS$DISK:[.SSL]
$!
$! Build The [.xxx.EXE.SSL]LIBSSL.OLB Library.
$!
$ @SSL-LIB LIBRARY 'DEBUGGER' "''COMPILER'" "''TCPIP_TYPE'" 'ISSEVEN'
$!
$! Go Back To The Main Directory.
$!
$ SET DEFAULT [-]
$!
$! Time To Return.
$!
$ RETURN
$!
$! Build The "[.xxx.EXE.SSL]SSL_TASK.EXE" Program.
$!
$ SSL_TASK:
$!
$! Tell The User What We Are Doing.
$!
$ WRITE SYS$OUTPUT ""
$ WRITE SYS$OUTPUT "Building DECNet Based SSL Engine, [.",ARCH,".EXE.SSL]SSL_TASK.EXE"
$!
$! Go To The [.SSL] Directory.
$!
$ SET DEFAULT SYS$DISK:[.SSL]
$!
$! Build The [.xxx.EXE.SSL]SSL_TASK.EXE
$!
$ @SSL-LIB SSL_TASK 'DEBUGGER' "''COMPILER'" "''TCPIP_TYPE'" 'ISSEVEN'
$!
$! Go Back To The Main Directory.
$!
$ SET DEFAULT [-]
$!
$! That's All, Time To RETURN.
$!
$ RETURN
$!
$! Build The OpenSSL Test Programs.
$!
$ TEST:
$!
$! Tell The User What We Are Doing.
$!
$ WRITE SYS$OUTPUT ""
$ WRITE SYS$OUTPUT "Building The OpenSSL [.",ARCH,".EXE.TEST] Test Utilities."
$!
$! Go To The [.TEST] Directory.
$!
$ SET DEFAULT SYS$DISK:[.TEST]
$!
$! Build The Test Programs.
$!
$ @MAKETESTS 'DEBUGGER' "''COMPILER'" "''TCPIP_TYPE'" 'ISSEVEN'
$!
$! Go Back To The Main Directory.
$!
$ SET DEFAULT [-]
$!
$! That's All, Time To RETURN.
$!
$ RETURN
$!
$! Build The OpenSSL Application Programs.
$!
$ APPS:
$!
$! Tell The User What We Are Doing.
$!
$ WRITE SYS$OUTPUT ""
$ WRITE SYS$OUTPUT "Building OpenSSL [.",ARCH,".EXE.APPS] Applications."
$!
$! Go To The [.APPS] Directory.
$!
$ SET DEFAULT SYS$DISK:[.APPS]
$!
$! Build The Application Programs.
$!
$ @MAKEAPPS 'DEBUGGER' "''COMPILER'" "''TCPIP_TYPE'" 'ISSEVEN'
$!
$! Go Back To The Main Directory.
$!
$ SET DEFAULT [-]
$!
$! That's All, Time To RETURN.
$!
$ RETURN
$!
$! Build The OpenSSL Application Programs.
$!
$ ENGINES:
$!
$! Tell The User What We Are Doing.
$!
$ WRITE SYS$OUTPUT ""
$ WRITE SYS$OUTPUT "Building OpenSSL [.",ARCH,".EXE.ENGINES] Engines."
$!
$! Go To The [.ENGINES] Directory.
$!
$ SET DEFAULT SYS$DISK:[.ENGINES]
$!
$! Build The Application Programs.
$!
$ @MAKEENGINES ENGINES 'DEBUGGER' "''COMPILER'" "''TCPIP_TYPE'" 'ISSEVEN' "''BUILDPART'"
$!
$! Go Back To The Main Directory.
$!
$ SET DEFAULT [-]
$!
$! That's All, Time To RETURN.
$!
$ RETURN
$!
$! Check The User's Options.
$!
$ CHECK_OPTIONS:
$!
$! Check if there's a "part", and separate it out
$!
$ BUILDPART = F$ELEMENT(1,"/",P1)
$ IF BUILDPART .EQS. "/"
$ THEN
$   BUILDPART = ""
$ ELSE
$   P1 = F$EXTRACT(0,F$LENGTH(P1) - F$LENGTH(BUILDPART) - 1, P1)
$ ENDIF
$!
$! Check To See If P1 Is Blank.
$!
$ IF (P1.EQS."ALL")
$ THEN
$!
$!   P1 Is ALL, So Build Everything.
$!
$    BUILDCOMMAND = "ALL"
$!
$! Else...
$!
$ ELSE
$!
$!  Else, Check To See If P1 Has A Valid Arguement.
$!
$   IF (P1.EQS."CONFIG").OR.(P1.EQS."BUILDINF").OR.(P1.EQS."SOFTLINKS") -
       .OR.(P1.EQS."BUILDALL") -
       .OR.(P1.EQS."CRYPTO").OR.(P1.EQS."SSL") -
       .OR.(P1.EQS."SSL_TASK").OR.(P1.EQS."TEST").OR.(P1.EQS."APPS") -
       .OR.(P1.EQS."ENGINES")
$   THEN
$!
$!    A Valid Arguement.
$!
$     BUILDCOMMAND = P1
$!
$!  Else...
$!
$   ELSE
$!
$!    Tell The User We Don't Know What They Want.
$!
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "USAGE:   @MAKEVMS.COM [Target] [not-used option] [Debug option] <Compiler>"
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "Example: @MAKEVMS.COM ALL NORSAREF NODEBUG "
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "The Target ",P1," Is Invalid.  The Valid Target Options Are:"
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "    ALL      :  Just Build Everything."
$     WRITE SYS$OUTPUT "    CONFIG   :  Just build the [.CRYPTO._''ARCH']OPENSSLCONF.H file."
$     WRITE SYS$OUTPUT "    BUILDINF :  Just build the [.CRYPTO._''ARCH']BUILDINF.H file."
$     WRITE SYS$OUTPUT "    SOFTLINKS:  Just Fix The Unix soft links."
$     WRITE SYS$OUTPUT "    BUILDALL :  Same as ALL, except CONFIG, BUILDINF and SOFTILNKS aren't done."
$     WRITE SYS$OUTPUT "    CRYPTO   :  To Build Just The [.xxx.EXE.CRYPTO]LIBCRYPTO.OLB Library."
$     WRITE SYS$OUTPUT "    CRYPTO/x :  To Build Just The x Part Of The"
$     WRITE SYS$OUTPUT "                [.xxx.EXE.CRYPTO]LIBCRYPTO.OLB Library."
$     WRITE SYS$OUTPUT "    SSL      :  To Build Just The [.xxx.EXE.SSL]LIBSSL.OLB Library."
$     WRITE SYS$OUTPUT "    SSL_TASK :  To Build Just The [.xxx.EXE.SSL]SSL_TASK.EXE Program."
$     WRITE SYS$OUTPUT "    TEST     :  To Build Just The OpenSSL Test Programs."
$     WRITE SYS$OUTPUT "    APPS     :  To Build Just The OpenSSL Application Programs."
$     WRITE SYS$OUTPUT "    ENGINES  :  To Build Just The ENGINES"
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT " Where 'xxx' Stands For:"
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "    ALPHA    :  Alpha Architecture."
$     WRITE SYS$OUTPUT "    IA64     :  IA64 Architecture."
$     WRITE SYS$OUTPUT "    VAX      :  VAX Architecture."
$     WRITE SYS$OUTPUT ""
$!
$!    Time To EXIT.
$!
$     GOTO TIDY
$!
$!  End The Valid Argument Check.
$!
$   ENDIF
$!
$! End The P1 Check.
$!
$ ENDIF
$!
$! Check To See If P3 Is Blank.
$!
$ IF (P3.EQS."NODEBUG")
$ THEN
$!
$!   P3 Is NODEBUG, So Compile Without Debugger Information.
$!
$    DEBUGGER = "NODEBUG"
$!
$! Else...
$!
$ ELSE
$!
$!  Check To See If We Are To Compile With Debugger Information.
$!
$   IF (P3.EQS."DEBUG")
$   THEN
$!
$!    Compile With Debugger Information.
$!
$     DEBUGGER = "DEBUG"
$!
$!  Else...
$!
$   ELSE
$!
$!    Tell The User Entered An Invalid Option..
$!
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "The Option ",P3," Is Invalid.  The Valid Options Are:"
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "    DEBUG    :  Compile With The Debugger Information."
$     WRITE SYS$OUTPUT "    NODEBUG  :  Compile Without The Debugger Information."
$     WRITE SYS$OUTPUT ""
$!
$!    Time To EXIT.
$!
$     GOTO TIDY
$!
$!  End The Valid Arguement Check.
$!
$   ENDIF
$!
$! End The P3 Check.
$!
$ ENDIF
$!
$! Check To See If P4 Is Blank.
$!
$ IF (P4.EQS."")
$ THEN
$!
$!  O.K., The User Didn't Specify A Compiler, Let's Try To
$!  Find Out Which One To Use.
$!
$!  Check To See If We Have GNU C.
$!
$   IF (F$TRNLNM("GNU_CC").NES."")
$   THEN
$!
$!    Looks Like GNUC, Set To Use GNUC.
$!
$     COMPILER = "GNUC"
$!
$!    Tell The User We Are Using GNUC.
$!
$     WRITE SYS$OUTPUT "Using GNU 'C' Compiler."
$!
$!  End The GNU C Compiler Check.
$!
$   ENDIF
$!
$!  Check To See If We Have VAXC Or DECC.
$!
$   IF (F$GETSYI("CPU").GE.128).OR.(F$TRNLNM("DECC$CC_DEFAULT").EQS."/DECC")
$   THEN 
$!
$!    Looks Like DECC, Set To Use DECC.
$!
$     COMPILER = "DECC"
$!
$!    Tell The User We Are Using DECC.
$!
$     WRITE SYS$OUTPUT "Using DECC 'C' Compiler."
$!
$!  Else...
$!
$   ELSE
$!
$!    Looks Like VAXC, Set To Use VAXC.
$!
$     COMPILER = "VAXC"
$!
$!    Tell The User We Are Using VAX C.
$!
$     WRITE SYS$OUTPUT "Using VAXC 'C' Compiler."
$!
$!  End The DECC & VAXC Compiler Check.
$!
$   ENDIF
$!
$! Else...
$!
$ ELSE
$!
$!  Check To See If The User Entered A Valid Paramter.
$!
$   IF (P4.EQS."VAXC").OR.(P4.EQS."DECC").OR.(P4.EQS."GNUC")!.OR.(P4.EQS."LINK")
$   THEN
$!
$!    Check To See If The User Wanted To Just LINK.
$!
$     IF (P4.EQS."LINK")
$     THEN
$!
$!      Looks Like LINK-only
$!
$       COMPILER = "LINK"
$!
$!      Tell The User We Are Only Linking.
$!
$       WRITE SYS$OUTPUT "LINK Only.  This actually NOT YET SUPPORTED!"
$!
$!    End LINK Check.
$!
$     ENDIF
$!
$!    Check To See If The User Wanted DECC.
$!
$     IF (P4.EQS."DECC")
$     THEN
$!
$!      Looks Like DECC, Set To Use DECC.
$!
$       COMPILER = "DECC"
$!
$!      Tell The User We Are Using DECC.
$!
$       WRITE SYS$OUTPUT "Using DECC 'C' Compiler."
$!
$!    End DECC Check.
$!
$     ENDIF
$!
$!    Check To See If We Are To Use VAXC.
$!
$     IF (P4.EQS."VAXC")
$     THEN
$!
$!      Looks Like VAXC, Set To Use VAXC.
$!
$       COMPILER = "VAXC"
$!
$!      Tell The User We Are Using VAX C.
$!
$       WRITE SYS$OUTPUT "Using VAXC 'C' Compiler."
$!
$!    End VAXC Check
$!
$     ENDIF
$!
$!    Check To See If We Are To Use GNU C.
$!
$     IF (P4.EQS."GNUC")
$     THEN
$!
$!      Looks Like GNUC, Set To Use GNUC.
$!
$       COMPILER = "GNUC"
$!
$!      Tell The User We Are Using GNUC.
$!
$       WRITE SYS$OUTPUT "Using GNU 'C' Compiler."
$!
$!    End The GNU C Check.
$!
$     ENDIF
$!
$!  Else The User Entered An Invalid Arguement.
$!
$   ELSE
$!
$!    Tell The User We Don't Know What They Want.
$!
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "The Option ",P4," Is Invalid.  The Valid Options Are:"
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "    VAXC  :  To Compile With VAX C."
$     WRITE SYS$OUTPUT "    DECC  :  To Compile With DEC C."
$     WRITE SYS$OUTPUT "    GNUC  :  To Compile With GNU C."
$     WRITE SYS$OUTPUT ""
$!
$!    Time To EXIT.
$!
$     GOTO TIDY
$!
$!  End The Valid Arguement Check.
$!
$   ENDIF
$!
$! End The P4 Check.
$!
$ ENDIF
$!
$! Time to check the contents of P5, and to make sure we get the correct library.
$!
$ IF P5.EQS."SOCKETSHR" .OR. P5.EQS."MULTINET" .OR. P5.EQS."UCX" -
     .OR. P5.EQS."TCPIP" .OR. P5.EQS."NONE"
$ THEN
$!
$!  Check to see if SOCKETSHR was chosen
$!
$   IF P5.EQS."SOCKETSHR"
$   THEN
$!
$!    Set the library to use SOCKETSHR
$!
$     TCPIP_LIB = "SYS$DISK:[-.VMS]SOCKETSHR_SHR.OPT/OPT"
$!
$!    Tell the user
$!
$     WRITE SYS$OUTPUT "Using SOCKETSHR for TCP/IP"
$!
$!    Done with SOCKETSHR
$!
$   ENDIF
$!
$!  Check to see if MULTINET was chosen
$!
$   IF P5.EQS."MULTINET"
$   THEN
$!
$!    Set the library to use UCX emulation.
$!
$     P5 = "UCX"
$!
$!    Tell the user
$!
$     WRITE SYS$OUTPUT "Using MultiNet via UCX emulation for TCP/IP"
$!
$!    Done with MULTINET
$!
$   ENDIF
$!
$!  Check to see if UCX was chosen
$!
$   IF P5.EQS."UCX"
$   THEN
$!
$!    Set the library to use UCX.
$!
$     TCPIP_LIB = "SYS$DISK:[-.VMS]UCX_SHR_DECC.OPT/OPT"
$!
$!    Tell the user
$!
$     WRITE SYS$OUTPUT "Using UCX or an emulation thereof for TCP/IP"
$!
$!    Done with UCX
$!
$   ENDIF
$!
$!  Check to see if TCPIP was chosen
$!
$   IF P5.EQS."TCPIP"
$   THEN
$!
$!    Set the library to use TCPIP (post UCX).
$!
$     TCPIP_LIB = "SYS$DISK:[-.VMS]TCPIP_SHR_DECC.OPT/OPT"
$!
$!    Tell the user
$!
$     WRITE SYS$OUTPUT "Using TCPIP (post UCX) for TCP/IP"
$!
$!    Done with TCPIP
$!
$   ENDIF
$!
$!  Check to see if NONE was chosen
$!
$   IF P5.EQS."NONE"
$   THEN
$!
$!    Do not use a TCPIP library.
$!
$     TCPIP_LIB = ""
$!
$!    Tell the user
$!
$     WRITE SYS$OUTPUT "A specific TCPIP library will not be used."
$!
$!    Done with NONE.
$!
$   ENDIF
$!
$!  Set the TCPIP_TYPE symbol
$!
$   TCPIP_TYPE = P5
$!
$!  Print info
$!
$   WRITE SYS$OUTPUT "TCP/IP library spec: ", TCPIP_LIB
$!
$!  Else The User Entered An Invalid Arguement.
$!
$ ELSE
$   IF P5 .NES. ""
$   THEN
$!
$!    Tell The User We Don't Know What They Want.
$!
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "The Option ",P5," Is Invalid.  The Valid Options Are:"
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "    SOCKETSHR  :  To link with SOCKETSHR TCP/IP library."
$     WRITE SYS$OUTPUT "    UCX        :  To link with UCX TCP/IP library."
$     WRITE SYS$OUTPUT "    TCPIP      :  To link with TCPIP TCP/IP (post UCX) library."
$     WRITE SYS$OUTPUT "    NONE       :  To not link with a specific TCP/IP library."
$     WRITE SYS$OUTPUT ""
$!
$!    Time To EXIT.
$!
$     GOTO TIDY
$   ELSE
$!
$! If TCPIP is not defined, then hardcode it to make
$! it clear that no TCPIP is desired.
$!
$     IF P5 .EQS. ""
$     THEN
$       TCPIP_LIB = ""
$       TCPIP_TYPE = "NONE"
$     ELSE
$!
$!    Set the TCPIP_TYPE symbol
$!
$       TCPIP_TYPE = P5
$     ENDIF
$   ENDIF
$!
$!  Done with TCP/IP libraries
$!
$ ENDIF
$!
$! Special Threads For OpenVMS v7.1 Or Later
$!
$! Written By:  Richard Levitte
$!              richard@levitte.org
$!
$!
$! Check To See If We Have A Option For P6.
$!
$ IF (P6.EQS."")
$ THEN
$!
$!  Get The Version Of VMS We Are Using.
$!
$   ISSEVEN :=
$   TMP = F$ELEMENT(0,"-",F$EXTRACT(1,4,F$GETSYI("VERSION")))
$   TMP = F$INTEGER(F$ELEMENT(0,".",TMP)+F$ELEMENT(1,".",TMP))
$!
$!  Check To See If The VMS Version Is v7.1 Or Later.
$!
$   IF (TMP.GE.71)
$   THEN
$!
$!    We Have OpenVMS v7.1 Or Later, So Use The Special Threads.
$!
$     ISSEVEN := ,PTHREAD_USE_D4
$!
$!  End The VMS Version Check.
$!
$   ENDIF
$!
$! End The P6 Check.
$!
$ ENDIF
$!
$!  Time To RETURN...
$!
$ RETURN
$!
$ TIDY:
$ SET DEFAULT 'DEF_ORIG'
$ EXIT
$!
