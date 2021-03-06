#! /usr/bin/env perl -w
use 5.10.0;
use strict;
use FindBin;
use lib "$FindBin::Bin/../openssl/";
use lib "$FindBin::Bin/../openssl/util/perl";
use File::Basename;
use File::Spec::Functions qw/:DEFAULT abs2rel rel2abs/;
use File::Copy;
use File::Path qw/make_path/;
use with_fallback qw(Text::Template);

# Read configdata from ../openssl/configdata.pm that is generated
# with ../openssl/Configure options arch
use configdata;

my $asm = $ARGV[0];

unless ($asm eq "asm" or $asm eq "asm_avx2" or $asm eq "no-asm") {
  die "Error: $asm is invalid argument";
}
my $arch = $ARGV[1];

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

my $src_dir = "../openssl";
my $arch_dir = "../config/archs/$arch";
my $base_dir = "$arch_dir/$asm";

my $is_win = ($arch =~/^VC-WIN/);
# VC-WIN32 and VC-WIN64A generate makefile but it can be available
# with only nmake. Use pre-created Makefile_VC_WIN32
# Makefile_VC-WIN64A instead.
my $makefile = $is_win ? "../config/Makefile_$arch": "Makefile";
# Generate arch dependent header files with Makefile
my $buildinf = "crypto/buildinf.h";
my $progs = "apps/progs.h";
my $cmd1 = "cd ../openssl; make -f $makefile clean build_generated $buildinf $progs;";
system($cmd1) == 0 or die "Error in system($cmd1)";

# Copy and move all arch dependent header files into config/archs
make_path("$base_dir/crypto/include/internal", "$base_dir/include/openssl",
          {
           error => \my $make_path_err});
if (@$make_path_err) {
  for my $diag (@$make_path_err) {
    my ($file, $message) = %$diag;
    die "make_path error: $file $message\n";
  }
}
copy("$src_dir/configdata.pm", "$base_dir/") or die "Copy failed: $!";
copy("$src_dir/include/openssl/opensslconf.h",
     "$base_dir/include/openssl/") or die "Copy failed: $!";
move("$src_dir/include/crypto/bn_conf.h",
     "$base_dir/crypto/include/internal/") or die "Move failed: $!";
move("$src_dir/include/crypto/dso_conf.h",
     "$base_dir/crypto/include/internal/") or die "Move failed: $!";
copy("$src_dir/$buildinf",
     "$base_dir/crypto/") or die "Copy failed: $!";
move("$src_dir/$progs",
     "$base_dir/include") or die "Copy failed: $!";

# read openssl source lists from configdata.pm
my @libapps_srcs = ();
foreach my $obj (@{$unified_info{sources}->{'apps/libapps.a'}}) {
    push(@libapps_srcs, ${$unified_info{sources}->{$obj}}[0]);
}

my @libssl_srcs = ();
foreach my $obj (@{$unified_info{sources}->{libssl}}) {
  push(@libssl_srcs, ${$unified_info{sources}->{$obj}}[0]);
}

my @libcrypto_srcs = ();
my @generated_srcs = ();
foreach my $obj (@{$unified_info{sources}->{libcrypto}}) {
  my $src = ${$unified_info{sources}->{$obj}}[0];
  # .S files should be preprocessed into .s
  if ($unified_info{generate}->{$src}) {
    # .S or .s files should be preprocessed into .asm for WIN
    $src =~ s\.[sS]$\.asm\ if ($is_win);
    push(@generated_srcs, $src);
  } else {
    push(@libcrypto_srcs, $src);
  }
}

my @apps_openssl_srcs = ();
foreach my $obj (@{$unified_info{sources}->{'apps/openssl'}}) {
  push(@apps_openssl_srcs, ${$unified_info{sources}->{$obj}}[0]);
}

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

# Create openssl-cl.gypi
my $cltemplate =
    Text::Template->new(TYPE => 'FILE',
                        SOURCE => 'openssl-cl.gypi.tmpl',
                        DELIMITERS => [ "%%-", "-%%" ]
                        );

my $clgypi = $cltemplate->fill_in(
    HASH => {
        apps_openssl_srcs => \@apps_openssl_srcs,
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
