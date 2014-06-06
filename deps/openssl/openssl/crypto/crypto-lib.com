$!
$!  CRYPTO-LIB.COM
$!  Written By:  Robert Byer
$!               Vice-President
$!               A-Com Computing, Inc.
$!               byer@mail.all-net.net
$!
$!  Changes by Richard Levitte <richard@levitte.org>
$!             Zoltan Arpadffy <arpadffy@polarhome.com>
$!
$!  This command files compiles and creates the "[.xxx.EXE.CRYPTO]LIBCRYPTO.OLB" 
$!  library for OpenSSL.  The "xxx" denotes the machine architecture, ALPHA,
$!  IA64 or VAX.
$!
$!  It was re-written so it would try to determine what "C" compiler to use 
$!  or you can specify which "C" compiler to use.
$!
$!  Specify the following as P1 to build just that part or ALL to just
$!  build everything.
$!
$!    	LIBRARY    To just compile the [.xxx.EXE.CRYPTO]LIBCRYPTO.OLB Library.
$!    	APPS       To just compile the [.xxx.EXE.CRYPTO]*.EXE
$!	ALL	   To do both LIBRARY and APPS
$!
$!  Specify DEBUG or NODEBUG as P2 to compile with or without debugger
$!  information.
$!
$!  Specify which compiler at P3 to try to compile under.
$!
$!	VAXC	   For VAX C.
$!	DECC	   For DEC C.
$!	GNUC	   For GNU C.
$!
$!  If you don't specify a compiler, it will try to determine which
$!  "C" compiler to use.
$!
$!  P4, if defined, sets a TCP/IP library to use, through one of the following
$!  keywords:
$!
$!	UCX	   For UCX
$!	TCPIP	   For TCPIP (post UCX)
$!	SOCKETSHR  For SOCKETSHR+NETLIB
$!
$!  P5, if defined, sets a compiler thread NOT needed on OpenVMS 7.1 (and up)
$!
$!  P6, if defined, sets a choice of crypto methods to compile.
$!  WARNING: this should only be done to recompile some part of an already
$!  fully compiled library.
$!
$!  P7, if defined, specifies the C pointer size.  Ignored on VAX.
$!      ("64=ARGV" gives more efficient code with HP C V7.3 or newer.)
$!      Supported values are:
$!
$!      ""       Compile with default (/NOPOINTER_SIZE)
$!      32       Compile with /POINTER_SIZE=32 (SHORT)
$!      64       Compile with /POINTER_SIZE=64[=ARGV] (LONG[=ARGV]).
$!               (Automatically select ARGV if compiler supports it.)
$!      64=      Compile with /POINTER_SIZE=64 (LONG).
$!      64=ARGV  Compile with /POINTER_SIZE=64=ARGV (LONG=ARGV).
$!
$!  P8, if defined, specifies a directory where ZLIB files (zlib.h,
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
$! (That Is, If We Need To Link To One.)
$!
$ TCPIP_LIB = ""
$ ZLIB_LIB = ""
$!
$! Check Which Architecture We Are Using.
$!
$ IF (F$GETSYI("CPU").LT.128)
$ THEN
$!
$!  The Architecture Is VAX
$!
$   ARCH = "VAX"
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
$ ARCHD = ARCH
$ LIB32 = "32"
$ OPT_FILE = ""
$ POINTER_SIZE = ""
$!
$! Define The Different Encryption Types.
$! NOTE: Some might think this list ugly.  However, it's made this way to
$! reflect the SDIRS variable in [-]Makefile.org as closely as possible,
$! thereby making it fairly easy to verify that the lists are the same.
$!
$ ET_WHIRLPOOL = "WHRLPOOL"
$ IF ARCH .EQS. "VAX" THEN ET_WHIRLPOOL = ""
$ ENCRYPT_TYPES = "Basic,"+ -
		  "OBJECTS,"+ -
		  "MD2,MD4,MD5,SHA,MDC2,HMAC,RIPEMD,"+ET_WHIRLPOOL+","+ -
		  "DES,AES,RC2,RC4,RC5,IDEA,BF,CAST,CAMELLIA,SEED,MODES,"+ -
		  "BN,EC,RSA,DSA,ECDSA,DH,ECDH,DSO,ENGINE,"+ -
		  "BUFFER,BIO,STACK,LHASH,RAND,ERR,"+ -
		  "EVP,EVP_2,EVP_3,ASN1,ASN1_2,PEM,X509,X509V3,"+ -
		  "CONF,TXT_DB,PKCS7,PKCS12,COMP,OCSP,UI,KRB5,"+ -
		  "CMS,PQUEUE,TS,JPAKE,STORE"
$!
$! Check To Make Sure We Have Valid Command Line Parameters.
$!
$ GOSUB CHECK_OPTIONS
$!
$! Define The OBJ and EXE Directories.
$!
$ OBJ_DIR := SYS$DISK:[-.'ARCHD'.OBJ.CRYPTO]
$ EXE_DIR := SYS$DISK:[-.'ARCHD'.EXE.CRYPTO]
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
$ LIB_NAME := 'EXE_DIR'SSL_LIBCRYPTO'LIB32'.OLB
$!
$! Define The CRYPTO-LIB We Are To Use.
$!
$ CRYPTO_LIB := 'EXE_DIR'SSL_LIBCRYPTO'LIB32'.OLB
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
$ APPS_DES = "DES/DES,CBC3_ENC"
$ APPS_PKCS7 = "ENC/ENC;DEC/DEC;SIGN/SIGN;VERIFY/VERIFY,EXAMPLE"
$
$ LIB_ = "cryptlib,mem,mem_clr,mem_dbg,cversion,ex_data,cpt_err,ebcdic,uid,o_time,o_str,o_dir"
$ LIB_MD2 = "md2_dgst,md2_one"
$ LIB_MD4 = "md4_dgst,md4_one"
$ LIB_MD5 = "md5_dgst,md5_one"
$ LIB_SHA = "sha_dgst,sha1dgst,sha_one,sha1_one,sha256,sha512"
$ LIB_MDC2 = "mdc2dgst,mdc2_one"
$ LIB_HMAC = "hmac,hm_ameth,hm_pmeth"
$ LIB_RIPEMD = "rmd_dgst,rmd_one"
$ LIB_WHRLPOOL = "wp_dgst,wp_block"
$ LIB_DES = "set_key,ecb_enc,cbc_enc,"+ -
	"ecb3_enc,cfb64enc,cfb64ede,cfb_enc,ofb64ede,"+ -
	"enc_read,enc_writ,ofb64enc,"+ -
	"ofb_enc,str2key,pcbc_enc,qud_cksm,rand_key,"+ -
	"des_enc,fcrypt_b,"+ -
	"fcrypt,xcbc_enc,rpc_enc,cbc_cksm,"+ -
	"ede_cbcm_enc,des_old,des_old2,read2pwd"
$ LIB_RC2 = "rc2_ecb,rc2_skey,rc2_cbc,rc2cfb64,rc2ofb64"
$ LIB_RC4 = "rc4_skey,rc4_enc"
$ LIB_RC5 = "rc5_skey,rc5_ecb,rc5_enc,rc5cfb64,rc5ofb64"
$ LIB_IDEA = "i_cbc,i_cfb64,i_ofb64,i_ecb,i_skey"
$ LIB_BF = "bf_skey,bf_ecb,bf_enc,bf_cfb64,bf_ofb64"
$ LIB_CAST = "c_skey,c_ecb,c_enc,c_cfb64,c_ofb64"
$ LIB_CAMELLIA = "camellia,cmll_misc,cmll_ecb,cmll_cbc,cmll_ofb,"+ -
	"cmll_cfb,cmll_ctr"
$ LIB_SEED = "seed,seed_ecb,seed_cbc,seed_cfb,seed_ofb"
$ LIB_MODES = "cbc128,ctr128,cts128,cfb128,ofb128"
$ LIB_BN_ASM = "[.asm]vms.mar,vms-helper"
$ IF F$TRNLNM("OPENSSL_NO_ASM") .OR. ARCH .NES. "VAX" THEN -
     LIB_BN_ASM = "bn_asm"
$ LIB_BN = "bn_add,bn_div,bn_exp,bn_lib,bn_ctx,bn_mul,bn_mod,"+ -
	"bn_print,bn_rand,bn_shift,bn_word,bn_blind,"+ -
	"bn_kron,bn_sqrt,bn_gcd,bn_prime,bn_err,bn_sqr,"+LIB_BN_ASM+","+ -
	"bn_recp,bn_mont,bn_mpi,bn_exp2,bn_gf2m,bn_nist,"+ -
	"bn_depr,bn_const"
$ LIB_EC = "ec_lib,ecp_smpl,ecp_mont,ecp_nist,ec_cvt,ec_mult,"+ -
	"ec_err,ec_curve,ec_check,ec_print,ec_asn1,ec_key,"+ -
	"ec2_smpl,ec2_mult,ec_ameth,ec_pmeth,eck_prn"
$ LIB_RSA = "rsa_eay,rsa_gen,rsa_lib,rsa_sign,rsa_saos,rsa_err,"+ -
	"rsa_pk1,rsa_ssl,rsa_none,rsa_oaep,rsa_chk,rsa_null,"+ -
	"rsa_pss,rsa_x931,rsa_asn1,rsa_depr,rsa_ameth,rsa_prn,"+ -
	"rsa_pmeth"
$ LIB_DSA = "dsa_gen,dsa_key,dsa_lib,dsa_asn1,dsa_vrf,dsa_sign,"+ -
	"dsa_err,dsa_ossl,dsa_depr,dsa_ameth,dsa_pmeth,dsa_prn"
$ LIB_ECDSA = "ecs_lib,ecs_asn1,ecs_ossl,ecs_sign,ecs_vrf,ecs_err"
$ LIB_DH = "dh_asn1,dh_gen,dh_key,dh_lib,dh_check,dh_err,dh_depr,"+ -
	"dh_ameth,dh_pmeth,dh_prn"
$ LIB_ECDH = "ech_lib,ech_ossl,ech_key,ech_err"
$ LIB_DSO = "dso_dl,dso_dlfcn,dso_err,dso_lib,dso_null,"+ -
	"dso_openssl,dso_win32,dso_vms,dso_beos"
$ LIB_ENGINE = "eng_err,eng_lib,eng_list,eng_init,eng_ctrl,"+ -
	"eng_table,eng_pkey,eng_fat,eng_all,"+ -
	"tb_rsa,tb_dsa,tb_ecdsa,tb_dh,tb_ecdh,tb_rand,tb_store,"+ -
	"tb_cipher,tb_digest,tb_pkmeth,tb_asnmth,"+ -
	"eng_openssl,eng_dyn,eng_cnf,eng_cryptodev"
$ LIB_AES = "aes_core,aes_misc,aes_ecb,aes_cbc,aes_cfb,aes_ofb,aes_ctr,"+ -
	"aes_ige,aes_wrap"
$ LIB_BUFFER = "buffer,buf_err"
$ LIB_BIO = "bio_lib,bio_cb,bio_err,"+ -
	"bss_mem,bss_null,bss_fd,"+ -
	"bss_file,bss_sock,bss_conn,"+ -
	"bf_null,bf_buff,b_print,b_dump,"+ -
	"b_sock,bss_acpt,bf_nbio,bss_rtcp,bss_bio,bss_log,"+ -
	"bss_dgram,"+ -
	"bf_lbuf"
$ LIB_STACK = "stack"
$ LIB_LHASH = "lhash,lh_stats"
$ LIB_RAND = "md_rand,randfile,rand_lib,rand_err,rand_egd,"+ -
	"rand_vms"
$ LIB_ERR = "err,err_all,err_prn"
$ LIB_OBJECTS = "o_names,obj_dat,obj_lib,obj_err,obj_xref"
$ LIB_EVP = "encode,digest,evp_enc,evp_key,evp_acnf,"+ -
	"e_des,e_bf,e_idea,e_des3,e_camellia,"+ -
	"e_rc4,e_aes,names,e_seed,"+ -
	"e_xcbc_d,e_rc2,e_cast,e_rc5"
$ LIB_EVP_2 = "m_null,m_md2,m_md4,m_md5,m_sha,m_sha1,m_wp," + -
	"m_dss,m_dss1,m_mdc2,m_ripemd,m_ecdsa,"+ -
	"p_open,p_seal,p_sign,p_verify,p_lib,p_enc,p_dec,"+ -
	"bio_md,bio_b64,bio_enc,evp_err,e_null,"+ -
	"c_all,c_allc,c_alld,evp_lib,bio_ok,"+-
	"evp_pkey,evp_pbe,p5_crpt,p5_crpt2"
$ LIB_EVP_3 = "e_old,pmeth_lib,pmeth_fn,pmeth_gn,m_sigver"
$ LIB_ASN1 = "a_object,a_bitstr,a_utctm,a_gentm,a_time,a_int,a_octet,"+ -
	"a_print,a_type,a_set,a_dup,a_d2i_fp,a_i2d_fp,"+ -
	"a_enum,a_utf8,a_sign,a_digest,a_verify,a_mbstr,a_strex,"+ -
	"x_algor,x_val,x_pubkey,x_sig,x_req,x_attrib,x_bignum,"+ -
	"x_long,x_name,x_x509,x_x509a,x_crl,x_info,x_spki,nsseq,"+ -
	"x_nx509,d2i_pu,d2i_pr,i2d_pu,i2d_pr"
$ LIB_ASN1_2 = "t_req,t_x509,t_x509a,t_crl,t_pkey,t_spki,t_bitst,"+ -
	"tasn_new,tasn_fre,tasn_enc,tasn_dec,tasn_utl,tasn_typ,"+ -
	"tasn_prn,ameth_lib,"+ -
	"f_int,f_string,n_pkey,"+ -
	"f_enum,x_pkey,a_bool,x_exten,bio_asn1,bio_ndef,asn_mime,"+ -
	"asn1_gen,asn1_par,asn1_lib,asn1_err,a_bytes,a_strnid,"+ -
	"evp_asn1,asn_pack,p5_pbe,p5_pbev2,p8_pkey,asn_moid"
$ LIB_PEM = "pem_sign,pem_seal,pem_info,pem_lib,pem_all,pem_err,"+ -
	"pem_x509,pem_xaux,pem_oth,pem_pk8,pem_pkey,pvkfmt"
$ LIB_X509 = "x509_def,x509_d2,x509_r2x,x509_cmp,"+ -
	"x509_obj,x509_req,x509spki,x509_vfy,"+ -
	"x509_set,x509cset,x509rset,x509_err,"+ -
	"x509name,x509_v3,x509_ext,x509_att,"+ -
	"x509type,x509_lu,x_all,x509_txt,"+ -
	"x509_trs,by_file,by_dir,x509_vpm"
$ LIB_X509V3 = "v3_bcons,v3_bitst,v3_conf,v3_extku,v3_ia5,v3_lib,"+ -
	"v3_prn,v3_utl,v3err,v3_genn,v3_alt,v3_skey,v3_akey,v3_pku,"+ -
	"v3_int,v3_enum,v3_sxnet,v3_cpols,v3_crld,v3_purp,v3_info,"+ -
	"v3_ocsp,v3_akeya,v3_pmaps,v3_pcons,v3_ncons,v3_pcia,v3_pci,"+ -
	"pcy_cache,pcy_node,pcy_data,pcy_map,pcy_tree,pcy_lib,"+ -
	"v3_asid,v3_addr"
$ LIB_CONF = "conf_err,conf_lib,conf_api,conf_def,conf_mod,conf_mall,conf_sap"
$ LIB_TXT_DB = "txt_db"
$ LIB_PKCS7 = "pk7_asn1,pk7_lib,pkcs7err,pk7_doit,pk7_smime,pk7_attr,"+ -
	"pk7_mime,bio_pk7"
$ LIB_PKCS12 = "p12_add,p12_asn,p12_attr,p12_crpt,p12_crt,p12_decr,"+ -
	"p12_init,p12_key,p12_kiss,p12_mutl,"+ -
	"p12_utl,p12_npas,pk12err,p12_p8d,p12_p8e"
$ LIB_COMP = "comp_lib,comp_err,"+ -
	"c_rle,c_zlib"
$ LIB_OCSP = "ocsp_asn,ocsp_ext,ocsp_ht,ocsp_lib,ocsp_cl,"+ -
	"ocsp_srv,ocsp_prn,ocsp_vfy,ocsp_err"
$ LIB_UI_COMPAT = ",ui_compat"
$ LIB_UI = "ui_err,ui_lib,ui_openssl,ui_util"+LIB_UI_COMPAT
$ LIB_KRB5 = "krb5_asn"
$ LIB_STORE = "str_err,str_lib,str_meth,str_mem"
$ LIB_CMS = "cms_lib,cms_asn1,cms_att,cms_io,cms_smime,cms_err,"+ -
	"cms_sd,cms_dd,cms_cd,cms_env,cms_enc,cms_ess"
$ LIB_PQUEUE = "pqueue"
$ LIB_TS = "ts_err,ts_req_utils,ts_req_print,ts_rsp_utils,ts_rsp_print,"+ -
	"ts_rsp_sign,ts_rsp_verify,ts_verify_ctx,ts_lib,ts_conf,"+ -
	"ts_asn1"
$ LIB_JPAKE = "jpake,jpake_err"
$!
$! Setup exceptional compilations
$!
$ CC3_SHOWN = 0
$ CC4_SHOWN = 0
$ CC5_SHOWN = 0
$ CC6_SHOWN = 0
$!
$! The following lists must have leading and trailing commas, and no
$! embedded spaces.  (They are scanned for ",name,".)
$!
$ ! Add definitions for no threads on OpenVMS 7.1 and higher.
$ COMPILEWITH_CC3 = ",bss_rtcp,"
$ ! Disable the DOLLARID warning.  Not needed with /STANDARD=RELAXED.
$ COMPILEWITH_CC4 = "" !!! ",a_utctm,bss_log,o_time,o_dir,"
$ ! Disable disjoint optimization on VAX with DECC.
$ COMPILEWITH_CC5 = ",md2_dgst,md4_dgst,md5_dgst,mdc2dgst," + -
                    "seed,sha_dgst,sha1dgst,rmd_dgst,bf_enc,"
$ ! Disable the MIXLINKAGE warning.
$ COMPILEWITH_CC6 = "" !!! ",enc_read,set_key,"
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
$ IF (F$EXTRACT(0,5,MODULE_NAME).EQS."ASN1_")
$ THEN
$   MODULE_NAME = "ASN1"
$ ENDIF
$ IF (F$EXTRACT(0,4,MODULE_NAME).EQS."EVP_")
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
$   WRITE SYS$ERROR "The module ",MODULE_NAME1," does not exist.  Continuing..."
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
$   WRITE SYS$OUTPUT "        ",FILE_NAME,""
$ ENDIF
$!
$! Compile The File.
$!
$ ON ERROR THEN GOTO NEXT_FILE
$ FILE_NAME0 = ","+ F$ELEMENT(0,".",FILE_NAME)+ ","
$ IF FILE_NAME - ".mar" .NES. FILE_NAME
$ THEN
$   MACRO/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$ ELSE
$   IF COMPILEWITH_CC3 - FILE_NAME0 .NES. COMPILEWITH_CC3
$   THEN
$     write sys$output "        \Using special rule (3)"
$     if (.not. CC3_SHOWN)
$     then
$       CC3_SHOWN = 1
$       x = "    "+ CC3
$       write /symbol sys$output x
$     endif
$     CC3/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$   ELSE
$     IF COMPILEWITH_CC4 - FILE_NAME0 .NES. COMPILEWITH_CC4
$     THEN
$       write /symbol sys$output "        \Using special rule (4)"
$       if (.not. CC4_SHOWN)
$       then
$         CC4_SHOWN = 1
$         x = "    "+ CC4
$         write /symbol sys$output x
$       endif
$       CC4/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$     ELSE
$       IF CC5_DIFFERENT .AND. -
         (COMPILEWITH_CC5 - FILE_NAME0 .NES. COMPILEWITH_CC5)
$       THEN
$         write sys$output "        \Using special rule (5)"
$         if (.not. CC5_SHOWN)
$         then
$           CC5_SHOWN = 1
$           x = "    "+ CC5
$           write /symbol sys$output x
$         endif
$         CC5/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$       ELSE
$         IF COMPILEWITH_CC6 - FILE_NAME0 .NES. COMPILEWITH_CC6
$         THEN
$           write sys$output "        \Using special rule (6)"
$           if (.not. CC6_SHOWN)
$           then
$             CC6_SHOWN = 1
$             x = "    "+ CC6
$             write /symbol sys$output x
$           endif
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
$   WRITE SYS$OUTPUT "        ",APPLICATION,".exe"
$!
$! Link The Program.
$!
$   ON ERROR THEN GOTO NEXT_APPLICATION
$!
$!  Link With A TCP/IP Library.
$!
$   LINK /'DEBUGGER' /'LINKMAP' /'TRACEBACK' -
     /EXE='EXE_DIR''APPLICATION'.EXE -
     'OBJ_DIR''APPLICATION_OBJECTS', -
     'CRYPTO_LIB'/LIBRARY -
     'TCPIP_LIB' -
     'ZLIB_LIB' -
     ,'OPT_FILE' /OPTIONS
$!
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
! Default System Options File To Link Against 
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
! Default System Options File To Link Against 
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
! Default System Options File To Link Against 
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
! Default System Options File For non-VAX To Link Against 
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
$!  Else, Check To See If P1 Has A Valid Argument.
$!
$   IF (P1.EQS."LIBRARY").OR.(P1.EQS."APPS")
$   THEN
$!
$!    A Valid Argument.
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
$     WRITE SYS$OUTPUT "    ALPHA[64]:  Alpha Architecture."
$     WRITE SYS$OUTPUT "    IA64[64] :  IA64 Architecture."
$     WRITE SYS$OUTPUT "    VAX      :  VAX Architecture."
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
$! Check To See If P2 Is Blank.
$!
$ IF (P2.EQS."NODEBUG")
$ THEN
$!
$!  P2 Is NODEBUG, So Compile Without The Debugger Information.
$!
$   DEBUGGER = "NODEBUG"
$   LINKMAP = "NOMAP"
$   TRACEBACK = "NOTRACEBACK" 
$   GCC_OPTIMIZE = "OPTIMIZE"
$   CC_OPTIMIZE = "OPTIMIZE"
$   MACRO_OPTIMIZE = "OPTIMIZE"
$   WRITE SYS$OUTPUT "No Debugger Information Will Be Produced During Compile."
$   WRITE SYS$OUTPUT "Compiling With Compiler Optimization."
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
$     LINKMAP = "MAP"
$     TRACEBACK = "TRACEBACK"
$     GCC_OPTIMIZE = "NOOPTIMIZE"
$     CC_OPTIMIZE = "NOOPTIMIZE"
$     MACRO_OPTIMIZE = "NOOPTIMIZE"
$     WRITE SYS$OUTPUT "Debugger Information Will Be Produced During Compile."
$     WRITE SYS$OUTPUT "Compiling Without Compiler Optimization."
$   ELSE 
$!
$!    They Entered An Invalid Option.
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
$!  End The Valid Argument Check.
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
$! Check P7 (POINTER_SIZE).
$!
$ IF (P7 .NES. "") .AND. (ARCH .NES. "VAX")
$ THEN
$!
$   IF (P7 .EQS. "32")
$   THEN
$     POINTER_SIZE = " /POINTER_SIZE=32"
$   ELSE
$     POINTER_SIZE = F$EDIT( P7, "COLLAPSE, UPCASE")
$     IF ((POINTER_SIZE .EQS. "64") .OR. -
       (POINTER_SIZE .EQS. "64=") .OR. -
       (POINTER_SIZE .EQS. "64=ARGV"))
$     THEN
$       ARCHD = ARCH+ "_64"
$       LIB32 = ""
$       POINTER_SIZE = " /POINTER_SIZE=64"
$     ELSE
$!
$!      Tell The User Entered An Invalid Option.
$!
$       WRITE SYS$OUTPUT ""
$       WRITE SYS$OUTPUT "The Option ", P7, -
         " Is Invalid.  The Valid Options Are:"
$       WRITE SYS$OUTPUT ""
$       WRITE SYS$OUTPUT -
         "    """"       :  Compile with default (short) pointers."
$       WRITE SYS$OUTPUT -
         "    32       :  Compile with 32-bit (short) pointers."
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
$! End The P7 (POINTER_SIZE) Check.
$!
$ ENDIF
$!
$! Set basic C compiler /INCLUDE directories.
$!
$ CC_INCLUDES = "SYS$DISK:[.''ARCHD'],SYS$DISK:[],SYS$DISK:[-],"+ -
   "SYS$DISK:[.ENGINE.VENDOR_DEFNS],SYS$DISK:[.ASN1],SYS$DISK:[.EVP]"
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
$ CCDISABLEWARNINGS = "" !!! "LONGLONGTYPE,LONGLONGSUFX,FOUNDCR"
$ IF F$TYPE(USER_CCDISABLEWARNINGS) .NES. "" THEN -
	CCDISABLEWARNINGS = CCDISABLEWARNINGS + "," + USER_CCDISABLEWARNINGS
$!
$! Check To See If We Have A ZLIB Option.
$!
$ ZLIB = P8
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
$     EXIT
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
$! End The ZLIB Check.
$!
$ ENDIF
$!
$!  Check To See If The User Entered A Valid Parameter.
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
$     CC = CC + " /''CC_OPTIMIZE' /''DEBUGGER' /STANDARD=RELAXED"+ -
       "''POINTER_SIZE' /NOLIST /PREFIX=ALL" + -
       " /INCLUDE=(''CC_INCLUDES')"+ -
       CCEXTRAFLAGS
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
       "/INCLUDE=(''CC_INCLUDES')"+ -
	   CCEXTRAFLAGS
$     CCDEFS = """VAXC""," + CCDEFS
$!
$!    Define <sys> As SYS$COMMON:[SYSLIB]
$!
$     DEFINE/NOLOG SYS SYS$COMMON:[SYSLIB]
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
       "/INCLUDE=(''CC_INCLUDES')"+ -
	   CCEXTRAFLAGS
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
$       CC6DISABLEWARNINGS = "MIXLINKAGE"
$     ELSE
$       CC4DISABLEWARNINGS = CCDISABLEWARNINGS + ",DOLLARID"
$       CC6DISABLEWARNINGS = CCDISABLEWARNINGS + ",MIXLINKAGE"
$       CCDISABLEWARNINGS = " /WARNING=(DISABLE=(" + CCDISABLEWARNINGS + "))"
$     ENDIF
$     CC4DISABLEWARNINGS = " /WARNING=(DISABLE=(" + CC4DISABLEWARNINGS + "))"
$     CC6DISABLEWARNINGS = " /WARNING=(DISABLE=(" + CC6DISABLEWARNINGS + "))"
$   ELSE
$     CCDISABLEWARNINGS = ""
$     CC4DISABLEWARNINGS = ""
$     CC6DISABLEWARNINGS = ""
$   ENDIF
$   CC3 = CC + " /DEFINE=(" + CCDEFS + ISSEVEN + ")" + CCDISABLEWARNINGS
$   CC = CC + " /DEFINE=(" + CCDEFS + ")" + CCDISABLEWARNINGS
$   IF ARCH .EQS. "VAX" .AND. COMPILER .EQS. "DECC" .AND. P2 .NES. "DEBUG"
$   THEN
$     CC5 = CC + " /OPTIMIZE=NODISJOINT"
$     CC5_DIFFERENT = 1
$   ELSE
$     CC5 = CC
$     CC5_DIFFERENT = 0
$   ENDIF
$   CC4 = CC - CCDISABLEWARNINGS + CC4DISABLEWARNINGS
$   CC6 = CC - CCDISABLEWARNINGS + CC6DISABLEWARNINGS
$!
$!  Show user the result
$!
$   WRITE/SYMBOL SYS$OUTPUT "Main C Compiling Command: ",CC
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
$   WRITE SYS$OUTPUT "    VAXC  :  To Compile With VAX C."
$   WRITE SYS$OUTPUT "    DECC  :  To Compile With DEC C."
$   WRITE SYS$OUTPUT "    GNUC  :  To Compile With GNU C."
$   WRITE SYS$OUTPUT ""
$!
$!  Time To EXIT.
$!
$   EXIT
$!
$! End The Valid Argument Check.
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
$     TCPIP_LIB = ",SYS$DISK:[-.VMS]SOCKETSHR_SHR.OPT /OPTIONS"
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
$   IF P4.EQS."TCPIP"
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
$   WRITE SYS$OUTPUT "TCP/IP library spec: ", TCPIP_LIB- ","
$!
$!  Else The User Entered An Invalid Argument.
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
$ __TOP = __HERE - "CRYPTO]"
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
