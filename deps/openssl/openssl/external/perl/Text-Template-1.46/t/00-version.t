#!perl

use Text::Template;
print "1..1\n";

if ($Text::Template::VERSION == 1.46) {
        print "ok 1\n";
} else {
        print "not ok 1\n";
}

