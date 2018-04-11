#! /usr/bin/env perl
# Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use File::Spec::Functions;
use File::Basename;
use File::Copy;
use File::Path;
use FindBin;
use lib "$FindBin::Bin/perl";
use OpenSSL::Glob;
use Getopt::Long;
use Pod::Usage;

use lib '.';
use configdata;

# We know we are in the 'util' directory and that our perl modules are
# in util/perl
use lib catdir(dirname($0), "perl");
use OpenSSL::Util::Pod;

my %options = ();
GetOptions(\%options,
           'sourcedir=s',       # Source directory
           'subdir=s%',         # Subdirectories to look through,
                                # with associated section numbers
           'destdir=s',         # Destination directory
           #'in=s@',             # Explicit files to process (ignores sourcedir)
           #'section=i',         # Default section used for --in files
           'type=s',            # The result type, 'man' or 'html'
           'suffix:s',          # Suffix to add to the extension.
                                # Only used with type=man
           'remove',            # To remove files rather than writing them
           'dry-run|n',         # Only output file names on STDOUT
           'debug|D+',
          );

unless ($options{subdir}) {
    $options{subdir} = { apps   => '1',
                         crypto => '3',
                         ssl    => '3' };
}
unless ($options{sourcedir}) {
    $options{sourcedir} = catdir($config{sourcedir}, "doc");
}
pod2usage(1) unless ( defined $options{subdir}
                      && defined $options{sourcedir}
                      && defined $options{destdir}
                      && defined $options{type}
                      && ($options{type} eq 'man'
                          || $options{type} eq 'html') );
pod2usage(1) if ( $options{type} eq 'html'
                  && defined $options{suffix} );

if ($options{debug}) {
    print STDERR "DEBUG: options:\n";
    print STDERR "DEBUG:   --sourcedir = $options{sourcedir}\n"
        if defined $options{sourcedir};
    print STDERR "DEBUG:   --destdir   = $options{destdir}\n"
        if defined $options{destdir};
    print STDERR "DEBUG:   --type      = $options{type}\n"
        if defined $options{type};
    print STDERR "DEBUG:   --suffix    = $options{suffix}\n"
        if defined $options{suffix};
    foreach (keys %{$options{subdir}}) {
        print STDERR "DEBUG:   --subdir    = $_=$options{subdir}->{$_}\n";
    }
    print STDERR "DEBUG:   --remove    = $options{remove}\n"
        if defined $options{remove};
    print STDERR "DEBUG:   --debug     = $options{debug}\n"
        if defined $options{debug};
    print STDERR "DEBUG:   --dry-run   = $options{\"dry-run\"}\n"
        if defined $options{"dry-run"};
}

my $symlink_exists = eval { symlink("",""); 1 };

foreach my $subdir (keys %{$options{subdir}}) {
    my $section = $options{subdir}->{$subdir};
    my $podsourcedir = catfile($options{sourcedir}, $subdir);
    my $podglob = catfile($podsourcedir, "*.pod");

    foreach my $podfile (glob $podglob) {
        my $podname = basename($podfile, ".pod");
        my $podpath = catfile($podfile);
        my %podinfo = extract_pod_info($podpath,
                                       { debug => $options{debug},
                                         section => $section });
        my @podfiles = grep { $_ ne $podname } @{$podinfo{names}};

        my $updir = updir();
        my $name = uc $podname;
        my $suffix = { man  => ".$podinfo{section}".($options{suffix} // ""),
                       html => ".html" } -> {$options{type}};
        my $generate = { man  => "pod2man --name=$name --section=$podinfo{section} --center=OpenSSL --release=$config{version} \"$podpath\"",
                         html => "pod2html \"--podroot=$options{sourcedir}\" --htmldir=$updir --podpath=apps:crypto:ssl \"--infile=$podpath\" \"--title=$podname\""
                         } -> {$options{type}};
        my $output_dir = catdir($options{destdir}, "man$podinfo{section}");
        my $output_file = $podname . $suffix;
        my $output_path = catfile($output_dir, $output_file);

        if (! $options{remove}) {
            my @output;
            print STDERR "DEBUG: Processing, using \"$generate\"\n"
                if $options{debug};
            unless ($options{"dry-run"}) {
                @output = `$generate`;
                map { s|href="http://man\.he\.net/(man\d/[^"]+)(?:\.html)?"|href="../$1.html|g; } @output
                    if $options{type} eq "html";
            }
            print STDERR "DEBUG: Done processing\n" if $options{debug};

            if (! -d $output_dir) {
                print STDERR "DEBUG: Creating directory $output_dir\n" if $options{debug};
                unless ($options{"dry-run"}) {
                    mkpath $output_dir
                        or die "Trying to create directory $output_dir: $!\n";
                }
            }
            print STDERR "DEBUG: Writing $output_path\n" if $options{debug};
            unless ($options{"dry-run"}) {
                open my $output_fh, '>', $output_path
                    or die "Trying to write to $output_path: $!\n";
                foreach (@output) {
                    print $output_fh $_;
                }
                close $output_fh;
            }
            print STDERR "DEBUG: Done writing $output_path\n" if $options{debug};
        } else {
            print STDERR "DEBUG: Removing $output_path\n" if $options{debug};
            unless ($options{"dry-run"}) {
                while (unlink $output_path) {}
            }
        }
        print "$output_path\n";

        foreach (@podfiles) {
            my $link_file = $_ . $suffix;
            my $link_path = catfile($output_dir, $link_file);
            if (! $options{remove}) {
                if ($symlink_exists) {
                    print STDERR "DEBUG: Linking $link_path -> $output_file\n"
                        if $options{debug};
                    unless ($options{"dry-run"}) {
                        symlink $output_file, $link_path;
                    }
                } else {
                    print STDERR "DEBUG: Copying $output_path to link_path\n"
                        if $options{debug};
                    unless ($options{"dry-run"}) {
                        copy $output_path, $link_path;
                    }
                }
            } else {
                print STDERR "DEBUG: Removing $link_path\n" if $options{debug};
                unless ($options{"dry-run"}) {
                    while (unlink $link_path) {}
                }
            }
            print "$link_path -> $output_path\n";
        }
    }
}

__END__

=pod

=head1 NAME

process_docs.pl - A script to process OpenSSL docs

=head1 SYNOPSIS

B<process_docs.pl>
[B<--sourcedir>=I<dir>]
B<--destdir>=I<dir>
B<--type>=B<man>|B<html>
[B<--suffix>=I<suffix>]
[B<--remove>]
[B<--dry-run>|B<-n>]
[B<--debug>|B<-D>]

=head1 DESCRIPTION

This script looks for .pod files in the subdirectories 'apps', 'crypto'
and 'ssl' under the given source directory.

The OpenSSL configuration data file F<configdata.pm> I<must> reside in
the current directory, I<or> perl must have the directory it resides in
in its inclusion array.  For the latter variant, a call like this would
work:

 perl -I../foo util/process_docs.pl {options ...}

=head1 OPTIONS

=over 4

=item B<--sourcedir>=I<dir>

Top directory where the source files are found.

=item B<--destdir>=I<dir>

Top directory where the resulting files should end up

=item B<--type>=B<man>|B<html>

Type of output to produce.  Currently supported are man pages and HTML files.

=item B<--suffix>=I<suffix>

A suffix added to the extension.  Only valid with B<--type>=B<man>

=item B<--remove>

Instead of writing the files, remove them.

=item B<--dry-run>|B<-n>

Do not perform any file writing, directory creation or file removal.

=item B<--debug>|B<-D>

Print extra debugging output.

=back

=head1 COPYRIGHT

Copyright 2013-2016 The OpenSSL Project Authors. All Rights Reserved.

Licensed under the OpenSSL license (the "License").  You may not use
this file except in compliance with the License.  You can obtain a copy
in the file LICENSE in the source distribution or at
https://www.openssl.org/source/license.html

=cut
