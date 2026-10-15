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
use File::Compare;
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
our $base_dir = "$arch_dir/$asm";

# These generators use source data rather than the target configuration.
my %common_headers = map { $_ => 1 } qw(
  crypto/asn1/charmap.h
  crypto/bn/bn_prime.h
  crypto/conf/conf_def.h
  crypto/objects/obj_dat.h
  crypto/objects/obj_xref.h
  include/crypto/ec_params.h
  include/crypto/rsa_params.h
  include/openssl/core_names.h
  include/openssl/obj_mac.h
);
my %common_files = map { $_ => 1 } grep {
  $common_headers{$_}
    || $unified_info{generate}->{$_}->[0] eq 'util/mkerr.pl'
    || (m|^providers/.*\.inc$| && grep {
      m{^util/perl\|OpenSSL/(?:paramnames|fipsparams)\.pm$}
    } @{$unified_info{depends}->{$_} || []})
} keys %{$unified_info{generate}};

my $is_win = ($arch =~/^VC-WIN/);
# VC-WIN32 and VC-WIN64A generate makefile but it can be available
# with only nmake. Use pre-created Makefile_VC_WIN32
# Makefile_VC-WIN64A instead.
my $makefile = $is_win ? "../config/Makefile_$arch": "Makefile";
generate_windows_makefile($makefile) if $is_win;
# Generate arch dependent header files with Makefile
my $buildinf = "crypto/buildinf.h";
my $progs = "apps/progs.h";
my @generated_headers = sort grep { /\.h$/ && $_ ne $progs }
    keys %{$unified_info{generate}};
my $generated_headers = join(' ', @generated_headers);
my $fips_ld = ($arch =~ m/linux/ ? "providers/fips.ld" : "");
my $cmd1 = "cd ../openssl; make -f $makefile clean build_generated $buildinf $progs $generated_headers $fips_ld;";
system($cmd1) == 0 or die "Error in system($cmd1)";

# Copy and move all arch dependent header files into config/archs
make_path("$base_dir/crypto/include/internal", "$base_dir/include/openssl",
	  "$base_dir/include/crypto", "$base_dir/include/internal",
	  "$base_dir/providers/common/include/prov", "$base_dir/apps",
          {
           error => \my $make_path_err});
if (@$make_path_err) {
  for my $diag (@$make_path_err) {
    my ($file, $message) = %$diag;
    die "make_path error: $file $message\n";
  }
}
copy("$src_dir/configdata.pm", "$base_dir/") or die "Copy failed: $!";

my @openssl_dir_headers = merge_headers(shift @ARGV, 'openssl');
copy_headers(@openssl_dir_headers, 'openssl');

my @crypto_dir_headers = merge_headers(shift @ARGV, 'crypto');
copy_headers(@crypto_dir_headers, 'crypto');

my @internal_dir_headers = merge_headers(shift @ARGV, 'internal');
copy_headers(@internal_dir_headers, 'internal');

move("$src_dir/include/crypto/dso_conf.h",
     "$base_dir/include/crypto/dso_conf.h") or die "Move failed: $!";

copy("$src_dir/$buildinf",
     "$base_dir/crypto/") or die "Copy failed: $!";
move("$src_dir/$progs",
     "$base_dir/include") or die "Copy failed: $!";
copy("$src_dir/apps/progs.c",
     "$base_dir/apps") or die "Copy failed: $!";
make_path("$base_dir/apps/include");
copy("$src_dir/apps/include/configuration.h", "$base_dir/apps/include/")
    or die "Copy apps/include/configuration.h failed: $!";

# OpenSSL 4.x generates provider parameter decoders alongside its sources.
my @generated_includes = sort grep { /\.inc$/ }
    keys %{$unified_info{generate}};
my %generated_include_dirs = ('../../../common' => 1);
foreach my $include (grep { !m|^include/| || $common_files{$_} }
                     @generated_headers, @generated_includes) {
  my $dir = dirname($include);
  copy_generated($include);
  my $include_dir = $dir =~ m{^(.*?/include)(?:/|$)} ? $1 : $dir;
  if ($common_files{$include}) {
    $generated_include_dirs{"../../../common/$include_dir"} = 1;
  } else {
    $generated_include_dirs{"./$include_dir"} = 1
        unless $include_dir eq 'providers/common/include';
  }
}
my @generated_include_dirs = sort keys %generated_include_dirs;

my $linker_script_dir = "<(PRODUCT_DIR)/../../deps/openssl/config/archs/$arch/$asm/providers";
my $fips_linker_script = "";
if ($fips_ld ne "" and not $is_win) {
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

my @libcrypto_srcs = ();
my @generated_srcs = ();
foreach my $obj (@{$unified_info{sources}->{'libcrypto'}}) {
  my $src = ${$unified_info{sources}->{$obj}}[0];
  #print("libcrypto src: $src \n");
  # .S files should be preprocessed into .s
  if ($unified_info{generate}->{$src}) {
    # .S or .s files should be preprocessed into .asm for WIN
    $src =~ s\.[sS]$\.asm\ if ($is_win);
    push(@generated_srcs, $src);
  } else {
    if ($src =~ m/\.c$/) { 
      push(@libcrypto_srcs, $src);
    }
  }
}

if ($arch eq 'linux32-s390x' || $arch eq  'linux64-s390x') {
  push(@libcrypto_srcs, 'crypto/bn/asm/s390x.S');
}

my @lib_defines = ();
foreach my $df (@{$unified_info{defines}->{libcrypto}}) {
  #print("libcrypto defines: $df\n");
  push(@lib_defines, $df);
}


foreach my $obj (@{$unified_info{sources}->{'providers/libdefault.a'}}) {
  my $src = ${$unified_info{sources}->{$obj}}[0];
  #print("libdefault src: $src \n");
  # .S files should be preprocessed into .s
  if ($unified_info{generate}->{$src}) {
    # .S or .s files should be preprocessed into .asm for WIN
    $src =~ s\.[sS]$\.asm\ if ($is_win);
    push(@generated_srcs, $src);
  } else {
    if ($src =~ m/\.c$/) { 
      push(@libcrypto_srcs, $src);
    }
  }
}

foreach my $obj (@{$unified_info{sources}->{'providers/libcommon.a'}}) {
  my $src = ${$unified_info{sources}->{$obj}}[0];
  #print("libimplementations src: $src \n");
  # .S files should be preprocessed into .s
  if ($unified_info{generate}->{$src}) {
    # .S or .s files should be preprocessed into .asm for WIN
    $src =~ s\.[sS]$\.asm\ if ($is_win);
    push(@generated_srcs, $src);
  } else {
    if ($src =~ m/\.c$/) { 
      push(@libcrypto_srcs, $src);
    }
  }
}

foreach my $obj (@{$unified_info{sources}->{'providers/liblegacy.a'}}) {
  my $src = ${$unified_info{sources}->{$obj}}[0];
  #print("liblegacy src: $src \n");
  # .S files should be preprocessed into .s
  if ($unified_info{generate}->{$src}) {
    # .S or .s files should be preprocessed into .asm for WIN
    $src =~ s\.[sS]$\.asm\ if ($is_win);
    push(@generated_srcs, $src);
  } else {
    if ($src =~ m/\.c$/) { 
      push(@libcrypto_srcs, $src);
    }
  }
}

foreach my $obj (@{$unified_info{sources}->{'providers/legacy'}}) {
  if ($obj eq 'providers/legacy.ld' and not $is_win) {
    push(@generated_srcs, $obj);
  } else {
    my $src = ${$unified_info{sources}->{$obj}}[0];
    #print("providers/fips obj: $obj, src: $src\n");
    if ($src =~ m/\.c$/) {
      push(@libcrypto_srcs, $src);
    }
  }
}

my @libfips_srcs = ();
foreach my $obj (@{$unified_info{sources}->{'providers/libfips.a'}}) {
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
  if ($obj eq 'providers/fips.ld' and not $is_win) {
    push(@generated_srcs, $obj);
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

# Generate all asm files and copy into config/archs
foreach my $src (@generated_srcs) {
  my $cmd = "cd ../openssl && CC=gcc ASM=nasm make -f $makefile $src";
  system($cmd) == 0 or die "Error in system($cmd)";
  copy_generated($src);
}

$target{'lib_cppflags'} =~ s/-D//g;
my @lib_cppflags = split(/ /, $target{'lib_cppflags'});

# Strip library flags for deps provided via GYP dependencies.
# zlib, brotli, and zstd are bundled in deps/ with proper GYP targets,
# so we must not link against system shared libraries.
$target{ex_libs} =~ s/-l(?:z|brotlienc|brotlidec|brotlicommon|zstd)\b//g;
$target{ex_libs} =~ s/\s+/ /g;
$target{ex_libs} =~ s/^\s+|\s+$//g;

# Filter out bare ZLIB/BROTLI/ZSTD defines added by Configure for compression
# support. These are internal to OpenSSL and clash with identifiers in
# Node.js source (e.g. ZLIB in async_wrap.h) when propagated via
# direct_dependent_settings. They are kept in the main defines for OpenSSL's
# own compilation but excluded from the exported defines.
my @config_defines_exported = grep { $_ !~ /^(?:ZLIB|BROTLI|ZSTD)$/ } @{$config{defines}};
my @target_defines_exported = grep { $_ !~ /^(?:ZLIB|BROTLI|ZSTD)$/ } @{$target{defines}};

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
        common_files => \%common_files,
        generated_include_dirs => \@generated_include_dirs,
        config => \%config,
        target => \%target,
        cflags => \@cflags,
        asm => \$asm,
        arch => \$arch,
        lib_cppflags => \@lib_cppflags,
        is_win => \$is_win,
        config_defines_exported => \@config_defines_exported,
        target_defines_exported => \@target_defines_exported,
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
        common_files => \%common_files,
        generated_include_dirs => \@generated_include_dirs,
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
my $cmd2 ="cd $src_dir && make -f $makefile clean && make -f $makefile distclean && " .
    "git clean -f $src_dir";
system($cmd2) == 0 or die "Error in system($cmd2)";

# Recreate wrappers after cleanup, including headers added by newer releases.
my @common_headers = sort grep { m|^include/.*\.h$| } keys %common_files;
my $headers_cmd = "$^X -I$src_dir ./generate_headers.pl \"@openssl_dir_headers\" \"@crypto_dir_headers\" \"@internal_dir_headers\" \"@common_headers\"";
system($headers_cmd) == 0 or die "Error in system($headers_cmd)";


sub copy_headers {
  my @headers = split / /, $_[0];
  my $inc_dir = $_[1];
  foreach my $header_name (@headers) {
    # Copy the header from OpenSSL source directory to the arch specific dir.
    #print("copy header $src_dir/include/$inc_dir/${header_name}.h to $base_dir/include/$inc_dir \n");
    copy_generated("include/$inc_dir/${header_name}.h");
   }
}

sub merge_headers {
  my ($headers, $inc_dir) = @_;
  my %headers = map { $_ => 1 } split / /, $headers;
  foreach my $path (keys %{$unified_info{generate}}) {
    next if $common_files{$path};
    $headers{$1} = 1 if $path =~ m|^include/$inc_dir/([^/]+)\.h$|;
  }
  return join(' ', sort keys %headers);
}

sub copy_generated {
  my ($file) = @_;
  my $dest = $common_files{$file} ? "./common/$file" : "$base_dir/$file";
  if ($common_files{$file} && -f $dest) {
    compare("$src_dir/$file", $dest) == 0
        or die "Common file $file differs for $arch/$asm";
    return;
  }
  make_path(dirname($dest));
  copy("$src_dir/$file", $dest) or die "Copy $file failed: $!";
}

sub generate_windows_makefile {
  my ($makefile) = @_;
  open my $input, '<', "$src_dir/$makefile" or die "Open $makefile failed: $!";
  my %rules;
  while (<$input>) {
    $rules{$1} = 1 if /^([^\s:]+):/;
  }
  close $input;

  open my $output, '>', "$src_dir/node-generated.mk" or die "Open node-generated.mk failed: $!";
  print $output "SRCDIR=.\nBLDDIR=.\n";
  my %sources = map { $_ => 1 }
      map { @{$unified_info{sources}->{$_}} } keys %{$unified_info{sources}};
  foreach my $file (sort keys %{$unified_info{generate}}) {
    my $is_asm = $file =~ /\.[sS]$/;
    next unless $file =~ /\.(?:h|c|inc|s|S)$/;
    next if $is_asm && !$sources{$file};
    my $output_file = $file;
    $output_file =~ s/\.[sS]$/.asm/ if $is_asm;
    next if $rules{$output_file};
    my @generator = @{$unified_info{generate}->{$file}};
    my $generator = shift @generator;
    next unless $generator =~ /\.(?:in|pl)$/;

    my @modules = ('-Mconfigdata');
    my %includes = ('.' => 1);
    my @depends = ('configdata.pm');
    foreach my $dep (@{$unified_info{depends}->{$file} || []}) {
      if ($dep =~ /^(.*?)\|(.*)\.pm$/) {
        my ($dir, $module) = ($1, $2);
        $includes{$dir} = 1;
        push @depends, "$dir/$module.pm";
        $module =~ s|/|::|g;
        push @modules, "-M$module";
      } else {
        push @depends, $dep;
        if ($dep =~ /\.pm$/) {
          $includes{dirname($dep)} = 1;
          push @modules, '-M' . basename($dep, '.pm');
        }
      }
    }
    my $perlflags = join(' ', (map { "-I$_" } sort keys %includes), @modules);
    my $command = $is_asm
        ? "\$(PERL) $generator @generator \"\$(PERLASM_SCHEME)\" \$(LIB_CFLAGS) \$(LIB_CPPFLAGS) \$(PROCESSOR) \$\@"
        : $generator =~ /\.in$/
        ? "\$(PERL) $perlflags util/dofile.pl -omakefile $generator @generator"
        : "\$(PERL) $generator @generator";
    $command .= ' > $@' unless $is_asm;
    print $output "GENERATED_MANDATORY += $output_file\n" unless $is_asm;
    print $output "GENERATED += $output_file\n";
    print $output "$output_file: $generator @depends\n\t$command\n";
  }
  close $output;
}
