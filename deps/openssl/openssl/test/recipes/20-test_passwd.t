#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use OpenSSL::Test;
use OpenSSL::Test::Utils;

setup("test_passwd");

# The following tests are an adaptation of those in
# https://www.akkadia.org/drepper/SHA-crypt.txt
my @sha_tests =
    ({ type => '5',
       salt => 'saltstring',
       key => 'Hello world!',
       expected => '$5$saltstring$5B8vYYiY.CVt1RlTTf8KbXBH3hsxY/GNooZaBBGWEc5' },
     { type => '5',
       salt => 'rounds=10000$saltstringsaltstring',
       key => 'Hello world!',
       expected => '$5$rounds=10000$saltstringsaltst$3xv.VbSHBb41AL9AvLeujZkZRBAwqFMz2.opqey6IcA' },
     { type => '5',
       salt => 'rounds=5000$toolongsaltstring',
       key => 'This is just a test',
       expected => '$5$rounds=5000$toolongsaltstrin$Un/5jzAHMgOGZ5.mWJpuVolil07guHPvOW8mGRcvxa5' },
     { type => '5',
       salt => 'rounds=1400$anotherlongsaltstring',
       key => 'a very much longer text to encrypt.  This one even stretches over morethan one line.',
       expected => '$5$rounds=1400$anotherlongsalts$Rx.j8H.h8HjEDGomFU8bDkXm3XIUnzyxf12oP84Bnq1' },
     { type => '5',
       salt => 'rounds=77777$short',
       key => 'we have a short salt string but not a short password',
       expected => '$5$rounds=77777$short$JiO1O3ZpDAxGJeaDIuqCoEFysAe1mZNJRs3pw0KQRd/' },
     { type => '5',
       salt => 'rounds=123456$asaltof16chars..',
       key => 'a short string',
       expected => '$5$rounds=123456$asaltof16chars..$gP3VQ/6X7UUEW3HkBn2w1/Ptq2jxPyzV/cZKmF/wJvD' },
     { type => '5',
       salt => 'rounds=10$roundstoolow',
       key => 'the minimum number is still observed',
       expected => '$5$rounds=1000$roundstoolow$yfvwcWrQ8l/K0DAWyuPMDNHpIVlTQebY9l/gL972bIC' },
     { type => '6',
       salt => 'saltstring',
       key => 'Hello world!',
       expected => '$6$saltstring$svn8UoSVapNtMuq1ukKS4tPQd8iKwSMHWjl/O817G3uBnIFNjnQJuesI68u4OTLiBFdcbYEdFCoEOfaS35inz1' },
     { type => '6',
       salt => 'rounds=10000$saltstringsaltstring',
       key => 'Hello world!',
       expected => '$6$rounds=10000$saltstringsaltst$OW1/O6BYHV6BcXZu8QVeXbDWra3Oeqh0sbHbbMCVNSnCM/UrjmM0Dp8vOuZeHBy/YTBmSK6H9qs/y3RnOaw5v.' },
     { type => '6',
       salt => 'rounds=5000$toolongsaltstring',
       key => 'This is just a test',
       expected => '$6$rounds=5000$toolongsaltstrin$lQ8jolhgVRVhY4b5pZKaysCLi0QBxGoNeKQzQ3glMhwllF7oGDZxUhx1yxdYcz/e1JSbq3y6JMxxl8audkUEm0' },
     { type => '6',
       salt => 'rounds=1400$anotherlongsaltstring',
       key => 'a very much longer text to encrypt.  This one even stretches over morethan one line.',
       expected => '$6$rounds=1400$anotherlongsalts$POfYwTEok97VWcjxIiSOjiykti.o/pQs.wPvMxQ6Fm7I6IoYN3CmLs66x9t0oSwbtEW7o7UmJEiDwGqd8p4ur1' },
     { type => '6',
       salt => 'rounds=77777$short',
       key => 'we have a short salt string but not a short password',
       expected => '$6$rounds=77777$short$WuQyW2YR.hBNpjjRhpYD/ifIw05xdfeEyQoMxIXbkvr0gge1a1x3yRULJ5CCaUeOxFmtlcGZelFl5CxtgfiAc0' },
     { type => '6',
       salt => 'rounds=123456$asaltof16chars..',
       key => 'a short string',
       expected => '$6$rounds=123456$asaltof16chars..$BtCwjqMJGx5hrJhZywWvt0RLE8uZ4oPwcelCjmw2kSYu.Ec6ycULevoBK25fs2xXgMNrCzIMVcgEJAstJeonj1' },
     { type => '6',
       salt => 'rounds=10$roundstoolow',
       key => 'the minimum number is still observed',
       expected => '$6$rounds=1000$roundstoolow$kUMsbe306n21p9R.FRkW3IGn.S9NPN0x50YhH1xhLsPuWGsUSklZt58jaTfF4ZEQpyUNGc0dqbpBYYBaHHrsX.' }
    );

plan tests => (disabled("des") ? 9 : 11) + scalar @sha_tests;


ok(compare1stline_re([qw{openssl passwd password}], '^.{13}\R$'),
   'crypt password with random salt') if !disabled("des");
ok(compare1stline_re([qw{openssl passwd -1 password}], '^\$1\$.{8}\$.{22}\R$'),
   'BSD style MD5 password with random salt');
ok(compare1stline_re([qw{openssl passwd -apr1 password}], '^\$apr1\$.{8}\$.{22}\R$'),
   'Apache style MD5 password with random salt');
ok(compare1stline_re([qw{openssl passwd -5 password}], '^\$5\$.{16}\$.{43}\R$'),
   'SHA256 password with random salt');
ok(compare1stline_re([qw{openssl passwd -6 password}], '^\$6\$.{16}\$.{86}\R$'),
   'Apache SHA512 password with random salt');

ok(compare1stline([qw{openssl passwd -salt xx password}], 'xxj31ZMTZzkVA'),
   'crypt password with salt xx') if !disabled("des");
ok(compare1stline([qw{openssl passwd -salt xxxxxxxx -1 password}], '$1$xxxxxxxx$UYCIxa628.9qXjpQCjM4a.'),
   'BSD style MD5 password with salt xxxxxxxx');
ok(compare1stline([qw{openssl passwd -salt xxxxxxxx -apr1 password}], '$apr1$xxxxxxxx$dxHfLAsjHkDRmG83UXe8K0'),
   'Apache style MD5 password with salt xxxxxxxx');
ok(compare1stline([qw{openssl passwd -salt xxxxxxxx -aixmd5 password}], 'xxxxxxxx$8Oaipk/GPKhC64w/YVeFD/'),
   'AIX style MD5 password with salt xxxxxxxx');
ok(compare1stline([qw{openssl passwd -salt xxxxxxxxxxxxxxxx -5 password}], '$5$xxxxxxxxxxxxxxxx$fHytsM.wVD..zPN/h3i40WJRggt/1f73XkAC/gkelkB'),
   'SHA256 password with salt xxxxxxxxxxxxxxxx');
ok(compare1stline([qw{openssl passwd -salt xxxxxxxxxxxxxxxx -6 password}], '$6$xxxxxxxxxxxxxxxx$VjGUrXBG6/8yW0f6ikBJVOb/lK/Tm9LxHJmFfwMvT7cpk64N9BW7ZQhNeMXAYFbOJ6HDG7wb0QpxJyYQn0rh81'),
   'SHA512 password with salt xxxxxxxxxxxxxxxx');

foreach (@sha_tests) {
    ok(compare1stline([qw{openssl passwd}, '-'.$_->{type}, '-salt', $_->{salt},
                       $_->{key}], $_->{expected}),
       { 5 => 'SHA256', 6 => 'SHA512' }->{$_->{type}} . ' password with salt ' . $_->{salt});
}


sub compare1stline_re {
    my ($cmdarray, $regexp) = @_;
    my @lines = run(app($cmdarray), capture => 1);

    return $lines[0] =~ m|$regexp|;
}

sub compare1stline {
    my ($cmdarray, $str) = @_;
    my @lines = run(app($cmdarray), capture => 1);

    return $lines[0] =~ m|^\Q${str}\E\R$|;
}
