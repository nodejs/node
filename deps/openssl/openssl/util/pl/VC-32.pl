#!/usr/local/bin/perl
# VC-32.pl - unified script for Microsoft Visual C++, covering Win32,
# Win64 and WinCE [follow $FLAVOR variable to trace the differences].
#

$ssl=	"ssleay32";
$crypto="libeay32";

$o='\\';
$cp='$(PERL) util/copy.pl';
$mkdir='$(PERL) util/mkdir-p.pl';
$rm='del /Q';

$zlib_lib="zlib1.lib";

# Santize -L options for ms link
$l_flags =~ s/-L("\[^"]+")/\/libpath:$1/g;
$l_flags =~ s/-L(\S+)/\/libpath:$1/g;

# C compiler stuff
$cc='cl';
if ($FLAVOR =~ /WIN64/)
    {
    # Note that we currently don't have /WX on Win64! There is a lot of
    # warnings, but only of two types:
    #
    # C4344: conversion from '__int64' to 'int/long', possible loss of data
    # C4267: conversion from 'size_t' to 'int/long', possible loss of data
    #
    # Amount of latter type is minimized by aliasing strlen to function of
    # own desing and limiting its return value to 2GB-1 (see e_os.h). As
    # per 0.9.8 release remaining warnings were explicitly examined and
    # considered safe to ignore.
    # 
    $base_cflags= " $mf_cflag";
    my $f = $shlib?' /MD':' /MT';
    $lib_cflag='/Zl' if (!$shlib);	# remove /DEFAULTLIBs from static lib
    $opt_cflags=$f.' /Ox';
    $dbg_cflags=$f.'d /Od -DDEBUG -D_DEBUG';
    $lflags="/nologo /subsystem:console /opt:ref";

    *::perlasm_compile_target = sub {
	my ($target,$source,$bname)=@_;
	my $ret;

	$bname =~ s/(.*)\.[^\.]$/$1/;
	$ret=<<___;
\$(TMP_D)$o$bname.asm: $source
	set ASM=\$(ASM)
	\$(PERL) $source \$\@

$target: \$(TMP_D)$o$bname.asm
	\$(ASM) $afile\$\@ \$(TMP_D)$o$bname.asm

___
	}
    }
elsif ($FLAVOR =~ /CE/)
    {
    # sanity check
    die '%OSVERSION% is not defined'	if (!defined($ENV{'OSVERSION'}));
    die '%PLATFORM% is not defined'	if (!defined($ENV{'PLATFORM'}));
    die '%TARGETCPU% is not defined'	if (!defined($ENV{'TARGETCPU'}));

    #
    # Idea behind this is to mimic flags set by eVC++ IDE...
    #
    $wcevers = $ENV{'OSVERSION'};			# WCENNN
    die '%OSVERSION% value is insane'	if ($wcevers !~ /^WCE([1-9])([0-9]{2})$/);
    $wcecdefs = "-D_WIN32_WCE=$1$2 -DUNDER_CE=$1$2";	# -D_WIN32_WCE=NNN
    $wcelflag = "/subsystem:windowsce,$1.$2";		# ...,N.NN

    $wceplatf =  $ENV{'PLATFORM'};
    $wceplatf =~ tr/a-z0-9 /A-Z0-9_/d;
    $wcecdefs .= " -DWCE_PLATFORM_$wceplatf";

    $wcetgt = $ENV{'TARGETCPU'};	# just shorter name...
    SWITCH: for($wcetgt) {
	/^X86/		&& do {	$wcecdefs.=" -Dx86 -D_X86_ -D_i386_ -Di_386_";
				$wcelflag.=" /machine:IX86";	last; };
	/^ARMV4[IT]/	&& do { $wcecdefs.=" -DARM -D_ARM_ -D$wcetgt";
				$wcecdefs.=" -DTHUMB -D_THUMB_" if($wcetgt=~/T$/);
				$wcecdefs.=" -QRarch4T -QRinterwork-return";
				$wcelflag.=" /machine:THUMB";	last; };
	/^ARM/		&& do {	$wcecdefs.=" -DARM -D_ARM_ -D$wcetgt";
				$wcelflag.=" /machine:ARM";	last; };
	/^MIPSIV/	&& do {	$wcecdefs.=" -DMIPS -D_MIPS_ -DR4000 -D$wcetgt";
				$wcecdefs.=" -D_MIPS64 -QMmips4 -QMn32";
				$wcelflag.=" /machine:MIPSFPU";	last; };
	/^MIPS16/	&& do {	$wcecdefs.=" -DMIPS -D_MIPS_ -DR4000 -D$wcetgt";
				$wcecdefs.=" -DMIPSII -QMmips16";
				$wcelflag.=" /machine:MIPS16";	last; };
	/^MIPSII/	&& do {	$wcecdefs.=" -DMIPS -D_MIPS_ -DR4000 -D$wcetgt";
				$wcecdefs.=" -QMmips2";
				$wcelflag.=" /machine:MIPS";	last; };
	/^R4[0-9]{3}/	&& do {	$wcecdefs.=" -DMIPS -D_MIPS_ -DR4000";
				$wcelflag.=" /machine:MIPS";	last; };
	/^SH[0-9]/	&& do {	$wcecdefs.=" -D$wcetgt -D_$wcetgt_ -DSHx";
				$wcecdefs.=" -Qsh4" if ($wcetgt =~ /^SH4/);
				$wcelflag.=" /machine:$wcetgt";	last; };
	{ $wcecdefs.=" -D$wcetgt -D_$wcetgt_";
	  $wcelflag.=" /machine:$wcetgt";			last; };
    }

    $cc='$(CC)';
    $base_cflags=' /W3 /WX /GF /Gy /nologo -DUNICODE -D_UNICODE -DOPENSSL_SYSNAME_WINCE -DWIN32_LEAN_AND_MEAN -DL_ENDIAN -DDSO_WIN32 -DNO_CHMOD -DOPENSSL_SMALL_FOOTPRINT';
    $base_cflags.=" $wcecdefs";
    $base_cflags.=' -I$(WCECOMPAT)/include'		if (defined($ENV{'WCECOMPAT'}));
    $base_cflags.=' -I$(PORTSDK_LIBPATH)/../../include'	if (defined($ENV{'PORTSDK_LIBPATH'}));
    $opt_cflags=' /MC /O1i';	# optimize for space, but with intrinsics...
    $dbg_clfags=' /MC /Od -DDEBUG -D_DEBUG';
    $lflags="/nologo /opt:ref $wcelflag";
    }
else	# Win32
    {
    $base_cflags= " $mf_cflag";
    my $f = $shlib?' /MD':' /MT';
    $lib_cflag='/Zl' if (!$shlib);	# remove /DEFAULTLIBs from static lib
    $opt_cflags=$f.' /Ox /O2 /Ob2';
    $dbg_cflags=$f.'d /Od -DDEBUG -D_DEBUG';
    $lflags="/nologo /subsystem:console /opt:ref";
    }
$mlflags='';

$out_def ="out32";	$out_def.="dll"			if ($shlib);
			$out_def.='_$(TARGETCPU)'	if ($FLAVOR =~ /CE/);
$tmp_def ="tmp32";	$tmp_def.="dll"			if ($shlib);
			$tmp_def.='_$(TARGETCPU)'	if ($FLAVOR =~ /CE/);
$inc_def="inc32";

if ($debug)
	{
	$cflags=$dbg_cflags.$base_cflags;
	}
else
	{
	$cflags=$opt_cflags.$base_cflags;
	}

# generate symbols.pdb unconditionally
$app_cflag.=" /Zi /Fd\$(TMP_D)/app";
$lib_cflag.=" /Zi /Fd\$(TMP_D)/lib";
$lflags.=" /debug";

$obj='.obj';
$asm_suffix='.asm';
$ofile="/Fo";

# EXE linking stuff
$link="link";
$rsc="rc";
$efile="/out:";
$exep='.exe';
if ($no_sock)		{ $ex_libs=''; }
elsif ($FLAVOR =~ /CE/)	{ $ex_libs='winsock.lib'; }
else			{ $ex_libs='ws2_32.lib'; }

if ($FLAVOR =~ /CE/)
	{
	$ex_libs.=' $(WCECOMPAT)/lib/wcecompatex.lib'	if (defined($ENV{'WCECOMPAT'}));
	$ex_libs.=' $(PORTSDK_LIBPATH)/portlib.lib'	if (defined($ENV{'PORTSDK_LIBPATH'}));
	$ex_libs.=' /nodefaultlib:oldnames.lib coredll.lib corelibc.lib' if ($ENV{'TARGETCPU'} eq "X86");
	}
else
	{
	$ex_libs.=' gdi32.lib advapi32.lib crypt32.lib user32.lib';
	$ex_libs.=' bufferoverflowu.lib' if ($FLAVOR =~ /WIN64/ and `cl 2>&1` =~ /14\.00\.4[0-9]{4}\./);
	# WIN32 UNICODE build gets linked with unicows.lib for
	# backward compatibility with Win9x.
	$ex_libs="unicows.lib $ex_libs" if ($FLAVOR =~ /WIN32/ and $cflags =~ /\-DUNICODE/);
	}

# static library stuff
$mklib='lib /nologo';
$ranlib='';
$plib="";
$libp=".lib";
$shlibp=($shlib)?".dll":".lib";
$lfile='/out:';

$shlib_ex_obj="";
$app_ex_obj="setargv.obj" if ($FLAVOR !~ /CE/);
if ($FLAVOR =~ /WIN64A/) {
	if (`nasm -v 2>NUL` =~ /NASM version ([0-9]+\.[0-9]+)/ && $1 >= 2.0) {
		$asm='nasm -f win64 -DNEAR -Ox -g';
		$afile='-o ';
	} else {
		$asm='ml64 /c /Cp /Cx /Zi';
		$afile='/Fo';
	}
} elsif ($FLAVOR =~ /WIN64I/) {
	$asm='ias -d debug';
	$afile="-o ";
} elsif ($nasm) {
	my $ver=`nasm -v 2>NUL`;
	my $vew=`nasmw -v 2>NUL`;
	# pick newest version
	$asm=($ver ge $vew?"nasm":"nasmw")." -f win32";
	$asmtype="win32n";
	$afile='-o ';
} else {
	$asm='ml /nologo /Cp /coff /c /Cx /Zi';
	$afile='/Fo';
	$asmtype="win32";
}

$bn_asm_obj='';
$bn_asm_src='';
$des_enc_obj='';
$des_enc_src='';
$bf_enc_obj='';
$bf_enc_src='';

if (!$no_asm)
	{
	win32_import_asm($mf_bn_asm, "bn", \$bn_asm_obj, \$bn_asm_src);
	win32_import_asm($mf_aes_asm, "aes", \$aes_asm_obj, \$aes_asm_src);
	win32_import_asm($mf_des_asm, "des", \$des_enc_obj, \$des_enc_src);
	win32_import_asm($mf_bf_asm, "bf", \$bf_enc_obj, \$bf_enc_src);
	win32_import_asm($mf_cast_asm, "cast", \$cast_enc_obj, \$cast_enc_src);
	win32_import_asm($mf_rc4_asm, "rc4", \$rc4_enc_obj, \$rc4_enc_src);
	win32_import_asm($mf_rc5_asm, "rc5", \$rc5_enc_obj, \$rc5_enc_src);
	win32_import_asm($mf_md5_asm, "md5", \$md5_asm_obj, \$md5_asm_src);
	win32_import_asm($mf_sha_asm, "sha", \$sha1_asm_obj, \$sha1_asm_src);
	win32_import_asm($mf_rmd_asm, "ripemd", \$rmd160_asm_obj, \$rmd160_asm_src);
	win32_import_asm($mf_wp_asm, "whrlpool", \$whirlpool_asm_obj, \$whirlpool_asm_src);
	win32_import_asm($mf_cpuid_asm, "", \$cpuid_asm_obj, \$cpuid_asm_src);
	$perl_asm = 1;
	}

if ($shlib && $FLAVOR !~ /CE/)
	{
	$mlflags.=" $lflags /dll";
	$lib_cflag.=" -D_WINDLL";
	#
	# Engage Applink...
	#
	$app_ex_obj.=" \$(OBJ_D)\\applink.obj /implib:\$(TMP_D)\\junk.lib";
	$cflags.=" -DOPENSSL_USE_APPLINK -I.";
	# I'm open for better suggestions than overriding $banner...
	$banner=<<'___';
	@echo Building OpenSSL

$(OBJ_D)\applink.obj:	ms\applink.c
	$(CC) /Fo$(OBJ_D)\applink.obj $(APP_CFLAGS) -c ms\applink.c
$(OBJ_D)\uplink.obj:	ms\uplink.c ms\applink.c
	$(CC) /Fo$(OBJ_D)\uplink.obj $(SHLIB_CFLAGS) -c ms\uplink.c
$(INCO_D)\applink.c:	ms\applink.c
	$(CP) ms\applink.c $(INCO_D)\applink.c

EXHEADER= $(EXHEADER) $(INCO_D)\applink.c

LIBS_DEP=$(LIBS_DEP) $(OBJ_D)\applink.obj
CRYPTOOBJ=$(OBJ_D)\uplink.obj $(CRYPTOOBJ)
___
	$banner.=<<'___' if ($FLAVOR =~ /WIN64/);
CRYPTOOBJ=ms\uptable.obj $(CRYPTOOBJ)
___
	}
elsif ($shlib && $FLAVOR =~ /CE/)
	{
	$mlflags.=" $lflags /dll";
	$lflags.=' /entry:mainCRTstartup' if(defined($ENV{'PORTSDK_LIBPATH'}));
	$lib_cflag.=" -D_WINDLL -D_DLL";
	}

sub do_lib_rule
	{
	local($objs,$target,$name,$shlib)=@_;
	local($ret);

	$taget =~ s/\//$o/g if $o ne '/';
	if ($name ne "")
		{
		$name =~ tr/a-z/A-Z/;
		$name = "/def:ms/${name}.def";
		}

#	$target="\$(LIB_D)$o$target";
	$ret.="$target: $objs\n";
	if (!$shlib)
		{
#		$ret.="\t\$(RM) \$(O_$Name)\n";
		$ret.="\t\$(MKLIB) $lfile$target @<<\n  $objs\n<<\n";
		}
	else
		{
		local($ex)=($target =~ /O_CRYPTO/)?'':' $(L_CRYPTO)';
		$ex.=" $zlib_lib" if $zlib_opt == 1 && $target =~ /O_CRYPTO/;
		$ret.="\t\$(LINK) \$(MLFLAGS) $efile$target $name @<<\n  \$(SHLIB_EX_OBJ) $objs $ex \$(EX_LIBS)\n<<\n";
		$ret.="\tIF EXIST \$@.manifest mt -nologo -manifest \$@.manifest -outputresource:\$@;2\n\n";
		}
	$ret.="\n";
	return($ret);
	}

sub do_link_rule
	{
	local($target,$files,$dep_libs,$libs)=@_;
	local($ret,$_);
	
	$file =~ s/\//$o/g if $o ne '/';
	$n=&bname($targer);
	$ret.="$target: $files $dep_libs\n";
	$ret.="\t\$(LINK) \$(LFLAGS) $efile$target @<<\n";
	$ret.="  \$(APP_EX_OBJ) $files $libs\n<<\n";
	$ret.="\tIF EXIST \$@.manifest mt -nologo -manifest \$@.manifest -outputresource:\$@;1\n\n";
	return($ret);
	}

sub win32_import_asm
	{
	my ($mf_var, $asm_name, $oref, $sref) = @_;
	my $asm_dir;
	if ($asm_name eq "")
		{
		$asm_dir = "crypto\\";
		}
	else
		{
		$asm_dir = "crypto\\$asm_name\\asm\\";
		}

	$$oref = "";
	$mf_var =~ s/\.o$/.obj/g;

	foreach (split(/ /, $mf_var))
		{
		$$oref .= $asm_dir . $_ . " ";
		}
	$$oref =~ s/ $//;
	$$sref = $$oref;
	$$sref =~ s/\.obj/.asm/g;

	}


1;
