#! /usr/bin/env perl -w
use strict;
use File::Copy;
use File::Path qw(make_path);

# Read configdata from ../openssl/configdata.pm that is generated
# with ../openssl/Configure options arch
use configdata;

my $asm = $ARGV[0];
unless ($asm eq "asm" or $asm eq "no-asm") {
  die "Error: $asm is invalid argument";
}
my $arch = $ARGV[1];

# nasm version check
my $nasm_banner = `nasm -v`;
die "Error: nasm is not installed." if (!$nasm_banner);

my $nasm_version_min = 2.10;
my ($nasm_version) = ($nasm_banner =~/^NASM version ([0-9]\.[0-9][0-9])+/);
if ($nasm_version < $nasm_version_min) {
  die "Error: nasm version $nasm_version is too old." .
    "$nasm_version_min or higher is required.";
}

my $src_dir = "../openssl";
my $base_dir = "../config/archs/$arch/$asm";

my $is_win = ($arch =~/^VC-WIN/);
# VC-WIN32 and VC-WIN64A generate makefile but it can be available
# with only nmake. Use pre-created Makefile_VC_WIN32
# Makefile_VC-WIN64A instead.
my $makefile = $is_win ? "../config/Makefile_$arch": "Makefile";

# Generate arch dependent header files with Makefile
my $cmd1 = "cd ../openssl; make -f $makefile build_generated crypto/buildinf.h;";
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
move("$src_dir/crypto/include/internal/bn_conf.h",
     "$base_dir/crypto/include/internal/") or die "Move failed: $!";
move("$src_dir/crypto/include/internal/dso_conf.h",
     "$base_dir/crypto/include/internal/") or die "Move failed: $!";
copy("$src_dir/crypto/buildinf.h",
     "$base_dir/crypto/") or die "Copy failed: $!";

# read openssl source lists from configdata.pm
my @libssl_srcs = ();
foreach my $obj (@{$unified_info{sources}->{libssl}}) {
  push(@libssl_srcs, ${$unified_info{sources}->{$obj}}[0]);
}

my @libcrypto_srcs = ();
my @generated_srcs = ();
foreach my $obj (@{$unified_info{sources}->{libcrypto}}) {
  my $src = ${$unified_info{sources}->{$obj}}[0];
  # .S files should be proprocessed into .s
  if ($unified_info{generate}->{$src}) {
    # .S or .s files should be proprocessed into .asm for WIN
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

# Create openssl.gypi
open(GYPI, "> ../config/archs/$arch/$asm/openssl.gypi");

print GYPI << 'GYPI1';
{
  'variables': {
    'openssl_sources': [
GYPI1

foreach my $src (@libssl_srcs) {
  print GYPI "      'openssl/$src',\n";
}

foreach my $src (@libcrypto_srcs) {
  print GYPI "      'openssl/$src',\n";
}

print GYPI << "GYPI2";
    ],
    'openssl_sources_$arch': [
GYPI2

foreach my $src (@generated_srcs) {
  print GYPI "      './config/archs/$arch/$asm/$src',\n";
}

print GYPI << "GYP3";
    ],
    'openssl_defines_$arch': [
GYP3

foreach my $define (@{$config{defines}}) {
  print GYPI "      '$define',\n";
}

print GYPI << "GYP4";
    ],
    'openssl_cflags_$arch': [
      '$target{cflags}',
    ],
    'openssl_ex_libs_$arch': [
      '$target{ex_libs}',
    ],
  },
  'include_dirs': [
    '.',
    './include',
    './crypto',
    './crypto/include/internal',
  ],
  'defines': ['<@(openssl_defines_$arch)'],
GYP4

if (!$is_win) {
  print GYPI "  'cflags' : ['<@(openssl_cflags_$arch)'],\n";
  print GYPI "  'libraries': ['<@(openssl_ex_libs_$arch)'],\n";
}

print GYPI << "GYPI5";
  'sources': ['<@(openssl_sources)', '<@(openssl_sources_$arch)'],
}
GYPI5

close(GYPI);

# Create openssl-cl.gypi
open(CLGYPI, "> ../config/archs/$arch/$asm/openssl-cl.gypi");

print CLGYPI << "CLGYPI1";
{
  'variables': {
    'openssl_defines_$arch': [
CLGYPI1

foreach my $define (@{$config{defines}}) {
  print CLGYPI "      '$define',\n";
}

print CLGYPI << "CLGYPI2";
    ],
    'openssl_cflags_$arch': [
      '$target{cflags}',
    ],
    'openssl_ex_libs_$arch': [
      '$target{ex_libs}',
    ],
    'openssl_cli_srcs_$arch': [
CLGYPI2

foreach my $src (@apps_openssl_srcs) {
  print CLGYPI "      'openssl/$src',\n";
}

print CLGYPI << "CLGYPI3";
    ],
  },
  'defines': ['<@(openssl_defines_$arch)'],
CLGYPI3

if (!$is_win) {
  print CLGYPI "  'cflags' : ['<@(openssl_cflags_$arch)'],\n";
  print CLGYPI "  'libraries': ['<@(openssl_ex_libs_$arch)'],\n";
}

print CLGYPI << "CLGYPI4";
  'sources': ['<@(openssl_cli_srcs_$arch)'],
}
CLGYPI4

close(CLGYPI);

# Clean Up
my $cmd2 ="cd $src_dir; make -f $makefile clean; make -f $makefile distclean;" .
  "find $src_dir/crypto \\( -name \*.S -o -name \*.s -o -name \*.asm \\)" .
  " -exec rm '{}' \\;";
system($cmd2) == 0 or die "Error in system($cmd2)";
