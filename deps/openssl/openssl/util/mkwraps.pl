#! /usr/bin/env perl
# Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use Getopt::Long;
use File::Spec::Functions qw(catdir catfile file_name_is_absolute rel2abs);
use File::Basename qw(dirname);

my $build_info_file;
my $target;
my @extra_includes;
my $output_file;
my $mode = 'both';
my $cc;
my $use_system = 1;
my $verbose = 0;
my $help = 0;

GetOptions('build-info=s' => \$build_info_file,
           'target=s'     => \$target,
           'include=s'    => \@extra_includes,
           'output=s'     => \$output_file,
           'mode=s'       => \$mode,
           'cc=s'         => \$cc,
           'system!'      => \$use_system,
           'verbose'      => \$verbose,
           'help'         => \$help)
    or die "Error in command line arguments\n";

$cc = $ENV{CC} || 'cc' unless defined $cc;

sub help
{
    print STDERR <<"EOF";
mkwraps.pl [options]

Options:

    --build-info FILE  build.info file containing a WRAP[<target>] entry.

    --target NAME      Test program name used as the WRAP[]/INCLUDE[] key.

    --include DIR      Extra include directory to search.  Cumulative.

    --output FILE      Output file.  Defaults to stdout.

    --mode MODE        What to emit: 'wraps', 'expects' or 'both'.
                       Defaults to 'both'.

    --cc NAME          C compiler used to discover the default system
                       include search paths.  Defaults to \$CC or 'cc'.

    --no-system        Do not fall back to the compiler's default system
                       include directories.  By default, functions not
                       found in the project headers (typically libc/POSIX
                       functions) are looked up there.

    --verbose          Print progress to stderr.

    --help             Show this help text.

For each function name listed in WRAP[<target>], the script searches
the headers (*.h) under each directory in INCLUDE[<target>], resolved
relative to the directory containing the build.info file, plus any
extra --include directories.  The search recurses into subdirectories,
mirroring how the compiler resolves <subdir/header.h> via -I flags.
The first header containing a matching declaration provides the
prototype.

Functions not found in those directories (typically system functions
such as read() or socket()) are then looked up under the compiler's
default include search paths, as reported by the C compiler.  System
prototypes are emitted with angle-bracket #include directives.  Use
--no-system to disable this fallback.

The output is meant as a stub for further editing.  Custom logic
(out-parameters, variadic forwarding, side effects on globals) still
needs to be written manually, and the emitted #include directives may
need to be adjusted (e.g. to add internal headers for opaque types).
Long lines in the output are not wrapped.
EOF
}

if ($help) {
    &help();
    exit 0;
}

unless (defined $build_info_file && defined $target) {
    &help();
    exit 1;
}

die "--mode must be 'wraps', 'expects' or 'both'\n"
    unless $mode =~ /^(?:wraps|expects|both)$/;

my @t = localtime();
my $YEAR = $t[5] + 1900;

# Parse the build.info file.  We are only interested in two directives,
# WRAP[<target>] and INCLUDE[<target>], either of which may be split
# across several physical lines using the usual backslash continuation.

my @wraps;
my @includes;

open(IN, "<$build_info_file") || die "Can't open $build_info_file, $!,";
my $content = do { local $/; <IN> };
close IN;

$content =~ s/\\\n\s*/ /g;

foreach (split /\n/, $content) {
    next if /^\s*#/ || /^\s*$/;
    if (/^\s*WRAP\[\Q$target\E\]\s*=\s*(.+?)\s*$/) {
        push @wraps, split(/\s+/, $1);
    } elsif (/^\s*INCLUDE\[\Q$target\E\]\s*=\s*(.+?)\s*$/) {
        push @includes, split(/\s+/, $1);
    }
}

die "No WRAP[$target] entry found in $build_info_file\n" unless @wraps;

my $bi_dir = dirname($build_info_file);
my @search_dirs;
foreach my $inc (@includes, @extra_includes) {
    push @search_dirs,
        file_name_is_absolute($inc) ? $inc : rel2abs(catdir($bi_dir, $inc));
}

print STDERR "Wraps:\n  ", join("\n  ", @wraps), "\n" if $verbose;
print STDERR "Search dirs:\n  ", join("\n  ", @search_dirs), "\n" if $verbose;

# Walk all search directories and get them in the order that level ones are
# first followed by subdirs so if there are nested includes, we get the
# shortest ones first searched.  Each base is a [dir, is_system] pair and the
# is_system flag is carried onto every header found beneath it, so that system
# prototypes can later be emitted with angle-bracket includes.
sub find_headers
{
    my (@bases) = @_;
    my @found;
    my @queue = map { [$_->[0], '', $_->[1]] } @bases;

    while (@queue) {
        my @next;
        my @level_files;
        foreach my $entry (@queue) {
            my ($dir, $rel, $sys) = @$entry;
            next unless -d $dir;
            opendir(my $dh, $dir) or next;
            foreach my $name (readdir $dh) {
                next if $name =~ /^\./;
                my $full = catfile($dir, $name);
                my $newrel = $rel eq '' ? $name : "$rel/$name";
                if (-d $full) {
                    push @next, [$full, $newrel, $sys];
                } elsif (-f $full && $name =~ /\.h$/) {
                    push @level_files, [$full, $newrel, $sys];
                }
            }
            closedir $dh;
        }
        # Stable sort within a level for deterministic ordering.
        push @found, sort { $a->[1] cmp $b->[1] } @level_files;
        @queue = sort { $a->[1] cmp $b->[1] } @next;
    }
    return @found;
}

# Ask the C compiler for its default "#include <...>" search paths.  Returns
# the list of existing directories, in search order.  Empty on failure.
sub system_include_dirs
{
    my ($compiler) = @_;
    my $out = `$compiler -xc -E -v /dev/null 2>&1`;
    return () unless defined $out
        && $out =~ /search starts here:(.*?)End of search list\./s;
    my @dirs;
    foreach my $line (split /\n/, $1) {
        $line =~ s/^\s+//;
        $line =~ s/\s+$//;
        # clang annotates framework directories; skip those.
        next if $line eq '' || $line =~ /\(framework directory\)$/;
        push @dirs, $line if -d $line;
    }
    return @dirs;
}

# Project headers are searched first (with is_system = 0) so that a project
# declaration always wins over a colliding system one.  The system headers are
# appended lazily, only if some function is not found in the project headers.
my @search_files = find_headers(map { [$_, 0] } @search_dirs);
my $system_loaded = 0;

sub load_system_headers
{
    return if $system_loaded;
    $system_loaded = 1;
    return unless $use_system;
    my @sys_dirs = system_include_dirs($cc);
    unless (@sys_dirs) {
        warn "WARNING: could not determine system include dirs from '$cc'\n";
        return;
    }
    print STDERR "System include dirs:\n  ", join("\n  ", @sys_dirs), "\n"
        if $verbose;
    push @search_files, find_headers(map { [$_, 1] } @sys_dirs);
}

my %file_cache;

sub strip_c_comments
{
    my $s = shift;
    $s =~ s{/\*.*?\*/}{}sg;
    $s =~ s{//[^\n]*}{}g;
    return $s;
}

# Drop attribute and qualifier macros that should not appear in the
# emitted return type.
sub strip_attribute_macros
{
    my $s = shift;
    foreach my $kw (qw(__owur __pure __malloc__ ossl_inline static inline
                       extern OSSL_DEPRECATEDIN_0_9_8
                       OSSL_DEPRECATEDIN_1_0_0 OSSL_DEPRECATEDIN_1_1_0
                       OSSL_DEPRECATEDIN_3_0 OSSL_DEPRECATEDIN_3_1
                       OSSL_DEPRECATEDIN_3_2 OSSL_DEPRECATEDIN_3_3
                       OSSL_DEPRECATEDIN_3_4 OSSL_DEPRECATEDIN_3_5)) {
        $s =~ s/\b\Q$kw\E\b//g;
    }
    $s =~ s/\b__attribute__\s*\(\([^)]*\)\)//g;
    $s =~ s/\s+/ /g;
    $s =~ s/^\s+|\s+$//g;
    return $s;
}

# Consume reserved-namespace decorations between a declaration's closing
# parenthesis and its semicolon, e.g. glibc's __THROW, __wur or
# __attr_access ((...)).  Only __-prefixed tokens (with an optional balanced
# argument list) are eaten, so a genuine following declaration is left alone.
sub skip_trailing_attributes
{
    my $s = shift;
    while (1) {
        $s =~ s/^\s+//;
        last unless $s =~ /^(__\w+)/;
        $s = substr($s, length($1));
        $s =~ s/^\s+//;
        if ($s =~ /^\(/) {
            my $depth = 0;
            my $i = 0;
            while ($i < length($s)) {
                my $c = substr($s, $i, 1);
                $depth++ if $c eq '(';
                $depth-- if $c eq ')';
                $i++;
                last if $depth == 0;
            }
            return $s if $depth != 0;
            $s = substr($s, $i);
        }
    }
    return $s;
}

sub find_function_decl
{
    my ($funcname) = @_;

    foreach my $entry (@search_files) {
        my ($file, $relpath, $is_system) = @$entry;
        unless (exists $file_cache{$file}) {
            my $text = '';
            if (open(my $fh, '<', $file)) {
                local $/;
                $text = <$fh>;
                close $fh;
            }
            $file_cache{$file} = strip_c_comments($text);
        }
        my $text = $file_cache{$file};

        while ($text =~ /\b\Q$funcname\E\s*\(/g) {
            my $name_start = $-[0];
            my $paren_start = pos($text);

            # Find matching closing paren, respecting nesting.
            my $depth = 1;
            my $cursor = $paren_start;
            while ($cursor < length($text) && $depth > 0) {
                my $c = substr($text, $cursor, 1);
                $depth++ if $c eq '(';
                $depth-- if $c eq ')';
                $cursor++;
            }
            next if $depth != 0;
            my $params_str =
                substr($text, $paren_start, $cursor - $paren_start - 1);

            # What follows must be ; for this to be a declaration, possibly
            # after trailing attribute macros (__THROW, __wur, ...).
            my $after = substr($text, $cursor);
            $after = skip_trailing_attributes($after);
            next unless $after =~ /^;/;

            # Anything since the previous statement terminator is the return
            # type expression.
            my $pre = substr($text, 0, $name_start);
            $pre =~ s/\s+$//;
            my $ret_start = 0;
            $ret_start = $-[2] if $pre =~ /([;}\n])([^;}\n]*)$/s;
            my $rettype = strip_attribute_macros(substr($pre, $ret_start));

            # Normalise to forward slashes for use as an #include path.
            my $include_path = $relpath;
            $include_path =~ s|\\|/|g;

            return { name         => $funcname,
                     rettype      => $rettype,
                     params       => $params_str,
                     file         => $file,
                     include_path => $include_path,
                     system       => $is_system };
        }
    }
    return undef;
}

# Split a parameter list on top-level commas, respecting parentheses
sub split_params
{
    my $str = shift;
    $str =~ s/^\s+|\s+$//g;
    return () if $str eq '' || $str eq 'void';

    my @parts;
    my $current = '';
    my $depth = 0;
    foreach my $c (split //, $str) {
        if ($c eq '(') {
            $depth++;
            $current .= $c;
        } elsif ($c eq ')') {
            $depth--;
            $current .= $c;
        } elsif ($c eq ',' && $depth == 0) {
            push @parts, $current;
            $current = '';
        } else {
            $current .= $c;
        }
    }
    push @parts, $current if $current ne '';
    foreach my $p (@parts) {
        $p =~ s/^\s+|\s+$//g;
    }
    return @parts;
}

sub parse_param
{
    my $param = shift;
    return { type => '', name => '', is_variadic => 1, is_ptr => 0 }
        if $param eq '...';

    # Drop the restrict qualifier; it plays no role in a mock signature.
    $param =~ s/\b(?:__restrict(?:__)?|restrict)\b//g;

    # Reduce TYPE NAME[size] to TYPE * NAME for our purposes.
    my $is_array = 0;
    $is_array = 1 if $param =~ s/\[\s*[^\]]*\s*\]\s*$//;

    my ($type, $name);
    if ($param =~ /^(.*?)([A-Za-z_]\w*)\s*$/) {
        $type = $1;
        $name = $2;
        $type =~ s/\s+$//;
    } else {
        $type = $param;
        $name = '';
    }

    # System headers name parameters in the reserved __ namespace; strip the
    # leading underscores so the generated wrap uses ordinary local names.
    $name =~ s/^_+//;

    return { type        => $type,
             name        => $name,
             is_ptr      => (($type =~ /\*/) || $is_array) ? 1 : 0,
             is_variadic => 0,
             is_array    => $is_array };
}

sub is_void_type
{
    my $t = shift;
    $t =~ s/^\s+|\s+$//g;
    $t =~ s/\s+/ /g;
    return $t eq 'void';
}

sub is_ptr_type { return $_[0] =~ /\*/; }

# Look up each WRAP entry's signature.  Functions we cannot find are
# skipped with a warning rather than aborting.
my @signatures;
my @found_includes;
my %seen_include;
foreach my $func (@wraps) {
    my $info = find_function_decl($func);
    if (!defined $info && !$system_loaded) {
        # Not in the project headers: pull in the system ones and retry.
        load_system_headers();
        $info = find_function_decl($func);
    }
    unless (defined $info) {
        warn "WARNING: $func: declaration not found in any include dir\n";
        next;
    }

    my @params = map { parse_param($_) } split_params($info->{params});

    my $idx = 0;
    foreach my $p (@params) {
        next if $p->{is_variadic};
        $p->{name} = "arg$idx" if $p->{name} eq '';
        $idx++;
    }

    push @signatures, { name    => $func,
                        rettype => $info->{rettype},
                        params  => \@params };

    unless ($seen_include{$info->{include_path}}) {
        $seen_include{$info->{include_path}} = 1;
        push @found_includes, { path   => $info->{include_path},
                                system => $info->{system} };
    }
    print STDERR "  found $func in $info->{file}\n" if $verbose;
}

die "No declarations found, nothing to emit\n" unless @signatures;

my $out_fh;
if (defined $output_file) {
    open($out_fh, '>', $output_file) or die "$output_file: $!\n";
} else {
    $out_fh = \*STDOUT;
}

print $out_fh <<"EOF";
/*
 * Copyright $YEAR The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

EOF

print $out_fh "#include <cmocka.h>\n";
print $out_fh "\n";
if (@found_includes) {
    my @angle;
    my @local;
    foreach my $inc (sort { $a->{path} cmp $b->{path} } @found_includes) {
        if ($inc->{system} || $inc->{path} =~ m|^openssl/|) {
            push @angle, $inc->{path};
        } else {
            push @local, $inc->{path};
        }
    }
    foreach my $inc (@angle) {
        print $out_fh "#include <$inc>\n";
    }
    print $out_fh "\n" if @angle && @local;
    foreach my $inc (@local) {
        print $out_fh "#include \"$inc\"\n";
    }
    print $out_fh "\n";
}

if ($mode eq 'wraps' || $mode eq 'both') {
    print $out_fh "/* wraps */\n\n";
    foreach my $f (@signatures) {
        print $out_fh format_signature($f->{rettype},
                                       "__wrap_$f->{name}",
                                       $f->{params}), ";\n";
    }
    print $out_fh "\n";

    foreach my $f (@signatures) {
        emit_wrap($out_fh, $f);
        print $out_fh "\n";
    }
}

if ($mode eq 'expects' || $mode eq 'both') {
    print $out_fh "/* expectations */\n\n";
    foreach my $f (@signatures) {
        emit_expect($out_fh, $f);
        print $out_fh "\n";
    }
}

close $out_fh if defined $output_file;

exit 0;

sub format_decl
{
    my ($type, $name) = @_;
    return $type =~ /\*$/ ? "$type$name" : "$type $name";
}

sub format_param
{
    my $p = shift;
    return '...' if $p->{is_variadic};
    return format_decl($p->{type}, $p->{name});
}

sub format_signature
{
    my ($rettype, $name, $params) = @_;
    my $param_str = @$params
        ? join(', ', map { format_param($_) } @$params)
        : 'void';
    return format_decl($rettype, $name) . "($param_str)";
}

sub emit_wrap
{
    my ($fh, $f) = @_;
    print $fh format_signature($f->{rettype}, "__wrap_$f->{name}",
                               $f->{params}), "\n{\n";
    print $fh "    function_called();\n";
    foreach my $p (@{$f->{params}}) {
        next if $p->{is_variadic};
        if ($p->{is_ptr}) {
            print $fh "    check_expected_ptr($p->{name});\n";
        } else {
            print $fh "    check_expected($p->{name});\n";
        }
    }
    if ( ! is_void_type($f->{rettype})) {
        if (is_ptr_type($f->{rettype})) {
            print $fh "\n    return mock_ptr_type($f->{rettype});\n";
        } else {
            print $fh "\n    return mock_type($f->{rettype});\n";
        }
    }
    print $fh "}\n";
}

sub emit_expect
{
    my ($fh, $f) = @_;
    my $name = $f->{name};
    my @params = @{$f->{params}};

    my @sig_parts;
    foreach my $p (@params) {
        next if $p->{is_variadic};
        push @sig_parts, format_param($p);
    }

    my $needs_rc = !is_void_type($f->{rettype});
    push @sig_parts, format_decl($f->{rettype}, 'rc') if $needs_rc;

    my $param_str = @sig_parts ? join(', ', @sig_parts) : 'void';
    print $fh "static void expect_$name($param_str)\n{\n";
    print $fh "    expect_function_call(__wrap_$name);\n";
    foreach my $p (@params) {
        next if $p->{is_variadic};
        print $fh "    expect_value(__wrap_$name, $p->{name}, $p->{name});\n";
    }
    print $fh "    will_return(__wrap_$name, rc);\n" if $needs_rc;
    print $fh "}\n";
}
