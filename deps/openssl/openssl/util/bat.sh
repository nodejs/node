#!/usr/local/bin/perl

$infile="/home/eay/ssl/SSLeay/MINFO";

open(IN,"<$infile") || die "unable to open $infile:$!\n";
$_=<IN>;
for (;;)
	{
	chop;

	($key,$val)=/^([^=]+)=(.*)/;
	if ($key eq "RELATIVE_DIRECTORY")
		{
		if ($lib ne "")
			{
			$uc=$lib;
			$uc =~ s/^lib(.*)\.a/$1/;
			$uc =~ tr/a-z/A-Z/;
			$lib_nam{$uc}=$uc;
			$lib_obj{$uc}.=$libobj." ";
			}
		last if ($val eq "FINISHED");
		$lib="";
		$libobj="";
		$dir=$val;
		}

	if ($key eq "TEST")
		{ $test.=&var_add($dir,$val); }

	if (($key eq "PROGS") || ($key eq "E_OBJ"))
		{ $e_exe.=&var_add($dir,$val); }

	if ($key eq "LIB")
		{
		$lib=$val;
		$lib =~ s/^.*\/([^\/]+)$/$1/;
		}

	if ($key eq "EXHEADER")
		{ $exheader.=&var_add($dir,$val); }

	if ($key eq "HEADER")
		{ $header.=&var_add($dir,$val); }

	if ($key eq "LIBSRC")
		{ $libsrc.=&var_add($dir,$val); }

	if (!($_=<IN>))
		{ $_="RELATIVE_DIRECTORY=FINISHED\n"; }
	}
close(IN);

@a=split(/\s+/,$libsrc);
foreach (@a)
	{
	print "${_}.c\n";
	}

sub var_add
	{
	local($dir,$val)=@_;
	local(@a,$_,$ret);

	return("") if $no_engine && $dir =~ /\/engine/;
	return("") if $no_idea && $dir =~ /\/idea/;
	return("") if $no_rc2  && $dir =~ /\/rc2/;
	return("") if $no_rc4  && $dir =~ /\/rc4/;
	return("") if $no_rsa  && $dir =~ /\/rsa/;
	return("") if $no_rsa  && $dir =~ /^rsaref/;
	return("") if $no_dsa  && $dir =~ /\/dsa/;
	return("") if $no_dh   && $dir =~ /\/dh/;
	if ($no_des && $dir =~ /\/des/)
		{
		if ($val =~ /read_pwd/)
			{ return("$dir/read_pwd "); }
		else
			{ return(""); }
		}
	return("") if $no_mdc2 && $dir =~ /\/mdc2/;
	return("") if $no_sock && $dir =~ /\/proxy/;
	return("") if $no_bf   && $dir =~ /\/bf/;
	return("") if $no_cast && $dir =~ /\/cast/;

	$val =~ s/^\s*(.*)\s*$/$1/;
	@a=split(/\s+/,$val);
	grep(s/\.[och]$//,@a);

	@a=grep(!/^e_.*_3d$/,@a) if $no_des;
	@a=grep(!/^e_.*_d$/,@a) if $no_des;
	@a=grep(!/^e_.*_i$/,@a) if $no_idea;
	@a=grep(!/^e_.*_r2$/,@a) if $no_rc2;
	@a=grep(!/^e_.*_bf$/,@a) if $no_bf;
	@a=grep(!/^e_.*_c$/,@a) if $no_cast;
	@a=grep(!/^e_rc4$/,@a) if $no_rc4;

	@a=grep(!/(^s2_)|(^s23_)/,@a) if $no_ssl2;
	@a=grep(!/(^s3_)|(^s23_)/,@a) if $no_ssl3;

	@a=grep(!/(_sock$)|(_acpt$)|(_conn$)|(^pxy_)/,@a) if $no_sock;

	@a=grep(!/(^md2)|(_md2$)/,@a) if $no_md2;
	@a=grep(!/(^md5)|(_md5$)/,@a) if $no_md5;

	@a=grep(!/(^d2i_r_)|(^i2d_r_)/,@a) if $no_rsa;
	@a=grep(!/(^p_open$)|(^p_seal$)/,@a) if $no_rsa;
	@a=grep(!/(^pem_seal$)/,@a) if $no_rsa;

	@a=grep(!/(m_dss$)|(m_dss1$)/,@a) if $no_dsa;
	@a=grep(!/(^d2i_s_)|(^i2d_s_)|(_dsap$)/,@a) if $no_dsa;

	@a=grep(!/^n_pkey$/,@a) if $no_rsa || $no_rc4;

	@a=grep(!/_dhp$/,@a) if $no_dh;

	@a=grep(!/(^sha[^1])|(_sha$)|(m_dss$)/,@a) if $no_sha;
	@a=grep(!/(^sha1)|(_sha1$)|(m_dss1$)/,@a) if $no_sha1;
	@a=grep(!/_mdc2$/,@a) if $no_mdc2;

	@a=grep(!/^engine$/,@a) if $no_engine;
	@a=grep(!/(^rsa$)|(^genrsa$)|(^req$)|(^ca$)/,@a) if $no_rsa;
	@a=grep(!/(^dsa$)|(^gendsa$)|(^dsaparam$)/,@a) if $no_dsa;
	@a=grep(!/^gendsa$/,@a) if $no_sha1;
	@a=grep(!/(^dh$)|(^gendh$)/,@a) if $no_dh;

	@a=grep(!/(^dh)|(_sha1$)|(m_dss1$)/,@a) if $no_sha1;

	grep($_="$dir/$_",@a);
	@a=grep(!/(^|\/)s_/,@a) if $no_sock;
	@a=grep(!/(^|\/)bio_sock/,@a) if $no_sock;
	$ret=join(' ',@a)." ";
	return($ret);
	}

