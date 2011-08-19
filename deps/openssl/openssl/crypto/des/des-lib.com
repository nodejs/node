$!
$!  DES-LIB.COM
$!  Written By:  Robert Byer
$!               Vice-President
$!               A-Com Computing, Inc.
$!               byer@mail.all-net.net
$!
$!  Changes by Richard Levitte <richard@levitte.org>
$!
$!  This command files compiles and creates the 
$!  "[.xxx.EXE.CRYPTO.DES]LIBDES.OLB" library.  The "xxx" denotes the machine 
$!  architecture of ALPHA, IA64 or VAX.
$!
$!  It was re-written to try to determine which "C" compiler to try to use
$!  or the user can specify a compiler in P3.
$!
$!  Specify one of the following to build just that part, specify "ALL" to
$!  just build everything.
$!
$!         ALL       To Just Build "Everything".
$!         LIBRARY   To Just Build The [.xxx.EXE.CRYPTO.DES]LIBDES.OLB Library.
$!         DESTEST   To Just Build The [.xxx.EXE.CRYPTO.DES]DESTEST.EXE Program.
$!         SPEED     To Just Build The [.xxx.EXE.CRYPTO.DES]SPEED.EXE Program.
$!         RPW       To Just Build The [.xxx.EXE.CRYPTO.DES]RPW.EXE Program.
$!         DES       To Just Build The [.xxx.EXE.CRYPTO.DES]DES.EXE Program.
$!         DES_OPTS  To Just Build The [.xxx.EXE.CRYPTO.DES]DES_OPTS.EXE Program.
$!
$!  Specify either DEBUG or NODEBUG as P2 to compile with or without
$!  debugging information.
$!
$!  Specify which compiler at P3 to try to compile under.
$!
$!	   VAXC	 For VAX C.
$!	   DECC	 For DEC C.
$!	   GNUC	 For GNU C.
$!
$!  If you don't speficy a compiler, it will try to determine which
$!  "C" compiler to try to use.
$!
$!  P4, if defined, sets a compiler thread NOT needed on OpenVMS 7.1 (and up)
$!
$!
$! Make sure we know what architecture we run on.
$!
$!
$! Check Which Architecture We Are Using.
$!
$ IF (F$GETSYI("CPU").LT.128)
$ THEN
$!
$!  The Architecture Is VAX
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
$! Define The OBJ Directory Name.
$!
$ OBJ_DIR := SYS$DISK:[--.'ARCH'.OBJ.CRYPTO.DES]
$!
$! Define The EXE Directory Name.
$!
$ EXE_DIR :== SYS$DISK:[--.'ARCH'.EXE.CRYPTO.DES]
$!
$! Check To Make Sure We Have Valid Command Line Parameters.
$!
$ GOSUB CHECK_OPTIONS
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
$   CREATE/DIR 'EXE_DIR'
$!
$! End The Architecture Specific Directory Check.
$!
$ ENDIF
$!
$! Define The Library Name.
$!
$ LIB_NAME := 'EXE_DIR'LIBDES.OLB
$!
$! Check To See What We Are To Do.
$!
$ IF (BUILDALL.EQS."TRUE")
$ THEN
$!
$!  Since Nothing Special Was Specified, Do Everything.
$!
$   GOSUB LIBRARY
$   GOSUB DESTEST
$   GOSUB SPEED
$   GOSUB RPW
$   GOSUB DES
$   GOSUB DES_OPTS
$!
$! Else...
$!
$ ELSE
$!
$!    Build Just What The User Wants Us To Build.
$!
$     GOSUB 'BUILDALL'
$!
$! End The BUILDALL Check.
$!
$ ENDIF
$!
$! Time To EXIT.
$!
$ EXIT
$ LIBRARY:
$!
$! Tell The User That We Are Compiling.
$!
$ WRITE SYS$OUTPUT "Compiling The ",LIB_NAME," Files."
$!
$! Check To See If We Already Have A "[.xxx.EXE.CRYPTO.DES]LIBDES.OLB" Library...
$!
$ IF (F$SEARCH(LIB_NAME).EQS."")
$ THEN
$!
$! Guess Not, Create The Library.
$!
$   LIBRARY/CREATE/OBJECT 'LIB_NAME'
$!
$! End The Library Exist Check.
$!
$ ENDIF
$!
$! Define The DES Library Files.
$!
$ LIB_DES = "set_key,ecb_enc,cbc_enc,"+ -
		"ecb3_enc,cfb64enc,cfb64ede,cfb_enc,ofb64ede,"+ -
		"enc_read,enc_writ,ofb64enc,"+ -
		"ofb_enc,str2key,pcbc_enc,qud_cksm,rand_key,"+ -
		"des_enc,fcrypt_b,read2pwd,"+ -
		"fcrypt,xcbc_enc,read_pwd,rpc_enc,cbc_cksm,supp"
$!
$!  Define A File Counter And Set It To "0".
$!
$ FILE_COUNTER = 0
$!
$! Top Of The File Loop.
$!
$ NEXT_FILE:
$!
$! O.K, Extract The File Name From The File List.
$!
$ FILE_NAME = F$ELEMENT(FILE_COUNTER,",",LIB_DES)
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
$ SOURCE_FILE = "SYS$DISK:[]" + FILE_NAME + ".C"
$!
$!  Tell The User We Are Compiling The Source File.
$!
$ WRITE SYS$OUTPUT "	",FILE_NAME,".C"
$!
$! Create The Object File Name.
$!
$ OBJECT_FILE = OBJ_DIR + FILE_NAME + "." + ARCH + "OBJ"
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
$   EXIT
$!
$! End The File Exists Check.
$!
$ ENDIF
$!
$! Compile The File.
$!
$ ON ERROR THEN GOTO NEXT_FILE
$ CC/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$!
$! Add It To The Library.
$!
$ LIBRARY/REPLACE/OBJECT 'LIB_NAME' 'OBJECT_FILE'
$!
$! Time To Clean Up The Object File.
$!
$ DELETE 'OBJECT_FILE';*
$!
$! Go Back And Do It Again.
$!
$ GOTO NEXT_FILE
$!
$! All Done With This Library Part.
$!
$ FILE_DONE:
$!
$! Tell The User That We Are All Done.
$!
$ WRITE SYS$OUTPUT "Library ",LIB_NAME," Built."
$!
$! All Done, Time To Return.
$!
$ RETURN
$!
$!  Compile The DESTEST Program.
$!
$ DESTEST:
$!
$! Check To See If We Have The Proper Libraries.
$!
$ GOSUB LIB_CHECK
$!
$! Check To See If We Have A Linker Option File.
$!
$ GOSUB CHECK_OPT_FILE
$!
$! Check To See If The File We Want To Compile Actually Exists.
$!
$ IF (F$SEARCH("SYS$DISK:[]DESTEST.C").EQS."")
$ THEN
$!
$!  Tell The User That The File Dosen't Exist.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "The File DESTEST.C Dosen't Exist."
$   WRITE SYS$OUTPUT ""
$!
$!  Exit The Build.
$!
$   EXIT
$!
$! End The DESTEST.C File Check.
$!
$ ENDIF
$!
$! Tell The User What We Are Building.
$!
$ WRITE SYS$OUTPUT "Building ",EXE_DIR,"DESTEST.EXE"
$!
$! Compile The DESTEST Program.
$!
$ CC/OBJECT='OBJ_DIR'DESTEST.OBJ SYS$DISK:[]DESTEST.C
$!
$! Link The DESTEST Program.
$!
$ LINK/'DEBUGGER'/'TRACEBACK'/CONTIGUOUS/EXE='EXE_DIR'DESTEST.EXE -
      'OBJ_DIR'DESTEST.OBJ,'LIB_NAME'/LIBRARY,'OPT_FILE'/OPTION
$!
$! All Done, Time To Return.
$!
$ RETURN
$!
$!  Compile The SPEED Program.
$!
$ SPEED:
$!
$! Check To See If We Have The Proper Libraries.
$!
$ GOSUB LIB_CHECK
$!
$! Check To See If We Have A Linker Option File.
$!
$ GOSUB CHECK_OPT_FILE
$!
$! Check To See If The File We Want To Compile Actually Exists.
$!
$ IF (F$SEARCH("SYS$DISK:[]SPEED.C").EQS."")
$ THEN
$!
$!  Tell The User That The File Dosen't Exist.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "The File SPEED.C Dosen't Exist."
$   WRITE SYS$OUTPUT ""
$!
$!  Exit The Build.
$!
$   EXIT
$!
$! End The SPEED.C File Check.
$!
$ ENDIF
$!
$! Tell The User What We Are Building.
$!
$ WRITE SYS$OUTPUT "Building ",EXE_DIR,"SPEED.EXE"
$!
$! Compile The SPEED Program.
$!
$ CC/OBJECT='OBJ_DIR'SPEED.OBJ SYS$DISK:[]SPEED.C
$!
$! Link The SPEED Program.
$!
$ LINK/'DEBUGGER'/'TRACEBACK'/CONTIGUOUS/EXE='EXE_DIR'SPEED.EXE -
      'OBJ_DIR'SPEED.OBJ,'LIB_NAME'/LIBRARY,'OPT_FILE'/OPTION
$!
$! All Done, Time To Return.
$!
$ RETURN
$!
$!  Compile The RPW Program.
$!
$ RPW:
$!
$! Check To See If We Have The Proper Libraries.
$!
$ GOSUB LIB_CHECK
$!
$! Check To See If We Have A Linker Option File.
$!
$ GOSUB CHECK_OPT_FILE
$!
$! Check To See If The File We Want To Compile Actually Exists.
$!
$ IF (F$SEARCH("SYS$DISK:[]RPW.C").EQS."")
$ THEN
$!
$!  Tell The User That The File Dosen't Exist.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "The File RPW.C Dosen't Exist."
$   WRITE SYS$OUTPUT ""
$!
$!  Exit The Build.
$!
$   EXIT
$!
$! End The RPW.C File Check.
$!
$ ENDIF
$!
$! Tell The User What We Are Building.
$!
$ WRITE SYS$OUTPUT "Building ",EXE_DIR,"RPW.EXE"
$!
$! Compile The RPW Program.
$!
$ CC/OBJECT='OBJ_DIR'RPW.OBJ SYS$DISK:[]RPW.C
$!
$! Link The RPW Program.
$!
$ LINK/'DEBUGGER'/'TRACEBACK'/CONTIGUOUS/EXE='EXE_DIR'RPW.EXE -
      'OBJ_DIR'RPW.OBJ,'LIB_NAME'/LIBRARY,'OPT_FILE'/OPTION
$!
$! All Done, Time To Return.
$!
$ RETURN
$!
$!  Compile The DES Program.
$!
$ DES:
$!
$! Check To See If We Have The Proper Libraries.
$!
$ GOSUB LIB_CHECK
$!
$! Check To See If We Have A Linker Option File.
$!
$ GOSUB CHECK_OPT_FILE
$!
$! Check To See If The File We Want To Compile Actually Exists.
$!
$ IF (F$SEARCH("SYS$DISK:[]DES.C").EQS."")
$ THEN
$!
$!  Tell The User That The File Dosen't Exist.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "The File DES.C Dosen't Exist."
$   WRITE SYS$OUTPUT ""
$!
$!  Exit The Build.
$!
$   EXIT
$!
$! End The DES.C File Check.
$!
$ ENDIF
$!
$! Tell The User What We Are Building.
$!
$ WRITE SYS$OUTPUT "Building ",EXE_DIR,"DES.EXE"
$!
$! Compile The DES Program.
$!
$ CC/OBJECT='OBJ_DIR'DES.OBJ SYS$DISK:[]DES.C
$ CC/OBJECT='OBJ_DIR'DES.OBJ SYS$DISK:[]CBC3_ENC.C
$!
$! Link The DES Program.
$!
$ LINK/'DEBUGGER'/'TRACEBACK'/CONTIGUOUS/EXE='EXE_DIR'DES.EXE -
      'OBJ_DIR'DES.OBJ,'OBJ_DIR'CBC3_ENC.OBJ,-
      'LIB_NAME'/LIBRARY,'OPT_FILE'/OPTION
$!
$! All Done, Time To Return.
$!
$ RETURN
$!
$!  Compile The DES_OPTS Program.
$!
$ DES_OPTS:
$!
$! Check To See If We Have The Proper Libraries.
$!
$ GOSUB LIB_CHECK
$!
$! Check To See If We Have A Linker Option File.
$!
$ GOSUB CHECK_OPT_FILE
$!
$! Check To See If The File We Want To Compile Actually Exists.
$!
$ IF (F$SEARCH("SYS$DISK:[]DES_OPTS.C").EQS."")
$ THEN
$!
$!  Tell The User That The File Dosen't Exist.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "The File DES_OPTS.C Dosen't Exist."
$   WRITE SYS$OUTPUT ""
$!
$!  Exit The Build.
$!
$   EXIT
$!
$! End The DES_OPTS.C File Check.
$!
$ ENDIF
$!
$! Tell The User What We Are Building.
$!
$ WRITE SYS$OUTPUT "Building ",EXE_DIR,"DES_OPTS.EXE"
$!
$! Compile The DES_OPTS Program.
$!
$ CC/OBJECT='OBJ_DIR'DES_OPTS.OBJ SYS$DISK:[]DES_OPTS.C
$!
$! Link The DES_OPTS Program.
$!
$ LINK/'DEBUGGER'/'TRACEBACK'/CONTIGUOUS/EXE='EXE_DIR'DES_OPTS.EXE -
      'OBJ_DIR'DES_OPTS.OBJ,'LIB_NAME'/LIBRARY,'OPT_FILE'/OPTION
$!
$! All Done, Time To Return.
$!
$ RETURN
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
$!    Figure Out If We Need An non-VAX Or A VAX Linker Option File.
$!
$     IF (F$GETSYI("CPU").LT.128)
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
$! Library Check.
$!
$ LIB_CHECK:
$!
$!  Look For The Library LIBDES.OLB.
$!
$ IF (F$SEARCH(LIB_NAME).EQS."")
$ THEN
$!
$!    Tell The User We Can't Find The [.xxx.CRYPTO.DES]LIBDES.OLB Library.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "Can't Find The Library ",LIB_NAME,"."
$   WRITE SYS$OUTPUT "We Can't Link Without It."
$   WRITE SYS$OUTPUT ""
$!
$!    Since We Can't Link Without It, Exit.
$!
$   EXIT
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
$! Check To See If We Are To "Just Build Everything".
$!
$ IF (P1.EQS."ALL")
$ THEN
$!
$!   P1 Is "ALL", So Build Everything.
$!
$    BUILDALL = "TRUE"
$!
$! Else...
$!
$ ELSE
$!
$!  Else, Check To See If P1 Has A Valid Arguement.
$!
$   IF (P1.EQS."LIBRARY").OR.(P1.EQS."DESTEST").OR.(P1.EQS."SPEED") -
       .OR.(P1.EQS."RPW").OR.(P1.EQS."DES").OR.(P1.EQS."DES_OPTS")
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
$     WRITE SYS$OUTPUT "    ALL      :  Just Build Everything.
$     WRITE SYS$OUTPUT "    LIBRARY  :  To Compile Just The [.xxx.EXE.CRYPTO.DES]LIBDES.OLB Library."
$     WRITE SYS$OUTPUT "    DESTEST  :  To Compile Just The [.xxx.EXE.CRYPTO.DES]DESTEST.EXE Program."
$     WRITE SYS$OUTPUT "    SPEED    :  To Compile Just The [.xxx.EXE.CRYPTO.DES]SPEED.EXE Program."
$     WRITE SYS$OUTPUT "    RPW      :  To Compile Just The [.xxx.EXE.CRYPTO.DES]RPW.EXE Program."
$     WRITE SYS$OUTPUT "    DES      :  To Compile Just The [.xxx.EXE.CRYPTO.DES]DES.EXE Program."
$     WRITE SYS$OUTPUT "    DES_OPTS :  To Compile Just The [.xxx.EXE.CRYTPO.DES]DES_OPTS.EXE Program."
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT " Where 'xxx' Stands For: "
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
$! Check To See If We Are To Compile Without Debugger Information.
$!
$ IF (P2.EQS."NODEBUG")
$ THEN
$!
$!   P2 Is Blank, So Compile Without Debugger Information.
$!
$    DEBUGGER  = "NODEBUG"
$    TRACEBACK = "NOTRACEBACK" 
$    GCC_OPTIMIZE = "OPTIMIZE"
$    CC_OPTIMIZE = "OPTIMIZE"
$    WRITE SYS$OUTPUT "No Debugger Information Will Be Produced During Compile."
$    WRITE SYS$OUTPUT "Compiling With Compiler Optimization."
$!
$! Else...
$!
$ ELSE
$!
$!  Check To See If We Are To Compile With Debugger Information.
$!
$   IF (P2.EQS."DEBUG")
$   THEN
$!
$!    Compile With Debugger Information.
$!
$     DEBUGGER  = "DEBUG"
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
$!    Tell The User Entered An Invalid Option..
$!
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "The Option ",P2," Is Invalid.  The Valid Options Are:"
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "    DEBUG    :  Compile With The Debugger Information."
$     WRITE SYS$OUTPUT "    NODEBUG  :  Compile Without The Debugger Information."
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
$! Special Threads For OpenVMS v7.1 Or Later.
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
$   ISSEVEN := ""
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
$! Set Up Initial CC Definitions, Possibly With User Ones
$!
$ CCDEFS = ""
$ IF F$TYPE(USER_CCDEFS) .NES. "" THEN CCDEFS = USER_CCDEFS
$ CCEXTRAFLAGS = ""
$ IF F$TYPE(USER_CCFLAGS) .NES. "" THEN CCEXTRAFLAGS = USER_CCFLAGS
$ CCDISABLEWARNINGS = ""
$ IF F$TYPE(USER_CCDISABLEWARNINGS) .NES. "" THEN -
	CCDISABLEWARNINGS = USER_CCDISABLEWARNINGS
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
           "/NOLIST/PREFIX=ALL" + CCEXTRAFLAGS
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
$     CC = CC + "/''CC_OPTIMIZE'/''DEBUGGER'/NOLIST" + CCEXTRAFLAGS
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
$     CC = "GCC/NOCASE_HACK/''GCC_OPTIMIZE'/''DEBUGGER'/NOLIST" + CCEXTRAFLAGS
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
$     ELSE
$       CC4DISABLEWARNINGS = CCDISABLEWARNINGS + ",DOLLARID"
$       CCDISABLEWARNINGS = "/WARNING=(DISABLE=(" + CCDISABLEWARNINGS + "))"
$     ENDIF
$     CC4DISABLEWARNINGS = "/WARNING=(DISABLE=(" + CC4DISABLEWARNINGS + "))"
$   ELSE
$     CCDISABLEWARNINGS = ""
$     CC4DISABLEWARNINGS = ""
$   ENDIF
$   CC = CC + "/DEFINE=(" + CCDEFS + ")" + CCDISABLEWARNINGS
$!
$!  Show user the result
$!
$   WRITE SYS$OUTPUT "Main Compiling Command: ",CC
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
$! End The P3 Check.
$!
$ ENDIF
$!
$!  Time To RETURN...
$!
$ RETURN
