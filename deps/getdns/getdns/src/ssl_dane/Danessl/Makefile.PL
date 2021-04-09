#! /usr/bin/perl
use 5.012005;
use Config;

use ExtUtils::MakeMaker;

WriteMakefile(
    NAME              => 'Danessl',
    VERSION_FROM      => 'lib/Danessl.pm',
    ABSTRACT_FROM     => 'lib/Danessl.pm',
    AUTHOR            => 'Viktor Dukhovni <postfix-users@dukhovni.org>',
    LIBS              => [ "-L.. -ldanessl -lssl -lcrypto" ],
    INC               => "-I..",
);
