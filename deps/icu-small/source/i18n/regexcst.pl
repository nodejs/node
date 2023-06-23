#!/usr/bin/perl
# Copyright (C) 2016 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html
#  ********************************************************************
#  * COPYRIGHT:
#  * Copyright (c) 2002-2016, International Business Machines Corporation and
#  * others. All Rights Reserved.
#  ********************************************************************
#
#  regexcst.pl
#            Compile the regular expression parser state table data into initialized C data.
#            Usage:
#                   cd icu4c/source/i18n
#                   perl regexcst.pl < regexcst.txt > regexcst.h
#
#             The output file, regexcst.h, is included by some of the .cpp regex
#             implementation files.   This perl script is NOT run as part
#             of a normal ICU build.  It is run by hand when needed, and the
#             regexcst.h generated file is put back into the source code repository.
#
#             See regexcst.txt for a description of the input format for this script.
#
#             This script is derived from rbbicst.pl, which peforms the same function
#             for the Rule Based Break Iterator Rule Parser.  Perhaps they could be
#             merged?
#


$num_states = 1;         # Always the state number for the line being compiled.
$line_num  = 0;          # The line number in the input file.

$states{"pop"} = 255;    # Add the "pop"  to the list of defined state names.
                         # This prevents any state from being labelled with "pop",
                         #  and resolves references to "pop" in the next state field.

line_loop: while (<>) {
    chomp();
    $line = $_;
    @fields = split();
    $line_num++;

    # Remove # comments, which are any fields beginning with a #, plus all
    #  that follow on the line.
    for ($i=0; $i<@fields; $i++) {
        if ($fields[$i] =~ /^#/) {
            @fields = @fields[0 .. $i-1];
            last;
        }
    }
    # ignore blank lines, and those with no fields left after stripping comments..
    if (@fields == 0) {
        next;
    }

    #
    # State Label:  handling.
    #    Does the first token end with a ":"?  If so, it's the name  of a state.
    #    Put in a hash, together with the current state number,
    #        so that we can later look up the number from the name.
    #
    if (@fields[0] =~ /.*:$/) {
        $state_name = @fields[0];
        $state_name =~ s/://;        # strip off the colon from the state name.

        if ($states{$state_name} != 0) {
            print "  rbbicst: at line $line-num duplicate definition of state $state_name\n";
        }
        $states{$state_name} = $num_states;
        $stateNames[$num_states] = $state_name;

        # if the label was the only thing on this line, go on to the next line,
        # otherwise assume that a state definition is on the same line and fall through.
        if (@fields == 1) {
            next line_loop;
        }
        shift @fields;                       # shift off label field in preparation
                                             #  for handling the rest of the line.
    }

    #
    # State Transition line.
    #   syntax is this,
    #       character   [n]  target-state  [^push-state]  [function-name]
    #   where
    #      [something]   is an optional something
    #      character     is either a single quoted character e.g. '['
    #                       or a name of a character class, e.g. white_space
    #

    $state_line_num[$num_states] = $line_num;   # remember line number with each state
                                                #  so we can make better error messages later.
    #
    # First field, character class or literal character for this transition.
    #
    if ($fields[0] =~ /^'.'$/) {
        # We've got a quoted literal character.
        $state_literal_chars[$num_states] = $fields[0];
        $state_literal_chars[$num_states] =~ s/'//g;
    } else {
        # We've got the name of a character class.
        $state_char_class[$num_states] = $fields[0];
        if ($fields[0] =~ /[\W]/) {
            print "  rbbicsts:  at line $line_num, bad character literal or character class name.\n";
            print "     scanning $fields[0]\n";
            exit(-1);
        }
    }
    shift @fields;

    #
    # do the 'n' flag
    #
    $state_flag[$num_states] = "false";
    if ($fields[0] eq "n") {
        $state_flag[$num_states] = "true";
        shift @fields;
    }

    #
    # do the destination state.
    #
    $state_dest_state[$num_states] = $fields[0];
    if ($fields[0] eq "") {
        print "  rbbicsts:  at line $line_num, destination state missing.\n";
        exit(-1);
    }
    shift @fields;

    #
    # do the push state, if present.
    #
    if ($fields[0] =~ /^\^/) {
        $fields[0] =~ s/^\^//;
        $state_push_state[$num_states] = $fields[0];
        if ($fields[0] eq "" ) {
            print "  rbbicsts:  at line $line_num, expected state after ^ (no spaces).\n";
            exit(-1);
        }
        shift @fields;
    }

    #
    # Lastly, do the optional action name.
    #
    if ($fields[0] ne "") {
        $state_func_name[$num_states] = $fields[0];
        shift @fields;
    }

    #
    #  There should be no fields left on the line at this point.
    #
    if (@fields > 0) {
       print "  rbbicsts:  at line $line_num, unexpected extra stuff on input line.\n";
       print "     scanning $fields[0]\n";
   }
   $num_states++;
}

#
# We've read in the whole file, now go back and output the
#   C source code for the state transition table.
#
# We read all states first, before writing anything,  so that the state numbers
# for the destination states are all available to be written.
#

#
# Make hashes for the names of the character classes and
#      for the names of the actions that appeared.
#
for ($state=1; $state < $num_states; $state++) {
    if ($state_char_class[$state] ne "") {
        if ($charClasses{$state_char_class[$state]} == 0) {
            $charClasses{$state_char_class[$state]} = 1;
        }
    }
    if ($state_func_name[$state] eq "") {
        $state_func_name[$state] = "doNOP";
    }
    if ($actions{$state_action_name[$state]} == 0) {
        $actions{$state_func_name[$state]} = 1;
    }
}

#
# Check that all of the destination states have been defined
#
#
$states{"exit"} = 0;              # Predefined state name, terminates state machine.
for ($state=1; $state<$num_states; $state++) {
   if ($states{$state_dest_state[$state]} == 0 && $state_dest_state[$state] ne "exit") {
       print "Error at line $state_line_num[$state]: target state \"$state_dest_state[$state]\" is not defined.\n";
       $errors++;
   }
   if ($state_push_state[$state] ne "" && $states{$state_push_state[$state]} == 0) {
       print "Error at line $state_line_num[$state]: target state \"$state_push_state[$state]\" is not defined.\n";
       $errors++;
   }
}

die if ($errors>0);

print "// © 2016 and later: Unicode, Inc. and others.\n";
print "// License & terms of use: http://www.unicode.org/copyright.html\n";
print "//---------------------------------------------------------------------------------\n";
print "//\n";
print "// Generated Header File.  Do not edit by hand.\n";
print "//    This file contains the state table for the ICU Regular Expression Pattern Parser\n";
print "//    It is generated by the Perl script \"regexcst.pl\" from\n";
print "//    the rule parser state definitions file \"regexcst.txt\".\n";
print "//\n";
print "//   Copyright (C) 2002-2016 International Business Machines Corporation \n";
print "//   and others. All rights reserved.  \n";
print "//\n";
print "//---------------------------------------------------------------------------------\n";
print "#ifndef RBBIRPT_H\n";
print "#define RBBIRPT_H\n";
print "\n";
print "#include \"unicode/utypes.h\"\n";
print "\n";
print "U_NAMESPACE_BEGIN\n";

#
# Emit the constants for indices of Unicode Sets
#   Define one constant for each of the character classes encountered.
#   At the same time, store the index corresponding to the set name back into hash.
#
print "//\n";
print "// Character classes for regex pattern scanning.\n";
print "//\n";
$i = 128;                   # State Table values for Unicode char sets range from 128-250.
                            # Sets "default", "quoted", etc. get special handling.
                            #  They have no corresponding UnicodeSet object in the state machine,
                            #    but are handled by special case code.  So we emit no reference
                            #    to a UnicodeSet object to them here.
foreach $setName (keys %charClasses) {
    if ($setName eq "default") {
        $charClasses{$setName} = 255;}
    elsif ($setName eq "quoted") {
        $charClasses{$setName} = 254;}
    elsif ($setName eq "eof") {
        $charClasses{$setName} = 253;}
    else {
        # Normal character class.  Fill in array with a ptr to the corresponding UnicodeSet in the state machine.
       print "    static const uint8_t kRuleSet_$setName = $i;\n";
        $charClasses{$setName} = $i;
        $i++;
    }
}
print "    constexpr uint32_t kRuleSet_count = $i-128;";
print "\n\n";

#
# Emit the enum for the actions to be performed.
#
print "enum Regex_PatternParseAction {\n";
foreach $act (keys %actions) {
    print "    $act,\n";
}
print "    rbbiLastAction};\n\n";

#
# Emit the struct definition for transition table elements.
#
print "//-------------------------------------------------------------------------------\n";
print "//\n";
print "//  RegexTableEl       represents the structure of a row in the transition table\n";
print "//                     for the pattern parser state machine.\n";
print "//-------------------------------------------------------------------------------\n";
print "struct RegexTableEl {\n";
print "    Regex_PatternParseAction      fAction;\n";
print "    uint8_t                       fCharClass;       // 0-127:    an individual ASCII character\n";
print "                                                    // 128-255:  character class index\n";
print "    uint8_t                       fNextState;       // 0-250:    normal next-state numbers\n";
print "                                                    // 255:      pop next-state from stack.\n";
print "    uint8_t                       fPushState;\n";
print "    UBool                         fNextChar;\n";
print "};\n\n";

#
# emit the state transition table
#
print "static const struct RegexTableEl gRuleParseStateTable[] = {\n";
print "    {doNOP, 0, 0, 0, true}\n";    # State 0 is a dummy.  Real states start with index = 1.
for ($state=1; $state < $num_states; $state++) {
    print "    , {$state_func_name[$state],";
    if ($state_literal_chars[$state] ne "") {
        $c = $state_literal_chars[$state];
        printf(" %d /* $c */,", ord($c));   #  use numeric value, so EBCDIC machines are ok.
    }else {
        print " $charClasses{$state_char_class[$state]},";
    }
    print " $states{$state_dest_state[$state]},";

    # The push-state field is optional.  If omitted, fill field with a zero, which flags
    #   the state machine that there is no push state.
    if ($state_push_state[$state] eq "") {
        print "0, ";
    } else {
        print " $states{$state_push_state[$state]},";
    }
    print " $state_flag[$state]} ";

    # Put out a C++ comment showing the number (index) of this state row,
    #   and, if this is the first row of the table for this state, the state name.
    print "    //  $state ";
    if ($stateNames[$state] ne "") {
        print "     $stateNames[$state]";
    }
    print "\n";
};
print " };\n";


#
# emit a mapping array from state numbers to state names.
#
#    This array is used for producing debugging output from the pattern parser.
#
print "static const char * const RegexStateNames[] = {";
for ($state=0; $state<$num_states; $state++) {
    if ($stateNames[$state] ne "") {
        print "     \"$stateNames[$state]\",\n";
    } else {
        print "    0,\n";
    }
}
print "    0};\n\n";

print "U_NAMESPACE_END\n";
print "#endif\n";



