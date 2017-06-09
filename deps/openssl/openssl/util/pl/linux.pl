#!/usr/local/bin/perl
#
# linux.pl - the standard unix makefile stuff.
#

$o='/';
$cp='/bin/cp';
$rm='/bin/rm -f';

# C compiler stuff

$cc='gcc';
if ($debug)
	{ $cflags="-g2 -ggdb -DREF_CHECK -DCRYPTO_MDEBUG"; }
elsif ($profile)
	{ $cflags="-pg -O3"; }
else
	{ $cflags="-O3 -fomit-frame-pointer"; }

if (!$no_asm)
	{
	$bn_asm_obj='$(OBJ_D)/bn86-elf.o';
	$bn_asm_src='crypto/bn/asm/bn86unix.cpp';
	$bnco_asm_obj='$(OBJ_D)/co86-elf.o';
	$bnco_asm_src='crypto/bn/asm/co86unix.cpp';
	$des_enc_obj='$(OBJ_D)/dx86-elf.o $(OBJ_D)/yx86-elf.o';
	$des_enc_src='crypto/des/asm/dx86unix.cpp crypto/des/asm/yx86unix.cpp';
	$bf_enc_obj='$(OBJ_D)/bx86-elf.o';
	$bf_enc_src='crypto/bf/asm/bx86unix.cpp';
	$cast_enc_obj='$(OBJ_D)/cx86-elf.o';
	$cast_enc_src='crypto/cast/asm/cx86unix.cpp';
	$rc4_enc_obj='$(OBJ_D)/rx86-elf.o';
	$rc4_enc_src='crypto/rc4/asm/rx86unix.cpp';
	$rc5_enc_obj='$(OBJ_D)/r586-elf.o';
	$rc5_enc_src='crypto/rc5/asm/r586unix.cpp';
	$md5_asm_obj='$(OBJ_D)/mx86-elf.o';
	$md5_asm_src='crypto/md5/asm/mx86unix.cpp';
	$rmd160_asm_obj='$(OBJ_D)/rm86-elf.o';
	$rmd160_asm_src='crypto/ripemd/asm/rm86unix.cpp';
	$sha1_asm_obj='$(OBJ_D)/sx86-elf.o';
	$sha1_asm_src='crypto/sha/asm/sx86unix.cpp';
	$cflags.=" -DBN_ASM -DMD5_ASM -DSHA1_ASM -DOPENSSL_BN_ASM_PART_WORDS";
	}

$cflags.=" -DTERMIO -DL_ENDIAN -m486 -Wall";

if ($shlib)
	{
	$shl_cflag=" -DPIC -fpic";
	$shlibp=".so.$ssl_version";
	$so_shlibp=".so";
	}

sub do_shlib_rule
	{
	local($obj,$target,$name,$shlib,$so_name)=@_;
	local($ret,$_,$Name);

	$target =~ s/\//$o/g if $o ne '/';
	($Name=$name) =~ tr/a-z/A-Z/;

	$ret.="$target: \$(${Name}OBJ)\n";
	$ret.="\t\$(RM) target\n";
	$ret.="\tgcc \${CFLAGS} -shared -Wl,-soname,$target -o $target \$(${Name}OBJ)\n";
	($t=$target) =~ s/(^.*)\/[^\/]*$/$1/;
	if ($so_name ne "")
		{
		$ret.="\t\$(RM) \$(LIB_D)$o$so_name\n";
		$ret.="\tln -s $target \$(LIB_D)$o$so_name\n\n";
		}
	}

sub do_link_rule
	{
	local($target,$files,$dep_libs,$libs)=@_;
	local($ret,$_);
	
	$file =~ s/\//$o/g if $o ne '/';
	$n=&bname($target);
	$ret.="$target: $files $dep_libs\n";
	$ret.="\t\$(LINK_CMD) ${efile}$target \$(LFLAGS) $files $libs\n\n";
	return($ret);
	}

sub do_asm_rule
	{
	local($target,$src)=@_;
	local($ret,@s,@t,$i);

	$target =~ s/\//$o/g if $o ne "/";
	$src =~ s/\//$o/g if $o ne "/";

	@s=split(/\s+/,$src);
	@t=split(/\s+/,$target);

	for ($i=0; $i<=$#s; $i++)
		{
		$ret.="$t[$i]: $s[$i]\n";
		$ret.="\tgcc -E -DELF \$(SRC_D)$o$s[$i]|\$(AS) $afile$t[$i]\n\n";
		}
	return($ret);
	}

1;
