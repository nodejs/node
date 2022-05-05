#! /usr/bin/env perl -w
use 5.10.0;
use strict;
use FindBin;
use lib "$FindBin::Bin/../openssl/";
use lib "$FindBin::Bin/../openssl/util/perl";
use lib "$FindBin::Bin/../openssl/util/perl/OpenSSL";
use File::Basename;
use File::Spec::Functions qw/:DEFAULT abs2rel rel2abs/;
use File::Copy;
use File::Path qw/make_path/;
#use with_fallback qw(Text::Template);
use fallback qw(Text::Template);

# Read configdata from ../openssl/configdata.pm that is generated
# with ../openssl/Configure options arch
use configdata;

my $asm = shift @ARGV;

unless ($asm eq "asm" or $asm eq "asm_avx2" or $asm eq "no-asm") {
  die "Error: $asm is invalid argument";
}
my $arch = shift @ARGV;

# nasm version check
my $nasm_banner = `nasm -v`;
die "Error: nasm is not installed." if (!$nasm_banner);

# gas version check
my $gas_version_min = 2.30;
my $gas_banner = `gcc -Wa,-v -c -o /dev/null -x assembler /dev/null 2>&1`;
my ($gas_version) = ($gas_banner =~/GNU assembler version ([2-9]\.[0-9]+)/);
if ($gas_version < $gas_version_min) {
  die "Error: gas version $gas_version is too old." .
    "$gas_version_min or higher is required.";
}

our $src_dir = "../openssl";
our $arch_dir = "../config/archs/$arch";
our $arch_common_dir = "$arch_dir/common";
our $base_dir = "$arch_dir/$asm";

my $is_win = ($arch =~/^VC-WIN/);
# VC-WIN32 and VC-WIN64A generate makefile but it can be available
# with only nmake. Use pre-created Makefile_VC_WIN32
# Makefile_VC-WIN64A instead.
my $makefile = $is_win ? "../config/Makefile_$arch": "Makefile";
# Generate arch dependent header files with Makefile
my $buildinf = "crypto/buildinf.h";
my $progs = "apps/progs.h";
my $prov_headers = "providers/common/include/prov/der_dsa.h providers/common/include/prov/der_wrap.h providers/common/include/prov/der_rsa.h providers/common/include/prov/der_ecx.h providers/common/include/prov/der_sm2.h providers/common/include/prov/der_ec.h providers/common/include/prov/der_digests.h";
my $fips_ld = ($arch =~ m/linux/ ? "providers/fips.ld" : "");
my $cmd1 = "cd ../openssl; make -f $makefile clean build_generated $buildinf $progs $prov_headers $fips_ld;";
system($cmd1) == 0 or die "Error in system($cmd1)";

# Copy and move all arch dependent header files into config/archs
make_path("$base_dir/include/openssl",
	  "$base_dir/include/crypto",
	  "$base_dir/providers/common/include/prov",
	  "$base_dir/apps",
          {
           error => \my $make_path_err});

if (not $is_win) {
make_path("$arch_common_dir/include/openssl",
	  "$arch_common_dir/include/crypto",
	  "$arch_common_dir/providers/common/include/prov",
          {
           error => \my $make_path_err});
}

if (@$make_path_err) {
  for my $diag (@$make_path_err) {
    my ($file, $message) = %$diag;
    die "make_path error: $file $message\n";
  }
}
copy("$src_dir/configdata.pm", "$base_dir/") or die "Copy failed: $!";

my @openssl_dir_headers = split / /, shift @ARGV;
copy_openssl_headers(\@openssl_dir_headers);

my @crypto_dir_headers = split / /, shift @ARGV;
copy_crypto_headers(\@crypto_dir_headers);

my @providers_headers = (
  "$src_dir/providers/common/include/prov/der_dsa.h",
  "$src_dir/providers/common/include/prov/der_wrap.h",
  "$src_dir/providers/common/include/prov/der_rsa.h",
  "$src_dir/providers/common/include/prov/der_ecx.h",
  "$src_dir/providers/common/include/prov/der_sm2.h",
  "$src_dir/providers/common/include/prov/der_ec.h",
  "$src_dir/providers/common/include/prov/der_digests.h");
copy_provider_headers(\@providers_headers);

copy("$src_dir/$buildinf",
     "$base_dir/crypto/") or die "Copy failed: $!";
move("$src_dir/$progs",
     "$base_dir/include") or die "Copy failed: $!";
copy("$src_dir/apps/progs.c",
     "$base_dir/apps") or die "Copy failed: $!";

my $linker_script_dir = "<(PRODUCT_DIR)/../../deps/openssl/config/archs/$arch/$asm/providers";
my $fips_linker_script = "";
if ($fips_ld ne "") {
  $fips_linker_script = "$linker_script_dir/fips.ld";
  copy("$src_dir/providers/fips.ld",
       "$base_dir/providers/fips.ld") or die "Copy failed: $!";
}


# read openssl source lists from configdata.pm
my @libapps_srcs = ();
foreach my $obj (@{$unified_info{sources}->{'apps/libapps.a'}}) {
    #print("libapps ${$unified_info{sources}->{$obj}}[0]\n");
    push(@libapps_srcs, ${$unified_info{sources}->{$obj}}[0]);
}

my @libssl_srcs = ();
foreach my $obj (@{$unified_info{sources}->{libssl}}) {
  push(@libssl_srcs, ${$unified_info{sources}->{$obj}}[0]);
}

my %generated_srcs_hash;
my %libcrypto_srcs_hash;
my @libcrypto_srcs = ();
my @generated_srcs = ();
foreach my $obj (@{$unified_info{sources}->{'libcrypto'}}) {
  my $src = ${$unified_info{sources}->{$obj}}[0];
  # .S files should be preprocessed into .s
  if ($unified_info{generate}->{$src}) {
    # .S or .s files should be preprocessed into .asm for WIN
    $src =~ s\.[sS]$\.asm\ if ($is_win);
    $generated_srcs_hash{$src} = 1;
  } else {
    if ($src =~ m/\.c$/) { 
      $libcrypto_srcs_hash{$src} = 1;
    }
  }
}

if ($arch eq 'linux32-s390x' || $arch eq  'linux64-s390x') {
  $libcrypto_srcs_hash{'crypto/bn/asm/s390x.S'} = 1;
}

my @lib_defines = ();
foreach my $df (@{$unified_info{defines}->{libcrypto}}) {
  push(@lib_defines, $df);
}


foreach my $obj (@{$unified_info{sources}->{'providers/libdefault.a'}}) {
  my $src = ${$unified_info{sources}->{$obj}}[0];
  # .S files should be preprocessed into .s
  if ($unified_info{generate}->{$src}) {
    # .S or .s files should be preprocessed into .asm for WIN
    $src =~ s\.[sS]$\.asm\ if ($is_win);
    $generated_srcs_hash{$src} = 1;
  } else {
    if ($src =~ m/\.c$/) { 
      $libcrypto_srcs_hash{$src} = 1;
    }
  }
}

foreach my $obj (@{$unified_info{sources}->{'providers/libcommon.a'}}) {
  my $src = ${$unified_info{sources}->{$obj}}[0];
  # .S files should be preprocessed into .s
  if ($unified_info{generate}->{$src}) {
    # .S or .s files should be preprocessed into .asm for WIN
    $src =~ s\.[sS]$\.asm\ if ($is_win);
    $generated_srcs_hash{$src} = 1;
  } else {
    if ($src =~ m/\.c$/) { 
      $libcrypto_srcs_hash{$src} = 1;
    }
  }
}

foreach my $obj (@{$unified_info{sources}->{'providers/liblegacy.a'}}) {
  my $src = ${$unified_info{sources}->{$obj}}[0];
  # .S files should be preprocessed into .s
  if ($unified_info{generate}->{$src}) {
    # .S or .s files should be preprocessed into .asm for WIN
    $src =~ s\.[sS]$\.asm\ if ($is_win);
    $generated_srcs_hash{$src} = 1;
  } else {
    if ($src =~ m/\.c$/) { 
      $libcrypto_srcs_hash{$src} = 1;
    }
  }
}

foreach my $obj (@{$unified_info{sources}->{'providers/legacy'}}) {
  if ($obj eq 'providers/legacy.ld') {
    $generated_srcs_hash{$obj} = 1;
  } else {
    my $src = ${$unified_info{sources}->{$obj}}[0];
    if ($src =~ m/\.c$/) {
      $libcrypto_srcs_hash{$src} = 1;
    }
  }
}

my @libfips_srcs = ();
foreach my $obj (@{$unified_info{sources}->{'providers/libfips.a'}}) {
  my $src = ${$unified_info{sources}->{$obj}}[0];
  # .S files should be preprocessed into .s
  if ($unified_info{generate}->{$src}) {
    # .S or .s files should be preprocessed into .asm for WIN
    #$src =~ s\.[sS]$\.asm\ if ($is_win);
    #push(@generated_srcs, $src);
  } else {
    if ($src =~ m/\.c$/) {
      push(@libfips_srcs, $src);
    }
  }
}

foreach my $obj (@{$unified_info{sources}->{'providers/libcommon.a'}}) {
  my $src = ${$unified_info{sources}->{$obj}}[0];
  #print("providers/libfips.a obj: $obj src: $src \n");
  # .S files should be preprocessed into .s
  if ($unified_info{generate}->{$src}) {
    # .S or .s files should be preprocessed into .asm for WIN
    #$src =~ s\.[sS]$\.asm\ if ($is_win);
    #push(@generated_srcs, $src);
  } else {
    if ($src =~ m/\.c$/) {
      push(@libfips_srcs, $src);
    }
  }
}

foreach my $obj (@{$unified_info{sources}->{'providers/fips'}}) {
  if ($obj eq 'providers/fips.ld') {
    $generated_srcs_hash{$obj} = 1;
  } else {
    my $src = ${$unified_info{sources}->{$obj}}[0];
    #print("providers/fips obj: $obj, src: $src\n");
    if ($src =~ m/\.c$/) {
      push(@libfips_srcs, $src);
    }
  }
}

my @libfips_defines = ();
foreach my $df (@{$unified_info{defines}->{'providers/libfips.a'}}) {
  #print("libfips defines: $df\n");
  push(@libfips_defines, $df);
}

foreach my $df (@{$unified_info{defines}->{'providers/fips'}}) {
  #print("libfips defines: $df\n");
  push(@libfips_defines, $df);
}

my @apps_openssl_srcs = ();
foreach my $obj (@{$unified_info{sources}->{'apps/openssl'}}) {
  push(@apps_openssl_srcs, ${$unified_info{sources}->{$obj}}[0]);
}

@generated_srcs = sort(keys %generated_srcs_hash);
@libcrypto_srcs = sort(keys %libcrypto_srcs_hash);

# Generate all asm files and copy into config/archs
foreach my $src (@generated_srcs) {
  my $cmd = "cd ../openssl; CC=gcc ASM=nasm make -f $makefile $src;" .
    "cp --parents $src ../config/archs/$arch/$asm; cd ../config";
  system("$cmd") == 0 or die "Error in system($cmd)";
}

$target{'lib_cppflags'} =~ s/-D//g;
my @lib_cppflags = split(/ /, $target{'lib_cppflags'});

my @cflags = ();
push(@cflags, @{$config{'cflags'}});
push(@cflags, @{$config{'CFLAGS'}});
push(@cflags, $target{'cflags'});
push(@cflags, $target{'CFLAGS'});

# AIX has own assembler not GNU as that does not support --noexecstack
if ($arch =~ /aix/) {
  @cflags = grep $_ ne '-Wa,--noexecstack', @cflags;
}

# Create openssl.gypi
my $template =
    Text::Template->new(TYPE => 'FILE',
                        SOURCE => 'openssl.gypi.tmpl',
                        DELIMITERS => [ "%%-", "-%%" ]
                        );
my $gypi = $template->fill_in(
    HASH => {
        libssl_srcs => \@libssl_srcs,
        libcrypto_srcs => \@libcrypto_srcs,
        lib_defines => \@lib_defines,
        generated_srcs => \@generated_srcs,
        config => \%config,
        target => \%target,
        cflags => \@cflags,
        asm => \$asm,
        arch => \$arch,
        lib_cppflags => \@lib_cppflags,
        is_win => \$is_win,
    });

open(GYPI, "> ./archs/$arch/$asm/openssl.gypi");
print GYPI "$gypi";
close(GYPI);
#
# Create openssl-fips.gypi
my $fipstemplate =
    Text::Template->new(TYPE => 'FILE',
                        SOURCE => 'openssl-fips.gypi.tmpl',
                        DELIMITERS => [ "%%-", "-%%" ]
                        );
my $fipsgypi = $fipstemplate->fill_in(
    HASH => {
        libfips_srcs => \@libfips_srcs,
        libfips_defines => \@libfips_defines,
        generated_srcs => \@generated_srcs,
        config => \%config,
        target => \%target,
        cflags => \@cflags,
        asm => \$asm,
        arch => \$arch,
        lib_cppflags => \@lib_cppflags,
        is_win => \$is_win,
	linker_script => $fips_linker_script,
    });

open(FIPSGYPI, "> ./archs/$arch/$asm/openssl-fips.gypi");
print FIPSGYPI "$fipsgypi";
close(FIPSGYPI);

# Create openssl-cl.gypi
my $cltemplate =
    Text::Template->new(TYPE => 'FILE',
                        SOURCE => 'openssl-cl.gypi.tmpl',
                        DELIMITERS => [ "%%-", "-%%" ]
                        );

my $clgypi = $cltemplate->fill_in(
    HASH => {
        apps_openssl_srcs => \@apps_openssl_srcs,
        lib_defines => \@lib_defines,
        libapps_srcs => \@libapps_srcs,
        config => \%config,
        target => \%target,
        cflags => \@cflags,
        asm => \$asm,
        arch => \$arch,
        lib_cppflags => \@lib_cppflags,
        is_win => \$is_win,
    });

open(CLGYPI, "> ./archs/$arch/$asm/openssl-cl.gypi");
print CLGYPI "$clgypi";
close(CLGYPI);

# Clean Up
my $cmd2 ="cd $src_dir; make -f $makefile clean; make -f $makefile distclean;" .
    "git clean -f $src_dir/crypto";
system($cmd2) == 0 or die "Error in system($cmd2)";

sub copy_provider_headers {
  my @headers = @{$_[0]};
  my $include_path = 'providers/common/include/prov';
  my $common_include_path = "../../../../../common/";
  copy_headers(\@headers, $include_path, $common_include_path);
}

sub copy_openssl_headers {
  my @headers = @{$_[0]};
  my $include_path = 'include/openssl';
  my $common_include_path = '../../../common/';
  foreach my $header (@headers) {
    $header =  $src_dir . "/$include_path/${header}.h";
  }
  copy_headers(\@headers, $include_path, $common_include_path);
}

sub copy_crypto_headers {
  my @headers = @{$_[0]};
  my $include_path = 'include/crypto';
  my $common_include_path = '../../../common/';
  foreach my $header (@headers) {
    $header =  $src_dir . "/$include_path/${header}.h";
  }
  copy_headers(\@headers, $include_path, $common_include_path);
}

sub copy_headers {
  my @headers = @{$_[0]};
  my $include_path = $_[1];
  my $common_include_path = $_[2];

  my $arch_header_include_template = Text::Template->new(
      TYPE => 'FILE',
      SOURCE => 'arch_include.h.tmpl',
      DELIMITERS => [ "%%-", "-%%" ]);
  foreach my $src_header_file (@headers) {
    my $header_file_name = basename($src_header_file);
    if ($is_win || $header_file_name eq "configuration.h" ||
        $header_file_name eq "conf.h") {
      print("copy header $src_header_file to $base_dir/$include_path \n");
      copy($src_header_file,
           "$base_dir/$include_path/") or die "Copy failed: $!";
    } else {
      my($header_path) = $include_path . '/' . $header_file_name;
      print("copy header $src_header_file to $arch_common_dir/$include_path \n");
      copy($src_header_file,
           "$arch_common_dir/$include_path") or die "Copy failed: $!";
      my $arch_header_include = $arch_header_include_template->fill_in(
         HASH => {
             path => "$common_include_path/$header_path",
          });
      open(ARCH_HEADER_INC, "> $base_dir/$header_path");
      print ARCH_HEADER_INC $arch_header_include;
      close(ARCH_HEADER_INC);
    }
  }
}
