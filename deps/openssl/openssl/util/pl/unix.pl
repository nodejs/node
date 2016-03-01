#!/usr/local/bin/perl
#
# unix.pl - the standard unix makefile stuff.
#

$o='/';
$cp='/bin/cp';
$rm='/bin/rm -f';

# C compiler stuff

if ($gcc)
	{
	$cc='gcc';
	if ($debug)
		{ $cflags="-g2 -ggdb"; }
	else
		{ $cflags="-O3 -fomit-frame-pointer"; }
	}
else
	{
	$cc='cc';
	if ($debug)
		{ $cflags="-g"; }
	else
		{ $cflags="-O"; }
	}
$obj='.o';
$ofile='-o ';

# EXE linking stuff
$link='${CC}';
$lflags='${CFLAGS}';
$efile='-o ';
$exep='';
$ex_libs="";

# static library stuff
$mklib='ar r';
$mlflags='';
$ranlib=&which("ranlib") or $ranlib="true";
$plib='lib';
$libp=".a";
$shlibp=".a";
$lfile='';

$asm='as';
$afile='-o ';
$bn_asm_obj="";
$bn_asm_src="";
$des_enc_obj="";
$des_enc_src="";
$bf_enc_obj="";
$bf_enc_src="";

sub do_lib_rule
	{
	local($obj,$target,$name,$shlib)=@_;
	local($ret,$_,$Name);

	$target =~ s/\//$o/g if $o ne '/';
	$target="$target";
	($Name=$name) =~ tr/a-z/A-Z/;

	$ret.="$target: \$(${Name}OBJ)\n";
	$ret.="\t\$(RM) $target\n";
	$ret.="\t\$(MKLIB) $target \$(${Name}OBJ)\n";
	$ret.="\t\$(RANLIB) $target\n\n";
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

sub which
	{
	my ($name)=@_;
	my $path;
	foreach $path (split /:/, $ENV{PATH})
		{
		if (-x "$path/$name")
			{
			return "$path/$name";
			}
		}
	}

1;
