#! /usr/bin/env perl
# -*- mode: perl; -*-
# Copyright 2016-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

# This is a collection of extra attributes to be used as input for creating
# shared libraries, currently on any Unix variant, including Unix like
# environments on Windows.

sub detect_gnu_ld {
    my @lines =
        `$config{CROSS_COMPILE}$config{CC} -Wl,-V /dev/null 2>&1`;
    return grep /^GNU ld/, @lines;
}
sub detect_gnu_cc {
    my @lines =
        `$config{CROSS_COMPILE}$config{CC} -v 2>&1`;
    return grep /gcc/, @lines;
}

my %shared_info;
%shared_info = (
    'gnu-shared' => {
        shared_ldflag         => '-shared -Wl,-Bsymbolic',
        shared_sonameflag     => '-Wl,-soname=',
    },
    'linux-shared' => sub {
        return {
            %{$shared_info{'gnu-shared'}},
            shared_defflag    => '-Wl,--version-script=',
        };
    },
    'bsd-gcc-shared' => sub { return $shared_info{'linux-shared'}; },
    'bsd-shared' => sub {
        return $shared_info{'gnu-shared'} if detect_gnu_ld();
        return {
            shared_ldflag     => '-shared -nostdlib',
        };
    },
    'darwin-shared' => {
        module_ldflags        => '-bundle',
        shared_ldflag         => '-dynamiclib -current_version $(SHLIB_VERSION_NUMBER) -compatibility_version $(SHLIB_VERSION_NUMBER)',
        shared_sonameflag     => '-install_name $(INSTALLTOP)/$(LIBDIR)/',
    },
    'cygwin-shared' => {
        shared_ldflag         => '-shared -Wl,--enable-auto-image-base',
        shared_impflag        => '-Wl,--out-implib=',
    },
    'mingw-shared' => sub {
        return {
            %{$shared_info{'cygwin-shared'}},
            # def_flag made to empty string so  it still generates
            # something
            shared_defflag    => '',
        };
    },
    'alpha-osf1-shared' => sub {
        return $shared_info{'gnu-shared'} if detect_gnu_ld();
        return {
            module_ldflags    => '-shared -Wl,-Bsymbolic',
            shared_ldflag     => '-shared -Wl,-Bsymbolic -set_version $(SHLIB_VERSION_NUMBER)',
        };
    },
    'svr3-shared' => sub {
        return $shared_info{'gnu-shared'} if detect_gnu_ld();
        return {
            shared_ldflag     => '-G',
            shared_sonameflag => '-h ',
        };
    },
    'svr5-shared' => sub {
        return $shared_info{'gnu-shared'} if detect_gnu_ld();
        return {
            shared_ldflag     => detect_gnu_cc() ? '-shared' : '-G',
            shared_sonameflag => '-h ',
        };
    },
);
