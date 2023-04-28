#! /usr/bin/env perl
#
# Copyright 2020-2022 The OpenSSL Project Authors. All Rights Reserved.
# Copyright Siemens AG 2019-2022
#
# Licensed under the Apache License 2.0 (the "License").
# You may not use this file except in compliance with the License.
# You can obtain a copy in the file LICENSE in the source distribution
# or at https://www.openssl.org/source/license.html
#
# check-format.pl
# - check formatting of C source according to OpenSSL coding style
#
# usage:
#   check-format.pl [-l|--sloppy-len] [-l|--sloppy-bodylen]
#                   [-s|--sloppy-space] [-c|--sloppy-comment]
#                   [-m|--sloppy-macro] [-h|--sloppy-hang]
#                   [-e|--eol-comment] [-1|--1-stmt]
#                   <files>
#
# run self-tests:
#   util/check-format.pl util/check-format-test-positives.c
#   util/check-format.pl util/check-format-test-negatives.c
#
# checks adherence to the formatting rules of the OpenSSL coding guidelines
# assuming that the input files contain syntactically correct C code.
# This pragmatic tool is incomplete and yields some false positives.
# Still it should be useful for detecting most typical glitches.
#
# options:
#  -l | --sloppy-len     increase accepted max line length from 80 to 84
#  -l | --sloppy-bodylen do not report function body length > 200
#  -s | --sloppy-space   do not report whitespace nits
#  -c | --sloppy-comment do not report indentation of comments
#                        Otherwise for each multi-line comment the indentation of
#                        its lines is checked for consistency. For each comment
#                        that does not begin to the right of normal code its
#                        indentation must be as for normal code, while in case it
#                        also has no normal code to its right it is considered to
#                        refer to the following line and may be indented equally.
#  -m | --sloppy-macro   allow missing extra indentation of macro bodies
#  -h | --sloppy-hang    when checking hanging indentation, do not report
#                        * same indentation as on line before
#                        * same indentation as non-hanging indent level
#                        * indentation moved left (not beyond non-hanging indent)
#                          just to fit contents within the line length limit
#  -e | --eol-comment    report needless intermediate multiple consecutive spaces also before end-of-line comments
#  -1 | --1-stmt         do more aggressive checks for { 1 stmt } - see below
#
# There are non-trivial false positives and negatives such as the following.
#
# * When a line contains several issues of the same kind only one is reported.
#
# * When a line contains more than one statement this is (correctly) reported
#   but in some situations the indentation checks for subsequent lines go wrong.
#
# * There is the special OpenSSL rule not to unnecessarily use braces around
#   single statements:
#   {
#       stmt;
#   }
#   except within if ... else constructs where some branch contains more than one
#   statement. Since the exception is hard to recognize when such branches occur
#   after the current position (such that false positives would be reported)
#   the tool by checks for this rule by default only for do/while/for bodies.
#   Yet with the --1-stmt option false positives are preferred over negatives.
#   False negatives occur if the braces are more than two non-blank lines apart.
#
# * The presence of multiple consecutive spaces is regarded a coding style nit
#   except when this is before end-of-line comments (unless the --eol-comment is given) and
#   except when done in order to align certain columns over multiple lines, e.g.:
#   # define AB  1
#   # define CDE 22
#   # define F   3333
#   This pattern is recognized - and consequently extra space not reported -
#   for a given line if in the non-blank line before or after (if existing)
#   for each occurrence of "  \S" (where \S means non-space) in the given line
#   there is " \S" in the other line in the respective column position.
#   This may lead to both false negatives (in case of coincidental " \S")
#   and false positives (in case of more complex multi-column alignment).
#
# * When just part of control structures depend on #if(n)(def), which can be
#   considered bad programming style, indentation false positives occur, e.g.:
#   #if X
#       if (1) /* bad style */
#   #else
#       if (2) /* bad style resulting in false positive */
#   #endif
#           c; /* resulting further false positive */

use strict;
# use List::Util qw[min max];
use POSIX;

use constant INDENT_LEVEL => 4;
use constant MAX_LINE_LENGTH => 80;
use constant MAX_BODY_LENGTH => 200;

# global variables @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

# command-line options
my $max_length = MAX_LINE_LENGTH;
my $sloppy_bodylen = 0;
my $sloppy_SPC = 0;
my $sloppy_hang = 0;
my $sloppy_cmt = 0;
my $sloppy_macro = 0;
my $eol_cmt = 0;
my $extended_1_stmt = 0;

while ($ARGV[0] =~ m/^-(\w|-[\w\-]+)$/) {
    my $arg = $1; shift;
    if ($arg =~ m/^(l|-sloppy-len)$/) {
        $max_length += INDENT_LEVEL;
    } elsif ($arg =~ m/^(b|-sloppy-bodylen)$/) {
        $sloppy_bodylen = 1;
    } elsif ($arg =~ m/^(s|-sloppy-space)$/) {
        $sloppy_SPC= 1;
    } elsif ($arg =~ m/^(c|-sloppy-comment)$/) {
        $sloppy_cmt = 1;
    } elsif ($arg =~ m/^(m|-sloppy-macro)$/) {
        $sloppy_macro = 1;
    } elsif ($arg =~ m/^(h|-sloppy-hang)$/) {
        $sloppy_hang = 1;
    } elsif ($arg =~ m/^(e|-eol-comment)$/) {
        $eol_cmt = 1;
    } elsif ($arg =~ m/^(1|-1-stmt)$/) {
        $extended_1_stmt = 1;
    } else {
        die("unknown option: -$arg");
    }
}

# status variables
my $self_test;             # whether the current input file is regarded to contain (positive/negative) self-tests
my $line;                  # current line number
my $line_before;           # number of previous not essentially blank line (containing at most whitespace and '\')
my $line_before2;          # number of not essentially blank line before previous not essentially blank line
my $contents;              # contents of current line (without blinding)
#  $_                      # current line, where comments etc. get blinded
my $code_contents_before;  # contents of previous non-comment non-directive line (without blinding), initially ""
my $contents_before;       # contents of $line_before (without blinding), if $line_before > 0
my $contents_before_;      # contents of $line_before after blinding comments etc., if $line_before > 0
my $contents_before2;      # contents of $line_before2  (without blinding), if $line_before2 > 0
my $contents_before_2;     # contents of $line_before2 after blinding comments etc., if $line_before2 > 0
my $in_multiline_string;   # line starts within multi-line string literal
my $count;                 # -1 or number of leading whitespace characters (except newline) in current line,
                           # which should be $block_indent + $hanging_offset + $local_offset or $expr_indent
my $count_before;          # number of leading whitespace characters (except line ending chars) in $contents_before
my $has_label;             # current line contains label
my $local_offset;          # current extra indent due to label, switch case/default, or leading closing brace(s)
my $line_body_start;       # number of line where last function body started, or 0
my $line_function_start;   # number of line where last function definition started, used if $line_body_start != 0
my $last_function_header;  # header containing name of last function defined, used if $line_function_start != 0
my $line_opening_brace;    # number of previous line with opening brace after do/while/for, optionally for if/else

my $keyword_opening_brace; # name of previous keyword, used if $line_opening_brace != 0
my $ifdef__cplusplus;      # line before contained '#ifdef __cplusplus' (used in header files)
my $block_indent;          # currently required normal indentation at block/statement level
my $hanging_offset;        # extra indent, which may be nested, for just one hanging statement or expr or typedef
my @in_do_hanging_offsets; # stack of hanging offsets for nested 'do' ... 'while'
my @in_if_hanging_offsets; # stack of hanging offsets for nested 'if' (but not its potential 'else' branch)
my $if_maybe_terminated;   # 'if' ends and $hanging_offset should be reset unless the next line starts with 'else'
my @nested_block_indents;  # stack of indentations at block/statement level, needed due to hanging statements
my @nested_hanging_offsets;# stack of nested $hanging_offset values, in parallel to @nested_block_indents
my @nested_in_typedecl;    # stack of nested $in_typedecl values, partly in parallel to @nested_block_indents
my @nested_indents;        # stack of hanging indents due to parentheses, braces, brackets, or conditionals
my @nested_symbols;        # stack of hanging symbols '(', '{', '[', or '?', in parallel to @nested_indents
my @nested_conds_indents;  # stack of hanging indents due to conditionals ('?' ... ':')
my $expr_indent;           # resulting hanging indent within (multi-line) expressions including type exprs, else 0
my $hanging_symbol;        # character ('(', '{', '[', not: '?') responsible for $expr_indent, if $expr_indent != 0
my $in_block_decls;        # number of local declaration lines after block opening before normal statements, or -1 if no block opening
my $in_expr;               # in expression after if/while/for/switch/return/enum/LHS of assignment
my $in_paren_expr;         # in parenthesized if/while/for condition and switch expression, if $expr_indent != 0
my $in_typedecl;           # nesting level of typedef/struct/union/enum
my $in_directive;          # number of lines so far within preprocessor directive, e.g., macro definition
my $directive_nesting;     # currently required indentation of preprocessor directive according to #if(n)(def)
my $directive_offset;      # indent offset within multi-line preprocessor directive, if $in_directive > 0
my $in_macro_header;       # number of open parentheses + 1 in (multi-line) header of #define, if $in_directive > 0
my $in_comment;            # number of lines so far within multi-line comment, or < 0 when end is on current line
my $leading_comment;       # multi-line comment has no code before its beginning delimiter
my $formatted_comment;     # multi-line comment beginning with "/*-", which indicates/allows special formatting
my $comment_indent;        # comment indent, if $in_comment != 0
my $num_reports_line = 0;  # number of issues found on current line
my $num_reports = 0;       # total number of issues found
my $num_indent_reports = 0;# total number of indentation issues found
my $num_nesting_issues = 0;# total number of directive nesting issues found
my $num_syntax_issues = 0; # total number of syntax issues found during sanity checks
my $num_SPC_reports = 0;   # total number of whitespace issues found
my $num_length_reports = 0;# total number of line length issues found

sub reset_file_state {
    $line = 0;
    $line_before = 0;
    $line_before2 = 0;
    $code_contents_before = "";
    @nested_block_indents = ();
    @nested_hanging_offsets = ();
    @nested_in_typedecl = ();
    @nested_symbols = ();
    @nested_indents = ();
    @nested_conds_indents = ();
    $expr_indent = 0;
    $in_block_decls = -1;
    $in_expr = 0;
    $in_paren_expr = 0;
    $hanging_offset = 0;
    @in_do_hanging_offsets = ();
    @in_if_hanging_offsets = ();
    $if_maybe_terminated = 0;
    $block_indent = 0;
    $ifdef__cplusplus = 0;
    $in_multiline_string = 0;
    $line_body_start = 0;
    $line_opening_brace = 0;
    $in_typedecl = 0;
    $in_directive = 0;
    $directive_nesting = 0;
    $in_comment = 0;
}

# auxiliary submodules @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

sub report_flexibly {
    my $line = shift;
    my $msg = shift;
    my $contents = shift;
    my $report_SPC = $msg =~ /space/;
    return if $report_SPC && $sloppy_SPC;

    print "$ARGV:$line:$msg:$contents" unless $self_test;
    $num_reports_line++;
    $num_reports++;
    $num_indent_reports++ if $msg =~ m/indent/;
    $num_nesting_issues++ if $msg =~ m/directive nesting/;
    $num_syntax_issues++  if $msg =~ m/unclosed|unexpected/;
    $num_SPC_reports++    if $report_SPC;
    $num_length_reports++ if $msg =~ m/length/;
}

sub report {
    my $msg = shift;
    report_flexibly($line, $msg, $contents);
}

sub parens_balance { # count balance of opening parentheses - closing parentheses
    my $str = shift;
    return $str =~ tr/\(// - $str =~ tr/\)//;
}

sub blind_nonspace { # blind non-space text of comment as @, preserving length and spaces
    # the @ character is used because it cannot occur in normal program code so there is no confusion
    # comment text is not blinded to whitespace in order to be able to check extra SPC also in comments
    my $comment_text = shift;
    $comment_text =~ s/([\.\?\!])\s\s/$1. /g; # in extra SPC checks allow one extra SPC after period '.', '?', or '!' in comments
    return $comment_text =~ tr/ /@/cr;
}

# submodule for indentation checking/reporting @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

sub check_indent { # used for lines outside multi-line string literals
    my $stmt_indent = $block_indent + $hanging_offset + $local_offset;
    $stmt_indent = 0 if $stmt_indent < 0; # TODO maybe give warning/error
    my $stmt_desc = $contents =~
        m/^\s*\/\*/ ? "intra-line comment" :
        $has_label ? "label" :
        ($hanging_offset != 0 ? "hanging " : "").
        ($hanging_offset != 0 ? "stmt/expr" : "stmt/decl"); # $in_typedecl is not fully to the point here
    my ($ref_desc, $ref_indent) = $expr_indent == 0 ? ($stmt_desc, $stmt_indent)
                                                    : ("hanging '$hanging_symbol'", $expr_indent);
    my ($alt_desc, $alt_indent) = ("", $ref_indent);

    # allow indent 1 for labels - this cannot happen for leading ':'
    ($alt_desc, $alt_indent) = ("outermost position", 1) if $expr_indent == 0 && $has_label;

    if (@nested_conds_indents != 0 && substr($_, $count, 1) eq ":") {
        # leading ':' within stmt/expr/decl - this cannot happen for labels, leading '&&', or leading '||'
        # allow special indent at level of corresponding "?"
        ($alt_desc, $alt_indent) = ("leading ':'", @nested_conds_indents[-1]);
    }
    # allow extra indent offset leading '&&' or '||' - this cannot happen for leading ":"
    ($alt_desc, $alt_indent) = ("leading '$1'", $ref_indent + INDENT_LEVEL) if $contents =~ m/^[\s@]*(\&\&|\|\|)/;

    if ($expr_indent < 0) { # implies @nested_symbols != 0 && @nested_symbols[0] eq "{" && @nested_indents[-1] < 0
        # allow normal stmt indentation level for hanging initializer/enum expressions after trailing '{'
        # this cannot happen for labels and overrides special treatment of ':', '&&' and '||' for this line
        ($alt_desc, $alt_indent) = ("lines after '{'", $stmt_indent);
        # decide depending on current actual indentation, preventing forth and back
        @nested_indents[-1] = $count == $stmt_indent ? $stmt_indent : -@nested_indents[-1]; # allow $stmt_indent
        $ref_indent = $expr_indent = @nested_indents[-1];
    }

    # check consistency of indentation within multi-line comment (i.e., between its first, inner, and last lines)
    if ($in_comment != 0 && $in_comment != 1) { # in multi-line comment but not on its first line
        if (!$sloppy_cmt) {
            if ($in_comment > 0) { # not at its end
                report("indent = $count != $comment_indent within multi-line comment")
                    if $count != $comment_indent;
            } else {
                my $tweak = $in_comment == -2 ? 1 : 0;
                report("indent = ".($count + $tweak)." != $comment_indent at end of multi-line comment")
                    if $count + $tweak != $comment_indent;
            }
        }
        # do not check indentation of last line of non-leading multi-line comment
        if ($in_comment < 0 && !$leading_comment) {
            s/^(\s*)@/$1*/; # blind first '@' as '*' to prevent below delayed check for the line before
            return;
        }
        return if $in_comment > 0; # not on its last line
        # $comment_indent will be checked by the below checks for end of multi-line comment
    }

    # else check indentation of entire-line comment or entire-line end of multi-line comment
    # ... w.r.t. indent of the following line by delayed check for the line before
    if (($in_comment == 0 || $in_comment == 1) # no comment, intra-line comment, or begin of multi-line comment
        && $line_before > 0 # there is a line before
        && $contents_before_ =~ m/^(\s*)@[\s@]*$/) { # line before begins with '@', no code follows (except '\')
        report_flexibly($line_before, "entire-line comment indent = $count_before != $count (of following line)",
            $contents_before) if !$sloppy_cmt && $count_before != $count;
    }
    # ... but allow normal indentation for the current line, else above check will be done for the line before
    if (($in_comment == 0 || $in_comment < 0) # (no comment,) intra-line comment or end of multi-line comment
        && m/^(\s*)@[\s@]*$/) { # line begins with '@', no code follows (except '\')
        if ($count == $ref_indent) { # indentation is like for (normal) code in this line
            s/^(\s*)@/$1*/; # blind first '@' as '*' to prevent above delayed check for the line before
            return;
        }
        return if !eof; # defer check of entire-line comment to next line
    }

    # else check indentation of leading intra-line comment or end of multi-line comment
    if (m/^(\s*)@/) { # line begins with '@', i.e., any (remaining type of) comment
        if (!$sloppy_cmt && $count != $ref_indent) {
            report("intra-line comment indent = $count != $ref_indent") if $in_comment == 0;
            report("multi-line comment indent = $count != $ref_indent") if $in_comment < 0;
        }
        return;
    }

    if ($sloppy_hang && ($hanging_offset != 0 || $expr_indent != 0)) {
        # do not report same indentation as on the line before (potentially due to same violations)
        return if $line_before > 0 && $count == $count_before;

        # do not report indentation at normal indentation level while hanging expression indent would be required
        return if $expr_indent != 0 && $count == $stmt_indent;

        # do not report if contents have been shifted left of nested expr indent (but not as far as stmt indent)
        # apparently aligned to the right in order to fit within line length limit
        return if $stmt_indent < $count && $count < $expr_indent &&
            length($contents) == MAX_LINE_LENGTH + length("\n");
    }

    report("indent = $count != $ref_indent for $ref_desc".
           ($alt_desc eq ""
            || $alt_indent == $ref_indent # prevent showing alternative that happens to have equal value
            ? "" : " or $alt_indent for $alt_desc"))
        if $count != $ref_indent && $count != $alt_indent;
}

# submodules handling indentation within expressions @@@@@@@@@@@@@@@@@@@@@@@@@@@

sub update_nested_indents { # may reset $in_paren_expr and in this case also resets $in_expr
    my $str = shift;
    my $start = shift; # defaults to 0
    my $terminator_position = -1;
    for (my $i = $start; $i < length($str); $i++) {
        my $c;
        my $curr = substr($str, $i);
        if ($curr =~ m/^(.*?)([{}()?:;\[\]])(.*)$/) { # match from position $i the first {}()?:;[]
            $c = $2;
        } else {
            last;
        }
        my ($head, $tail) = (substr($str, 0, $i).$1, $3);
        $i += length($1) + length($2) - 1;

        # stop at terminator outside 'for(..;..;..)', assuming that 'for' is followed by '('
        return $i if $c eq ";" && (!$in_paren_expr || @nested_indents == 0);

        my $in_stmt = $in_expr || @nested_symbols != 0; # not: || $in_typedecl != 0
        if ($c =~ m/[{([?]/) { # $c is '{', '(', '[', or '?'
            if ($c eq "{") { # '{' in any context
                $in_block_decls = 0 if !$in_expr && $in_typedecl == 0;
                # cancel newly hanging_offset if opening brace '{' is after non-whitespace non-comment:
                $hanging_offset -= INDENT_LEVEL if $hanging_offset > 0 && $head =~ m/[^\s\@]/;
                push @nested_block_indents, $block_indent;
                push @nested_hanging_offsets, $in_expr ? $hanging_offset : 0;
                push @nested_in_typedecl, $in_typedecl if $in_typedecl != 0;
                $block_indent += INDENT_LEVEL + $hanging_offset;
                $hanging_offset = 0;
            }
            if ($c ne "{" || $in_stmt) { # for '{' inside stmt/expr (not: decl), for '(', '[', or '?' anywhere
                $tail =~ m/^([\s@]*)([^\s\@])/;
                push @nested_indents, defined $2
                    ? $i + 1 + length($1) # actual indentation of following non-space non-comment
                    : $c ne "{" ? +($i + 1)  # just after '(' or '[' if only whitespace thereafter
                                : -($i + 1); # allow also $stmt_indent if '{' with only whitespace thereafter
                push @nested_symbols, $c; # done also for '?' to be able to check correct nesting
                push @nested_conds_indents, $i if $c eq "?"; # remember special alternative indent for ':'
            }
        } elsif ($c =~ m/[})\]:]/) { # $c is '}', ')', ']', or ':'
            my $opening_c = ($c =~ tr/})]:/{([/r);
            if (($c ne ":" || $in_stmt    # ignore ':' outside stmt/expr/decl
                # in the presence of ':', one could add this sanity check:
                # && !(# ':' after initial label/case/default
                #      $head =~ m/^([\s@]*)(case\W.*$|\w+$)/ || # this matching would not work for
                #                                               # multi-line expr after 'case'
                #      # bitfield length within unsigned type decl
                #      $tail =~ m/^[\s@]*\d+/                   # this matching would need improvement
                #     )
                )) {
                if ($c ne "}" || $in_stmt) { # for '}' inside stmt/expr/decl, ')', ']', or ':'
                    if (@nested_symbols != 0 &&
                        @nested_symbols[-1] == $opening_c) { # for $c there was a corresponding $opening_c
                        pop @nested_indents;
                        pop @nested_symbols;
                        pop @nested_conds_indents if $opening_c eq "?";
                    } else {
                        report("unexpected '$c' @ ".($in_paren_expr ? "(expr)" : "expr"));
                        next;
                    }
                }
                if ($c eq "}") { # '}' at block level but also inside stmt/expr/decl
                    if (@nested_block_indents == 0) {
                        report("unexpected '}'");
                    } else {
                        $block_indent = pop @nested_block_indents;
                        $hanging_offset = pop @nested_hanging_offsets;
                        $in_typedecl = pop @nested_in_typedecl if @nested_in_typedecl != 0;
                    }
                }
                if ($in_paren_expr && !grep(/\(/, @nested_symbols)) { # end of (expr)
                    check_nested_nonblock_indents("(expr)");
                    $in_paren_expr = $in_expr = 0;
                    report("code after (expr)")
                        if $tail =~ m/^([^{]*)/ && $1 =~ m/[^\s\@;]/; # non-space non-';' before any '{'
                }
            }
        }
    }
    return -1;
}

sub check_nested_nonblock_indents {
    my $position = shift;
    while (@nested_symbols != 0) {
        my $symbol = pop @nested_symbols;
        report("unclosed '$symbol' in $position");
        if ($symbol eq "{") { # repair stack of blocks
            $block_indent = pop @nested_block_indents;
            $hanging_offset = pop @nested_hanging_offsets;
            $in_typedecl = pop @nested_in_typedecl if @nested_in_typedecl != 0;
        }
    }
    @nested_indents = ();
    @nested_conds_indents = ();
}

# start of main program @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

reset_file_state();

while (<>) { # loop over all lines of all input files
    $self_test = $ARGV =~ m/check-format-test/;
    $_ = "" if $self_test && m/ blank line within local decls /;
    $line++;
    s/\r$//; # strip any trailing CR '\r' (which are typical on Windows systems)
    $contents = $_;

    # check for illegal characters
    if (m/(.*?)([\x00-\x09\x0B-\x1F\x7F-\xFF])/) {
        my $col = length($1);
        report(($2 eq "\x09" ? "TAB" : $2 eq "\x0D" ? "CR " : $2 =~ m/[\x00-\x1F]/ ? "non-printable"
                : "non-7bit char") . " at column $col") ;
    }

    # check for whitespace at EOL
    report("trailing whitespace at EOL") if m/\s\n$/;

    # assign to $count the actual indentation level of the current line
    chomp; # remove trailing NL '\n'
    m/^(\s*)/;
    $count = length($1); # actual indentation
    $has_label = 0;
    $local_offset = 0;

    # character/string literals @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    s/\\["']/@@/g; # blind all '"' and "'" escaped by '\' (typically within character literals or string literals)

    # handle multi-line string literals to avoid confusion on starting/ending '"' and trailing '\'
    if ($in_multiline_string) {
        if (s#^([^"]*)"#($1 =~ tr/"/@/cr).'@'#e) { # string literal terminated by '"'
            # string contents and its terminating '"' have been blinded as '@'
            $count = -1; # do not check indentation
        } else {
            report("multi-line string literal not terminated by '\"' and trailing '\' is missing")
                unless s#^([^\\]*)\s*\\\s*$#$1#; # strip trailing '\' plus any whitespace around
            goto LINE_FINISHED;
        }
    }

    # blind contents of character and string literals as @, preserving length (but not spaces)
    # this prevents confusing any of the matching below, e.g., of whitespace and comment delimiters
    s#('[^']*')#$1 =~ tr/'/@/cr#eg; # handle all intra-line character literals
    s#("[^"]*")#$1 =~ tr/"/@/cr#eg; # handle all intra-line string literals
    $in_multiline_string =          # handle trailing string literal terminated by '\'
        s#^(([^"]*"[^"]*")*[^"]*)("[^"]*)\\(\s*)$#$1.($3 =~ tr/"/@/cr).'"'.$4#e;
        # its contents have been blinded and the trailing '\' replaced by '"'

    # strip any other trailing '\' along with any whitespace around it such that it does not interfere with various
    # matching below; the later handling of multi-line macro definitions uses $contents where it is not stripped
    s#^(.*?)\s*\\\s*$#$1#; # trailing '\' possibly preceded and/or followed by whitespace

    # comments @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    # do/prepare checks within multi-line comments
    my $self_test_exception = $self_test ? "@" : "";
    if ($in_comment > 0) { # this still includes the last line of multi-line comment
        my ($head, $any_symbol, $cmt_text) = m/^(\s*)(.?)(.*)$/;
        if ($any_symbol eq "*") {
            report("missing space or '*' after leading '*' in multi-line comment") if $cmt_text =~ m|^[^*\s/$self_test_exception]|;
        } else {
            report("missing leading '*' in multi-line comment");
        }
        $in_comment++;
    }

    # detect end of comment, must be within multi-line comment, check if it is preceded by non-whitespace text
    if ((my ($head, $tail) = m|^(.*?)\*/(.*)$|) && $1 ne '/') { # ending comment: '*/'
        report("neither space nor '*' before '*/'") if $head =~ m/[^*\s]$/;
        report("missing space after '*/'") if $tail =~ m/^[^\s,;)}\]]/; # no space or ,;)}] after '*/'
        if (!($head =~ m|/\*|)) { # not begin of comment '/*', which is is handled below
            if ($in_comment == 0) {
                report("unexpected '*/' outside comment");
                $_ = "$head@@".$tail; # blind the "*/"
            } else {
                report("text before '*/' in multi-line comment") if ($head =~ m/[^*\s]/); # non-SPC before '*/'
                $in_comment = -1; # indicate that multi-line comment ends on current line
                if ($count > 0) {
                    # make indentation of end of multi-line comment appear like of leading intra-line comment
                    $head =~ s/^(\s*)\s/$1@/; # replace the last leading space by '@'
                    $count--;
                    $in_comment = -2; # indicate that multi-line comment ends on current line, with tweak
                }
                my $cmt_text = $head;
                $_ = blind_nonspace($cmt_text)."@@".$tail;
            }
        }
    }

    # detect begin of comment, check if it is followed by non-space text
  MATCH_COMMENT:
    if (my ($head, $opt_minus, $tail) = m|^(.*?)/\*(-?)(.*)$|) { # begin of comment: '/*'
        report("missing space before '/*'")
            if $head =~ m/[^\s(\*]$/; # not space, '(', or or '*' (needed to allow '*/') before comment delimiter
        report("missing space, '*' or '!' after '/*' or '/*-'") if $tail =~ m/^[^*\s!$self_test_exception]/;
        my $cmt_text = $opt_minus.$tail; # preliminary
        if ($in_comment > 0) {
            report("unexpected '/*' inside multi-line comment");
        } elsif ($tail =~ m|^(.*?)\*/(.*)$|) { # comment end: */ on same line
            report("unexpected '/*' inside intra-line comment") if $1 =~ /\/\*/;
            # blind comment text, preserving length and spaces
            ($cmt_text, my $rest) = ($opt_minus.$1, $2);
            $_ = "$head@@".blind_nonspace($cmt_text)."@@".$rest;
            goto MATCH_COMMENT;
        } else { # begin of multi-line comment
            my $self_test_exception = $self_test ? "(@\d?)?" : "";
            report("text after '/*' in multi-line comment")
                unless $tail =~ m/^$self_test_exception.?[*\s]*$/;
            # tail not essentially blank, first char already checked
            # adapt to actual indentation of first line
            $comment_indent = length($head) + 1;
            $_ = "$head@@".blind_nonspace($cmt_text);
            $in_comment = 1;
            $leading_comment = $head =~ m/^\s*$/; # there is code before beginning delimiter
            $formatted_comment = $opt_minus eq "-";
        }
    } elsif (($head, $tail) = m|^\{-(.*)$|) { # begin of Perl pragma: '{-'
    }

    if ($in_comment > 1) { # still inside multi-line comment (not at its begin or end)
        m/^(\s*)\*?(\s*)(.*)$/;
        $_ = $1."@".$2.blind_nonspace($3);
    }

    # handle special case of line after '#ifdef __cplusplus' (which typically appears in header files)
    if ($ifdef__cplusplus) {
        $ifdef__cplusplus = 0;
        $_ = "$1 $2" if $contents =~ m/^(\s*extern\s*"C"\s*)\{(\s*)$/; # ignore opening brace in 'extern "C" {'
        goto LINE_FINISHED if m/^\s*\}\s*$/; # ignore closing brace '}'
    }

    # check for over-long lines,
    # while allowing trailing (also multi-line) string literals to go past $max_length
    my $len = length; # total line length (without trailing '\n')
    if ($len > $max_length &&
        !(m/^(.*)"[^"]*"\s*[\)\}\]]*[,;]?\s*$/ # string literal terminated by '"' (or '\'), then maybe )}],;
          && length($1) < $max_length)
        # this allows over-long trailing string literals with beginning col before $max_length
        ) {
        report("line length = $len > ".MAX_LINE_LENGTH);
    }

    # handle C++ / C99 - style end-of-line comments
    if (my ($head, $cmt_text) = m|^(.*?)//(.*$)|) {
        report("'//' end-of-line comment");  # the '//' comment style is not allowed for C90
        # blind comment text, preserving length and spaces
        $_ = "$head@@".blind_nonspace($cmt_text);
    }

    # at this point all non-space portions of any types of comments have been blinded as @

    goto LINE_FINISHED if m/^\s*$/; # essentially blank line: just whitespace (and maybe a trailing '\')

    # intra-line whitespace nits @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    my $in_multiline_comment = ($in_comment > 1 || $in_comment < 0); # $in_multiline_comment refers to line before
    if (!$sloppy_SPC && !($in_multiline_comment && $formatted_comment)) {
        sub extra_SPC {
            my $intra_line = shift;
            return "extra space".($intra_line =~ m/@\s\s/ ?
                                  $in_comment != 0 ? " in multi-line comment"
                                                   : " in intra-line comment" : "");
        }
        sub split_line_head { # split line contents into header containing leading spaces and the first non-space char, and the rest of the line
            my $comment_symbol =
                $in_comment != 0 ? "@" : ""; # '@' will match the blinded leading '*' in multi-line comment
                                             # $in_comment may pertain to the following line due to delayed check
            # do not check for extra SPC in leading spaces including any '#' (or '*' within multi-line comment)
            shift =~ m/^(\s*([#$comment_symbol]\s*)?)(.*?)\s*$/;
            return ($1, $3);
        }
        my ($head , $intra_line ) = split_line_head($_);
        my ($head1, $intra_line1) = split_line_head($contents_before_ ) if $line_before > 0;
        my ($head2, $intra_line2) = split_line_head($contents_before_2) if $line_before2 > 0;
        if ($line_before > 0) { # check with one line delay, such that at least $contents_before is available
            sub column_alignments_only { # return 1 if the given line has multiple consecutive spaces only at columns that match the reference line
                # all parameter strings are assumed to contain contents after blinding comments etc.
                my $head = shift;     # leading spaces and the first non-space char
                my $intra = shift;    # the rest of the line contents
                my $contents = shift; # reference line
                # check if all extra SPC in $intra is used only for multi-line column alignment with $contents
                my $offset = length($head);
                for (my $col = 0; $col < length($intra) - 2; $col++) {
                    my $substr = substr($intra, $col);
                    next unless $substr =~ m/^\s\s\S/; # extra SPC (but not in leading spaces of the line)
                    next if !$eol_cmt && $substr =~ m/^[@\s]+$/; # end-of-line comment
                    return 0 unless substr($contents, $col + $offset + 1, 2) =~ m/\s\S/; # reference line contents do not match
                }
                return 1;
            }
            report_flexibly($line_before, extra_SPC($intra_line1), $contents_before) if $intra_line1 =~ m/\s\s\S/ &&
               !(    column_alignments_only($head1, $intra_line1, $_                )    # compare with $line
                 || ($line_before2 > 0 &&
                     column_alignments_only($head1, $intra_line1, $contents_before_2))); # compare w/ $line_before2
            report(extra_SPC($intra_line)) if $intra_line  =~ m/\s\s\S/ && eof
                && ! column_alignments_only($head , $intra_line , $contents_before_ )  ; # compare w/ $line_before
        } elsif (eof) { # special case: just one line exists
            report(extra_SPC($intra_line)) if $intra_line  =~ m/\s\s\S/;
        }
        # ignore paths in #include
        $intra_line =~ s/^(include\s*)(".*?"|<.*?>)/$1/e if $head =~ m/#/;
        # treat op= and comparison operators as simple '=', simplifying matching below
        $intra_line =~ s/([\+\-\*\/\/%\&\|\^\!<>=]|<<|>>)=/=/g;
        # treat (type) variables within macro, indicated by trailing '\', as 'int' simplifying matching below
        $intra_line =~ s/[A-Z_]+/int/g if $contents =~ m/^(.*?)\s*\\\s*$/;
        # treat double &&, ||, <<, and >> as single ones, simplifying matching below
        $intra_line =~ s/(&&|\|\||<<|>>)/substr($1, 0, 1)/eg;
        # remove blinded comments etc. directly after [{(
        while ($intra_line =~ s/([\[\{\(])@+\s?/$1/e) {} # /g does not work here
        # remove blinded comments etc. directly before ,;)}]
        while ($intra_line =~ s/\s?@+([,;\)\}\]])/$1/e) {} # /g does not work here
        # treat remaining blinded comments and string literal contents as (single) space during matching below
        $intra_line =~ s/@+/ /g;                     # note that extra SPC has already been handled above
        $intra_line =~ s/\s+$//;                     # strip any (resulting) space at EOL
        $intra_line =~ s/(for\s*\([^;]*);;(\))/"$1$2"/eg; # strip trailing ';;' in for (;;)
        $intra_line =~ s/(for\s*\([^;]+;[^;]+);(\))/"$1$2"/eg; # strip trailing ';' in for (;;)
        $intra_line =~ s/(for\s*\();(;)/"$1$2"/eg;   # replace leading ';;' in for (;;) by ';'
        $intra_line =~ s/(=\s*)\{ /"$1@ "/eg;        # do not report {SPC in initializers such as ' = { 0, };'
        $intra_line =~ s/, \};/, @;/g;               # do not report SPC} in initializers such as ' = { 0, };'
        report("space before '$1'") if $intra_line =~ m/[\w)\]]\s+(\+\+|--)/;  # postfix ++/-- with preceding space
        report("space after '$1'")  if $intra_line =~ m/(\+\+|--)\s+[a-zA-Z_(]/; # prefix ++/-- with following space
        $intra_line =~ s/\.\.\./@/g;                 # blind '...'
        report("space before '$1'") if $intra_line =~ m/\s(\.|->)/;            # '.' or '->' with preceding space
        report("space after '$1'")  if $intra_line =~ m/(\.|->)\s/;            # '.' or '->' with following space
        $intra_line =~ s/\-\>|\+\+|\-\-/@/g;         # blind '->,', '++', and '--'
        report("space before '$1'")     if $intra_line =~ m/[^:)]\s+(;)/;      # space before ';' but not after ':' or ')'
        report("space before '$1'")     if $intra_line =~ m/\s([,)\]])/;       # space before ,)]
        report("space after '$1'")      if $intra_line =~ m/([(\[~!])\s/;      # space after ([~!
        report("space after '$1'")      if $intra_line =~ m/(defined)\s/;      # space after 'defined'
        report("missing space before '=' or '<op>='") if $intra_line =~ m/\S(=)/;   # '=' etc. without preceding space
        report("missing space before '$1'")  if $intra_line =~ m/\S([|\/%<>^\?])/;  # |/%<>^? without preceding space
        # TODO ternary ':' without preceding SPC, while allowing no SPC before ':' after 'case'
        report("missing space before binary '$2'")  if $intra_line =~ m/([^\s{()\[e])([+\-])/; # '+'/'-' without preceding space or {()[e
        # ')' may be used for type casts or before "->", 'e' may be used for numerical literals such as "1e-6"
        report("missing space before binary '$1'")  if $intra_line =~ m/[^\s{()\[*!]([*])/; # '*' without preceding space or {()[*!
        report("missing space before binary '$1'")  if $intra_line =~ m/[^\s{()\[]([&])/;  # '&' without preceding space or {()[
        report("missing space after ternary '$1'") if $intra_line =~ m/(:)[^\s\d]/; # ':' without following space or digit
        report("missing space after '$1'")   if $intra_line =~ m/([,;=|\/%<>^\?])\S/; # ,;=|/%<>^? without following space
        report("missing space after binary '$1'") if $intra_line=~m/[^{(\[]([*])[^\sa-zA-Z_(),*]/;# '*' w/o space or \w(),* after
        # TODO unary '*' must not be followed by SPC
        report("missing space after binary '$1'") if $intra_line=~m/([&])[^\sa-zA-Z_(]/;  # '&' w/o following space or \w(
        # TODO unary '&' must not be followed by SPC
        report("missing space after binary '$1'") if $intra_line=~m/[^{(\[]([+\-])[^\s\d(]/;  # +/- w/o following space or \d(
        # TODO unary '+' and '-' must not be followed by SPC
        report("missing space after '$2'")   if $intra_line =~ m/(^|\W)(if|while|for|switch|case)[^\w\s]/; # kw w/o SPC
        report("missing space after '$2'")   if $intra_line =~ m/(^|\W)(return)[^\w\s;]/;  # return w/o SPC or ';'
        report("space after function/macro name")
                                      if $intra_line =~ m/(\w+)\s+\(/        # fn/macro name with space before '('
       && !($1 =~ m/^(sizeof|if|else|while|do|for|switch|case|default|break|continue|goto|return|void|char|signed|unsigned|int|short|long|float|double|typedef|enum|struct|union|auto|extern|static|const|volatile|register)$/) # not keyword
                                    && !(m/^\s*#\s*define\s/); # we skip macro definitions here because macros
                                    # without parameters but with body beginning with '(', e.g., '#define X (1)',
                                    # would lead to false positives - TODO also check for macros with parameters
        report("missing space before '{'")   if $intra_line =~ m/[^\s{(\[]\{/;      # '{' without preceding space or {([
        report("missing space after '}'")    if $intra_line =~ m/\}[^\s,;\])}]/;    # '}' without following space or ,;])}
    }

    # preprocessor directives @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    # handle preprocessor directives
    if (m/^\s*#(\s*)(\w+)/) { # line beginning with '#'
        my $space_count = length($1); # maybe could also use indentation before '#'
        my $directive = $2;
        report("indent = $count != 0 for '#'") if $count != 0;
        $directive_nesting-- if $directive =~ m/^(else|elif|endif)$/;
        if ($directive_nesting < 0) {
            $directive_nesting = 0;
            report("unexpected '#$directive'");
        }
        report("'#' directive nesting = $space_count != $directive_nesting") if $space_count != $directive_nesting;
        $directive_nesting++ if $directive =~ m/^if|ifdef|ifndef|else|elif$/;
        $ifdef__cplusplus = m/^\s*#\s*ifdef\s+__cplusplus\s*$/;
        goto POSTPROCESS_DIRECTIVE unless $directive =~ m/^define$/; # skip normal code handling except for #define
        # TODO improve handling of indents of preprocessor directives ('\', $in_directive != 0) vs. normal C code
        $count = -1; # do not check indentation of #define
    }

    # adapt required indentation @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    s/(\w*ASN1_[A-Z_]+END\w*([^(]|\(.*?\)|$))/$1;/g; # treat *ASN1_*END*(..) macro calls as if followed by ';'

    my $nested_indents_position = 0;

    # update indents according to leading closing brace(s) '}' or label or switch case
    my $in_stmt = $in_expr || @nested_symbols != 0 || $in_typedecl != 0;
    if ($in_stmt) { # expr/stmt/type decl/var def/fn hdr, i.e., not at block level
        if (m/^([\s@]*\})/) { # leading '}' within stmt, any preceding blinded comment must not be matched
            $in_block_decls = -1;
            my $head = $1;
            update_nested_indents($head);
            $nested_indents_position = length($head);
            if (@nested_symbols >= 1) {
                $hanging_symbol = @nested_symbols[-1];
                $expr_indent = @nested_indents[-1];
            } else { # typically end of initialiizer expr or enum
                $expr_indent = 0;
            }
        } elsif (m/^([\s@]*)(static_)?ASN1_ITEM_TEMPLATE_END(\W|$)/) { # workaround for ASN1 macro indented as '}'
            $local_offset = -INDENT_LEVEL;
            $expr_indent = 0;
        } elsif (m/;.*?\}/) { # expr ends with ';' before '}'
            report("code before '}'");
        }
    }
    if (@in_do_hanging_offsets != 0 && # note there is nothing like "unexpected 'while'"
        m/^[\s@]*while(\W|$)/) { # leading 'while'
        $hanging_offset = pop @in_do_hanging_offsets;
    }
    if ($if_maybe_terminated) {
        if (m/(^|\W)else(\W|$)/) { # (not necessarily leading) 'else'
            if (@in_if_hanging_offsets == 0) {
                report("unexpected 'else'");
            } else {
                $hanging_offset = pop @in_if_hanging_offsets;
            }
        } else {
            @in_if_hanging_offsets = (); # note there is nothing like "unclosed 'if'"
            $hanging_offset = 0;
        }
    }
    if (!$in_stmt) { # at block level, i.e., outside expr/stmt/type decl/var def/fn hdr
        $if_maybe_terminated = 0;
        if (my ($head, $before, $tail) = m/^([\s@]*([^{}]*)\})[\s@]*(.*)$/) { # leading closing '}', but possibly
                                                                              # with non-whitespace non-'{' before
            report("code after '}'") unless $tail eq "" || $tail =~ m/(else|while|OSSL_TRACE_END)(\W|$)/;
            my $outermost_level = @nested_block_indents == 1 && @nested_block_indents[0] == 0;
            if (!$sloppy_bodylen && $outermost_level && $line_body_start != 0) {
                my $body_len = $line - $line_body_start - 1;
                report_flexibly($line_function_start, "function body length = $body_len > ".MAX_BODY_LENGTH." lines",
                    $last_function_header) if $body_len > MAX_BODY_LENGTH;
                $line_body_start = 0;
            }
            if ($before ne "") { # non-whitespace non-'{' before '}'
                report("code before '}'");
            } else { # leading '}' outside stmt, any preceding blinded comment must not be matched
                $in_block_decls = -1;
                $local_offset = $block_indent + $hanging_offset - INDENT_LEVEL;
                update_nested_indents($head);
                $nested_indents_position = length($head);
                $local_offset -= ($block_indent + $hanging_offset);
                # in effect $local_offset = -INDENT_LEVEL relative to $block_indent + $hanging_offset values before
            }
        }

        # handle opening brace '{' after if/else/while/for/switch/do on line before
        if ($hanging_offset > 0 && m/^[\s@]*{/ && # leading opening '{'
            $line_before > 0 &&
            $contents_before_ =~ m/(^|^.*\W)(if|else|while|for|switch|do)(\W.*$|$)/) {
            $keyword_opening_brace = $1;
            $hanging_offset -= INDENT_LEVEL; # cancel newly hanging_offset
        }

        if (m/^[\s@]*(case|default)(\W.*$|$)/) { # leading 'case' or 'default'
            my $keyword = $1;
            report("code after $keyword: ") if $2 =~ /:.*[^\s@].*$/;
            $local_offset = -INDENT_LEVEL;
        } else {
            if (m/^([\s@]*)(\w+):/) { # (leading) label, cannot be "default"
                $local_offset = -INDENT_LEVEL;
                $has_label = 1;
            }
        }
    }

    # potential adaptations of indent in first line of macro body in multi-line macro definition
    if ($in_directive > 0 && $in_macro_header > 0) {
        if ($in_macro_header > 1) { # still in macro definition header
            $in_macro_header += parens_balance($_);
        } else { # begin of macro body
            $in_macro_header = 0;
            if ($count == $block_indent - $directive_offset # body began with same indentation as preceding code
                && $sloppy_macro) { # workaround for this situation is enabled
                $block_indent -= $directive_offset;
                $directive_offset = 0;
            }
        }
    }

    # check required indentation @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    check_indent() if $count >= 0; # not for #define and not if multi-line string literal is continued

    # check for blank lines within/after local decls @@@@@@@@@@@@@@@@@@@@@@@@@@@

    if ($in_block_decls >= 0 &&
        $in_comment == 0 && !m/^\s*\*?@/ && # not in multi-line comment nor an intra-line comment
        !$in_expr && $expr_indent == 0 && $in_typedecl == 0) {
        my $blank_line_before = $line > 1
            && $code_contents_before =~ m/^\s*(\\\s*)?$/; # essentially blank line: just whitespace (and maybe a trailing '\')
        if (m/^[\s(]*(char|signed|unsigned|int|short|long|float|double|enum|struct|union|auto|extern|static|const|volatile|register)(\W|$)/ # clear start of local decl
            || (m/^(\s*(\w+|\[\]|[\*()]))+?\s+[\*\(]*\w+(\s*(\)|\[[^\]]*\]))*\s*[;,=]/ # weak check for decl involving user-defined type
                && !m/^\s*(\}|sizeof|if|else|while|do|for|switch|case|default|break|continue|goto|return)(\W|$)/)) {
            $in_block_decls++;
            report_flexibly($line - 1, "blank line within local decls, before", $contents) if $blank_line_before;
        } else {
            report_flexibly($line, "missing blank line after local decls", "\n$contents_before$contents")
                if $in_block_decls > 0 && !$blank_line_before;
            $in_block_decls = -1 unless
                m/^\s*(\\\s*)?$/ # essentially blank line: just whitespace (and maybe a trailing '\')
            || $in_comment != 0 || m/^\s*\*?@/; # in multi-line comment or an intra-line comment
        }
    }

    $in_comment = 0 if $in_comment < 0; # multi-line comment has ended

    # do some further checks @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    my $outermost_level = $block_indent == 0 + ($in_directive > 0 ? $directive_offset : 0);

    report("more than one stmt") if !m/(^|\W)for(\W.*|$)/ && # no 'for' - TODO improve matching
        m/;.*;/; # two or more terminators ';', so more than one statement

    # check for code block containing a single line/statement
    if ($line_before2 > 0 && !$outermost_level && # within function body
        $in_typedecl == 0 && @nested_indents == 0 && # neither within type declaration nor inside stmt/expr
        m/^[\s@]*\}/) { # leading closing brace '}', any preceding blinded comment must not be matched
        # TODO extend detection from single-line to potentially multi-line statement
        if ($line_opening_brace > 0 &&
            ($line_opening_brace == $line_before2 ||
             $line_opening_brace == $line_before)
            && $contents_before =~ m/;/) { # there is at least one terminator ';', so there is some stmt
            # TODO do not report cases where a further else branch
            # follows with a block containing more than one line/statement
            report_flexibly($line_before, "'$keyword_opening_brace' { 1 stmt }", $contents_before);
        }
    }

    report("single-letter name '$2'") if (m/(^|.*\W)([IO])(\W.*|$)/); # single-letter name 'I' or 'O' # maybe re-add 'l'?
    # constant on LHS of comparison or assignment, e.g., NULL != x or 'a' < c, but not a + 1 == b
    report("constant on LHS of '$3'")
        if (m/(['"]|([\+\-\*\/\/%\&\|\^<>]\s*)?\W[0-9]+L?|\WNULL)\s*([\!<>=]=|[<=>])([<>]?)/ &&
            $2 eq "" && (($3 ne "<" && $3 ne "='" && $3 ne ">") || $4 eq ""));

    # TODO report #if 0 and #if 1

    # TODO report needless use of parentheses, while
    #      macro parameters should always be in parens (except when passed on), e.g., '#define ID(x) (x)'

    # adapt required indentation for following lines @@@@@@@@@@@@@@@@@@@@@@@@@@@

    # set $in_expr, $in_paren_expr, and $hanging_offset for if/while/for/switch, return/enum, and assignment RHS
    my $paren_expr_start = 0;
    my $return_enum_start = 0;
    my $assignment_start = 0;
    my $tmp = $_;
    $tmp =~ s/[\!<>=]=/@@/g; # blind (in-)equality symbols like '<=' as '@@' to prevent matching them as '=' below
    if      (m/^((^|.*\W)(if|while|for|switch))(\W.*|$)$/) { # (last) if/for/while/switch
        $paren_expr_start = 1;
    } elsif (m/^((^|.*\W)(return|enum))(\W.*|$)/             # (last) return/enum
        && !$in_expr && @nested_indents == 0 && parens_balance($1) == 0) { # not nested enum
        $return_enum_start = 1;
    } elsif ($tmp =~ m/^(([^=]*)(=))(.*)$/                   # (last) '=', i.e., assignment
        && !$in_expr && @nested_indents == 0 && parens_balance($1) == 0) { # not nested assignment
        $assignment_start = 1;
    }
    if ($paren_expr_start || $return_enum_start || $assignment_start)
    {
        my ($head, $mid, $tail) = ($1, $3, $4);
        $keyword_opening_brace = $mid if $mid ne "=";
        # to cope with multi-line expressions, do this also if !($tail =~ m/\{/)
        push @in_if_hanging_offsets, $hanging_offset if $mid eq "if";

        # already handle $head, i.e., anything before expression
        update_nested_indents($head, $nested_indents_position);
        $nested_indents_position = length($head);
        # now can set $in_expr and $in_paren_expr
        $in_expr = 1;
        $in_paren_expr = 1 if $paren_expr_start;
        if ($mid eq "while" && @in_do_hanging_offsets != 0) {
            $hanging_offset = pop @in_do_hanging_offsets;
        } else {
            $hanging_offset += INDENT_LEVEL; # tentatively set hanging_offset, may be canceled by following '{'
        }
    }

    # set $hanging_offset and $keyword_opening_brace for do/else
    if (my ($head, $mid, $tail) = m/(^|^.*\W)(else|do)(\W.*|$)$/) { # last else/do, where 'do' is preferred
        my $code_before = $head =~ m/[^\s\@}]/; # leading non-whitespace non-comment non-'}'
        report("code before '$mid'") if $code_before;
        report("code after '$mid'" ) if $tail =~ m/[^\s\@{]/# trailing non-whitespace non-comment non-'{' (non-'\')
                                                    && !($mid eq "else" && $tail =~ m/[\s@]*if(\W|$)/);
        if ($mid eq "do") { # workarounds for code before 'do'
            if ($head =~ m/(^|^.*\W)(else)(\W.*$|$)/) { # 'else' ... 'do'
                $hanging_offset += INDENT_LEVEL; # tentatively set hanging_offset, may be canceled by following '{'
            }
            if ($head =~ m/;/) { # terminator ';' ... 'do'
                @in_if_hanging_offsets = (); # note there is nothing like "unclosed 'if'"
                $hanging_offset = 0;
            }
        }
        push @in_do_hanging_offsets, $hanging_offset if $mid eq "do";
        if ($code_before && $mid eq "do") {
            $hanging_offset = length($head) - $block_indent;
        }
        if (!$in_paren_expr) {
            $keyword_opening_brace = $mid if $tail =~ m/\{/;
            $hanging_offset += INDENT_LEVEL;
        }
    }

    # set $in_typedecl and potentially $hanging_offset for type declaration
    if (!$in_expr && @nested_indents == 0 # not in expression
        && m/(^|^.*\W)(typedef|enum|struct|union)(\W.*|$)$/
        && parens_balance($1) == 0 # not in newly started expression or function arg list
        && ($2 eq "typedef" || !($3 =~ m/\s*\w++\s*(.)/ && $1 ne "{")) # 'struct'/'union'/'enum' <name> not followed by '{'
        # not needed: && $keyword_opening_brace = $2 if $3 =~ m/\{/;
        ) {
        $in_typedecl++;
        $hanging_offset += INDENT_LEVEL if m/\*.*\(/; # '*' followed by '(' - seems consistent with Emacs C mode
    }

    my $bak_in_expr = $in_expr;
    my $terminator_position = update_nested_indents($_, $nested_indents_position);

    if ($bak_in_expr) {
        # on end of non-if/while/for/switch (multi-line) expression (i.e., return/enum/assignment) and
        # on end of statement/type declaration/variable definition/function header
        if ($terminator_position >= 0 && ($in_typedecl == 0 || @nested_indents == 0)) {
            check_nested_nonblock_indents("expr");
            $in_expr = 0;
        }
    } else {
        check_nested_nonblock_indents($in_typedecl == 0 ? "stmt" : "decl") if $terminator_position >= 0;
    }

    # on ';', which terminates the current statement/type declaration/variable definition/function declaration
    if ($terminator_position >= 0) {
        my $tail = substr($_, $terminator_position + 1);
        if (@in_if_hanging_offsets != 0) {
            if ($tail =~ m/\s*else(\W|$)/) {
                pop @in_if_hanging_offsets;
                $hanging_offset -= INDENT_LEVEL;
            } elsif ($tail =~ m/[^\s@]/) { # code (not just comment) follows
                @in_if_hanging_offsets = (); # note there is nothing like "unclosed 'if'"
                $hanging_offset = 0;
            } else {
                $if_maybe_terminated = 1;
            }
        } elsif ($tail =~ m/^[\s@]*$/) { # ';' has been trailing, i.e. there is nothing but whitespace and comments
            $hanging_offset = 0; # reset in case of terminated assignment ('=') etc.
        }
        $in_typedecl-- if $in_typedecl != 0 && @nested_in_typedecl == 0; # TODO handle multiple type decls per line
        m/(;[^;]*)$/; # match last ';'
        $terminator_position = length($_) - length($1) if $1;
        # new $terminator_position value may be after the earlier one in case multiple terminators on current line
        # TODO check treatment in case of multiple terminators on current line
        update_nested_indents($_, $terminator_position + 1);
    }

    # set hanging expression indent according to nested indents - TODO maybe do better in update_nested_indents()
    # also if $in_expr is 0: in statement/type declaration/variable definition/function header
    $expr_indent = 0;
    for (my $i = -1; $i >= -@nested_symbols; $i--) {
        if (@nested_symbols[$i] ne "?") { # conditionals '?' ... ':' are treated specially in check_indent()
            $hanging_symbol = @nested_symbols[$i];
            $expr_indent = $nested_indents[$i];
            # $expr_indent is guaranteed to be != 0 unless @nested_indents contains just outer conditionals
            last;
        }
    }

    # remember line number and header containing name of last function defined for reports w.r.t. MAX_BODY_LENGTH
    if ($outermost_level && m/(\w+)\s*\(/ && $1 ne "STACK_OF") {
        $line_function_start = $line;
        $last_function_header = $contents;
    }

    # special checks for last, typically trailing opening brace '{' in line
    if (my ($head, $tail) = m/^(.*)\{(.*)$/) { # match last ... '{'
        if ($in_directive == 0 && !$in_expr && $in_typedecl == 0) {
            if ($outermost_level) {
                if (!$assignment_start && !$bak_in_expr) {
                    # at end of function definition header (or stmt or var definition)
                    report("'{' not at beginning") if $head ne "";
                    $line_body_start = $contents =~ m/LONG BODY/ ? 0 : $line;
                }
            } else {
                $line_opening_brace = $line if $keyword_opening_brace =~ m/do|while|for/;
                # using, not assigning, $keyword_opening_brace here because it could be on an earlier line
                $line_opening_brace = $line if $keyword_opening_brace =~ m/if|else/ && $extended_1_stmt &&
                # TODO prevent false positives for if/else where braces around single-statement branches
                # should be avoided but only if all branches have just single statements
                # The following helps detecting the exception when handling multiple 'if ... else' branches:
                    !($keyword_opening_brace eq "else" && $line_opening_brace < $line_before2);
            }
            report("code after '{'") if $tail=~ m/[^\s\@]/ && # trailing non-whitespace non-comment (non-'\')
                                      !($tail=~ m/\}/);  # missing '}' after last '{'
        }
    }

    # check for opening brace after if/while/for/switch/do not on same line
    # note that "missing '{' on same line after '} else'" is handled further below
    if (/^[\s@]*{/ && # leading '{'
        $line_before > 0 && !($contents_before_ =~ m/^\s*#/) && # not preprocessor directive '#if
        (my ($head, $mid, $tail) = ($contents_before_ =~ m/(^|^.*\W)(if|while|for|switch|do)(\W.*$|$)/))) {
        my $brace_after  = $tail =~ /^[\s@]*{/; # any whitespace or comments then '{'
        report("'{' not on same line as preceding '$mid'") if !$brace_after;
    }
    # check for closing brace on line before 'else' not followed by leading '{'
    elsif (my ($head, $tail) = m/(^|^.*\W)else(\W.*$|$)/) {
        if (parens_balance($tail) == 0 &&  # avoid false positive due to unfinished expr on current line
            !($tail =~ m/{/) && # after 'else' missing '{' on same line
            !($head =~ m/}[\s@]*$/) && # not: '}' then any whitespace or comments before 'else'
            $line_before > 0 && $contents_before_ =~ /}[\s@]*$/) { # trailing '}' on line before
            report("missing '{' on same line after '} else'");
        }
    }

    # check for closing brace before 'while' not on same line
    if (my ($head, $tail) = m/(^|^.*\W)while(\W.*$|$)/) {
        my $brace_before = $head =~ m/}[\s@]*$/; # '}' then any whitespace or comments
        # possibly 'if (...)' (with potentially inner '(' and ')') then any whitespace or comments then '{'
        if (!$brace_before &&
            # does not work here: @in_do_hanging_offsets != 0 && #'while' terminates loop
            parens_balance($tail) == 0 &&  # avoid false positive due to unfinished expr on current line
            $tail =~ /;/ && # 'while' terminates loop (by ';')
            $line_before > 0 &&
            $contents_before_ =~ /}[\s@]*$/) { # on line before: '}' then any whitespace or comments
                report("'while' not on same line as preceding '}'");
            }
    }

    # check for missing brace on same line before or after 'else'
    if (my ($head, $tail) = m/(^|^.*\W)else(\W.*$|$)/) {
        my $brace_before = $head =~ /}[\s@]*$/; # '}' then any whitespace or comments
        my $brace_after  = $tail =~ /^[\s@]*if[\s@]*\(.*\)[\s@]*{|[\s@]*{/;
        # possibly 'if (...)' (with potentially inner '(' and ')') then any whitespace or comments then '{'
        if (!$brace_before) {
            if ($line_before > 0 && $contents_before_ =~ /}[\s@]*$/) {
                report("'else' not on same line as preceding '}'");
            } elsif (parens_balance($tail) == 0) { # avoid false positive due to unfinished expr on current line
                report("missing '}' on same line before 'else ... {'") if $brace_after;
            }
        } elsif (parens_balance($tail) == 0) { # avoid false positive due to unfinished expr on current line
            report("missing '{' on same line after '} else'") if $brace_before && !$brace_after;
        }
    }

  POSTPROCESS_DIRECTIVE:
    # on begin of multi-line preprocessor directive, adapt indent
    # need to use original line contents because trailing '\' may have been stripped above
    if ($contents =~ m/^(.*?)[\s@]*\\[\s@]*$/) { # trailing '\' (which is not stripped from $contents),
        # typically used in macro definitions (or other preprocessor directives)
        if ($in_directive == 0) {
            $in_macro_header = m/^\s*#\s*define(\W|$)?(.*)/ ? 1 + parens_balance($2) : 0; # '#define' is beginning
            $directive_offset = INDENT_LEVEL;
            $block_indent += $directive_offset;
        }
        $in_directive += 1;
    }

    # post-processing at end of line @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

  LINE_FINISHED:
    $code_contents_before = $contents if
        !m/^\s*#(\s*)(\w+)/ && # not single-line directive
        $in_comment == 0 && !m/^\s*\*?@/; # not in multi-line comment nor an intra-line comment

    # on end of multi-line preprocessor directive, adapt indent
    if ($in_directive > 0 &&
        # need to use original line contents because trailing \ may have been stripped
        !($contents =~ m/^(.*?)[\s@]*\\[\s@]*$/)) { # no trailing '\'
        $block_indent -= $directive_offset;
        $in_directive = 0;
        # macro body typically does not include terminating ';'
        $hanging_offset = 0; # compensate for this in case macro ends, e.g., as 'while (0)'
    }

    if (m/^\s*$/) { # at begin of file essentially blank line: just whitespace (and maybe a '\')
            report("leading ".($1 eq "" ? "blank" :"whitespace")." line") if $line == 1 && !$sloppy_SPC;
    } else {
        if ($line_before > 0) {
            my $linediff = $line - $line_before - 1;
            report("$linediff blank lines before") if $linediff > 1 && !$sloppy_SPC;
        }
        $line_before2      = $line_before;
        $contents_before2  = $contents_before;
        $contents_before_2 = $contents_before_;
        $line_before       = $line;
        $contents_before   = $contents;
        $contents_before_  = $_;
        $count_before      = $count;
    }

    if ($self_test) { # debugging
        my $should_report = $contents =~ m/\*@(\d)?/ ? 1 : 0;
        $should_report = +$1 if $should_report != 0 && defined $1;
        print("$ARGV:$line:$num_reports_line reports on:$contents")
            if $num_reports_line != $should_report;
    }
    $num_reports_line = 0;

    # post-processing at end of file @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    if (eof) {
        # check for essentially blank line (which may include a '\') just before EOF
        report(($1 eq "\n" ? "blank line" : $2 ne "" ? "'\\'" : "whitespace")." at EOF")
            if $contents =~ m/^(\s*(\\?)\s*)$/ && !$sloppy_SPC;

        # report unclosed expression-level nesting
        check_nested_nonblock_indents("expr at EOF"); # also adapts @nested_block_indents

        # sanity-check balance of block-level { ... } via final $block_indent at end of file
        report_flexibly($line, +@nested_block_indents." unclosed '{'", "(EOF)\n") if @nested_block_indents != 0;

        # sanity-check balance of #if ... #endif via final preprocessor directive indent at end of file
        report_flexibly($line, "$directive_nesting unclosed '#if'", "(EOF)\n") if $directive_nesting != 0;

        reset_file_state();
    }
}

# final summary report @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

my $num_other_reports = $num_reports - $num_indent_reports - $num_nesting_issues
    - $num_syntax_issues - $num_SPC_reports - $num_length_reports;
print "$num_reports ($num_indent_reports indentation, $num_nesting_issues directive nesting, ".
    "$num_syntax_issues syntax, $num_SPC_reports whitespace, $num_length_reports length, $num_other_reports other)".
    " issues have been found by $0\n" if $num_reports != 0 && !$self_test;
