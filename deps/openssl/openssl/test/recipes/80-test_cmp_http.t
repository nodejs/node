#! /usr/bin/env perl
# Copyright 2007-2025 The OpenSSL Project Authors. All Rights Reserved.
# Copyright Nokia 2007-2019
# Copyright Siemens AG 2015-2019
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use POSIX;
use OpenSSL::Test qw/:DEFAULT cmdstr data_file data_dir srctop_dir bldtop_dir result_dir/;
use OpenSSL::Test::Utils;

BEGIN {
    setup("test_cmp_http");
}
use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

plan skip_all => "These tests are not supported in a fuzz build"
    if config('options') =~ /-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION|enable-fuzz-afl/;

plan skip_all => "These tests are not supported in a no-cmp build"
    if disabled("cmp");
plan skip_all => "These tests are not supported in a no-ecx build"
    if disabled("ecx"); # EC and EDDSA test certs, e.g., in Mock/newWithNew.pem
plan skip_all => "These tests are not supported in a no-sock build"
    if disabled("sock");
plan skip_all => "These tests are not supported in a no-http build"
    if disabled("http");
plan skip_all => "These tests are not supported in a no-cms build"
    if disabled("cms"); # central key pair generation

plan skip_all => "Tests involving local HTTP server not available on Windows or VMS"
    if $^O =~ /^(VMS|MSWin32|msys)$/;
plan skip_all => "Tests involving local HTTP server not available in cross-compile builds"
    if defined $ENV{EXE_SHELL};

sub chop_dblquot { # chop any leading and trailing '"' (needed for Windows)
    my $str = shift;
    $str =~ s/^\"(.*?)\"$/$1/;
    return $str;
}

my $proxy = chop_dblquot($ENV{http_proxy} // $ENV{HTTP_PROXY} // "");
$proxy = "<EMPTY>" if $proxy eq "";
$proxy =~ s{^https?://}{}i;
my $no_proxy = $ENV{no_proxy} // $ENV{NO_PROXY};

my @app = qw(openssl cmp);

# the server-dependent client configuration consists of:
my $ca_dn;      # The CA's Distinguished Name
my $server_dn;  # The server's Distinguished Name
my $server_host;# The server's hostname or IP address
my $server_port;# The server's port
my $server_tls; # The server's TLS port, if any, or 0
my $server_path;# The server's CMP alias
my $server_cert;# The server's cert
my $kur_port;   # The server's port for kur (cert update)
my $pbm_port;   # The server port to be used for PBM
my $pbm_ref;    # The reference for PBM
my $pbm_secret; # The secret for PBM
my $column;     # The column number of the expected result
my $sleep = 0;  # The time to sleep between two requests

# the dynamic server info:
my $server_fh;  # Server file handle

sub subst_env {
    my $val = shift;
    return '""""' if $val eq "";
    return $ENV{$1} if $val =~ /^\$\{ENV::(\w+)}$/;
    return $val;
}

# The local $server_name variables in the subroutines below are used
# both as the name of a sub-directory with server-specific credentials
# and as the name of a server-dependent client config section.

sub load_config {
    my $server_name = shift;
    my $section = shift;
    my $test_config = $ENV{OPENSSL_CMP_CONFIG} // "$server_name/test.cnf";
    open (CH, $test_config) or die "Cannot open $test_config: $!";
    my $active = 0;
    while (<CH>) {
        if (m/\[\s*$section\s*\]/) {
            $active = 1;
        } elsif (m/\[\s*.*?\s*\]/) {
            $active = 0;
        } elsif ($active) {
            # if there are multiple entries with same key, the last one prevails
            $ca_dn       = subst_env($1) if m/^\s*ca_dn\s*=\s*(.*)?\s*$/;
            $server_dn   = subst_env($1) if m/^\s*server_dn\s*=\s*(.*)?\s*$/;
            $server_host = subst_env($1) if m/^\s*server_host\s*=\s*(\S*)?\s*(\#.*)?$/;
            $server_port = subst_env($1) if m/^\s*server_port\s*=\s*(\S*)?\s*(\#.*)?$/;
            $server_tls  = subst_env($1) if m/^\s*server_tls\s*=\s*(.*)?\s*$/;
            $server_path = subst_env($1) if m/^\s*server_path\s*=\s*(.*)?\s*$/;
            $server_cert = subst_env($1) if m/^\s*server_cert\s*=\s*(.*)?\s*$/;
            $kur_port    = subst_env($1) if m/^\s*kur_port\s*=\s*(.*)?\s*$/;
            $pbm_port    = subst_env($1) if m/^\s*pbm_port\s*=\s*(.*)?\s*$/;
            $pbm_ref     = subst_env($1) if m/^\s*pbm_ref\s*=\s*(.*)?\s*$/;
            $pbm_secret  = subst_env($1) if m/^\s*pbm_secret\s*=\s*(.*)?\s*$/;
            $column      = subst_env($1) if m/^\s*column\s*=\s*(.*)?\s*$/;
            $sleep       = subst_env($1) if m/^\s*sleep\s*=\s*(.*)?\s*$/;
        }
    }
    close CH;
    die "Cannot find all server-dependent config values in $test_config section [$section]\n"
        if !defined $ca_dn
        || !defined $server_dn || !defined $server_host
        || !defined $server_port || !defined $server_tls
        || !defined $server_path || !defined $server_cert
        || !defined $kur_port || !defined $pbm_port
        || !defined $pbm_ref || !defined $pbm_secret
        || !defined $column || !defined $sleep;
    die "Invalid server_port number in $test_config section [$section]: $server_port"
        unless $server_port =~ m/^\d+$/;
    $server_dn = $server_dn // $ca_dn;
}

my @server_configurations = ("Mock");
@server_configurations = split /\s+/, $ENV{OPENSSL_CMP_SERVER} if $ENV{OPENSSL_CMP_SERVER};
# set env variable, e.g., OPENSSL_CMP_SERVER="Mock Insta" to include further CMP servers

my @all_aspects = ("connection", "verification", "credentials", "commands", "enrollment");
@all_aspects = split /\s+/, $ENV{OPENSSL_CMP_ASPECTS} if $ENV{OPENSSL_CMP_ASPECTS};
# set env variable, e.g., OPENSSL_CMP_ASPECTS="commands enrollment" to select specific aspects

my $Mock_serverlog;
my $faillog;
my $faillog_file = $ENV{HARNESS_FAILLOG} // "failed_client_invocations.txt"; # pathname relative to result_dir
open($faillog, ">", $faillog_file) or die "Cannot open '$faillog_file' for writing: $!";

sub test_cmp_http {
    my $server_name = shift;
    my $aspect = shift;
    my $n = shift;
    my $i = shift;
    my $title = shift;
    my $params = shift;
    my $expected_result = shift;
    $params = [ '-server', "$server_host:$server_port", @$params ]
        if ($server_name eq "Mock" && !(grep { $_ eq '-server' } @$params));
    my $cmd = app([@app, @$params]);

    unless (is(my $actual_result = run($cmd), $expected_result, $title)) {
        if ($faillog) {
            my $quote_spc_empty = sub { $_ eq "" ? '""' : $_ =~ m/ / ? '"'.$_.'"' : $_ };
            my $invocation = cmdstr($cmd, display => 1);
            print $faillog "$server_name $aspect \"$title\" ($i/$n)".
                " expected=$expected_result (".
                ($expected_result ? "success" : "failure").")".
                " actual=$actual_result\n";
            print $faillog "$invocation\n\n";
        }
        sleep($sleep) if $expected_result == 1;
    }
}

sub test_cmp_http_aspect {
    my $server_name = shift;
    my $aspect = shift;
    my $tests = shift;
    subtest "CMP app CLI $server_name $aspect\n" => sub {
        my $n = scalar @$tests;
        plan tests => $n;
        my $i = 1;
        foreach (@$tests) {
            test_cmp_http($server_name, $aspect, $n, $i++, $$_[0], $$_[1], $$_[2]);
        }
    };
    # not unlinking test.cert.pem, test.cacerts.pem, and test.extracerts.pem
}

sub print_file_prefixed {
    my ($file, $desc) = @_;
    print "$desc (each line prefixed by \"# \"):\n";
    if (open F, $file) {
        while (<F>) {
            print "# $_";
        }
        close F;
    }
}

# The input files for the tests done here dynamically depend on the test server
# selected (where the mock server used by default is just one possibility).
# On the other hand the main test configuration file test.cnf, which references
# several server-dependent input files by relative file names, is static.
# Moreover the tests use much greater variety of input files than output files.
# Therefore we chose the current directory as a subdirectory of $SRCTOP and it
# was simpler to prepend the output file names by BLDTOP than doing the tests
# from $BLDTOP/test-runs/test_cmp_http and prepending the input files by SRCTOP.

indir data_dir() => sub {
    plan tests => 1 + @server_configurations * @all_aspects
        - (grep(/^Mock$/, @server_configurations)
           && grep(/^certstatus$/, @all_aspects));

    foreach my $server_name (@server_configurations) {
        $server_name = chop_dblquot($server_name);
        load_config($server_name, $server_name);
        {
          SKIP: {
            my $pid;
            if ($server_name eq "Mock") {
                indir "Mock" => sub {
                    $pid = start_server($server_name, "");
                    next unless $pid;
                }
            }
            foreach my $aspect (@all_aspects) {
                $aspect = chop_dblquot($aspect);
                if ($server_name eq "Mock" && $aspect eq "certstatus") {
                    print "Skipping certstatus check as not supported by $server_name server\n";
                    next;
                }
                load_config($server_name, $aspect); # update with any aspect-specific settings
                indir $server_name => sub {
                    my $tests = load_tests($server_name, $aspect);
                    test_cmp_http_aspect($server_name, $aspect, $tests);
                };
            };

            if ($server_name eq "Mock") {
                stop_server($server_name, $pid) if $pid;
                ok(1, "$server_name server has terminated");

                if (-s $faillog) {
                    indir "Mock" => sub {
                        print_file_prefixed($Mock_serverlog, "$server_name server STDERR output is");
                    }
                }
            }
          }
        }
    };
};

close($faillog) if $faillog;
if (-s $faillog_file) {
    print "# ------------------------------------------------------------------------------\n";
    print_file_prefixed($faillog_file, "Failed client invocations are");
    print "# ------------------------------------------------------------------------------\n";
}

sub load_tests {
    my $server_name = shift;
    my $aspect = shift;
    my $test_config = $ENV{OPENSSL_CMP_CONFIG} // "$server_name/test.cnf";
    my $file = data_file("test_$aspect.csv");
    my $result_dir = result_dir();
    my @result;

    open(my $data, '<', $file) || die "Cannot open '$file' for reading: $!";
  LOOP:
    while (my $line = <$data>) {
        chomp $line;
        $line =~ s{\r\n}{\n}g; # adjust line endings
        $line =~ s{_CA_DN}{$ca_dn}g;
        $line =~ s{_SERVER_DN}{$server_dn}g;
        $line =~ s{_SERVER_HOST}{$server_host}g;
        $line =~ s{_SERVER_PORT}{$server_port}g;
        $line =~ s{_SERVER_TLS}{$server_tls}g;
        $line =~ s{_SERVER_PATH}{$server_path}g;
        $line =~ s{_SERVER_CERT}{$server_cert}g;
        $line =~ s{_KUR_PORT}{$kur_port}g;
        $line =~ s{_PBM_PORT}{$pbm_port}g;
        $line =~ s{_PBM_REF}{$pbm_ref}g;
        $line =~ s{_PBM_SECRET}{$pbm_secret}g;
        $line =~ s{_RESULT_DIR}{$result_dir}g;

        next LOOP if $server_tls == 0 && $line =~ m/,\s*-tls_used\s*,/;
        my $noproxy = $no_proxy;
        my $server_plain = $server_host =~ m/^\[(.*)\]$/ ? $1 : $server_host;
        if ($line =~ m/,\s*-no_proxy\s*,(.*?)(,|$)/) {
            $noproxy = $1;
        } elsif ($server_plain eq "127.0.0.1" || $server_plain eq "::1") {
            # do connections to localhost (e.g., mock server) without proxy
            $line =~ s{-section,,}{-section,,-no_proxy,$server_plain,} ;
        }
        if ($line =~ m/,\s*-proxy\s*,/) {
            next LOOP if $no_proxy && ($noproxy =~ $server_plain);
        } else {
            $line =~ s{-section,,}{-section,,-proxy,$proxy,};
        }
        $line =~ s{-section,,}{-section,,-certout,$result_dir/test.cert.pem,}
            if $aspect ne "commands" || $line =~ m/,\s*-cmd\s*,\s*(ir|cr|p10cr|kur)\s*,/;
        $line =~ s{-section,,}{-config,../$test_config,-section,$server_name $aspect,};

        my @fields = grep /\S/, split ",", $line;
        s/^<EMPTY>$// for (@fields); # used for proxy=""
        s/^\s+// for (@fields); # remove leading whitespace from elements
        s/\s+$// for (@fields); # remove trailing whitespace from elements
        s/^\"(\".*?\")\"$/$1/ for (@fields); # remove escaping from quotation marks from elements
        my $expected_result = $fields[$column];
        my $description = 1;
        my $title = $fields[$description];
        next LOOP if (!defined($expected_result)
                      || ($expected_result ne 0 && $expected_result ne 1));
        next LOOP if ($line =~ m/-server,\[.*:.*\]/ && !have_IPv6());
        @fields = grep {$_ ne 'BLANK'} @fields[$description + 1 .. @fields - 1];
        push @result, [$title, \@fields, $expected_result];
    }
    close($data);
    return \@result;
}

sub start_server {
    my $server_name = shift;
    my $args = shift; # optional further CLI arguments
    my $cmd = cmdstr(app([@app, '-config', 'server.cnf',
                          $args ? $args : ()]), display => 1);
    print "Current directory is ".getcwd()."\n";
    print "Launching $server_name server: $cmd\n";
    $Mock_serverlog = result_dir()."/Mock_server_STDERR.txt";
    my $pid = open($server_fh, "$cmd 2>$Mock_serverlog |");
    unless ($pid) {
        print "Error launching $cmd, cannot obtain $server_name server PID";
        return 0;
    }
    print "$server_name server PID=$pid\n";

    if ($server_host eq '*' || $server_port == 0) {
        # Find out the actual server host and port and possibly different PID
        my ($host, $port);
        my $pid0 = $pid;
        while (<$server_fh>) {
            print "$server_name server output: $_";
            next if m/using section/;
            s/\R$//;                # Better chomp
            ($host, $port, $pid) = ($1, $2, $3)
                if /^ACCEPT\s(.*?):(\d+) PID=(\d+)$/;
            last; # Do not loop further to prevent hangs on server misbehavior
        }
        if ($server_host eq '*' && defined $host) {
            $server_host = "[::1]"     if $host eq "[::]";
            $server_host = "127.0.0.1" if $host eq "0.0.0.0";
        }
        $server_port = $port if $server_port == 0 && defined $port;
        if ($pid0 != $pid) {
            # kill the shell process
            kill('KILL', $pid0);
            waitpid($pid0, 0);
        }
    }
    if ($server_host eq '*' || $server_port == 0) {
        stop_server($server_name, $pid) if $pid;
        print "Cannot get expected output from the $server_name server\n";
        return 0;
    }
    $kur_port = $server_port if $kur_port eq "\$server_port";
    $pbm_port = $server_port if $pbm_port eq "\$server_port";
    $server_tls = $server_port if $server_tls;
    return $pid;

}

sub stop_server {
    my $server_name = shift;
    my $pid = shift;
    print "Killing $server_name server with PID=$pid\n";
    kill('KILL', $pid);
    waitpid($pid, 0);
}
