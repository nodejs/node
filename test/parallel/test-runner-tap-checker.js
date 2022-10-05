'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('assert');

const { TapParser } = require('internal/test_runner/tap_parser');
const { TapChecker } = require('internal/test_runner/tap_checker');

function TAPChecker(input) {
  // parse
  const parser = new TapParser({ specs: TapChecker.TAP14 });
  parser.parseSync(input);
  parser.check();
}

{
  assert.throws(() => TAPChecker('TAP version 14'), {
    name: 'TAPValidationError',
    message: 'missing TAP plan',
  });
}

{
  assert.throws(
    () =>
      TAPChecker(`
TAP version 14
1..1
`),
    {
      name: 'TAPValidationError',
      message: 'missing Test Points',
    }
  );
}

{

  // Valid TAP14 should not throw
  TAPChecker(`
TAP version 14
1..1
ok
`);
}

{
  assert.throws(
    () =>
      TAPChecker(`
TAP version 14
1..1
ok 2
`),
    {
      name: 'TAPValidationError',
      message: 'test 2 is out of plan range 1..1',
    }
  );
}

{
  assert.throws(
    () =>
      TAPChecker(`
TAP version 14
3..1
ok 2
`),
    {
      name: 'TAPValidationError',
      message: 'plan start 3 is greater than plan end 1',
    }
  );
}

{
  assert.throws(
    () =>
      TAPChecker(`
TAP version 14
2..3
ok 1
ok 2
ok 3
`),
    {
      name: 'TAPValidationError',
      message: 'test 1 is out of plan range 2..3',
    }
  );
}

{
// Valid comment line shout not throw.
  TAPChecker(`
TAP version 14
1..5
ok 1 - approved operating system
# $^0 is solaris
ok 2 - # SKIP no /sys directory
ok 3 - # SKIP no /sys directory
ok 4 - # SKIP no /sys directory
ok 5 - # SKIP no /sys directory
`);
}

{
// Valida empty test plan should not throw.
  TAPChecker(`
TAP version 14
1..0 # skip because English-to-French translator isn't installed
`);
}

{
// Valid test plan count should not throw.
  TAPChecker(`
TAP version 14
1..4
ok 1 - Creating test program
ok 2 - Test program runs, no error
not ok 3 - infinite loop # TODO halting problem unsolved
not ok 4 - infinite loop 2 # TODO halting problem unsolved
`);

}

{
// Valid YAML diagnostic should not throw.
  TAPChecker(`
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
}

{
  // Valid Bail out should not throw.
  TAPChecker(`
TAP version 14
1..573
not ok 1 - database handle
Bail out! Couldn't connect to database.
`);
}
