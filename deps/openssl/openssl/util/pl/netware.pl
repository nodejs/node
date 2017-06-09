# Metrowerks Codewarrior or gcc / nlmconv for NetWare
#

$version_header = "crypto/opensslv.h";
open(IN, "$version_header") or die "Couldn't open $version_header: $!";
while (<IN>) {
  if (/^#define[\s\t]+OPENSSL_VERSION_NUMBER[\s\t]+0x(\d)(\d{2})(\d{2})(\d{2})/)
  {
    # die "OpenSSL version detected: $1.$2.$3.$4\n";
    #$nlmvernum = "$1,$2,$3";
    $nlmvernum = "$1,".($2*10+$3).",".($4*1);
    #$nlmverstr = "$1.".($2*1).".".($3*1).($4?(chr(96+$4)):"");
    break;
  }
}
close(IN) or die "Couldn't close $version_header: $!";

$readme_file = "README";
open(IN, $readme_file) or die "Couldn't open $readme_file: $!";
while (<IN>) {
  if (/^[\s\t]+OpenSSL[\s\t]+(\d)\.(\d{1,2})\.(\d{1,2})([a-z])(.*)/)
  {
    #$nlmvernum = "$1,$2,$3";
    #$nlmvernum = "$1,".($2*10+$3).",".($4*1);
    $nlmverstr = "$1.$2.$3$4$5";
  }
  elsif (/^[\s\t]+(Copyright \(c\) \d{4}\-\d{4} The OpenSSL Project)$/)
  {
    $nlmcpystr = $1;
  }
  break if ($nlmvernum && $nlmcpystr);
}
close(IN) or die "Couldn't close $readme_file: $!";

# Define stacksize here
$nlmstack = "32768";

# some default settings here in case we failed to find them in README
$nlmvernum = "1,0,0" if (!$nlmvernum);
$nlmverstr = "OpenSSL" if (!$nlmverstr);
$nlmcpystr = "Copyright (c) 1998-now The OpenSSL Project" if (!$nlmcpystr);

# die "OpenSSL copyright: $nlmcpystr\nOpenSSL verstring: $nlmverstr\nOpenSSL vernumber: $nlmvernum\n";

# The import files and other misc imports needed to link
@misc_imports = ("GetProcessSwitchCount", "RunningProcess",
                 "GetSuperHighResolutionTimer");
if ($LIBC)
{
   @import_files = ("libc.imp");
   @module_files = ("libc");
   $libarch = "LIBC";
}
else
{
   # clib build
   @import_files = ("clib.imp");
   push(@import_files, "socklib.imp") if ($BSDSOCK);
   @module_files = ("clib");
   # push(@misc_imports, "_rt_modu64%16", "_rt_divu64%16");
   $libarch = "CLIB";
}
if ($BSDSOCK)
{
   $libarch .= "-BSD";
}
else
{
   $libarch .= "-WS2";
   push(@import_files, "ws2nlm.imp");
}

# The "IMPORTS" environment variable must be set and point to the location
# where import files (*.imp) can be found.
# Example:  set IMPORTS=c:\ndk\nwsdk\imports
$import_path = $ENV{"IMPORTS"} || die ("IMPORTS environment variable not set\n");


# The "PRELUDE" environment variable must be set and point to the location
# and name of the prelude source to link with ( nwpre.obj is recommended ).
# Example: set PRELUDE=c:\codewar\novell support\metrowerks support\libraries\runtime\nwpre.obj
$prelude = $ENV{"PRELUDE"} || die ("PRELUDE environment variable not set\n");

# The "INCLUDES" environment variable must be set and point to the location
# where import files (*.imp) can be found.
$include_path = $ENV{"INCLUDE"} || die ("INCLUDES environment variable not set\n");
$include_path =~ s/\\/\//g;
$include_path = join(" -I", split(/;/, $include_path));

# check for gcc compiler
$gnuc = $ENV{"GNUC"};

#$ssl=   "ssleay32";
#$crypto="libeay32";

if ($gnuc)
{
   # C compiler
   $cc='gcc';
   # Linker
   $link='nlmconv';
   # librarian
   $mklib='ar';
   $o='/';
   # cp command
   $cp='cp -af';
   # rm command
   $rm='rm -f';
   # mv command
   $mv='mv -f';
   # mkdir command
   $mkdir='gmkdir';
   #$ranlib='ranlib';
}
else
{
   # C compiler
   $cc='mwccnlm';
   # Linker
   $link='mwldnlm';
   # librarian
   $mklib='mwldnlm';
   # Path separator
   $o='\\';
   # cp command
   $cp='copy >nul:';
   # rm command
   $rm='del /f /q';
}

# assembler
if ($nw_nasm)
{
   $asm=(`nasm -v 2>NUL` gt `nasmw -v 2>NUL`?"nasm":"nasmw");
   if ($gnuc)
   {
      $asm.=" -s -f elf";
   }
   else
   {
      $asm.=" -s -f coff -d __coff__";
   }
   $afile="-o ";
   $asm.=" -g" if $debug;
}
elsif ($nw_mwasm)
{
   $asm="mwasmnlm -maxerrors 20";
   $afile="-o ";
   $asm.=" -g" if $debug;
}
elsif ($nw_masm)
{
# masm assembly settings - it should be possible to use masm but haven't
# got it working.
# $asm='ml /Cp /coff /c /Cx';
# $asm.=" /Zi" if $debug;
# $afile='/Fo';
   die("Support for masm assembler not yet functional\n");
}
else
{
   $asm="";
   $afile="";
}



if ($gnuc)
{
   # compile flags for GNUC
   # additional flags based upon debug | non-debug
   if ($debug)
   {
      $cflags="-g -DDEBUG";
   }
   else
   {
      $cflags="-O2";
   }
   $cflags.=" -nostdinc -I$include_path \\
         -fno-builtin -fpcc-struct-return -fno-strict-aliasing \\
         -funsigned-char -Wall -Wno-unused -Wno-uninitialized";

   # link flags
   $lflags="-T";
}
else
{
   # compile flags for CodeWarrior
   # additional flags based upon debug | non-debug
   if ($debug)
   {
      $cflags="-opt off -g -sym internal -DDEBUG";
   }
   else
   {
   # CodeWarrior compiler has a problem with optimizations for floating
   # points - no optimizations until further investigation
   #      $cflags="-opt all";
   }

   # NOTES: Several c files in the crypto subdirectory include headers from
   #        their local directories.  Metrowerks wouldn't find these h files
   #        without adding individual include directives as compile flags
   #        or modifying the c files.  Instead of adding individual include
   #        paths for each subdirectory a recursive include directive
   #        is used ( -ir crypto ).
   #
   #        A similar issue exists for the engines and apps subdirectories.
   #
   #        Turned off the "possible" warnings ( -w nopossible ).  Metrowerks
   #        complained a lot about various stuff.  May want to turn back
   #        on for further development.
   $cflags.=" -nostdinc -ir crypto -ir ssl -ir engines -ir apps -I$include_path \\
         -msgstyle gcc -align 4 -processor pentium -char unsigned \\
         -w on -w nolargeargs -w nopossible -w nounusedarg -w nounusedexpr \\
         -w noimplicitconv -relax_pointers -nosyspath -maxerrors 20";

   # link flags
   $lflags="-msgstyle gcc -zerobss -nostdlib -sym internal -commandfile";
}

# common defines
$cflags.=" -DL_ENDIAN -DOPENSSL_SYSNAME_NETWARE -U_WIN32";

# If LibC build add in NKS_LIBC define and set the entry/exit
# routines - The default entry/exit routines are for CLib and don't exist
# in LibC
if ($LIBC)
{
   $cflags.=" -DNETWARE_LIBC";
   $nlmstart = "_LibCPrelude";
   $nlmexit = "_LibCPostlude";
   @nlm_flags = ("pseudopreemption", "flag_on 64");
}
else
{
   $cflags.=" -DNETWARE_CLIB";
   $nlmstart = "_Prelude";
   $nlmexit = "_Stop";
}

# If BSD Socket support is requested, set a define for the compiler
if ($BSDSOCK)
{
   $cflags.=" -DNETWARE_BSDSOCK";
   if (!$LIBC)
   {
      $cflags.=" -DNETDB_USE_INTERNET";
   }
}


# linking stuff
# for the output directories use the mk1mf.pl values with "_nw" appended
if ($shlib)
{
   if ($LIBC)
   {
      $out_def.="_nw_libc_nlm";
      $tmp_def.="_nw_libc_nlm";
      $inc_def.="_nw_libc_nlm";
   }
   else  # NETWARE_CLIB
   {
      $out_def.="_nw_clib_nlm";
      $tmp_def.="_nw_clib_nlm";
      $inc_def.="_nw_clib_nlm";
   }
}
else
{
   if ($gnuc) # GNUC Tools
   {
      $libp=".a";
      $shlibp=".a";
      $lib_flags="-cr";
   }
   else       # CodeWarrior
   {
      $libp=".lib";
      $shlibp=".lib";
      $lib_flags="-nodefaults -type library -o";
   }
   if ($LIBC)
   {
      $out_def.="_nw_libc";
      $tmp_def.="_nw_libc";
      $inc_def.="_nw_libc";
   }
   else  # NETWARE_CLIB
   {
      $out_def.="_nw_clib";
      $tmp_def.="_nw_clib";
      $inc_def.="_nw_clib";
   }
}

# used by mk1mf.pl
$obj='.o';
$ofile='-o ';
$efile='';
$exep='.nlm';
$ex_libs='';

if (!$no_asm)
{
   $bn_asm_obj="\$(OBJ_D)${o}bn-nw${obj}";
   $bn_asm_src="crypto${o}bn${o}asm${o}bn-nw.asm";
   $bnco_asm_obj="\$(OBJ_D)${o}co-nw${obj}";
   $bnco_asm_src="crypto${o}bn${o}asm${o}co-nw.asm";
   $aes_asm_obj="\$(OBJ_D)${o}a-nw${obj}";
   $aes_asm_src="crypto${o}aes${o}asm${o}a-nw.asm";
   $des_enc_obj="\$(OBJ_D)${o}d-nw${obj} \$(OBJ_D)${o}y-nw${obj}";
   $des_enc_src="crypto${o}des${o}asm${o}d-nw.asm crypto${o}des${o}asm${o}y-nw.asm";
   $bf_enc_obj="\$(OBJ_D)${o}b-nw${obj}";
   $bf_enc_src="crypto${o}bf${o}asm${o}b-nw.asm";
   $cast_enc_obj="\$(OBJ_D)${o}c-nw${obj}";
   $cast_enc_src="crypto${o}cast${o}asm${o}c-nw.asm";
   $rc4_enc_obj="\$(OBJ_D)${o}r4-nw${obj}";
   $rc4_enc_src="crypto${o}rc4${o}asm${o}r4-nw.asm";
   $rc5_enc_obj="\$(OBJ_D)${o}r5-nw${obj}";
   $rc5_enc_src="crypto${o}rc5${o}asm${o}r5-nw.asm";
   $md5_asm_obj="\$(OBJ_D)${o}m5-nw${obj}";
   $md5_asm_src="crypto${o}md5${o}asm${o}m5-nw.asm";
   $sha1_asm_obj="\$(OBJ_D)${o}s1-nw${obj} \$(OBJ_D)${o}sha256-nw${obj} \$(OBJ_D)${o}sha512-nw${obj}";
   $sha1_asm_src="crypto${o}sha${o}asm${o}s1-nw.asm crypto${o}sha${o}asm${o}sha256-nw.asm crypto${o}sha${o}asm${o}sha512-nw.asm";
   $rmd160_asm_obj="\$(OBJ_D)${o}rm-nw${obj}";
   $rmd160_asm_src="crypto${o}ripemd${o}asm${o}rm-nw.asm";
   $whirlpool_asm_obj="\$(OBJ_D)${o}wp-nw${obj}";
   $whirlpool_asm_src="crypto${o}whrlpool${o}asm${o}wp-nw.asm";
   $cpuid_asm_obj="\$(OBJ_D)${o}x86cpuid-nw${obj}";
   $cpuid_asm_src="crypto${o}x86cpuid-nw.asm";
   $cflags.=" -DOPENSSL_CPUID_OBJ -DBN_ASM -DOPENSSL_BN_ASM_PART_WORDS -DMD5_ASM -DWHIRLPOOL_ASM";
   $cflags.=" -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM";
   $cflags.=" -DAES_ASM -DRMD160_ASM";
}
else
{
   $bn_asm_obj='';
   $bn_asm_src='';
   $bnco_asm_obj='';
   $bnco_asm_src='';
   $aes_asm_obj='';
   $aes_asm_src='';
   $des_enc_obj='';
   $des_enc_src='';
   $bf_enc_obj='';
   $bf_enc_src='';
   $cast_enc_obj='';
   $cast_enc_src='';
   $rc4_enc_obj='';
   $rc4_enc_src='';
   $rc5_enc_obj='';
   $rc5_enc_src='';
   $md5_asm_obj='';
   $md5_asm_src='';
   $sha1_asm_obj='';
   $sha1_asm_src='';
   $rmd160_asm_obj='';
   $rmd160_asm_src='';
   $whirlpool_asm_obj='';
   $whirlpool_asm_src='';
   $cpuid_asm_obj='';
   $cpuid_asm_src='';
}

# create the *.def linker command files in \openssl\netware\ directory
sub do_def_file
{
   # strip off the leading path
   my($target) = bname(shift);
   my($i);

   if ($target =~ /(.*).nlm/)
   {
      $target = $1;
   }

   # special case for openssl - the mk1mf.pl defines E_EXE = openssl
   if ($target =~ /E_EXE/)
   {
      $target =~ s/\$\(E_EXE\)/openssl/;
   }

   # Note: originally tried to use full path ( \openssl\netware\$target.def )
   # Metrowerks linker choked on this with an assertion failure. bug???
   #
   my($def_file) = "netware${o}$target.def";

   open(DEF_OUT, ">$def_file") || die("unable to open file $def_file\n");

   print( DEF_OUT "# command file generated by netware.pl for NLM target.\n" );
   print( DEF_OUT "# do not edit this file - all your changes will be lost!!\n" );
   print( DEF_OUT "#\n");
   print( DEF_OUT "DESCRIPTION \"$target ($libarch) - OpenSSL $nlmverstr\"\n");
   print( DEF_OUT "COPYRIGHT \"$nlmcpystr\"\n");
   print( DEF_OUT "VERSION $nlmvernum\n");
   print( DEF_OUT "STACK $nlmstack\n");
   print( DEF_OUT "START $nlmstart\n");
   print( DEF_OUT "EXIT $nlmexit\n");

   # special case for openssl
   if ($target eq "openssl")
   {
      print( DEF_OUT "SCREENNAME \"OpenSSL $nlmverstr\"\n");
   }
   else
   {
      print( DEF_OUT "SCREENNAME \"DEFAULT\"\n");
   }

   foreach $i (@misc_imports)
   {
      print( DEF_OUT "IMPORT $i\n");
   }

   foreach $i (@import_files)
   {
      print( DEF_OUT "IMPORT \@$import_path${o}$i\n");
   }

   foreach $i (@module_files)
   {
      print( DEF_OUT "MODULE $i\n");
   }

   foreach $i (@nlm_flags)
   {
      print( DEF_OUT "$i\n");
   }

   if ($gnuc)
   {
      if ($target =~ /openssl/)
      {
         print( DEF_OUT "INPUT ${tmp_def}${o}openssl${obj}\n");
         print( DEF_OUT "INPUT ${tmp_def}${o}openssl${libp}\n");
      }
      else
      {
         print( DEF_OUT "INPUT ${tmp_def}${o}${target}${obj}\n");
      }
      print( DEF_OUT "INPUT $prelude\n");
      print( DEF_OUT "INPUT ${out_def}${o}${ssl}${libp} ${out_def}${o}${crypto}${libp}\n");
      print( DEF_OUT "OUTPUT $target.nlm\n");
   }

   close(DEF_OUT);
   return($def_file);
}

sub do_lib_rule
{
   my($objs,$target,$name,$shlib)=@_;
   my($ret);

   $ret.="$target: $objs\n";
   if (!$shlib)
   {
      $ret.="\t\@echo Building Lib: $name\n";
      $ret.="\t\$(MKLIB) $lib_flags $target $objs\n";
      $ret.="\t\@echo .\n"
   }
   else
   {
      die( "Building as NLM not currently supported!" );
   }

   $ret.="\n";
   return($ret);
}

sub do_link_rule
{
   my($target,$files,$dep_libs,$libs)=@_;
   my($ret);
   my($def_file) = do_def_file($target);

   $ret.="$target: $files $dep_libs\n";

   # NOTE:  When building the test nlms no screen name is given
   #  which causes the console screen to be used.  By using the console
   #  screen there is no "<press any key to continue>" message which
   #  requires user interaction.  The test script ( do_tests.pl ) needs
   #  to be able to run the tests without requiring user interaction.
   #
   #  However, the sample program "openssl.nlm" is used by the tests and is
   #  a interactive sample so a screen is desired when not be run by the
   #  tests.  To solve the problem, two versions of the program are built:
   #    openssl2 - no screen used by tests
   #    openssl - default screen - use for normal interactive modes
   #

   # special case for openssl - the mk1mf.pl defines E_EXE = openssl
   if ($target =~ /E_EXE/)
   {
      my($target2) = $target;

      $target2 =~ s/\(E_EXE\)/\(E_EXE\)2/;

      # openssl2
      my($def_file2) = do_def_file($target2);

      if ($gnuc)
      {
         $ret.="\t\$(MKLIB) $lib_flags \$(TMP_D)${o}\$(E_EXE).a \$(filter-out \$(TMP_D)${o}\$(E_EXE)${obj},$files)\n";
         $ret.="\t\$(LINK_CMD) \$(LFLAGS) $def_file2\n";
         $ret.="\t\@$mv \$(E_EXE)2.nlm \$(TEST_D)\n";
      }
      else
      {
         $ret.="\t\$(LINK_CMD) \$(LFLAGS) $def_file2 $files \"$prelude\" $libs -o $target2\n";
      }
   }
   if ($gnuc)
   {
      $ret.="\t\$(LINK_CMD) \$(LFLAGS) $def_file\n";
      $ret.="\t\@$mv \$(\@F) \$(TEST_D)\n";
   }
   else
   {
      $ret.="\t\$(LINK_CMD) \$(LFLAGS) $def_file $files \"$prelude\" $libs -o $target\n";
   }

   $ret.="\n";
   return($ret);

}

1;
