$! INSTALL.COM -- Installs the files in a given directory tree
$!
$! Author: Richard Levitte <richard@levitte.org>
$! Time of creation: 22-MAY-1998 10:13
$!
$! Changes by Zoltan Arpadffy <zoli@polarhome.com>
$!
$! P1  root of the directory tree
$! P2  "64" for 64-bit pointers.
$!
$!
$! Announce/identify.
$!
$ proc = f$environment( "procedure")
$ write sys$output "@@@ "+ -
   f$parse( proc, , , "name")+ f$parse( proc, , , "type")
$!
$ on error then goto tidy
$ on control_c then goto tidy
$!
$ if (p1 .eqs. "")
$ then
$   write sys$output "First argument missing."
$   write sys$output -
     "It should be the directory where you want things installed."
$     exit
$ endif
$!
$ if (f$getsyi( "cpu") .lt. 128)
$ then
$   arch = "VAX"
$ else
$   arch = f$edit( f$getsyi( "arch_name"), "upcase")
$   if (arch .eqs. "") then arch = "UNK"
$ endif
$!
$ archd = arch
$ lib32 = "32"
$ shr = "_SHR32"
$!
$ if (p2 .nes. "")
$ then
$   if (p2 .eqs. "64")
$   then
$     archd = arch+ "_64"
$     lib32 = ""
$     shr = "_SHR"
$   else
$     if (p2 .nes. "32")
$     then
$       write sys$output "Second argument invalid."
$       write sys$output "It should be "32", "64", or nothing."
$       exit
$     endif
$   endif
$ endif
$!
$ root = f$parse( p1, "[]A.;0", , , "syntax_only, no_conceal") - "A.;0"
$ root_dev = f$parse( root, , , "device", "syntax_only")
$ root_dir = f$parse( root, , , "directory", "syntax_only") - -
   "[000000." - "][" - "[" - "]"
$ root = root_dev + "[" + root_dir
$!
$ define /nolog wrk_sslroot 'root'.] /trans=conc
$ define /nolog wrk_sslinclude wrk_sslroot:[include]
$ define /nolog wrk_sslxlib wrk_sslroot:['arch'_lib]
$!
$ if f$parse("wrk_sslroot:[000000]") .eqs. "" then -
   create /directory /log wrk_sslroot:[000000]
$ if f$parse("wrk_sslinclude:") .eqs. "" then -
   create /directory /log wrk_sslinclude:
$ if f$parse("wrk_sslxlib:") .eqs. "" then -
   create /directory /log wrk_sslxlib:
$!
$ sdirs := , -
   'archd', -
   objects, -
   md2, md4, md5, sha, mdc2, hmac, ripemd, whrlpool, -
   des, aes, rc2, rc4, rc5, idea, bf, cast, camellia, seed, -
   bn, ec, rsa, dsa, ecdsa, dh, ecdh, dso, engine, -
   buffer, bio, stack, lhash, rand, err, -
   evp, asn1, pem, x509, x509v3, conf, txt_db, pkcs7, pkcs12, comp, ocsp, -
   ui, krb5, -
   store, cms, pqueue, ts, jpake
$!
$ exheader_ := crypto.h, opensslv.h, ebcdic.h, symhacks.h, ossl_typ.h
$ exheader_'archd' := opensslconf.h
$ exheader_objects := objects.h, obj_mac.h
$ exheader_md2 := md2.h
$ exheader_md4 := md4.h
$ exheader_md5 := md5.h
$ exheader_sha := sha.h
$ exheader_mdc2 := mdc2.h
$ exheader_hmac := hmac.h
$ exheader_ripemd := ripemd.h
$ exheader_whrlpool := whrlpool.h
$ exheader_des := des.h, des_old.h
$ exheader_aes := aes.h
$ exheader_rc2 := rc2.h
$ exheader_rc4 := rc4.h
$ exheader_rc5 := rc5.h
$ exheader_idea := idea.h
$ exheader_bf := blowfish.h
$ exheader_cast := cast.h
$ exheader_camellia := camellia.h
$ exheader_seed := seed.h
$ exheader_modes := modes.h
$ exheader_bn := bn.h
$ exheader_ec := ec.h
$ exheader_rsa := rsa.h
$ exheader_dsa := dsa.h
$ exheader_ecdsa := ecdsa.h
$ exheader_dh := dh.h
$ exheader_ecdh := ecdh.h
$ exheader_dso := dso.h
$ exheader_engine := engine.h
$ exheader_buffer := buffer.h
$ exheader_bio := bio.h
$ exheader_stack := stack.h, safestack.h
$ exheader_lhash := lhash.h
$ exheader_rand := rand.h
$ exheader_err := err.h
$ exheader_evp := evp.h
$ exheader_asn1 := asn1.h, asn1_mac.h, asn1t.h
$ exheader_pem := pem.h, pem2.h
$ exheader_x509 := x509.h, x509_vfy.h
$ exheader_x509v3 := x509v3.h
$ exheader_conf := conf.h, conf_api.h
$ exheader_txt_db := txt_db.h
$ exheader_pkcs7 := pkcs7.h
$ exheader_pkcs12 := pkcs12.h
$ exheader_comp := comp.h
$ exheader_ocsp := ocsp.h
$ exheader_ui := ui.h, ui_compat.h
$ exheader_krb5 := krb5_asn.h
$! exheader_store := store.h, str_compat.h
$ exheader_store := store.h
$ exheader_cms := cms.h
$ exheader_pqueue := pqueue.h
$ exheader_ts := ts.h
$ exheader_jpake := jpake.h
$ libs := ssl_libcrypto
$!
$ exe_dir := [-.'archd'.exe.crypto]
$!
$! Header files.
$!
$ i = 0
$ loop_sdirs: 
$   d = f$edit( f$element( i, ",", sdirs), "trim")
$   i = i + 1
$   if d .eqs. "," then goto loop_sdirs_end
$   tmp = exheader_'d'
$   if (d .nes. "") then d = "."+ d
$   copy /protection = w:re ['d']'tmp' wrk_sslinclude: /log
$ goto loop_sdirs
$ loop_sdirs_end:
$!
$! Object libraries, shareable images.
$!
$ i = 0
$ loop_lib: 
$   e = f$edit( f$element( i, ",", libs), "trim")
$   i = i + 1
$   if e .eqs. "," then goto loop_lib_end
$   set noon
$   file = exe_dir+ e+ lib32+ ".olb"
$   if f$search( file) .nes. ""
$   then
$     copy /protection = w:re 'file' wrk_sslxlib: /log
$   endif
$!
$   file = exe_dir+ e+ shr+ ".exe"
$   if f$search( file) .nes. ""
$   then
$     copy /protection = w:re 'file' wrk_sslxlib: /log
$   endif
$   set on
$ goto loop_lib
$ loop_lib_end:
$!
$ tidy:
$!
$ call deass wrk_sslroot
$ call deass wrk_sslinclude
$ call deass wrk_sslxlib
$!
$ exit
$!
$ deass: subroutine
$ if (f$trnlnm( p1, "LNM$PROCESS") .nes. "")
$ then
$   deassign /process 'p1'
$ endif
$ endsubroutine
$!
