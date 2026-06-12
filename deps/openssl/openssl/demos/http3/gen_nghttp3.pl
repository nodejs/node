#!/usr/bin/env perl
#

use File::Copy;
use File::Path;
use Fcntl ':flock';
use strict;
use warnings;

#open STDOUT, '>&STDERR';

chdir "demos/http3";
open(my $fh, '>>', './build.info') or die "Could not open build.info - $!";
flock($fh, LOCK_EX) or die "Could not lock build.info - $!";

if (-d "./nghttp3") {
    rmtree("./nghttp3") or die "Cannot remove nghttp3: $!";
}
system("git clone https://github.com/ngtcp2/nghttp3.git");

chdir "nghttp3";
mkdir "build";
system("git submodule init ./lib/sfparse ./tests/munit");
system("git submodule update");
system("cmake -DENABLE_LIB_ONLY=1 -S . -B build");
system("cmake --build build");

my $libs="./build/lib/libnghttp*";

for my $file (glob $libs) {
    copy($file, "..");
}

chdir "../../..";
close($fh);

exit(0);
