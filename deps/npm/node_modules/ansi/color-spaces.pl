#!/usr/bin/perl
# Author: Todd Larason <jtl@molehill.org>
# $XFree86: xc/programs/xterm/vttests/256colors2.pl,v 1.1 1999/07/11 08:49:54 dawes Exp $

print "256 color mode\n\n";

# display back ground colors

for ($fgbg = 38; $fgbg <= 48; $fgbg +=10) {

# first the system ones:
print "System colors:\n";
for ($color = 0; $color < 8; $color++) {
    print "\x1b[${fgbg};5;${color}m::";
}
print "\x1b[0m\n";
for ($color = 8; $color < 16; $color++) {
    print "\x1b[${fgbg};5;${color}m::";
}
print "\x1b[0m\n\n";

# now the color cube
print "Color cube, 6x6x6:\n";
for ($green = 0; $green < 6; $green++) {
    for ($red = 0; $red < 6; $red++) {
	for ($blue = 0; $blue < 6; $blue++) {
	    $color = 16 + ($red * 36) + ($green * 6) + $blue;
	    print "\x1b[${fgbg};5;${color}m::";
	}
	print "\x1b[0m ";
    }
    print "\n";
}

# now the grayscale ramp
print "Grayscale ramp:\n";
for ($color = 232; $color < 256; $color++) {
    print "\x1b[${fgbg};5;${color}m::";
}
print "\x1b[0m\n\n";

}

print "Examples for the 3-byte color mode\n\n";

for ($fgbg = 38; $fgbg <= 48; $fgbg +=10) {

# now the color cube
print "Color cube\n";
for ($green = 0; $green < 256; $green+=51) {
    for ($red = 0; $red < 256; $red+=51) {
	for ($blue = 0; $blue < 256; $blue+=51) {
            print "\x1b[${fgbg};2;${red};${green};${blue}m::";
	}
	print "\x1b[0m ";
    }
    print "\n";
}

# now the grayscale ramp
print "Grayscale ramp:\n";
for ($gray = 8; $gray < 256; $gray+=10) {
    print "\x1b[${fgbg};2;${gray};${gray};${gray}m::";
}
print "\x1b[0m\n\n";

}
