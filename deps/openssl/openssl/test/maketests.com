$!
$!  MAKETESTS.COM
$!  Written By:  Robert Byer
$!               Vice-President
$!               A-Com Computing, Inc.
$!               byer@mail.all-net.net
$!
$!  Changes by Richard Levitte <richard@levitte.org>
$!
$!  This command files compiles and creates all the various different
$!  "test" programs for the different types of encryption for OpenSSL.
$!  It was written so it would try to determine what "C" compiler to
$!  use or you can specify which "C" compiler to use.
$!
$!  The test "executables" will be placed in a directory called
$!  [.xxx.EXE.TEST] where "xxx" denotes ALPHA, IA64, or VAX, depending
$!  on your machine architecture.
$!
$!  Specify DEBUG or NODEBUG P1 to compile with or without debugger
$!  information.
$!
$!  Specify which compiler at P2 to try to compile under.
$!
$!	   VAXC	 For VAX C.
$!	   DECC	 For DEC C.
$!	   GNUC	 For GNU C.
$!
$!  If you don't specify a compiler, it will try to determine which
$!  "C" compiler to use.
$!
$!  P3, if defined, sets a TCP/IP library to use, through one of the following
$!  keywords:
$!
$!	UCX		for UCX
$!	SOCKETSHR	for SOCKETSHR+NETLIB
$!
$!  P4, if defined, sets a compiler thread NOT needed on OpenVMS 7.1 (and up)
$!
$!
$!  P5, if defined, specifies the C pointer size.  Ignored on VAX.
$!      ("64=ARGV" gives more efficient code with HP C V7.3 or newer.)
$!      Supported values are:
$!
$!      ""       Compile with default (/NOPOINTER_SIZE)
$!      32       Compile with /POINTER_SIZE=32 (SHORT)
$!      64       Compile with /POINTER_SIZE=64[=ARGV] (LONG[=ARGV])
$!               (Automatically select ARGV if compiler supports it.)
$!      64=      Compile with /POINTER_SIZE=64 (LONG).
$!      64=ARGV  Compile with /POINTER_SIZE=64=ARGV (LONG=ARGV).
$!
$!  P6, if defined, specifies a directory where ZLIB files (zlib.h,
$!  libz.olb) may be found.  Optionally, a non-default object library
$!  name may be included ("dev:[dir]libz_64.olb", for example).
$!
$!
$! Announce/identify.
$!
$ proc = f$environment( "procedure")
$ write sys$output "@@@ "+ -
   f$parse( proc, , , "name")+ f$parse( proc, , , "type")
$!
$! Define A TCP/IP Library That We Will Need To Link To.
$! (That is, If We Need To Link To One.)
$!
$ TCPIP_LIB = ""
$ ZLIB_LIB = ""
$!
$! Check Which Architecture We Are Using.
$!
$ if (f$getsyi( "cpu") .lt. 128)
$ then
$    ARCH = "VAX"
$ else
$    ARCH = f$edit( f$getsyi( "ARCH_NAME"), "UPCASE")
$    if (ARCH .eqs. "") then ARCH = "UNK"
$ endif
$!
$ ARCHD = ARCH
$ LIB32 = "32"
$ OPT_FILE = ""
$ POINTER_SIZE = ""
$!
$! Check To Make Sure We Have Valid Command Line Parameters.
$!
$ GOSUB CHECK_OPTIONS
$!
$! Define The OBJ and EXE Directories.
$!
$ OBJ_DIR := SYS$DISK:[-.'ARCHD'.OBJ.TEST]
$ EXE_DIR := SYS$DISK:[-.'ARCHD'.EXE.TEST]
$!
$! Specify the destination directory in any /MAP option.
$!
$ if (LINKMAP .eqs. "MAP")
$ then
$   LINKMAP = LINKMAP+ "=''EXE_DIR'"
$ endif
$!
$! Add the location prefix to the linker options file name.
$!
$ if (OPT_FILE .nes. "")
$ then
$   OPT_FILE = EXE_DIR+ OPT_FILE
$ endif
$!
$! Initialise logical names and such
$!
$ GOSUB INITIALISE
$!
$! Tell The User What Kind of Machine We Run On.
$!
$ WRITE SYS$OUTPUT "Host system architecture: ''ARCHD'"
$!
$! Define The CRYPTO-LIB We Are To Use.
$!
$ CRYPTO_LIB := SYS$DISK:[-.'ARCHD'.EXE.CRYPTO]SSL_LIBCRYPTO'LIB32'.OLB
$!
$! Define The SSL We Are To Use.
$!
$ SSL_LIB := SYS$DISK:[-.'ARCHD'.EXE.SSL]SSL_LIBSSL'LIB32'.OLB
$!
$! Create the OBJ and EXE Directories, if needed.
$!
$ IF (F$PARSE(OBJ_DIR).EQS."") THEN -
   CREATE /DIRECTORY 'OBJ_DIR'
$ IF (F$PARSE(EXE_DIR).EQS."") THEN -
   CREATE /DIRECTORY 'EXE_DIR'
$!
$! Check To See If We Have The Proper Libraries.
$!
$ GOSUB LIB_CHECK
$!
$! Check To See If We Have A Linker Option File.
$!
$ GOSUB CHECK_OPT_FILE
$!
$! Define The TEST Files.
$! NOTE: Some might think this list ugly.  However, it's made this way to
$! reflect the EXE variable in Makefile as closely as possible,
$! thereby making it fairly easy to verify that the lists are the same.
$!
$ TEST_FILES = "BNTEST,ECTEST,ECDSATEST,ECDHTEST,IDEATEST,"+ -
	       "MD2TEST,MD4TEST,MD5TEST,HMACTEST,WP_TEST,"+ -
	       "RC2TEST,RC4TEST,RC5TEST,"+ -
	       "DESTEST,SHATEST,SHA1TEST,SHA256T,SHA512T,"+ -
	       "MDC2TEST,RMDTEST,"+ -
	       "RANDTEST,DHTEST,ENGINETEST,"+ -
	       "BFTEST,CASTTEST,SSLTEST,EXPTEST,DSATEST,RSA_TEST,"+ -
	       "EVP_TEST,IGETEST,JPAKETEST,ASN1TEST"
$! Should we add MTTEST,PQ_TEST,LH_TEST,DIVTEST,TABTEST as well?
$!
$! Additional directory information.
$ T_D_BNTEST     := [-.crypto.bn]
$ T_D_ECTEST     := [-.crypto.ec]
$ T_D_ECDSATEST  := [-.crypto.ecdsa]
$ T_D_ECDHTEST   := [-.crypto.ecdh]
$ T_D_IDEATEST   := [-.crypto.idea]
$ T_D_MD2TEST    := [-.crypto.md2]
$ T_D_MD4TEST    := [-.crypto.md4]
$ T_D_MD5TEST    := [-.crypto.md5]
$ T_D_HMACTEST   := [-.crypto.hmac]
$ T_D_WP_TEST    := [-.crypto.whrlpool]
$ T_D_RC2TEST    := [-.crypto.rc2]
$ T_D_RC4TEST    := [-.crypto.rc4]
$ T_D_RC5TEST    := [-.crypto.rc5]
$ T_D_DESTEST    := [-.crypto.des]
$ T_D_SHATEST    := [-.crypto.sha]
$ T_D_SHA1TEST   := [-.crypto.sha]
$ T_D_SHA256T    := [-.crypto.sha]
$ T_D_SHA512T    := [-.crypto.sha]
$ T_D_MDC2TEST   := [-.crypto.mdc2]
$ T_D_RMDTEST    := [-.crypto.ripemd]
$ T_D_RANDTEST   := [-.crypto.rand]
$ T_D_DHTEST     := [-.crypto.dh]
$ T_D_ENGINETEST := [-.crypto.engine]
$ T_D_BFTEST     := [-.crypto.bf]
$ T_D_CASTTEST   := [-.crypto.cast]
$ T_D_SSLTEST    := [-.ssl]
$ T_D_EXPTEST    := [-.crypto.bn]
$ T_D_DSATEST    := [-.crypto.dsa]
$ T_D_RSA_TEST   := [-.crypto.rsa]
$ T_D_EVP_TEST   := [-.crypto.evp]
$ T_D_IGETEST    := [-.test]
$ T_D_JPAKETEST  := [-.crypto.jpake]
$ T_D_ASN1TEST   := [-.test]
$!
$ TCPIP_PROGRAMS = ",,"
$ IF COMPILER .EQS. "VAXC" THEN -
     TCPIP_PROGRAMS = ",SSLTEST,"
$!
$! Define A File Counter And Set It To "0".
$!
$ FILE_COUNTER = 0
$!
$! Top Of The File Loop.
$!
$ NEXT_FILE:
$!
$! O.K, Extract The File Name From The File List.
$!
$ FILE_NAME = F$ELEMENT(FILE_COUNTER,",",TEST_FILES)
$!
$! Check To See If We Are At The End Of The File List.
$!
$ IF (FILE_NAME.EQS.",") THEN GOTO FILE_DONE
$!
$! Increment The Counter.
$!
$ FILE_COUNTER = FILE_COUNTER + 1
$!
$! Create The Source File Name.
$!
$ SOURCE_FILE = "SYS$DISK:" + T_D_'FILE_NAME' + FILE_NAME + ".C"
$!
$! Create The Object File Name.
$!
$ OBJECT_FILE = OBJ_DIR + FILE_NAME + ".OBJ"
$!
$! Create The Executable File Name.
$!
$ EXE_FILE = EXE_DIR + FILE_NAME + ".EXE"
$ ON WARNING THEN GOTO NEXT_FILE
$!
$! Check To See If The File We Want To Compile Actually Exists.
$!
$ IF (F$SEARCH(SOURCE_FILE).EQS."")
$ THEN
$!
$!  Tell The User That The File Dosen't Exist.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "The File ",SOURCE_FILE," Dosen't Exist."
$   WRITE SYS$OUTPUT ""
$!
$!  Exit The Build.
$!
$   GOTO EXIT
$ ENDIF
$!
$! Tell The User What We Are Building.
$!
$ WRITE SYS$OUTPUT "Building The ",FILE_NAME," Test Program."
$!
$! Compile The File.
$!
$ ON ERROR THEN GOTO NEXT_FILE
$ CC /OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$ ON WARNING THEN GOTO NEXT_FILE
$!
$! Check If What We Are About To Compile Works Without A TCP/IP Library.
$!
$ IF ((TCPIP_LIB.EQS."").AND.((TCPIP_PROGRAMS-FILE_NAME).NES.TCPIP_PROGRAMS))
$ THEN
$!
$!  Inform The User That A TCP/IP Library Is Needed To Compile This Program.
$!
$   WRITE SYS$OUTPUT -
	  FILE_NAME," Needs A TCP/IP Library.  Can't Link.  Skipping..."
$   GOTO NEXT_FILE
$!
$! End The TCP/IP Library Check.
$!
$ ENDIF
$!
$! Link The Program, Check To See If We Need To Link With RSAREF Or Not.
$! Check To See If We Are To Link With A Specific TCP/IP Library.
$!
$!  Don't Link With The RSAREF Routines And TCP/IP Library.
$!
$ LINK /'DEBUGGER' /'LINKMAP' /'TRACEBACK' /EXECTABLE = 'EXE_FILE' -
   'OBJECT_FILE', -
   'SSL_LIB' /LIBRARY, -
   'CRYPTO_LIB' /LIBRARY -
   'TCPIP_LIB' -
   'ZLIB_LIB' -
   ,'OPT_FILE' /OPTIONS
$!
$! Go Back And Do It Again.
$!
$ GOTO NEXT_FILE
$!
$! All Done With This Library Part.
$!
$ FILE_DONE:
$!
$! All Done, Time To Exit.
$!
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
! Default System Options File To Link Against 
! The Sharable VAX C Runtime Library.
!
SYS$SHARE:VAXCRTL.EXE /SHAREABLE
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
! Default System Options File To Link Against 
! The Sharable C Runtime Library.
!
GNU_CC:[000000]GCCLIB.OLB /LIBRARY
SYS$SHARE:VAXCRTL.EXE /SHAREABLE
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
$     IF (ARCH.EQS."VAX")
$     THEN
$!
$!      We Need A DEC C Linker Option File For VAX.
$!
$       CREATE 'OPT_FILE'
$DECK
!
! Default System Options File To Link Against 
! The Sharable DEC C Runtime Library.
!
SYS$SHARE:DECC$SHR.EXE /SHAREABLE
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
! Default System Options File For non-VAX To Link Against 
! The Sharable C Runtime Library.
!
SYS$SHARE:CMA$OPEN_LIB_SHR.EXE /SHAREABLE
SYS$SHARE:CMA$OPEN_RTL.EXE /SHAREABLE
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
$! Check To See If We Have The Appropiate Libraries.
$!
$ LIB_CHECK:
$!
$! Look For The Library LIBCRYPTO.OLB.
$!
$ IF (F$SEARCH(CRYPTO_LIB).EQS."")
$ THEN
$!
$!  Tell The User We Can't Find The LIBCRYPTO.OLB Library.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "Can't Find The Library ",CRYPTO_LIB,"."
$   WRITE SYS$OUTPUT "We Can't Link Without It."
$   WRITE SYS$OUTPUT ""
$!
$!  Since We Can't Link Without It, Exit.
$!
$   EXIT
$!
$! End The Crypto Library Check.
$!
$ ENDIF
$!
$! Look For The Library LIBSSL.OLB.
$!
$ IF (F$SEARCH(SSL_LIB).EQS."")
$ THEN
$!
$!  Tell The User We Can't Find The LIBSSL.OLB Library.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "Can't Find The Library ",SSL_LIB,"."
$   WRITE SYS$OUTPUT "Some Of The Test Programs Need To Link To It."
$   WRITE SYS$OUTPUT ""
$!
$!  Since We Can't Link Without It, Exit.
$!
$   EXIT
$!
$! End The SSL Library Check.
$!
$ ENDIF
$!
$! Time To Return.
$!
$ RETURN
$!
$! Check The User's Options.
$!
$ CHECK_OPTIONS:
$!
$! Set basic C compiler /INCLUDE directories.
$!
$ CC_INCLUDES = "SYS$DISK:[-],SYS$DISK:[-.CRYPTO]"
$!
$! Check To See If P1 Is Blank.
$!
$ IF (P1.EQS."NODEBUG")
$ THEN
$!
$!  P1 Is NODEBUG, So Compile Without Debugger Information.
$!
$   DEBUGGER  = "NODEBUG"
$   LINKMAP = "NOMAP"
$   TRACEBACK = "NOTRACEBACK" 
$   GCC_OPTIMIZE = "OPTIMIZE"
$   CC_OPTIMIZE = "OPTIMIZE"
$   WRITE SYS$OUTPUT "No Debugger Information Will Be Produced During Compile."
$   WRITE SYS$OUTPUT "Compiling With Compiler Optimization."
$!
$! Else...
$!
$ ELSE
$!
$!  Check To See If We Are To Compile With Debugger Information.
$!
$   IF (P1.EQS."DEBUG")
$   THEN
$!
$!    Compile With Debugger Information.
$!
$     DEBUGGER  = "DEBUG"
$     LINKMAP = "MAP"
$     TRACEBACK = "TRACEBACK"
$     GCC_OPTIMIZE = "NOOPTIMIZE"
$     CC_OPTIMIZE = "NOOPTIMIZE"
$     WRITE SYS$OUTPUT "Debugger Information Will Be Produced During Compile."
$     WRITE SYS$OUTPUT "Compiling Without Compiler Optimization."
$!
$!  Else...
$!
$   ELSE
$!
$!    Tell The User Entered An Invalid Option.
$!
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "The Option ",P1," Is Invalid.  The Valid Options Are:"
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "    DEBUG    :  Compile With The Debugger Information."
$     WRITE SYS$OUTPUT "    NODEBUG  :  Compile Without The Debugger Information."
$     WRITE SYS$OUTPUT ""
$!
$!    Time To EXIT.
$!
$     EXIT
$!
$!  End The Valid Argument Check.
$!
$   ENDIF
$!
$! End The P1 Check.
$!
$ ENDIF
$!
$! Check P5 (POINTER_SIZE).
$!
$ IF (P5 .NES. "") .AND. (ARCH .NES. "VAX")
$ THEN
$!
$   IF (P5 .EQS. "32")
$   THEN
$     POINTER_SIZE = " /POINTER_SIZE=32"
$   ELSE
$     POINTER_SIZE = F$EDIT( P5, "COLLAPSE, UPCASE")
$     IF ((POINTER_SIZE .EQS. "64") .OR. -
       (POINTER_SIZE .EQS. "64=") .OR. -
       (POINTER_SIZE .EQS. "64=ARGV"))
$     THEN
$       ARCHD = ARCH+ "_64"
$       LIB32 = ""
$       IF (F$EXTRACT( 2, 1, POINTER_SIZE) .EQS. "=")
$       THEN
$!        Explicit user choice: "64" or "64=ARGV".
$         IF (POINTER_SIZE .EQS. "64=") THEN POINTER_SIZE = "64"
$       ELSE
$         SET NOON
$         DEFINE /USER_MODE SYS$OUTPUT NL:
$         DEFINE /USER_MODE SYS$ERROR NL:
$         CC /NOLIST /NOOBJECT /POINTER_SIZE=64=ARGV NL:
$         IF ($STATUS .AND. %X0FFF0000) .EQ. %X00030000
$         THEN
$           ! If we got here, it means DCL complained like this:
$           ! %DCL-W-NOVALU, value not allowed - remove value specification
$           !  \64=\
$           !
$           ! If the compiler was run, logicals defined in /USER would
$           ! have been deassigned automatically.  However, when DCL
$           ! complains, they aren't, so we do it here (it might be
$           ! unnecessary, but just in case there will be another error
$           ! message further on that we don't want to miss)
$           DEASSIGN /USER_MODE SYS$ERROR
$           DEASSIGN /USER_MODE SYS$OUTPUT
$         ELSE
$           POINTER_SIZE = POINTER_SIZE + "=ARGV"
$         ENDIF
$         SET ON
$       ENDIF
$       POINTER_SIZE = " /POINTER_SIZE=''POINTER_SIZE'"
$     ELSE
$!
$!      Tell The User Entered An Invalid Option.
$!
$       WRITE SYS$OUTPUT ""
$       WRITE SYS$OUTPUT "The Option ", P5, -
         " Is Invalid.  The Valid Options Are:"
$       WRITE SYS$OUTPUT ""
$       WRITE SYS$OUTPUT -
         "    """"  :  Compile with default (short) pointers."
$       WRITE SYS$OUTPUT -
         "    32  :  Compile with 32-bit (short) pointers."
$       WRITE SYS$OUTPUT -
         "    64       :  Compile with 64-bit (long) pointers (auto ARGV)."
$       WRITE SYS$OUTPUT -
         "    64=      :  Compile with 64-bit (long) pointers (no ARGV)."
$       WRITE SYS$OUTPUT -
         "    64=ARGV  :  Compile with 64-bit (long) pointers (ARGV)."
$       WRITE SYS$OUTPUT ""
$! 
$!      Time To EXIT.
$!
$       EXIT
$!
$     ENDIF
$!
$   ENDIF
$!
$! End The P5 (POINTER_SIZE) Check.
$!
$ ENDIF
$!
$! Check To See If P2 Is Blank.
$!
$ IF (P2.EQS."")
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
$     P2 = "GNUC"
$!
$!  End The GNU C Compiler Check.
$!
$   ELSE
$!
$!  Check To See If We Have VAXC Or DECC.
$!
$     IF (ARCH.NES."VAX").OR.(F$TRNLNM("DECC$CC_DEFAULT").NES."")
$     THEN 
$!
$!      Looks Like DECC, Set To Use DECC.
$!
$       P2 = "DECC"
$!
$!      Else...
$!
$     ELSE
$!
$!      Looks Like VAXC, Set To Use VAXC.
$!
$       P2 = "VAXC"
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
$! Check To See If We Have A Option For P3.
$!
$ IF (P3.EQS."")
$ THEN
$!
$!  Find out what socket library we have available
$!
$   IF F$PARSE("SOCKETSHR:") .NES. ""
$   THEN
$!
$!    We have SOCKETSHR, and it is my opinion that it's the best to use.
$!
$     P3 = "SOCKETSHR"
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
$	P3 = "UCX"
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
$ CCDEFS = "TCPIP_TYPE_''P3'"
$ IF F$TYPE(USER_CCDEFS) .NES. "" THEN CCDEFS = CCDEFS + "," + USER_CCDEFS
$ CCEXTRAFLAGS = ""
$ IF F$TYPE(USER_CCFLAGS) .NES. "" THEN CCEXTRAFLAGS = USER_CCFLAGS
$ CCDISABLEWARNINGS = "" !!! "LONGLONGTYPE,LONGLONGSUFX,FOUNDCR"
$ IF F$TYPE(USER_CCDISABLEWARNINGS) .NES. "" THEN -
	CCDISABLEWARNINGS = CCDISABLEWARNINGS + "," + USER_CCDISABLEWARNINGS
$!
$! Check To See If We Have A ZLIB Option.
$!
$ ZLIB = P6
$ IF (ZLIB .NES. "")
$ THEN
$!
$!  Check for expected ZLIB files.
$!
$   err = 0
$   file1 = f$parse( "zlib.h", ZLIB, , , "SYNTAX_ONLY")
$   if (f$search( file1) .eqs. "")
$   then
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "The Option ", ZLIB, " Is Invalid."
$     WRITE SYS$OUTPUT "    Can't find header: ''file1'"
$     err = 1
$   endif
$   file1 = f$parse( "A.;", ZLIB)- "A.;"
$!
$   file2 = f$parse( ZLIB, "libz.olb", , , "SYNTAX_ONLY")
$   if (f$search( file2) .eqs. "")
$   then
$     if (err .eq. 0)
$     then
$       WRITE SYS$OUTPUT ""
$       WRITE SYS$OUTPUT "The Option ", ZLIB, " Is Invalid."
$     endif
$     WRITE SYS$OUTPUT "    Can't find library: ''file2'"
$     WRITE SYS$OUTPUT ""
$     err = err+ 2
$   endif
$   if (err .eq. 1)
$   then
$     WRITE SYS$OUTPUT ""
$   endif
$!
$   if (err .ne. 0)
$   then
$     GOTO EXIT
$   endif
$!
$   CCDEFS = """ZLIB=1"", "+ CCDEFS
$   CC_INCLUDES = CC_INCLUDES+ ", "+ file1
$   ZLIB_LIB = ", ''file2' /library"
$!
$!  Print info
$!
$   WRITE SYS$OUTPUT "ZLIB library spec: ", file2
$!
$! End The P8 Check.
$!
$ ENDIF
$!
$!  Check To See If The User Entered A Valid Parameter.
$!
$ IF (P2.EQS."VAXC").OR.(P2.EQS."DECC").OR.(P2.EQS."GNUC")
$ THEN
$!
$!  Check To See If The User Wanted DECC.
$!
$   IF (P2.EQS."DECC")
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
	 THEN CC = "CC /DECC"
$     CC = CC + " /''CC_OPTIMIZE' /''DEBUGGER' /STANDARD=RELAXED"+ -
       "''POINTER_SIZE' /NOLIST /PREFIX=ALL" + -
       " /INCLUDE=(''CC_INCLUDES') " + CCEXTRAFLAGS
$!
$!    Define The Linker Options File Name.
$!
$     OPT_FILE = "VAX_DECC_OPTIONS.OPT"
$!
$!  End DECC Check.
$!
$   ENDIF
$!
$!  Check To See If We Are To Use VAXC.
$!
$   IF (P2.EQS."VAXC")
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
$     IF F$TRNLNM("DECC$CC_DEFAULT").EQS."/DECC" THEN CC = "CC /VAXC"
$     CC = CC + "/''CC_OPTIMIZE' /''DEBUGGER' /NOLIST" + -
	   "/INCLUDE=(''CC_INCLUDES')" + CCEXTRAFLAGS
$     CCDEFS = CCDEFS + ",""VAXC"""
$!
$!    Define <sys> As SYS$COMMON:[SYSLIB]
$!
$     DEFINE /NOLOG SYS SYS$COMMON:[SYSLIB]
$!
$!    Define The Linker Options File Name.
$!
$     OPT_FILE = "VAX_VAXC_OPTIONS.OPT"
$!
$!  End VAXC Check
$!
$   ENDIF
$!
$!  Check To See If We Are To Use GNU C.
$!
$   IF (P2.EQS."GNUC")
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
$     CC = "GCC /NOCASE_HACK /''GCC_OPTIMIZE' /''DEBUGGER' /NOLIST" + -
	   "/INCLUDE=(''CC_INCLUDES')" + CCEXTRAFLAGS
$!
$!    Define The Linker Options File Name.
$!
$     OPT_FILE = "VAX_GNUC_OPTIONS.OPT"
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
$     ELSE
$       CC4DISABLEWARNINGS = CCDISABLEWARNINGS + ",DOLLARID"
$       CCDISABLEWARNINGS = " /WARNING=(DISABLE=(" + CCDISABLEWARNINGS + "))"
$     ENDIF
$     CC4DISABLEWARNINGS = " /WARNING=(DISABLE=(" + CC4DISABLEWARNINGS + "))"
$   ELSE
$     CCDISABLEWARNINGS = ""
$     CC4DISABLEWARNINGS = ""
$   ENDIF
$   CC = CC + " /DEFINE=(" + CCDEFS + ")" + CCDISABLEWARNINGS
$!
$!  Show user the result
$!
$   WRITE /SYMBOL SYS$OUTPUT "Main Compiling Command: ", CC
$!
$!  Else The User Entered An Invalid Argument.
$!
$ ELSE
$!
$!  Tell The User We Don't Know What They Want.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "The Option ",P2," Is Invalid.  The Valid Options Are:"
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "    VAXC  :  To Compile With VAX C."
$   WRITE SYS$OUTPUT "    DECC  :  To Compile With DEC C."
$   WRITE SYS$OUTPUT "    GNUC  :  To Compile With GNU C."
$   WRITE SYS$OUTPUT ""
$!
$!  Time To EXIT.
$!
$   EXIT
$ ENDIF
$!
$! Time to check the contents, and to make sure we get the correct library.
$!
$ IF P3.EQS."SOCKETSHR" .OR. P3.EQS."MULTINET" .OR. P3.EQS."UCX" -
     .OR. P3.EQS."TCPIP" .OR. P3.EQS."NONE"
$ THEN
$!
$!  Check to see if SOCKETSHR was chosen
$!
$   IF P3.EQS."SOCKETSHR"
$   THEN
$!
$!    Set the library to use SOCKETSHR
$!
$     TCPIP_LIB = ",SYS$DISK:[-.VMS]SOCKETSHR_SHR.OPT /OPTIONS"
$!
$!    Done with SOCKETSHR
$!
$   ENDIF
$!
$!  Check to see if MULTINET was chosen
$!
$   IF P3.EQS."MULTINET"
$   THEN
$!
$!    Set the library to use UCX emulation.
$!
$     P3 = "UCX"
$!
$!    Done with MULTINET
$!
$   ENDIF
$!
$!  Check to see if UCX was chosen
$!
$   IF P3.EQS."UCX"
$   THEN
$!
$!    Set the library to use UCX.
$!
$     TCPIP_LIB = ",SYS$DISK:[-.VMS]UCX_SHR_DECC.OPT /OPTIONS"
$     IF F$TRNLNM("UCX$IPC_SHR") .NES. ""
$     THEN
$       TCPIP_LIB = ",SYS$DISK:[-.VMS]UCX_SHR_DECC_LOG.OPT /OPTIONS"
$     ELSE
$       IF COMPILER .NES. "DECC" .AND. ARCH .EQS. "VAX" THEN -
	  TCPIP_LIB = ",SYS$DISK:[-.VMS]UCX_SHR_VAXC.OPT /OPTIONS"
$     ENDIF
$!
$!    Done with UCX
$!
$   ENDIF
$!
$!  Check to see if TCPIP was chosen
$!
$   IF P3.EQS."TCPIP"
$   THEN
$!
$!    Set the library to use TCPIP (post UCX).
$!
$     TCPIP_LIB = ",SYS$DISK:[-.VMS]TCPIP_SHR_DECC.OPT /OPTIONS"
$!
$!    Done with TCPIP
$!
$   ENDIF
$!
$!  Check to see if NONE was chosen
$!
$   IF P3.EQS."NONE"
$   THEN
$!
$!    Do not use a TCPIP library.
$!
$     TCPIP_LIB = ""
$!
$!    Done with NONE
$!
$   ENDIF
$!
$!  Print info
$!
$   WRITE SYS$OUTPUT "TCP/IP library spec: ", TCPIP_LIB- ","
$!
$!  Else The User Entered An Invalid Argument.
$!
$ ELSE
$!
$!  Tell The User We Don't Know What They Want.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "The Option ",P3," Is Invalid.  The Valid Options Are:"
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
$! Special Threads For OpenVMS v7.1 Or Later
$!
$! Written By:  Richard Levitte
$!              richard@levitte.org
$!
$!
$! Check To See If We Have A Option For P4.
$!
$ IF (P4.EQS."")
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
$! End The P4 Check.
$!
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
$ __TOP = __HERE - "TEST]"
$ __INCLUDE = __TOP + "INCLUDE.OPENSSL]"
$!
$! Set up the logical name OPENSSL to point at the include directory
$!
$ DEFINE OPENSSL /NOLOG '__INCLUDE'
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
$   DEFINE /NOLOG OPENSSL '__SAVE_OPENSSL'
$ ENDIF
$!
$! Done
$!
$ RETURN
