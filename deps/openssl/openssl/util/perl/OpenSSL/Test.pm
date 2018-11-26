# Copyright 2016-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

package OpenSSL::Test;

use strict;
use warnings;

use Test::More 0.96;

use Exporter;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
$VERSION = "0.8";
@ISA = qw(Exporter);
@EXPORT = (@Test::More::EXPORT, qw(setup indir app fuzz  perlapp test perltest
                                   run));
@EXPORT_OK = (@Test::More::EXPORT_OK, qw(bldtop_dir bldtop_file
                                         srctop_dir srctop_file
                                         data_file
                                         pipe with cmdstr quotify
                                         openssl_versions));

=head1 NAME

OpenSSL::Test - a private extension of Test::More

=head1 SYNOPSIS

  use OpenSSL::Test;

  setup("my_test_name");

  ok(run(app(["openssl", "version"])), "check for openssl presence");

  indir "subdir" => sub {
    ok(run(test(["sometest", "arg1"], stdout => "foo.txt")),
       "run sometest with output to foo.txt");
  };

=head1 DESCRIPTION

This module is a private extension of L<Test::More> for testing OpenSSL.
In addition to the Test::More functions, it also provides functions that
easily find the diverse programs within a OpenSSL build tree, as well as
some other useful functions.

This module I<depends> on the environment variables C<$TOP> or C<$SRCTOP>
and C<$BLDTOP>.  Without one of the combinations it refuses to work.
See L</ENVIRONMENT> below.

With each test recipe, a parallel data directory with (almost) the same name
as the recipe is possible in the source directory tree.  For example, for a
recipe C<$SRCTOP/test/recipes/99-foo.t>, there could be a directory
C<$SRCTOP/test/recipes/99-foo_data/>.

=cut

use File::Copy;
use File::Spec::Functions qw/file_name_is_absolute curdir canonpath splitdir
                             catdir catfile splitpath catpath devnull abs2rel
                             rel2abs/;
use File::Path 2.00 qw/rmtree mkpath/;
use File::Basename;


# The name of the test.  This is set by setup() and is used in the other
# functions to verify that setup() has been used.
my $test_name = undef;

# Directories we want to keep track of TOP, APPS, TEST and RESULTS are the
# ones we're interested in, corresponding to the environment variables TOP
# (mandatory), BIN_D, TEST_D, UTIL_D and RESULT_D.
my %directories = ();

# The environment variables that gave us the contents in %directories.  These
# get modified whenever we change directories, so that subprocesses can use
# the values of those environment variables as well
my @direnv = ();

# A bool saying if we shall stop all testing if the current recipe has failing
# tests or not.  This is set by setup() if the environment variable STOPTEST
# is defined with a non-empty value.
my $end_with_bailout = 0;

# A set of hooks that is affected by with() and may be used in diverse places.
# All hooks are expected to be CODE references.
my %hooks = (

    # exit_checker is used by run() directly after completion of a command.
    # it receives the exit code from that command and is expected to return
    # 1 (for success) or 0 (for failure).  This is the value that will be
    # returned by run().
    # NOTE: When run() gets the option 'capture => 1', this hook is ignored.
    exit_checker => sub { return shift == 0 ? 1 : 0 },

    );

# Debug flag, to be set manually when needed
my $debug = 0;

# Declare some utility functions that are defined at the end
sub bldtop_file;
sub bldtop_dir;
sub srctop_file;
sub srctop_dir;
sub quotify;

# Declare some private functions that are defined at the end
sub __env;
sub __cwd;
sub __apps_file;
sub __results_file;
sub __fixup_cmd;
sub __build_cmd;

=head2 Main functions

The following functions are exported by default when using C<OpenSSL::Test>.

=cut

=over 4

=item B<setup "NAME">

C<setup> is used for initial setup, and it is mandatory that it's used.
If it's not used in a OpenSSL test recipe, the rest of the recipe will
most likely refuse to run.

C<setup> checks for environment variables (see L</ENVIRONMENT> below),
checks that C<$TOP/Configure> or C<$SRCTOP/Configure> exists, C<chdir>
into the results directory (defined by the C<$RESULT_D> environment
variable if defined, otherwise C<$BLDTOP/test> or C<$TOP/test>, whichever
is defined).

=back

=cut

sub setup {
    my $old_test_name = $test_name;
    $test_name = shift;

    BAIL_OUT("setup() must receive a name") unless $test_name;
    warn "setup() detected test name change.  Innocuous, so we continue...\n"
        if $old_test_name && $old_test_name ne $test_name;

    return if $old_test_name;

    BAIL_OUT("setup() needs \$TOP or \$SRCTOP and \$BLDTOP to be defined")
        unless $ENV{TOP} || ($ENV{SRCTOP} && $ENV{BLDTOP});
    BAIL_OUT("setup() found both \$TOP and \$SRCTOP or \$BLDTOP...")
        if $ENV{TOP} && ($ENV{SRCTOP} || $ENV{BLDTOP});

    __env();

    BAIL_OUT("setup() expects the file Configure in the source top directory")
        unless -f srctop_file("Configure");

    __cwd($directories{RESULTS});
}

=over 4

=item B<indir "SUBDIR" =E<gt> sub BLOCK, OPTS>

C<indir> is used to run a part of the recipe in a different directory than
the one C<setup> moved into, usually a subdirectory, given by SUBDIR.
The part of the recipe that's run there is given by the codeblock BLOCK.

C<indir> takes some additional options OPTS that affect the subdirectory:

=over 4

=item B<create =E<gt> 0|1>

When set to 1 (or any value that perl preceives as true), the subdirectory
will be created if it doesn't already exist.  This happens before BLOCK
is executed.

=item B<cleanup =E<gt> 0|1>

When set to 1 (or any value that perl preceives as true), the subdirectory
will be cleaned out and removed.  This happens both before and after BLOCK
is executed.

=back

An example:

  indir "foo" => sub {
      ok(run(app(["openssl", "version"]), stdout => "foo.txt"));
      if (ok(open(RESULT, "foo.txt"), "reading foo.txt")) {
          my $line = <RESULT>;
          close RESULT;
          is($line, qr/^OpenSSL 1\./,
             "check that we're using OpenSSL 1.x.x");
      }
  }, create => 1, cleanup => 1;

=back

=cut

sub indir {
    my $subdir = shift;
    my $codeblock = shift;
    my %opts = @_;

    my $reverse = __cwd($subdir,%opts);
    BAIL_OUT("FAILURE: indir, \"$subdir\" wasn't possible to move into")
	unless $reverse;

    $codeblock->();

    __cwd($reverse);

    if ($opts{cleanup}) {
	rmtree($subdir, { safe => 0 });
    }
}

=over 4

=item B<app ARRAYREF, OPTS>

=item B<test ARRAYREF, OPTS>

Both of these functions take a reference to a list that is a command and
its arguments, and some additional options (described further on).

C<app> expects to find the given command (the first item in the given list
reference) as an executable in C<$BIN_D> (if defined, otherwise C<$TOP/apps>
or C<$BLDTOP/apps>).

C<test> expects to find the given command (the first item in the given list
reference) as an executable in C<$TEST_D> (if defined, otherwise C<$TOP/test>
or C<$BLDTOP/test>).

Both return a CODEREF to be used by C<run>, C<pipe> or C<cmdstr>.

The options that both C<app> and C<test> can take are in the form of hash
values:

=over 4

=item B<stdin =E<gt> PATH>

=item B<stdout =E<gt> PATH>

=item B<stderr =E<gt> PATH>

In all three cases, the corresponding standard input, output or error is
redirected from (for stdin) or to (for the others) a file given by the
string PATH, I<or>, if the value is C<undef>, C</dev/null> or similar.

=back

=item B<perlapp ARRAYREF, OPTS>

=item B<perltest ARRAYREF, OPTS>

Both these functions function the same way as B<app> and B<test>, except
that they expect the command to be a perl script.  Also, they support one
more option:

=over 4

=item B<interpreter_args =E<gt> ARRAYref>

The array reference is a set of arguments for perl rather than the script.
Take care so that none of them can be seen as a script!  Flags and their
eventual arguments only!

=back

An example:

  ok(run(perlapp(["foo.pl", "arg1"],
                 interpreter_args => [ "-I", srctop_dir("test") ])));

=back

=cut

sub app {
    my $cmd = shift;
    my %opts = @_;
    return sub { my $num = shift;
		 return __build_cmd($num, \&__apps_file, $cmd, %opts); }
}

sub fuzz {
    my $cmd = shift;
    my %opts = @_;
    return sub { my $num = shift;
		 return __build_cmd($num, \&__fuzz_file, $cmd, %opts); }
}

sub test {
    my $cmd = shift;
    my %opts = @_;
    return sub { my $num = shift;
		 return __build_cmd($num, \&__test_file, $cmd, %opts); }
}

sub perlapp {
    my $cmd = shift;
    my %opts = @_;
    return sub { my $num = shift;
		 return __build_cmd($num, \&__perlapps_file, $cmd, %opts); }
}

sub perltest {
    my $cmd = shift;
    my %opts = @_;
    return sub { my $num = shift;
		 return __build_cmd($num, \&__perltest_file, $cmd, %opts); }
}

=over 4

=item B<run CODEREF, OPTS>

This CODEREF is expected to be the value return by C<app> or C<test>,
anything else will most likely cause an error unless you know what you're
doing.

C<run> executes the command returned by CODEREF and return either the
resulting output (if the option C<capture> is set true) or a boolean indicating
if the command succeeded or not.

The options that C<run> can take are in the form of hash values:

=over 4

=item B<capture =E<gt> 0|1>

If true, the command will be executed with a perl backtick, and C<run> will
return the resulting output as an array of lines.  If false or not given,
the command will be executed with C<system()>, and C<run> will return 1 if
the command was successful or 0 if it wasn't.

=back

For further discussion on what is considered a successful command or not, see
the function C<with> further down.

=back

=cut

sub run {
    my ($cmd, $display_cmd) = shift->(0);
    my %opts = @_;

    return () if !$cmd;

    my $prefix = "";
    if ( $^O eq "VMS" ) {	# VMS
	$prefix = "pipe ";
    }

    my @r = ();
    my $r = 0;
    my $e = 0;

    # In non-verbose, we want to shut up the command interpreter, in case
    # it has something to complain about.  On VMS, it might complain both
    # on stdout and stderr
    my $save_STDOUT;
    my $save_STDERR;
    if ($ENV{HARNESS_ACTIVE} && !$ENV{HARNESS_VERBOSE}) {
        open $save_STDOUT, '>&', \*STDOUT or die "Can't dup STDOUT: $!";
        open $save_STDERR, '>&', \*STDERR or die "Can't dup STDERR: $!";
        open STDOUT, ">", devnull();
        open STDERR, ">", devnull();
    }

    # The dance we do with $? is the same dance the Unix shells appear to
    # do.  For example, a program that gets aborted (and therefore signals
    # SIGABRT = 6) will appear to exit with the code 134.  We mimic this
    # to make it easier to compare with a manual run of the command.
    if ($opts{capture}) {
	@r = `$prefix$cmd`;
	$e = ($? & 0x7f) ? ($? & 0x7f)|0x80 : ($? >> 8);
    } else {
	system("$prefix$cmd");
	$e = ($? & 0x7f) ? ($? & 0x7f)|0x80 : ($? >> 8);
	$r = $hooks{exit_checker}->($e);
    }

    if ($ENV{HARNESS_ACTIVE} && !$ENV{HARNESS_VERBOSE}) {
        close STDOUT;
        close STDERR;
        open STDOUT, '>&', $save_STDOUT or die "Can't restore STDOUT: $!";
        open STDERR, '>&', $save_STDERR or die "Can't restore STDERR: $!";
    }

    print STDERR "$prefix$display_cmd => $e\n"
        if !$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE};

    # At this point, $? stops being interesting, and unfortunately,
    # there are Test::More versions that get picky if we leave it
    # non-zero.
    $? = 0;

    if ($opts{capture}) {
	return @r;
    } else {
	return $r;
    }
}

END {
    my $tb = Test::More->builder;
    my $failure = scalar(grep { $_ == 0; } $tb->summary);
    if ($failure && $end_with_bailout) {
	BAIL_OUT("Stoptest!");
    }
}

=head2 Utility functions

The following functions are exported on request when using C<OpenSSL::Test>.

  # To only get the bldtop_file and srctop_file functions.
  use OpenSSL::Test qw/bldtop_file srctop_file/;

  # To only get the bldtop_file function in addition to the default ones.
  use OpenSSL::Test qw/:DEFAULT bldtop_file/;

=cut

# Utility functions, exported on request

=over 4

=item B<bldtop_dir LIST>

LIST is a list of directories that make up a path from the top of the OpenSSL
build directory (as indicated by the environment variable C<$TOP> or
C<$BLDTOP>).
C<bldtop_dir> returns the resulting directory as a string, adapted to the local
operating system.

=back

=cut

sub bldtop_dir {
    return __bldtop_dir(@_);	# This caters for operating systems that have
				# a very distinct syntax for directories.
}

=over 4

=item B<bldtop_file LIST, FILENAME>

LIST is a list of directories that make up a path from the top of the OpenSSL
build directory (as indicated by the environment variable C<$TOP> or
C<$BLDTOP>) and FILENAME is the name of a file located in that directory path.
C<bldtop_file> returns the resulting file path as a string, adapted to the local
operating system.

=back

=cut

sub bldtop_file {
    return __bldtop_file(@_);
}

=over 4

=item B<srctop_dir LIST>

LIST is a list of directories that make up a path from the top of the OpenSSL
source directory (as indicated by the environment variable C<$TOP> or
C<$SRCTOP>).
C<srctop_dir> returns the resulting directory as a string, adapted to the local
operating system.

=back

=cut

sub srctop_dir {
    return __srctop_dir(@_);	# This caters for operating systems that have
				# a very distinct syntax for directories.
}

=over 4

=item B<srctop_file LIST, FILENAME>

LIST is a list of directories that make up a path from the top of the OpenSSL
source directory (as indicated by the environment variable C<$TOP> or
C<$SRCTOP>) and FILENAME is the name of a file located in that directory path.
C<srctop_file> returns the resulting file path as a string, adapted to the local
operating system.

=back

=cut

sub srctop_file {
    return __srctop_file(@_);
}

=over 4

=item B<data_file LIST, FILENAME>

LIST is a list of directories that make up a path from the data directory
associated with the test (see L</DESCRIPTION> above) and FILENAME is the name
of a file located in that directory path.  C<data_file> returns the resulting
file path as a string, adapted to the local operating system.

=back

=cut

sub data_file {
    return __data_file(@_);
}

=over 4

=item B<pipe LIST>

LIST is a list of CODEREFs returned by C<app> or C<test>, from which C<pipe>
creates a new command composed of all the given commands put together in a
pipe.  C<pipe> returns a new CODEREF in the same manner as C<app> or C<test>,
to be passed to C<run> for execution.

=back

=cut

sub pipe {
    my @cmds = @_;
    return
	sub {
	    my @cs  = ();
	    my @dcs = ();
	    my @els = ();
	    my $counter = 0;
	    foreach (@cmds) {
		my ($c, $dc, @el) = $_->(++$counter);

		return () if !$c;

		push @cs, $c;
		push @dcs, $dc;
		push @els, @el;
	    }
	    return (
		join(" | ", @cs),
		join(" | ", @dcs),
		@els
		);
    };
}

=over 4

=item B<with HASHREF, CODEREF>

C<with> will temporarly install hooks given by the HASHREF and then execute
the given CODEREF.  Hooks are usually expected to have a coderef as value.

The currently available hoosk are:

=over 4

=item B<exit_checker =E<gt> CODEREF>

This hook is executed after C<run> has performed its given command.  The
CODEREF receives the exit code as only argument and is expected to return
1 (if the exit code indicated success) or 0 (if the exit code indicated
failure).

=back

=back

=cut

sub with {
    my $opts = shift;
    my %opts = %{$opts};
    my $codeblock = shift;

    my %saved_hooks = ();

    foreach (keys %opts) {
	$saved_hooks{$_} = $hooks{$_}	if exists($hooks{$_});
	$hooks{$_} = $opts{$_};
    }

    $codeblock->();

    foreach (keys %saved_hooks) {
	$hooks{$_} = $saved_hooks{$_};
    }
}

=over 4

=item B<cmdstr CODEREF, OPTS>

C<cmdstr> takes a CODEREF from C<app> or C<test> and simply returns the
command as a string.

C<cmdstr> takes some additiona options OPTS that affect the string returned:

=over 4

=item B<display =E<gt> 0|1>

When set to 0, the returned string will be with all decorations, such as a
possible redirect of stderr to the null device.  This is suitable if the
string is to be used directly in a recipe.

When set to 1, the returned string will be without extra decorations.  This
is suitable for display if that is desired (doesn't confuse people with all
internal stuff), or if it's used to pass a command down to a subprocess.

Default: 0

=back

=back

=cut

sub cmdstr {
    my ($cmd, $display_cmd) = shift->(0);
    my %opts = @_;

    if ($opts{display}) {
        return $display_cmd;
    } else {
        return $cmd;
    }
}

=over 4

=item B<quotify LIST>

LIST is a list of strings that are going to be used as arguments for a
command, and makes sure to inject quotes and escapes as necessary depending
on the content of each string.

This can also be used to put quotes around the executable of a command.
I<This must never ever be done on VMS.>

=back

=cut

sub quotify {
    # Unix setup (default if nothing else is mentioned)
    my $arg_formatter =
	sub { $_ = shift;
	      ($_ eq '' || /\s|[\{\}\\\$\[\]\*\?\|\&:;<>]/) ? "'$_'" : $_ };

    if ( $^O eq "VMS") {	# VMS setup
	$arg_formatter = sub {
	    $_ = shift;
	    if ($_ eq '' || /\s|["[:upper:]]/) {
		s/"/""/g;
		'"'.$_.'"';
	    } else {
		$_;
	    }
	};
    } elsif ( $^O eq "MSWin32") { # MSWin setup
	$arg_formatter = sub {
	    $_ = shift;
	    if ($_ eq '' || /\s|["\|\&\*\;<>]/) {
		s/(["\\])/\\$1/g;
		'"'.$_.'"';
	    } else {
		$_;
	    }
	};
    }

    return map { $arg_formatter->($_) } @_;
}

=over 4

=item B<openssl_versions>

Returns a list of two numbers, the first representing the build version,
the second representing the library version.  See opensslv.h for more
information on those numbers.

=back

=cut

my @versions = ();
sub openssl_versions {
    unless (@versions) {
        my %lines =
            map { s/\R$//;
                  /^(.*): (0x[[:xdigit:]]{8})$/;
                  die "Weird line: $_" unless defined $1;
                  $1 => hex($2) }
            run(test(['versions']), capture => 1);
        @versions = ( $lines{'Build version'}, $lines{'Library version'} );
    }
    return @versions;
}

######################################################################
# private functions.  These are never exported.

=head1 ENVIRONMENT

OpenSSL::Test depends on some environment variables.

=over 4

=item B<TOP>

This environment variable is mandatory.  C<setup> will check that it's
defined and that it's a directory that contains the file C<Configure>.
If this isn't so, C<setup> will C<BAIL_OUT>.

=item B<BIN_D>

If defined, its value should be the directory where the openssl application
is located.  Defaults to C<$TOP/apps> (adapted to the operating system).

=item B<TEST_D>

If defined, its value should be the directory where the test applications
are located.  Defaults to C<$TOP/test> (adapted to the operating system).

=item B<STOPTEST>

If defined, it puts testing in a different mode, where a recipe with
failures will result in a C<BAIL_OUT> at the end of its run.

=back

=cut

sub __env {
    (my $recipe_datadir = basename($0)) =~ s/\.t$/_data/i;

    $directories{SRCTOP}  = $ENV{SRCTOP} || $ENV{TOP};
    $directories{BLDTOP}  = $ENV{BLDTOP} || $ENV{TOP};
    $directories{BLDAPPS} = $ENV{BIN_D}  || __bldtop_dir("apps");
    $directories{SRCAPPS} =                 __srctop_dir("apps");
    $directories{BLDFUZZ} =                 __bldtop_dir("fuzz");
    $directories{SRCFUZZ} =                 __srctop_dir("fuzz");
    $directories{BLDTEST} = $ENV{TEST_D} || __bldtop_dir("test");
    $directories{SRCTEST} =                 __srctop_dir("test");
    $directories{SRCDATA} =                 __srctop_dir("test", "recipes",
                                                         $recipe_datadir);
    $directories{RESULTS} = $ENV{RESULT_D} || $directories{BLDTEST};

    push @direnv, "TOP"       if $ENV{TOP};
    push @direnv, "SRCTOP"    if $ENV{SRCTOP};
    push @direnv, "BLDTOP"    if $ENV{BLDTOP};
    push @direnv, "BIN_D"     if $ENV{BIN_D};
    push @direnv, "TEST_D"    if $ENV{TEST_D};
    push @direnv, "RESULT_D"  if $ENV{RESULT_D};

    $end_with_bailout	  = $ENV{STOPTEST} ? 1 : 0;
};

sub __srctop_file {
    BAIL_OUT("Must run setup() first") if (! $test_name);

    my $f = pop;
    return catfile($directories{SRCTOP},@_,$f);
}

sub __srctop_dir {
    BAIL_OUT("Must run setup() first") if (! $test_name);

    return catdir($directories{SRCTOP},@_);
}

sub __bldtop_file {
    BAIL_OUT("Must run setup() first") if (! $test_name);

    my $f = pop;
    return catfile($directories{BLDTOP},@_,$f);
}

sub __bldtop_dir {
    BAIL_OUT("Must run setup() first") if (! $test_name);

    return catdir($directories{BLDTOP},@_);
}

sub __exeext {
    my $ext = "";
    if ($^O eq "VMS" ) {	# VMS
	$ext = ".exe";
    } elsif ($^O eq "MSWin32") { # Windows
	$ext = ".exe";
    }
    return $ENV{"EXE_EXT"} || $ext;
}

sub __test_file {
    BAIL_OUT("Must run setup() first") if (! $test_name);

    my $f = pop;
    my $out = catfile($directories{BLDTEST},@_,$f . __exeext());
    $out = catfile($directories{SRCTEST},@_,$f) unless -x $out;
    return $out;
}

sub __perltest_file {
    BAIL_OUT("Must run setup() first") if (! $test_name);

    my $f = pop;
    my $out = catfile($directories{BLDTEST},@_,$f);
    $out = catfile($directories{SRCTEST},@_,$f) unless -f $out;
    return ($^X, $out);
}

sub __apps_file {
    BAIL_OUT("Must run setup() first") if (! $test_name);

    my $f = pop;
    my $out = catfile($directories{BLDAPPS},@_,$f . __exeext());
    $out = catfile($directories{SRCAPPS},@_,$f) unless -x $out;
    return $out;
}

sub __fuzz_file {
    BAIL_OUT("Must run setup() first") if (! $test_name);

    my $f = pop;
    my $out = catfile($directories{BLDFUZZ},@_,$f . __exeext());
    $out = catfile($directories{SRCFUZZ},@_,$f) unless -x $out;
    return $out;
}

sub __perlapps_file {
    BAIL_OUT("Must run setup() first") if (! $test_name);

    my $f = pop;
    my $out = catfile($directories{BLDAPPS},@_,$f);
    $out = catfile($directories{SRCAPPS},@_,$f) unless -f $out;
    return ($^X, $out);
}

sub __data_file {
    BAIL_OUT("Must run setup() first") if (! $test_name);

    my $f = pop;
    return catfile($directories{SRCDATA},@_,$f);
}

sub __results_file {
    BAIL_OUT("Must run setup() first") if (! $test_name);

    my $f = pop;
    return catfile($directories{RESULTS},@_,$f);
}

sub __cwd {
    my $dir = catdir(shift);
    my %opts = @_;
    my $abscurdir = rel2abs(curdir());
    my $absdir = rel2abs($dir);
    my $reverse = abs2rel($abscurdir, $absdir);

    # PARANOIA: if we're not moving anywhere, we do nothing more
    if ($abscurdir eq $absdir) {
	return $reverse;
    }

    # Do not support a move to a different volume for now.  Maybe later.
    BAIL_OUT("FAILURE: \"$dir\" moves to a different volume, not supported")
	if $reverse eq $abscurdir;

    # If someone happened to give a directory that leads back to the current,
    # it's extremely silly to do anything more, so just simulate that we did
    # move.
    # In this case, we won't even clean it out, for safety's sake.
    return "." if $reverse eq "";

    $dir = canonpath($dir);
    if ($opts{create}) {
	mkpath($dir);
    }

    # We are recalculating the directories we keep track of, but need to save
    # away the result for after having moved into the new directory.
    my %tmp_directories = ();
    my %tmp_ENV = ();

    # For each of these directory variables, figure out where they are relative
    # to the directory we want to move to if they aren't absolute (if they are,
    # they don't change!)
    my @dirtags = sort keys %directories;
    foreach (@dirtags) {
	if (!file_name_is_absolute($directories{$_})) {
	    my $newpath = abs2rel(rel2abs($directories{$_}), rel2abs($dir));
	    $tmp_directories{$_} = $newpath;
	}
    }

    # Treat each environment variable that was used to get us the values in
    # %directories the same was as the paths in %directories, so any sub
    # process can use their values properly as well
    foreach (@direnv) {
	if (!file_name_is_absolute($ENV{$_})) {
	    my $newpath = abs2rel(rel2abs($ENV{$_}), rel2abs($dir));
	    $tmp_ENV{$_} = $newpath;
	}
    }

    # Should we just bail out here as well?  I'm unsure.
    return undef unless chdir($dir);

    if ($opts{cleanup}) {
	rmtree(".", { safe => 0, keep_root => 1 });
    }

    # We put back new values carefully.  Doing the obvious
    # %directories = ( %tmp_irectories )
    # will clear out any value that happens to be an absolute path
    foreach (keys %tmp_directories) {
        $directories{$_} = $tmp_directories{$_};
    }
    foreach (keys %tmp_ENV) {
        $ENV{$_} = $tmp_ENV{$_};
    }

    if ($debug) {
	print STDERR "DEBUG: __cwd(), directories and files:\n";
	print STDERR "  \$directories{BLDTEST} = \"$directories{BLDTEST}\"\n";
	print STDERR "  \$directories{SRCTEST} = \"$directories{SRCTEST}\"\n";
	print STDERR "  \$directories{SRCDATA} = \"$directories{SRCDATA}\"\n";
	print STDERR "  \$directories{RESULTS} = \"$directories{RESULTS}\"\n";
	print STDERR "  \$directories{BLDAPPS} = \"$directories{BLDAPPS}\"\n";
	print STDERR "  \$directories{SRCAPPS} = \"$directories{SRCAPPS}\"\n";
	print STDERR "  \$directories{SRCTOP}  = \"$directories{SRCTOP}\"\n";
	print STDERR "  \$directories{BLDTOP}  = \"$directories{BLDTOP}\"\n";
	print STDERR "\n";
	print STDERR "  current directory is \"",curdir(),"\"\n";
	print STDERR "  the way back is \"$reverse\"\n";
    }

    return $reverse;
}

sub __fixup_cmd {
    my $prog = shift;
    my $exe_shell = shift;

    my $prefix = __bldtop_file("util", "shlib_wrap.sh")." ";

    if (defined($exe_shell)) {
	$prefix = "$exe_shell ";
    } elsif ($^O eq "VMS" ) {	# VMS
	$prefix = ($prog =~ /^(?:[\$a-z0-9_]+:)?[<\[]/i ? "mcr " : "mcr []");
    } elsif ($^O eq "MSWin32") { # Windows
	$prefix = "";
    }

    # We test both with and without extension.  The reason
    # is that we might be passed a complete file spec, with
    # extension.
    if ( ! -x $prog ) {
	my $prog = "$prog";
	if ( ! -x $prog ) {
	    $prog = undef;
	}
    }

    if (defined($prog)) {
	# Make sure to quotify the program file on platforms that may
	# have spaces or similar in their path name.
	# To our knowledge, VMS is the exception where quotifying should
	# never happen.
	($prog) = quotify($prog) unless $^O eq "VMS";
	return $prefix.$prog;
    }

    print STDERR "$prog not found\n";
    return undef;
}

sub __build_cmd {
    BAIL_OUT("Must run setup() first") if (! $test_name);

    my $num = shift;
    my $path_builder = shift;
    # Make a copy to not destroy the caller's array
    my @cmdarray = ( @{$_[0]} ); shift;
    my %opts = @_;

    # We do a little dance, as $path_builder might return a list of
    # more than one.  If so, only the first is to be considered a
    # program to fix up, the rest is part of the arguments.  This
    # happens for perl scripts, where $path_builder will return
    # a list of two, $^X and the script name.
    # Also, if $path_builder returned more than one, we don't apply
    # the EXE_SHELL environment variable.
    my @prog = ($path_builder->(shift @cmdarray));
    my $first = shift @prog;
    my $exe_shell = @prog ? undef : $ENV{EXE_SHELL};
    my $cmd = __fixup_cmd($first, $exe_shell);
    if (@prog) {
	if ( ! -f $prog[0] ) {
	    print STDERR "$prog[0] not found\n";
	    $cmd = undef;
	}
    }
    my @args = (@prog, @cmdarray);
    if (defined($opts{interpreter_args})) {
        unshift @args, @{$opts{interpreter_args}};
    }

    return () if !$cmd;

    my $arg_str = "";
    my $null = devnull();


    $arg_str = " ".join(" ", quotify @args) if @args;

    my $fileornull = sub { $_[0] ? $_[0] : $null; };
    my $stdin = "";
    my $stdout = "";
    my $stderr = "";
    my $saved_stderr = undef;
    $stdin = " < ".$fileornull->($opts{stdin})  if exists($opts{stdin});
    $stdout= " > ".$fileornull->($opts{stdout}) if exists($opts{stdout});
    $stderr=" 2> ".$fileornull->($opts{stderr}) if exists($opts{stderr});

    my $display_cmd = "$cmd$arg_str$stdin$stdout$stderr";

    $stderr=" 2> ".$null
        unless $stderr || !$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE};

    $cmd .= "$arg_str$stdin$stdout$stderr";

    if ($debug) {
	print STDERR "DEBUG[__build_cmd]: \$cmd = \"$cmd\"\n";
	print STDERR "DEBUG[__build_cmd]: \$display_cmd = \"$display_cmd\"\n";
    }

    return ($cmd, $display_cmd);
}

=head1 SEE ALSO

L<Test::More>, L<Test::Harness>

=head1 AUTHORS

Richard Levitte E<lt>levitte@openssl.orgE<gt> with assitance and
inspiration from Andy Polyakov E<lt>appro@openssl.org<gt>.

=cut

1;
