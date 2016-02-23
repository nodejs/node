#!/usr/local/bin/perl
#
# linux.pl - the standard unix makefile stuff.
#

$o='/';
$cp='/bin/cp';
$rm='/bin/rm -f';

# C compiler stuff

$cc='cc';
if ($debug)
	{ $cflags="-g -DREF_CHECK -DCRYPTO_MDEBUG"; }
else
	{ $cflags="-O2"; }

$cflags.=" -std1 -DL_ENDIAN";

if (!$no_asm)
	{
	$bn_asm_obj='$(OBJ_D)/mips1.o';
	$bn_asm_src='crypto/bn/asm/mips1.s';
	}

sub do_link_rule
	{
	local($target,$files,$dep_libs,$libs)=@_;
	local($ret,$_);
	
	$file =~ s/\//$o/g if $o ne '/';
	$n=&bname($target);
	$ret.="$target: $files $dep_libs\n";
	$ret.="\t\$(LINK) ${efile}$target \$(LFLAGS) $files $libs\n\n";
	return($ret);
	}

1;
