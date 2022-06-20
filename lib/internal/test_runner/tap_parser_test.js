'use strict';

const util = require('util');
const { TapParser } = require('./tap_parser');

function TAP(input) {
  const parser = new TapParser(input, {debug: true});
  const ast = parser.parse();

  console.log(`s------------------------------`);
  console.log(util.inspect(ast, { depth: 10, colors: true }));
  console.log(`e------------------------------`);
  return parser;
}

//-------------TESTS-------------//

TAP(`
TAP version 14
1..4
`);


TAP(`


TAP version 14


1..4


`);

TAP(`TAP version 14
1..4 # this is a description
`);

TAP(`
TAP version 14
1..4 # reason for plan "\\ !"\\#$%&'()*+,\\-./:;<=>?@[]^_\`{|}~"
`);

TAP(`
TAP version 14
1..4 # reason for plan
ok
`);

TAP(`
TAP version 14
1..4 # reason for plan ok not ok not
ok
`);

TAP(`
TAP version 14
1..4 # reason for plan ok not
ok 1 this is a test
`);

TAP(`
TAP version 14
1..4 # description for test plan ok TAP version 14
ok 1 this is an ok test
ok 2 - this is  a test ok
not ok - 3 this is a not ok test
ok 4 this is a test`);

TAP(`
TAP version 14
1..4
ok 1 - this is an ok test
ok 2 this is  a test ok
not ok 3 this is a not ok test
ok 4 this is a test`);

TAP(`
TAP version 14
1..4
ok 1 - hello \\# \\\\ world # TODO escape \\# characters with \\\\
ok foo bar
`);

TAP(`
TAP version 14
1..8



# description: hello
# todo: true
ok 1 - hello # todo

# description: hello # todo
# todo: false
ok 2 - hello \\# todo

# description: hello
# todo: true
# todo reason: hash # character
ok 3 - hello # todo hash \\# character
# (assuming "character" isn't a known custom directive)
ok 4 - hello # todo hash # character

# description: hello \
# todo: true
# todo reason: hash # character
ok 5 - hello \\\\# todo hash \\# character
# (assuming "character" isn't a known custom directive)
ok 6 - hello \\\\# todo hash # character

# description: hello # description # todo
# todo: false
# (assuming "description" isn't a known custom directive)
ok 7 - hello # description # todo

# multiple escaped \ can appear in a row
# description: hello \\\# todo
# todo: false
ok 8 - hello \\\\\\\\\\\\\\# todo
`);

TAP(`
ok  - hello # # # # #
not ok  - hello # \\#


# skip: true
ok 7 - do it later # Skipped

`);


TAP(`
TAP version 14
1..4
ok
`);

TAP(`
1..4
TAP version 14
ok
`);

TAP(`
ok
1..4
TAP version 14
`);

TAP(`
ok
  ---
  message: "Failed with error 'hostname peebles.example.com not found'"
  severity: fail
  found:
    hostname: 'peebles.example.com'
    address: ~
  wanted:
    hostname: 'peebles.example.com'
    address: '85.193.201.85'
  at:
    file: test/dns-resolve.c
    line: 142
  ...

ok
  ---
  severity: none
  at:
    file: test/dns-resolve.c
    line: 142
  ...
`);

TAP(`
ok
Bail out! Couldn't connect to database.
`);

TAP(`
TAP version 14
# tell the parser to bail out on any failures from now on
pragma +bail

# tell the parser to execute in strict mode, treating any invalid TAP
# line as a test failure.
pragma +strict

# turn off a feature we don't want to be active right now
pragma -bail
`);

TAP(`
pragma +strict, -foo
`);

// subtests

TAP(`
ok 1 - subtest test point 1
    not ok 2 - subtest test point 2
        ok 3 - subtest test point 3 # todo
            ok 4 - subtest test point 4 # skip because
`);

TAP(`
            ok 4 - subtest test point 4 # skip because
        ok 3 - subtest test point 3 # todo
    not ok 2 - subtest test point 2
ok 1 - subtest test point 1
`);

TAP(`
# Subtest: subtest1
    ok 1 - subtest1 test point 1
ok 1 - subtest1
`);

TAP(`
# Subtest: subtest1
    ok - subtest1 test
ok - subtest1

# Subtest: subtest2
    ok - subtest2 test
ok - subtest2

# Subtest: subtest3
    ok - subtest3 test
ok - subtest3
`);

TAP(`
# Subtest: subtest1
    ok - subtest1 test
    # Subtest: subtest2
        ok - subtest2 test
    ok - subtest2
ok - subtest1


# Subtest: subtest3
    ok - subtest3 test

# this will not terminate the subtest3
ok - subtest xxx
`);


TAP(`
TAP version 14
1..2

# Subtest: foo.tap
    1..2
    ok 1
    ok 2 - this passed
ok 1 - foo.tap

# Subtest: bar.tap
    ok 1 - object should be a Bar
    not ok 2 - object.isBar should return true
      ---
      found: false
      wanted: true
      at:
        file: test/bar.ts
        line: 43
        column: 8
      ...
    ok 3 - object can bar bears # TODO
    1..3
not ok 2 - bar.tap
  ---
  fail: 1
  todo: 1
  ...
Bail out! Error: Couldn't connect to database.
`);

// more examples from https://testanything.org/tap-version-14-specification.html#examples

TAP(`
TAP version 14
1..6
#
# Create a new Board and Tile, then place
# the Tile onto the board.
#
ok 1 - The object isa Board
ok 2 - Board size is zero
ok 3 - The object isa Tile
ok 4 - Get possible places to put the Tile
ok 5 - Placing the tile produces no error
ok 6 - Board size is 1
`);

TAP(`
TAP version 14
ok 1 - retrieving servers from the database
# need to ping 6 servers
ok 2 - pinged diamond
ok 3 - pinged ruby
not ok 4 - pinged saphire
  ---
  message: 'hostname "saphire" unknown'
  severity: fail
  ...
ok 5 - pinged onyx
not ok 6 - pinged quartz
  ---
  message: 'timeout'
  severity: fail
  ...
ok 7 - pinged gold
1..7
`)

TAP(`
TAP version 14
1..573
not ok 1 - database handle
Bail out! Couldn't connect to database.
`);

TAP(`
TAP version 14
1..5
ok 1 - approved operating system
# $^0 is solaris
ok 2 - # SKIP no /sys directory
ok 3 - # SKIP no /sys directory
ok 4 - # SKIP no /sys directory
ok 5 - # SKIP no /sys directory
`);

TAP(`
TAP version 14
1..0 # skip because English-to-French translator isn't installed
`);

TAP(`
TAP version 14
1..4
ok 1 - Creating test program
ok 2 - Test program runs, no error
not ok 3 - infinite loop # TODO halting problem unsolved
not ok 4 - infinite loop 2 # TODO halting problem unsolved
`);

TAP(`
TAP version 14
ok - created Board
ok
ok
ok
ok
ok
ok
ok
  ---
  message: "Board layout"
  severity: comment
  dump:
     board:
       - '      16G         05C        '
       - '      G N C       C C G      '
       - '        G           C  +     '
       - '10C   01G         03C        '
       - 'R N G G A G       C C C      '
       - '  R     G           C  +     '
       - '      01G   17C   00C        '
       - '      G A G G N R R N R      '
       - '        G     R     G        '
  ...
ok - board has 7 tiles + starter tile
1..9
`);
