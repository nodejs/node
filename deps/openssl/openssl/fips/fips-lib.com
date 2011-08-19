$!
$!  FIPS-LIB.COM
$!  Written By:  Robert Byer
$!               Vice-President
$!               A-Com Computing, Inc.
$!               byer@mail.all-net.net
$!
$!  Changes by Richard Levitte <richard@levitte.org>
$!
$!  This command files compiles and creates the FIPS parts of the
$!  "[.xxx.EXE.CRYPTO]LIBCRYPTO.OLB" library for OpenSSL.  The "xxx"
$!  denotes the machine architecture of ALPHA, IA64 or VAX.
$!
$!  It was re-written so it would try to determine what "C" compiler to use 
$!  or you can specify which "C" compiler to use.
$!
$!  Specify the following as P1 to build just that part or ALL to just
$!  build everything.
$!
$!    		LIBRARY    To just compile the [.xxx.EXE.CRYPTO]LIBCRYPTO.OLB Library.
$!    		APPS       To just compile the [.xxx.EXE.CRYPTO]*.EXE
$!		ALL	   To do both LIBRARY and APPS
$!
$!  Specify DEBUG or NODEBUG as P2 to compile with or without debugger
$!  information.
$!
$!  Specify which compiler at P3 to try to compile under.
$!
$!	   VAXC	 For VAX C.
$!	   DECC	 For DEC C.
$!	   GNUC	 For GNU C.
$!
$!  If you don't speficy a compiler, it will try to determine which
$!  "C" compiler to use.
$!
$!  P4, if defined, sets a TCP/IP library to use, through one of the following
$!  keywords:
$!
$!	UCX		for UCX
$!	TCPIP		for TCPIP (post UCX)
$!	SOCKETSHR	for SOCKETSHR+NETLIB
$!
$!  P5, if defined, sets a compiler thread NOT needed on OpenVMS 7.1 (and up)
$!
$!  P6, if defined, sets a choice of crypto methods to compile.
$!  WARNING: this should only be done to recompile some part of an already
$!  fully compiled library.
$!
$!
$! Define A TCP/IP Library That We Will Need To Link To.
$! (That Is, If We Need To Link To One.)
$!
$ TCPIP_LIB = ""
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
$! Define The Different Encryption Types.
$!
$ ENCRYPT_TYPES = "Basic,SHA,RAND,DES,AES,DSA,RSA,DH,HMAC"
$!
$! Define The OBJ Directory.
$!
$ OBJ_DIR := SYS$DISK:[-.'ARCH'.OBJ.CRYPTO]
$!
$! Define The EXE Directory.
$!
$ EXE_DIR := SYS$DISK:[-.'ARCH'.EXE.CRYPTO]
$!
$! Check To Make Sure We Have Valid Command Line Parameters.
$!
$ GOSUB CHECK_OPTIONS
$!
$! Initialise logical names and such
$!
$ GOSUB INITIALISE
$!
$! Tell The User What Kind of Machine We Run On.
$!
$ WRITE SYS$OUTPUT "Compiling On A ",ARCH," Machine."
$!
$! Check To See If The Architecture Specific OBJ Directory Exists.
$!
$ IF (F$PARSE(OBJ_DIR).EQS."")
$ THEN
$!
$!  It Dosen't Exist, So Create It.
$!
$   CREATE/DIR 'OBJ_DIR'
$!
$! End The Architecture Specific OBJ Directory Check.
$!
$ ENDIF
$!
$! Check To See If The Architecture Specific Directory Exists.
$!
$ IF (F$PARSE(EXE_DIR).EQS."")
$ THEN
$!
$!  It Dosen't Exist, So Create It.
$!
$   CREATE/DIRECTORY 'EXE_DIR'
$!
$! End The Architecture Specific Directory Check.
$!
$ ENDIF
$!
$! Define The Library Name.
$!
$ LIB_NAME := 'EXE_DIR'LIBCRYPTO.OLB
$!
$! Define The CRYPTO-LIB We Are To Use.
$!
$ CRYPTO_LIB := 'EXE_DIR'LIBCRYPTO.OLB
$!
$! Check To See If We Already Have A "[.xxx.EXE.CRYPTO]LIBCRYPTO.OLB" Library...
$!
$ IF (F$SEARCH(LIB_NAME).EQS."")
$ THEN
$!
$! Guess Not, Create The Library.
$!
$   LIBRARY/CREATE/OBJECT 'LIB_NAME'
$!
$! End The Library Check.
$!
$ ENDIF
$!
$! Build our options file for the application
$!
$ GOSUB CHECK_OPT_FILE
$!
$! Define The Different Encryption "library" Strings.
$!
$ LIB_ = "fips,fips_err_wrapper"
$ LIB_SHA = "fips_sha1dgst,fips_sha1_selftest,fips_sha256,fips_sha512"
$ LIB_RAND = "fips_rand,fips_rand_selftest"
$ LIB_DES = "fips_des_enc,fips_des_selftest,fips_set_key"
$ LIB_AES = "fips_aes_core,fips_aes_selftest"
$ LIB_DSA = "fips_dsa_ossl,fips_dsa_gen,fips_dsa_selftest"
$ LIB_RSA = "fips_rsa_eay,fips_rsa_gen,fips_rsa_selftest,fips_rsa_x931g"
$ LIB_DH = "fips_dh_check,fips_dh_gen,fips_dh_key"
$ LIB_HMAC = "fips_hmac,fips_hmac_selftest"
$!
$! Setup exceptional compilations
$!
$ ! Add definitions for no threads on OpenVMS 7.1 and higher
$ COMPILEWITH_CC3 = ",bss_rtcp,"
$ ! Disable the DOLLARID warning
$ COMPILEWITH_CC4 = ",a_utctm,bss_log,o_time,"
$ ! Disable disjoint optimization
$ COMPILEWITH_CC5 = ",md2_dgst,md4_dgst,md5_dgst,mdc2dgst," + -
                    "sha_dgst,sha1dgst,rmd_dgst,bf_enc,"
$ ! Disable the MIXLINKAGE warning
$ COMPILEWITH_CC6 = ",fips_set_key,"
$!
$! Figure Out What Other Modules We Are To Build.
$!
$ BUILD_SET:
$!
$! Define A Module Counter.
$!
$ MODULE_COUNTER = 0
$!
$! Top Of The Loop.
$!
$ MODULE_NEXT:
$!
$! Extract The Module Name From The Encryption List.
$!
$ MODULE_NAME = F$ELEMENT(MODULE_COUNTER,",",ENCRYPT_TYPES)
$ IF MODULE_NAME.EQS."Basic" THEN MODULE_NAME = ""
$ MODULE_NAME1 = MODULE_NAME
$!
$! Check To See If We Are At The End Of The Module List.
$!
$ IF (MODULE_NAME.EQS.",") 
$ THEN 
$!
$!  We Are At The End Of The Module List, Go To MODULE_DONE.
$!
$   GOTO MODULE_DONE
$!
$! End The Module List Check.
$!
$ ENDIF
$!
$! Increment The Moudle Counter.
$!
$ MODULE_COUNTER = MODULE_COUNTER + 1
$!
$! Create The Library and Apps Module Names.
$!
$ LIB_MODULE = "LIB_" + MODULE_NAME
$ APPS_MODULE = "APPS_" + MODULE_NAME
$ IF (MODULE_NAME.EQS."ASN1_2")
$ THEN
$   MODULE_NAME = "ASN1"
$ ENDIF
$ IF (MODULE_NAME.EQS."EVP_2")
$ THEN
$   MODULE_NAME = "EVP"
$ ENDIF
$!
$! Set state (can be LIB and APPS)
$!
$ STATE = "LIB"
$ IF BUILDALL .EQS. "APPS" THEN STATE = "APPS"
$!
$! Check if the library module name actually is defined
$!
$ IF F$TYPE('LIB_MODULE') .EQS. ""
$ THEN
$   WRITE SYS$ERROR ""
$   WRITE SYS$ERROR "The module ",MODULE_NAME," does not exist.  Continuing..."
$   WRITE SYS$ERROR ""
$   GOTO MODULE_NEXT
$ ENDIF
$!
$! Top Of The Module Loop.
$!
$ MODULE_AGAIN:
$!
$! Tell The User What Module We Are Building.
$!
$ IF (MODULE_NAME1.NES."") 
$ THEN
$   IF STATE .EQS. "LIB"
$   THEN
$     WRITE SYS$OUTPUT "Compiling The ",MODULE_NAME1," Library Files. (",BUILDALL,",",STATE,")"
$   ELSE IF F$TYPE('APPS_MODULE') .NES. ""
$     THEN
$       WRITE SYS$OUTPUT "Compiling The ",MODULE_NAME1," Applications. (",BUILDALL,",",STATE,")"
$     ENDIF
$   ENDIF
$ ENDIF
$!
$!  Define A File Counter And Set It To "0".
$!
$ FILE_COUNTER = 0
$ APPLICATION = ""
$ APPLICATION_COUNTER = 0
$!
$! Top Of The File Loop.
$!
$ NEXT_FILE:
$!
$! Look in the LIB_MODULE is we're in state LIB
$!
$ IF STATE .EQS. "LIB"
$ THEN
$!
$!   O.K, Extract The File Name From The File List.
$!
$   FILE_NAME = F$ELEMENT(FILE_COUNTER,",",'LIB_MODULE')
$!
$!   else
$!
$ ELSE
$   FILE_NAME = ","
$!
$   IF F$TYPE('APPS_MODULE') .NES. ""
$   THEN
$!
$!     Extract The File Name From The File List.
$!     This part is a bit more complicated.
$!
$     IF APPLICATION .EQS. ""
$     THEN
$       APPLICATION = F$ELEMENT(APPLICATION_COUNTER,";",'APPS_MODULE')
$       APPLICATION_COUNTER = APPLICATION_COUNTER + 1
$       APPLICATION_OBJECTS = F$ELEMENT(1,"/",APPLICATION)
$       APPLICATION = F$ELEMENT(0,"/",APPLICATION)
$       FILE_COUNTER = 0
$     ENDIF
$
$!     WRITE SYS$OUTPUT "DEBUG: SHOW SYMBOL APPLICATION*"
$!     SHOW SYMBOL APPLICATION*
$!
$     IF APPLICATION .NES. ";"
$     THEN
$       FILE_NAME = F$ELEMENT(FILE_COUNTER,",",APPLICATION_OBJECTS)
$       IF FILE_NAME .EQS. ","
$       THEN
$         APPLICATION = ""
$         GOTO NEXT_FILE
$       ENDIF
$     ENDIF
$   ENDIF
$ ENDIF
$!
$! Check To See If We Are At The End Of The File List.
$!
$ IF (FILE_NAME.EQS.",") 
$ THEN 
$!
$!  We Are At The End Of The File List, Change State Or Goto FILE_DONE.
$!
$   IF STATE .EQS. "LIB" .AND. BUILDALL .NES. "LIBRARY"
$   THEN
$     STATE = "APPS"
$     GOTO MODULE_AGAIN
$   ELSE
$     GOTO FILE_DONE
$   ENDIF
$!
$! End The File List Check.
$!
$ ENDIF
$!
$! Increment The Counter.
$!
$ FILE_COUNTER = FILE_COUNTER + 1
$!
$! Create The Source File Name.
$!
$ TMP_FILE_NAME = F$ELEMENT(1,"]",FILE_NAME)
$ IF TMP_FILE_NAME .EQS. "]" THEN TMP_FILE_NAME = FILE_NAME
$ IF F$ELEMENT(0,".",TMP_FILE_NAME) .EQS. TMP_FILE_NAME THEN -
	FILE_NAME = FILE_NAME + ".c"
$ IF (MODULE_NAME.NES."")
$ THEN
$   SOURCE_FILE = "SYS$DISK:[." + MODULE_NAME+ "]" + FILE_NAME
$ ELSE
$   SOURCE_FILE = "SYS$DISK:[]" + FILE_NAME
$ ENDIF
$ SOURCE_FILE = SOURCE_FILE - "]["
$!
$! Create The Object File Name.
$!
$ OBJECT_FILE = OBJ_DIR + F$PARSE(FILE_NAME,,,"NAME","SYNTAX_ONLY") + ".OBJ"
$ ON WARNING THEN GOTO NEXT_FILE
$!
$! Check To See If The File We Want To Compile Is Actually There.
$!
$ IF (F$SEARCH(SOURCE_FILE).EQS."")
$ THEN
$!
$!  Tell The User That The File Doesn't Exist.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "The File ",SOURCE_FILE," Doesn't Exist."
$   WRITE SYS$OUTPUT ""
$!
$!  Exit The Build.
$!
$   GOTO EXIT
$!
$! End The File Exist Check.
$!
$ ENDIF
$!
$! Tell The User We Are Compiling The File.
$!
$ IF (MODULE_NAME.EQS."")
$ THEN
$   WRITE SYS$OUTPUT "Compiling The ",FILE_NAME," File.  (",BUILDALL,",",STATE,")"
$ ENDIF
$ IF (MODULE_NAME.NES."")
$ THEN 
$   WRITE SYS$OUTPUT "	",FILE_NAME,""
$ ENDIF
$!
$! Compile The File.
$!
$ ON ERROR THEN GOTO NEXT_FILE
$ FILE_NAME0 = F$ELEMENT(0,".",FILE_NAME)
$ IF FILE_NAME - ".mar" .NES. FILE_NAME
$ THEN
$   MACRO/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$ ELSE
$   IF COMPILEWITH_CC3 - FILE_NAME0 .NES. COMPILEWITH_CC3
$   THEN
$     CC3/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$   ELSE
$     IF COMPILEWITH_CC4 - FILE_NAME0 .NES. COMPILEWITH_CC4
$     THEN
$       CC4/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$     ELSE
$       IF COMPILEWITH_CC5 - FILE_NAME0 .NES. COMPILEWITH_CC5
$       THEN
$         CC5/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$       ELSE
$         IF COMPILEWITH_CC6 - FILE_NAME0 .NES. COMPILEWITH_CC6
$         THEN
$           CC6/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$         ELSE
$           CC/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$         ENDIF
$       ENDIF
$     ENDIF
$   ENDIF
$ ENDIF
$ IF STATE .EQS. "LIB"
$ THEN 
$!
$!   Add It To The Library.
$!
$   LIBRARY/REPLACE 'LIB_NAME' 'OBJECT_FILE'
$!
$!   Time To Clean Up The Object File.
$!
$   DELETE 'OBJECT_FILE';*
$ ENDIF
$!
$! Go Back And Do It Again.
$!
$ GOTO NEXT_FILE
$!
$! All Done With This Library Part.
$!
$ FILE_DONE:
$!
$! Time To Build Some Applications
$!
$ IF F$TYPE('APPS_MODULE') .NES. "" .AND. BUILDALL .NES. "LIBRARY"
$ THEN
$   APPLICATION_COUNTER = 0
$ NEXT_APPLICATION:
$   APPLICATION = F$ELEMENT(APPLICATION_COUNTER,";",'APPS_MODULE')
$   IF APPLICATION .EQS. ";" THEN GOTO APPLICATION_DONE
$
$   APPLICATION_COUNTER = APPLICATION_COUNTER + 1
$   APPLICATION_OBJECTS = F$ELEMENT(1,"/",APPLICATION)
$   APPLICATION = F$ELEMENT(0,"/",APPLICATION)
$
$!   WRITE SYS$OUTPUT "DEBUG: SHOW SYMBOL APPLICATION*"
$!   SHOW SYMBOL APPLICATION*
$!
$! Tell the user what happens
$!
$   WRITE SYS$OUTPUT "	",APPLICATION,".exe"
$!
$! Link The Program.
$!
$   ON ERROR THEN GOTO NEXT_APPLICATION
$!
$! Check To See If We Are To Link With A Specific TCP/IP Library.
$!
$   IF (TCPIP_LIB.NES."")
$   THEN
$!
$!    Link With A TCP/IP Library.
$!
$     LINK/'DEBUGGER'/'TRACEBACK'/EXE='EXE_DIR''APPLICATION'.EXE -
          'OBJ_DIR''APPLICATION_OBJECTS', -
	  'CRYPTO_LIB'/LIBRARY, -
          'TCPIP_LIB','OPT_FILE'/OPTION
$!
$! Else...
$!
$   ELSE
$!
$!    Don't Link With A TCP/IP Library.
$!
$     LINK/'DEBUGGER'/'TRACEBACK'/EXE='EXE_DIR''APPLICATION'.EXE -
          'OBJ_DIR''APPLICATION_OBJECTS',-
	  'CRYPTO_LIB'/LIBRARY, -
          'OPT_FILE'/OPTION
$!
$! End The TCP/IP Library Check.
$!
$   ENDIF
$   GOTO NEXT_APPLICATION
$  APPLICATION_DONE:
$ ENDIF
$!
$! Go Back And Get The Next Module.
$!
$ GOTO MODULE_NEXT
$!
$! All Done With This Module.
$!
$ MODULE_DONE:
$!
$! Tell The User That We Are All Done.
$!
$ WRITE SYS$OUTPUT "All Done..."
$ EXIT:
$ GOSUB CLEANUP
$ EXIT
$!
$! Check For The Link Option FIle.
$!
$ CHECK_OPT_FILE:
$!
$! Check To See If We Need To Make A VAX C Option File.
$!
$ IF (COMPILER.EQS."VAXC")
$ THEN
$!
$!  Check To See If We Already Have A VAX C Linker Option File.
$!
$   IF (F$SEARCH(OPT_FILE).EQS."")
$   THEN
$!
$!    We Need A VAX C Linker Option File.
$!
$     CREATE 'OPT_FILE'
$DECK
!
! Default System Options File To Link Agianst 
! The Sharable VAX C Runtime Library.
!
SYS$SHARE:VAXCRTL.EXE/SHARE
$EOD
$!
$!  End The Option File Check.
$!
$   ENDIF
$!
$! End The VAXC Check.
$!
$ ENDIF
$!
$! Check To See If We Need A GNU C Option File.
$!
$ IF (COMPILER.EQS."GNUC")
$ THEN
$!
$!  Check To See If We Already Have A GNU C Linker Option File.
$!
$   IF (F$SEARCH(OPT_FILE).EQS."")
$   THEN
$!
$!    We Need A GNU C Linker Option File.
$!
$     CREATE 'OPT_FILE'
$DECK
!
! Default System Options File To Link Agianst 
! The Sharable C Runtime Library.
!
GNU_CC:[000000]GCCLIB/LIBRARY
SYS$SHARE:VAXCRTL/SHARE
$EOD
$!
$!  End The Option File Check.
$!
$   ENDIF
$!
$! End The GNU C Check.
$!
$ ENDIF
$!
$! Check To See If We Need A DEC C Option File.
$!
$ IF (COMPILER.EQS."DECC")
$ THEN
$!
$!  Check To See If We Already Have A DEC C Linker Option File.
$!
$   IF (F$SEARCH(OPT_FILE).EQS."")
$   THEN
$!
$!    Figure Out If We Need A non-VAX Or A VAX Linker Option File.
$!
$     IF ARCH .EQS. "VAX"
$     THEN
$!
$!      We Need A DEC C Linker Option File For VAX.
$!
$       CREATE 'OPT_FILE'
$DECK
!
! Default System Options File To Link Agianst 
! The Sharable DEC C Runtime Library.
!
SYS$SHARE:DECC$SHR.EXE/SHARE
$EOD
$!
$!    Else...
$!
$     ELSE
$!
$!      Create The non-VAX Linker Option File.
$!
$       CREATE 'OPT_FILE'
$DECK
!
! Default System Options File For non-VAX To Link Agianst 
! The Sharable C Runtime Library.
!
SYS$SHARE:CMA$OPEN_LIB_SHR/SHARE
SYS$SHARE:CMA$OPEN_RTL/SHARE
$EOD
$!
$!    End The DEC C Option File Check.
$!
$     ENDIF
$!
$!  End The Option File Search.
$!
$   ENDIF
$!
$! End The DEC C Check.
$!
$ ENDIF
$!
$!  Tell The User What Linker Option File We Are Using.
$!
$ WRITE SYS$OUTPUT "Using Linker Option File ",OPT_FILE,"."	
$!
$! Time To RETURN.
$!
$ RETURN
$!
$! Check The User's Options.
$!
$ CHECK_OPTIONS:
$!
$! Check To See If P1 Is Blank.
$!
$ IF (P1.EQS."ALL")
$ THEN
$!
$!   P1 Is Blank, So Build Everything.
$!
$    BUILDALL = "TRUE"
$!
$! Else...
$!
$ ELSE
$!
$!  Else, Check To See If P1 Has A Valid Arguement.
$!
$   IF (P1.EQS."LIBRARY").OR.(P1.EQS."APPS")
$   THEN
$!
$!    A Valid Arguement.
$!
$     BUILDALL = P1
$!
$!  Else...
$!
$   ELSE
$!
$!    Tell The User We Don't Know What They Want.
$!
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "The Option ",P1," Is Invalid.  The Valid Options Are:"
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "    ALL      :  Just Build Everything."
$     WRITE SYS$OUTPUT "    LIBRARY  :  To Compile Just The [.xxx.EXE.CRYPTO]LIBCRYPTO.OLB Library."
$     WRITE SYS$OUTPUT "    APPS     :  To Compile Just The [.xxx.EXE.CRYPTO]*.EXE Programs."
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
$     EXIT
$!
$!  End The Valid Arguement Check.
$!
$   ENDIF
$!
$! End The P1 Check.
$!
$ ENDIF
$!
$! Check To See If P2 Is Blank.
$!
$ IF (P2.EQS."NODEBUG")
$ THEN
$!
$!   P2 Is NODEBUG, So Compile Without The Debugger Information.
$!
$    DEBUGGER = "NODEBUG"
$    TRACEBACK = "NOTRACEBACK" 
$    GCC_OPTIMIZE = "OPTIMIZE"
$    CC_OPTIMIZE = "OPTIMIZE"
$    MACRO_OPTIMIZE = "OPTIMIZE"
$    WRITE SYS$OUTPUT "No Debugger Information Will Be Produced During Compile."
$    WRITE SYS$OUTPUT "Compiling With Compiler Optimization."
$ ELSE
$!
$!  Check To See If We Are To Compile With Debugger Information.
$!
$   IF (P2.EQS."DEBUG")
$   THEN
$!
$!    Compile With Debugger Information.
$!
$     DEBUGGER = "DEBUG"
$     TRACEBACK = "TRACEBACK"
$     GCC_OPTIMIZE = "NOOPTIMIZE"
$     CC_OPTIMIZE = "NOOPTIMIZE"
$     MACRO_OPTIMIZE = "NOOPTIMIZE"
$     WRITE SYS$OUTPUT "Debugger Information Will Be Produced During Compile."
$     WRITE SYS$OUTPUT "Compiling Without Compiler Optimization."
$   ELSE 
$!
$!    They Entered An Invalid Option..
$!
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "The Option ",P2," Is Invalid.  The Valid Options Are:"
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "     DEBUG   :  Compile With The Debugger Information."
$     WRITE SYS$OUTPUT "     NODEBUG :  Compile Without The Debugger Information."
$     WRITE SYS$OUTPUT ""
$!
$!    Time To EXIT.
$!
$     EXIT
$!
$!  End The Valid Arguement Check.
$!
$   ENDIF
$!
$! End The P2 Check.
$!
$ ENDIF
$!
$! Special Threads For OpenVMS v7.1 Or Later
$!
$! Written By:  Richard Levitte
$!              richard@levitte.org
$!
$!
$! Check To See If We Have A Option For P5.
$!
$ IF (P5.EQS."")
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
$! End The P5 Check.
$!
$ ENDIF
$!
$! Check To See If P3 Is Blank.
$!
$ IF (P3.EQS."")
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
$     P3 = "GNUC"
$!
$!  Else...
$!
$   ELSE
$!
$!    Check To See If We Have VAXC Or DECC.
$!
$     IF (ARCH.NES."VAX").OR.(F$TRNLNM("DECC$CC_DEFAULT").NES."")
$     THEN 
$!
$!      Looks Like DECC, Set To Use DECC.
$!
$       P3 = "DECC"
$!
$!    Else...
$!
$     ELSE
$!
$!      Looks Like VAXC, Set To Use VAXC.
$!
$       P3 = "VAXC"
$!
$!    End The VAXC Compiler Check.
$!
$     ENDIF
$!
$!  End The DECC & VAXC Compiler Check.
$!
$   ENDIF
$!
$!  End The Compiler Check.
$!
$ ENDIF
$!
$! Check To See If We Have A Option For P4.
$!
$ IF (P4.EQS."")
$ THEN
$!
$!  Find out what socket library we have available
$!
$   IF F$PARSE("SOCKETSHR:") .NES. ""
$   THEN
$!
$!    We have SOCKETSHR, and it is my opinion that it's the best to use.
$!
$     P4 = "SOCKETSHR"
$!
$!    Tell the user
$!
$     WRITE SYS$OUTPUT "Using SOCKETSHR for TCP/IP"
$!
$!    Else, let's look for something else
$!
$   ELSE
$!
$!    Like UCX (the reason to do this before Multinet is that the UCX
$!    emulation is easier to use...)
$!
$     IF F$TRNLNM("UCX$IPC_SHR") .NES. "" -
	 .OR. F$PARSE("SYS$SHARE:UCX$IPC_SHR.EXE") .NES. "" -
	 .OR. F$PARSE("SYS$LIBRARY:UCX$IPC.OLB") .NES. ""
$     THEN
$!
$!	Last resort: a UCX or UCX-compatible library
$!
$	P4 = "UCX"
$!
$!      Tell the user
$!
$       WRITE SYS$OUTPUT "Using UCX or an emulation thereof for TCP/IP"
$!
$!	That was all...
$!
$     ENDIF
$   ENDIF
$ ENDIF
$!
$! Set Up Initial CC Definitions, Possibly With User Ones
$!
$ CCDEFS = "TCPIP_TYPE_''P4',DSO_VMS"
$ IF F$TYPE(USER_CCDEFS) .NES. "" THEN CCDEFS = CCDEFS + "," + USER_CCDEFS
$ CCEXTRAFLAGS = ""
$ IF F$TYPE(USER_CCFLAGS) .NES. "" THEN CCEXTRAFLAGS = USER_CCFLAGS
$ CCDISABLEWARNINGS = "LONGLONGTYPE,LONGLONGSUFX,FOUNDCR"
$ IF F$TYPE(USER_CCDISABLEWARNINGS) .NES. "" THEN -
	CCDISABLEWARNINGS = CCDISABLEWARNINGS + "," + USER_CCDISABLEWARNINGS
$!
$!  Check To See If The User Entered A Valid Paramter.
$!
$ IF (P3.EQS."VAXC").OR.(P3.EQS."DECC").OR.(P3.EQS."GNUC")
$ THEN
$!
$!    Check To See If The User Wanted DECC.
$!
$   IF (P3.EQS."DECC")
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
$!    Use DECC...
$!
$     CC = "CC"
$     IF ARCH.EQS."VAX" .AND. F$TRNLNM("DECC$CC_DEFAULT").NES."/DECC" -
	 THEN CC = "CC/DECC"
$     CC = CC + "/''CC_OPTIMIZE'/''DEBUGGER'/STANDARD=ANSI89" + -
           "/NOLIST/PREFIX=ALL" + -
	   "/INCLUDE=(SYS$DISK:[],SYS$DISK:[-],SYS$DISK:[-.CRYPTO])" + -
	   CCEXTRAFLAGS
$!
$!    Define The Linker Options File Name.
$!
$     OPT_FILE = "''EXE_DIR'VAX_DECC_OPTIONS.OPT"
$!
$!  End DECC Check.
$!
$   ENDIF
$!
$!  Check To See If We Are To Use VAXC.
$!
$   IF (P3.EQS."VAXC")
$   THEN
$!
$!    Looks Like VAXC, Set To Use VAXC.
$!
$     COMPILER = "VAXC"
$!
$!    Tell The User We Are Using VAX C.
$!
$     WRITE SYS$OUTPUT "Using VAXC 'C' Compiler."
$!
$!    Compile Using VAXC.
$!
$     CC = "CC"
$     IF ARCH.NES."VAX"
$     THEN
$	WRITE SYS$OUTPUT "There is no VAX C on ''ARCH'!"
$	EXIT
$     ENDIF
$     IF F$TRNLNM("DECC$CC_DEFAULT").EQS."/DECC" THEN CC = "CC/VAXC"
$     CC = CC + "/''CC_OPTIMIZE'/''DEBUGGER'/NOLIST" + -
	   "/INCLUDE=(SYS$DISK:[],SYS$DISK:[-],SYS$DISK:[-.CRYPTO])" + -
	   CCEXTRAFLAGS
$     CCDEFS = """VAXC""," + CCDEFS
$!
$!    Define <sys> As SYS$COMMON:[SYSLIB]
$!
$     DEFINE/NOLOG SYS SYS$COMMON:[SYSLIB]
$!
$!    Define The Linker Options File Name.
$!
$     OPT_FILE = "''EXE_DIR'VAX_VAXC_OPTIONS.OPT"
$!
$!  End VAXC Check
$!
$   ENDIF
$!
$!  Check To See If We Are To Use GNU C.
$!
$   IF (P3.EQS."GNUC")
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
$!    Use GNU C...
$!
$     CC = "GCC/NOCASE_HACK/''GCC_OPTIMIZE'/''DEBUGGER'/NOLIST" + -
	   "/INCLUDE=(SYS$DISK:[],SYS$DISK:[-],SYS$DISK:[-.CRYPTO])" + -
	   CCEXTRAFLAGS
$!
$!    Define The Linker Options File Name.
$!
$     OPT_FILE = "''EXE_DIR'VAX_GNUC_OPTIONS.OPT"
$!
$!  End The GNU C Check.
$!
$   ENDIF
$!
$!  Set up default defines
$!
$   CCDEFS = """FLAT_INC=1""," + CCDEFS
$!
$!  Finish up the definition of CC.
$!
$   IF COMPILER .EQS. "DECC"
$   THEN
$     IF CCDISABLEWARNINGS .EQS. ""
$     THEN
$       CC4DISABLEWARNINGS = "DOLLARID"
$       CC6DISABLEWARNINGS = "MIXLINKAGE"
$     ELSE
$       CC4DISABLEWARNINGS = CCDISABLEWARNINGS + ",DOLLARID"
$       CC6DISABLEWARNINGS = CCDISABLEWARNINGS + ",MIXLINKAGE"
$       CCDISABLEWARNINGS = "/WARNING=(DISABLE=(" + CCDISABLEWARNINGS + "))"
$     ENDIF
$     CC4DISABLEWARNINGS = "/WARNING=(DISABLE=(" + CC4DISABLEWARNINGS + "))"
$     CC6DISABLEWARNINGS = "/WARNING=(DISABLE=(" + CC6DISABLEWARNINGS + "))"
$   ELSE
$     CCDISABLEWARNINGS = ""
$     CC4DISABLEWARNINGS = ""
$     CC6DISABLEWARNINGS = ""
$   ENDIF
$   CC3 = CC + "/DEFINE=(" + CCDEFS + ISSEVEN + ")" + CCDISABLEWARNINGS
$   CC = CC + "/DEFINE=(" + CCDEFS + ")" + CCDISABLEWARNINGS
$   IF ARCH .EQS. "VAX" .AND. COMPILER .EQS. "DECC" .AND. P2 .NES. "DEBUG"
$   THEN
$     CC5 = CC + "/OPTIMIZE=NODISJOINT"
$   ELSE
$     CC5 = CC + "/NOOPTIMIZE"
$   ENDIF
$   CC4 = CC - CCDISABLEWARNINGS + CC4DISABLEWARNINGS
$   CC6 = CC - CCDISABLEWARNINGS + CC6DISABLEWARNINGS
$!
$!  Show user the result
$!
$   WRITE/SYMBOL SYS$OUTPUT "Main C Compiling Command: ",CC
$!
$!  Else The User Entered An Invalid Arguement.
$!
$ ELSE
$!
$!  Tell The User We Don't Know What They Want.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "The Option ",P3," Is Invalid.  The Valid Options Are:"
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "    VAXC  :  To Compile With VAX C."
$   WRITE SYS$OUTPUT "    DECC  :  To Compile With DEC C."
$   WRITE SYS$OUTPUT "    GNUC  :  To Compile With GNU C."
$   WRITE SYS$OUTPUT ""
$!
$!  Time To EXIT.
$!
$   EXIT
$!
$! End The Valid Arguement Check.
$!
$ ENDIF
$!
$! Build a MACRO command for the architecture at hand
$!
$ IF ARCH .EQS. "VAX" THEN MACRO = "MACRO/''DEBUGGER'"
$ IF ARCH .NES. "VAX" THEN MACRO = "MACRO/MIGRATION/''DEBUGGER'/''MACRO_OPTIMIZE'"
$!
$!  Show user the result
$!
$   WRITE/SYMBOL SYS$OUTPUT "Main MACRO Compiling Command: ",MACRO
$!
$! Time to check the contents, and to make sure we get the correct library.
$!
$ IF P4.EQS."SOCKETSHR" .OR. P4.EQS."MULTINET" .OR. P4.EQS."UCX" -
     .OR. P4.EQS."TCPIP" .OR. P4.EQS."NONE"
$ THEN
$!
$!  Check to see if SOCKETSHR was chosen
$!
$   IF P4.EQS."SOCKETSHR"
$   THEN
$!
$!    Set the library to use SOCKETSHR
$!
$     TCPIP_LIB = "SYS$DISK:[-.VMS]SOCKETSHR_SHR.OPT/OPT"
$!
$!    Done with SOCKETSHR
$!
$   ENDIF
$!
$!  Check to see if MULTINET was chosen
$!
$   IF P4.EQS."MULTINET"
$   THEN
$!
$!    Set the library to use UCX emulation.
$!
$     P4 = "UCX"
$!
$!    Done with MULTINET
$!
$   ENDIF
$!
$!  Check to see if UCX was chosen
$!
$   IF P4.EQS."UCX"
$   THEN
$!
$!    Set the library to use UCX.
$!
$     TCPIP_LIB = "SYS$DISK:[-.VMS]UCX_SHR_DECC.OPT/OPT"
$     IF F$TRNLNM("UCX$IPC_SHR") .NES. ""
$     THEN
$       TCPIP_LIB = "SYS$DISK:[-.VMS]UCX_SHR_DECC_LOG.OPT/OPT"
$     ELSE
$       IF COMPILER .NES. "DECC" .AND. ARCH .EQS. "VAX" THEN -
	  TCPIP_LIB = "SYS$DISK:[-.VMS]UCX_SHR_VAXC.OPT/OPT"
$     ENDIF
$!
$!    Done with UCX
$!
$   ENDIF
$!
$!  Check to see if TCPIP was chosen
$!
$   IF P4.EQS."TCPIP"
$   THEN
$!
$!    Set the library to use TCPIP (post UCX).
$!
$     TCPIP_LIB = "SYS$DISK:[-.VMS]TCPIP_SHR_DECC.OPT/OPT"
$!
$!    Done with TCPIP
$!
$   ENDIF
$!
$!  Check to see if NONE was chosen
$!
$   IF P4.EQS."NONE"
$   THEN
$!
$!    Do not use a TCPIP library.
$!
$     TCPIP_LIB = ""
$!
$!    Done with TCPIP
$!
$   ENDIF
$!
$!  Print info
$!
$   WRITE SYS$OUTPUT "TCP/IP library spec: ", TCPIP_LIB
$!
$!  Else The User Entered An Invalid Arguement.
$!
$ ELSE
$!
$!  Tell The User We Don't Know What They Want.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "The Option ",P4," Is Invalid.  The Valid Options Are:"
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "    SOCKETSHR  :  To link with SOCKETSHR TCP/IP library."
$   WRITE SYS$OUTPUT "    UCX        :  To link with UCX TCP/IP library."
$   WRITE SYS$OUTPUT "    TCPIP      :  To link with TCPIP (post UCX) TCP/IP library."
$   WRITE SYS$OUTPUT ""
$!
$!  Time To EXIT.
$!
$   EXIT
$!
$!  Done with TCP/IP libraries
$!
$ ENDIF
$!
$! Check if the user wanted to compile just a subset of all the encryption
$! methods.
$!
$ IF P6 .NES. ""
$ THEN
$   ENCRYPT_TYPES = P6
$ ENDIF
$!
$!  Time To RETURN...
$!
$ RETURN
$!
$ INITIALISE:
$!
$! Save old value of the logical name OPENSSL
$!
$ __SAVE_OPENSSL = F$TRNLNM("OPENSSL","LNM$PROCESS_TABLE")
$!
$! Save directory information
$!
$ __HERE = F$PARSE(F$PARSE("A.;",F$ENVIRONMENT("PROCEDURE"))-"A.;","[]A.;") - "A.;"
$ __HERE = F$EDIT(__HERE,"UPCASE")
$ __TOP = __HERE - "FIPS-1_0]"
$ __INCLUDE = __TOP + "INCLUDE.OPENSSL]"
$!
$! Set up the logical name OPENSSL to point at the include directory
$!
$ DEFINE OPENSSL/NOLOG '__INCLUDE'
$!
$! Done
$!
$ RETURN
$!
$ CLEANUP:
$!
$! Restore the logical name OPENSSL if it had a value
$!
$ IF __SAVE_OPENSSL .EQS. ""
$ THEN
$   DEASSIGN OPENSSL
$ ELSE
$   DEFINE/NOLOG OPENSSL '__SAVE_OPENSSL'
$ ENDIF
$!
$! Done
$!
$ RETURN
