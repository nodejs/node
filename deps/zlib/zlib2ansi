#!/usr/bin/perl

# Transform K&R C function definitions into ANSI equivalent.
#
# Author: Paul Marquess
# Version: 1.0
# Date: 3 October 2006

# TODO
#
# Asumes no function pointer parameters. unless they are typedefed.
# Assumes no literal strings that look like function definitions
# Assumes functions start at the beginning of a line

use strict;
use warnings;

local $/;
$_ = <>;

my $sp = qr{ \s* (?: /\* .*? \*/ )? \s* }x; # assume no nested comments

my $d1    = qr{ $sp (?: [\w\*\s]+ $sp)* $sp \w+ $sp [\[\]\s]* $sp }x ;
my $decl  = qr{ $sp (?: \w+ $sp )+ $d1 }xo ;
my $dList = qr{ $sp $decl (?: $sp , $d1 )* $sp ; $sp }xo ;


while (s/^
            (                  # Start $1
                (              #   Start $2
                    .*?        #     Minimal eat content
                    ( ^ \w [\w\s\*]+ )    #     $3 -- function name
                    \s*        #     optional whitespace
                )              # $2 - Matched up to before parameter list

                \( \s*         # Literal "(" + optional whitespace
                ( [^\)]+ )     # $4 - one or more anythings except ")"
                \s* \)         # optional whitespace surrounding a Literal ")"

                ( (?: $dList )+ ) # $5

                $sp ^ {        # literal "{" at start of line
            )                  # Remember to $1
        //xsom
      )
{
    my $all = $1 ;
    my $prefix = $2;
    my $param_list = $4 ;
    my $params = $5;

    StripComments($params);
    StripComments($param_list);
    $param_list =~ s/^\s+//;
    $param_list =~ s/\s+$//;

    my $i = 0 ;
    my %pList = map { $_ => $i++ }
                split /\s*,\s*/, $param_list;
    my $pMatch = '(\b' . join('|', keys %pList) . '\b)\W*$' ;

    my @params = split /\s*;\s*/, $params;
    my @outParams = ();
    foreach my $p (@params)
    {
        if ($p =~ /,/)
        {
            my @bits = split /\s*,\s*/, $p;
            my $first = shift @bits;
            $first =~ s/^\s*//;
            push @outParams, $first;
            $first =~ /^(\w+\s*)/;
            my $type = $1 ;
            push @outParams, map { $type . $_ } @bits;
        }
        else
        {
            $p =~ s/^\s+//;
            push @outParams, $p;
        }
    }


    my %tmp = map { /$pMatch/;  $_ => $pList{$1}  }
              @outParams ;

    @outParams = map  { "    $_" }
                 sort { $tmp{$a} <=> $tmp{$b} }
                 @outParams ;

    print $prefix ;
    print "(\n" . join(",\n", @outParams) . ")\n";
    print "{" ;

}

# Output any trailing code.
print ;
exit 0;


sub StripComments
{

  no warnings;

  # Strip C & C++ coments
  # From the perlfaq
  $_[0] =~

    s{
       /\*         ##  Start of /* ... */ comment
       [^*]*\*+    ##  Non-* followed by 1-or-more *'s
       (
         [^/*][^*]*\*+
       )*          ##  0-or-more things which don't start with /
                   ##    but do end with '*'
       /           ##  End of /* ... */ comment

     |         ##     OR  C++ Comment
       //          ## Start of C++ comment //
       [^\n]*      ## followed by 0-or-more non end of line characters

     |         ##     OR  various things which aren't comments:

       (
         "           ##  Start of " ... " string
         (
           \\.           ##  Escaped char
         |               ##    OR
           [^"\\]        ##  Non "\
         )*
         "           ##  End of " ... " string

       |         ##     OR

         '           ##  Start of ' ... ' string
         (
           \\.           ##  Escaped char
         |               ##    OR
           [^'\\]        ##  Non '\
         )*
         '           ##  End of ' ... ' string

       |         ##     OR

         .           ##  Anything other char
         [^/"'\\]*   ##  Chars which doesn't start a comment, string or escape
       )
     }{$2}gxs;

}
