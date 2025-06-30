#! /usr/bin/env perl -w
use 5.10.0;
use strict;
use FindBin;
use lib "$FindBin::Bin/../openssl/util/perl/OpenSSL";
use Text::Template;

our $src_dir = "../openssl";

my @openssl_headers = shift @ARGV;
my @crypto_headers = shift @ARGV;
my @internal_headers = shift @ARGV;

my $include_tmpl = Text::Template->new(TYPE => 'FILE',
                                       SOURCE => 'include.h.tmpl',
                                       DELIMITERS => [ "%%-", "-%%" ]);
my $include_conf_tmpl = Text::Template->new(TYPE => 'FILE',
                                            SOURCE => 'include_config.tmpl',
                                            DELIMITERS => [ "%%-", "-%%" ]);
my $include_asm_tmpl = Text::Template->new(TYPE => 'FILE',
                                           SOURCE => 'include_asm.h.tmpl',
                                           DELIMITERS => [ "%%-", "-%%" ]);
my $include_no_asm_tmpl = Text::Template->new(TYPE => 'FILE',
                                              SOURCE => 'include_no-asm.h.tmpl',
                                              DELIMITERS => [ "%%-", "-%%" ]);

gen_headers(@openssl_headers, 'openssl');
gen_headers(@crypto_headers, 'crypto');
gen_headers(@internal_headers, 'internal');

sub gen_headers {
  my @headers = split / /, $_[0];
  my $inc_dir = $_[1];
  foreach my $header_name (@headers) {
    print("Generating headers for: $header_name\n");

    # Populate and write header file that will be placed OpenSSL's include
    # directory.
    my $include = $include_tmpl->fill_in(HASH => { name => $header_name });
    open(INCLUDE, "> $src_dir/include/${inc_dir}/${header_name}.h");
    print INCLUDE "$include";
    close(INCLUDE);
    #
    # Poplulate and write the header in the config directory (currently the same
    # directory as this file) which will determine which include to use
    # depending on if asm is available or not.
    my $include_conf = $include_conf_tmpl->fill_in(
       HASH => { name => $header_name });
    open(INCLUDE_CONF, "> ./${header_name}.h");
    print INCLUDE_CONF "$include_conf";
    close(INCLUDE_CONF);

    # Poplulate and write the asm header.
    my $include_asm = $include_asm_tmpl->fill_in(
      HASH => { asmdir => 'asm', incdir => $inc_dir, name => $header_name });
    open(INCLUDE_ASM, "> ./${header_name}_asm.h");
    print INCLUDE_ASM "$include_asm";
    close(INCLUDE_ASM);

    # Poplulate and write the no-asm header.
    my $include_no_asm = $include_no_asm_tmpl->fill_in(
      HASH => { asmdir => 'no-asm',
                incdir => $inc_dir,
		name => $header_name });
    open(INCLUDE_NO_ASM, "> ./${header_name}_no-asm.h");
    print INCLUDE_NO_ASM "$include_no_asm";
    close(INCLUDE_NO_ASM);
  }
}
