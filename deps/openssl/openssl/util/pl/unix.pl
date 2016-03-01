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
$asm_suffix='.s';
$ofile='-o ';

# EXE linking stuff
$link='${CC}';
$lflags='${CFLAG}';
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

%perl1 = (
	  'md5-x86_64' => 'crypto/md5',
	  'x86_64-mont' => 'crypto/bn',
	  'x86_64-mont5' => 'crypto/bn',
	  'x86_64-gf2m' => 'crypto/bn',
	  'aes-x86_64' => 'crypto/aes',
	  'vpaes-x86_64' => 'crypto/aes',
	  'bsaes-x86_64' => 'crypto/aes',
	  'aesni-x86_64' => 'crypto/aes',
	  'aesni-sha1-x86_64' => 'crypto/aes',
	  'sha1-x86_64' => 'crypto/sha',
	  'e_padlock-x86_64' => 'engines',
	  'rc4-x86_64' => 'crypto/rc4',
	  'rc4-md5-x86_64' => 'crypto/rc4',
	  'ghash-x86_64' => 'crypto/modes',
	  'aesni-gcm-x86_64' => 'crypto/modes',
	  'aesni-sha256-x86_64' => 'crypto/aes',
          'rsaz-x86_64' => 'crypto/bn',
          'rsaz-avx2' => 'crypto/bn',
	  'aesni-mb-x86_64' => 'crypto/aes',
	  'sha1-mb-x86_64' => 'crypto/sha',
	  'sha256-mb-x86_64' => 'crypto/sha',
	  'ecp_nistz256-x86_64' => 'crypto/ec',
         );

# If I were feeling more clever, these could probably be extracted
# from makefiles.
sub platform_perlasm_compile_target
	{
	local($target, $source, $bname) = @_;

	for $p (keys %perl1)
	        {
		if ($target eq "\$(OBJ_D)/$p.o")
		        {
			return << "EOF";
\$(TMP_D)/$p.s: $perl1{$p}/asm/$p.pl
	\$(PERL) $perl1{$p}/asm/$p.pl \$(PERLASM_SCHEME) > \$@
EOF
		        }
	        }
	if ($target eq '$(OBJ_D)/x86_64cpuid.o')
		{
		return << 'EOF';
$(TMP_D)/x86_64cpuid.s: crypto/x86_64cpuid.pl
	$(PERL) crypto/x86_64cpuid.pl $(PERLASM_SCHEME) > $@
EOF
		}
	elsif ($target eq '$(OBJ_D)/sha256-x86_64.o')
		{
		return << 'EOF';
$(TMP_D)/sha256-x86_64.s: crypto/sha/asm/sha512-x86_64.pl
	$(PERL) crypto/sha/asm/sha512-x86_64.pl $(PERLASM_SCHEME) $@
EOF
	        }
	elsif ($target eq '$(OBJ_D)/sha512-x86_64.o')
		{
		return << 'EOF';
$(TMP_D)/sha512-x86_64.s: crypto/sha/asm/sha512-x86_64.pl
	$(PERL) crypto/sha/asm/sha512-x86_64.pl $(PERLASM_SCHEME) $@
EOF
	        }
	elsif ($target eq '$(OBJ_D)/sha512-x86_64.o')
		{
		return << 'EOF';
$(TMP_D)/sha512-x86_64.s: crypto/sha/asm/sha512-x86_64.pl
	$(PERL) crypto/sha/asm/sha512-x86_64.pl $(PERLASM_SCHEME) $@
EOF
	        }

	die $target;
	}

sub special_compile_target
	{
	local($target) = @_;

	if ($target eq 'crypto/bn/x86_64-gcc')
		{
		return << "EOF";
\$(TMP_D)/x86_64-gcc.o:	crypto/bn/asm/x86_64-gcc.c
	\$(CC) \$(CFLAGS) -c -o \$@ crypto/bn/asm/x86_64-gcc.c
EOF
		}
	return undef;
	}

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

sub fixtests
  {
  my ($str, $tests) = @_;

  foreach my $t (keys %$tests)
    {
    $str =~ s/(\.\/)?\$\($t\)/\$(TEST_D)\/$tests->{$t}/g;
    }

  return $str;
  }

sub fixdeps
 {
  my ($str, $fakes) = @_;

  my @t = split(/\s+/, $str);
  $str = '';
  foreach my $t (@t)
    {
    $str .= ' ' if $str ne '';
    if (exists($fakes->{$t}))
      {
      $str .= $fakes->{$t};
      next;
      }
    if ($t =~ /^[^\/]+$/)
      {
      $str .= '$(TEST_D)/' . $t;
      }
    else
      {
      $str .= $t;
      }
    }

  return $str;
  }

sub fixrules
  {
  my ($str) = @_;

  # Compatible with -j...
  $str =~ s/^(\s+@?)/$1cd \$(TEST_D) && /;
  return $str;

  # Compatible with not -j.
  my @t = split("\n", $str);
  $str = '';
  my $prev;
  foreach my $t (@t)
    {
    $t =~ s/^\s+//;
    if (!$prev)
      {
      if ($t =~ /^@/)
	{
        $t =~ s/^@/\@cd \$(TEST_D) && /;
        }
      elsif ($t !~ /^\s*#/)
	{
        $t = 'cd $(TEST_D) && ' . $t;
        }
      }
    $str .= "\t$t\n";
    $prev = $t =~/\\$/;
    }
  return $str;
}

sub copy_scripts
  {
  my ($sed, $src, @targets) = @_;

  my $s = '';
  foreach my $t (@targets)
    {
    # Copy first so we get file modes...
    $s .= "\$(TEST_D)/$t: \$(SRC_D)/$src/$t\n\tcp \$(SRC_D)/$src/$t \$(TEST_D)/$t\n";
    $s .= "\tsed -e 's/\\.\\.\\/apps/..\\/\$(OUT_D)/' -e 's/\\.\\.\\/util/..\\/\$(TEST_D)/' < \$(SRC_D)/$src/$t > \$(TEST_D)/$t\n" if $sed;
    $s .= "\n";
    }
  return $s;
  }

sub get_tests
  {
  my ($makefile) = @_;

  open(M, $makefile) || die "Can't open $makefile: $!";
  my %targets;
  my %deps;
  my %tests;
  my %alltests;
  my %fakes;
  while (my $line = <M>)
    {
    chomp $line;
    while ($line =~ /^(.*)\\$/)
      {
      $line = $1 . <M>;
      }

    if ($line =~ /^alltests:(.*)$/)
      {
      my @t = split(/\s+/, $1);
      foreach my $t (@t)
	{
	$targets{$t} = '';
	$alltests{$t} = undef;
        }
      }

    if (($line =~ /^(?<t>\S+):(?<d>.*)$/ && exists $targets{$1})
	|| $line =~ /^(?<t>test_(ss|gen) .*):(?<d>.*)/)
      {
      my $t = $+{t};
      my $d = $+{d};
      # If there are multiple targets stupid FreeBSD make runs the
      # rules once for each dependency that matches one of the
      # targets. Running the same rule twice concurrently causes
      # breakage, so replace with a fake target.
      if ($t =~ /\s/)
        {
	++$fake;
	my @targets = split /\s+/, $t;
	$t = "_fake$fake";
	foreach my $f (@targets)
	  {
	  $fakes{$f} = $t;
	  }
	}
      $deps{$t} = $d;
      $deps{$t} =~ s/#.*$//;
      for (;;)
	{
	$line = <M>;
	chomp $line;
	last if $line eq '';
	$targets{$t} .= "$line\n";
        }
      next;
      }

    if ($line =~ /^(\S+TEST)=\s*(\S+)$/)
      {
      $tests{$1} = $2;
      next;
      }
    }

  delete $alltests{test_jpake} if $no_jpake;
  delete $targets{test_ige} if $no_ige;
  delete $alltests{test_md2} if $no_md2;
  delete $alltests{test_rc5} if $no_rc5;

  my $tests;
  foreach my $t (keys %tests)
    {
    $tests .= "$t = $tests{$t}\n";
    }

  my $each;
  foreach my $t (keys %targets)
    {
    next if $t eq '';

    my $d = $deps{$t};
    $d =~ s/\.\.\/apps/\$(BIN_D)/g;
    $d =~ s/\.\.\/util/\$(TEST_D)/g;
    $d = fixtests($d, \%tests);
    $d = fixdeps($d, \%fakes);

    my $r = $targets{$t};
    $r =~ s/\.\.\/apps/..\/\$(BIN_D)/g;
    $r =~ s/\.\.\/util/..\/\$(TEST_D)/g;
    $r =~ s/\.\.\/(\S+)/\$(SRC_D)\/$1/g;
    $r = fixrules($r);

    next if $r eq '';

    $t =~ s/\s+/ \$(TEST_D)\//g;

    $each .= "$t: test_scripts $d\n\t\@echo '$t test started'\n$r\t\@echo '$t test done'\n\n";
    }

  # FIXME: Might be a clever way to figure out what needs copying
  my @copies = ( 'bctest',
		 'testgen',
		 'cms-test.pl',
		 'tx509',
		 'test.cnf',
		 'testenc',
		 'tocsp',
		 'testca',
		 'CAss.cnf',
		 'testtsa',
		 'CAtsa.cnf',
		 'Uss.cnf',
		 'P1ss.cnf',
		 'P2ss.cnf',
		 'tcrl',
		 'tsid',
		 'treq',
		 'tpkcs7',
		 'tpkcs7d',
		 'testcrl.pem',
		 'testx509.pem',
		 'v3-cert1.pem',
		 'v3-cert2.pem',
		 'testreq2.pem',
		 'testp7.pem',
		 'pkcs7-1.pem',
		 'trsa',
		 'testrsa.pem',
		 'testsid.pem',
		 'testss',
		 'testssl',
		 'testsslproxy',
		 'serverinfo.pem',
	       );
  my $copies = copy_scripts(1, 'test', @copies);
  $copies .= copy_scripts(0, 'test', ('smcont.txt'));

  my @utils = ( 'shlib_wrap.sh',
		'opensslwrap.sh',
	      );
  $copies .= copy_scripts(1, 'util', @utils);

  my @apps = ( 'CA.sh',
	       'openssl.cnf',
	       'server2.pem',
	     );
  $copies .= copy_scripts(1, 'apps', @apps);

  $copies .= copy_scripts(1, 'crypto/evp', ('evptests.txt'));

  $scripts = "test_scripts: \$(TEST_D)/CA.sh \$(TEST_D)/opensslwrap.sh \$(TEST_D)/openssl.cnf \$(TEST_D)/shlib_wrap.sh ocsp smime\n";
  $scripts .= "\nocsp:\n\tcp -R test/ocsp-tests \$(TEST_D)\n";
  $scripts .= "\smime:\n\tcp -R test/smime-certs \$(TEST_D)\n";

  my $all = 'test:';
  foreach my $t (keys %alltests)
    {
    if (exists($fakes{$t}))
      {
      $all .= " $fakes{$t}";
      }
    else
      {
      $all .= " $t";
      }
    }

  return "$scripts\n$copies\n$tests\n$all\n\n$each";
  }

1;
