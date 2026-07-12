'use strict';
require('../common');
const {
  hijackStdout,
  hijackStderr,
  restoreStdout,
  restoreStderr
} = require('../common/hijackstdio');

const assert = require('assert');
const Console = require('console').Console;

let c, stdout, stderr;

function setup(groupIndentation) {
  stdout = '';
  hijackStdout(function(data) {
    stdout += data;
  });

  stderr = '';
  hijackStderr(function(data) {
    stderr += data;
  });

  c = new Console({ stdout: process.stdout,
                    stderr: process.stderr,
                    colorMode: false,
                    groupIndentation: groupIndentation });
}

function teardown() {
  restoreStdout();
  restoreStderr();
}

// Basic group() functionality
{
  setup();
  const expectedOut = 'This is the outer level\n' +
                      '  Level 2\n' +
                      '    Level 3\n' +
                      '  Back to level 2\n' +
                      'Back to the outer level\n' +
                      'Still at the outer level\n';


  const expectedErr = '    More of level 3\n';

  c.log('This is the outer level');
  c.group();
  c.log('Level 2');
  c.group();
  c.log('Level 3');
  c.warn('More of level 3');
  c.groupEnd();
  c.log('Back to level 2');
  c.groupEnd();
  c.log('Back to the outer level');
  c.groupEnd();
  c.log('Still at the outer level');

  assert.strictEqual(stdout, expectedOut);
  assert.strictEqual(stderr, expectedErr);
  teardown();
}

// Group indentation is tracked per Console instance.
{
  setup();
  const expectedOut = 'No indentation\n' +
                      'None here either\n' +
                      '  Now the first console is indenting\n' +
                      'But the second one does not\n';
  const expectedErr = '';

  const c2 = new Console(process.stdout, process.stderr);
  c.log('No indentation');
  c2.log('None here either');
  c.group();
  c.log('Now the first console is indenting');
  c2.log('But the second one does not');

  assert.strictEqual(stdout, expectedOut);
  assert.strictEqual(stderr, expectedErr);
  teardown();
}

// Make sure labels work.
{
  setup();
  const expectedOut = 'This is a label\n' +
                      '  And this is the data for that label\n';
  const expectedErr = '';

  c.group('This is a label');
  c.log('And this is the data for that label');

  assert.strictEqual(stdout, expectedOut);
  assert.strictEqual(stderr, expectedErr);
  teardown();
}

// Check that console.groupCollapsed() is an alias of console.group()
{
  setup();
  const expectedOut = 'Label\n' +
                      '  Level 2\n' +
                      '    Level 3\n';
  const expectedErr = '';

  c.groupCollapsed('Label');
  c.log('Level 2');
  c.groupCollapsed();
  c.log('Level 3');

  assert.strictEqual(stdout, expectedOut);
  assert.strictEqual(stderr, expectedErr);
  teardown();
}

// Check that multiline strings and object output are indented properly.
{
  setup();
  const expectedOut = 'not indented\n' +
                      '  indented\n' +
                      '  also indented\n' +
                      '  {\n' +
                      "    also: 'a',\n" +
                      "    multiline: 'object',\n" +
                      "    should: 'be',\n" +
                      "    indented: 'properly',\n" +
                      "    kthx: 'bai'\n" +
                      '  }\n';
  const expectedErr = '';

  c.log('not indented');
  c.group();
  c.log('indented\nalso indented');
  c.log({ also: 'a',
          multiline: 'object',
          should: 'be',
          indented: 'properly',
          kthx: 'bai' });

  assert.strictEqual(stdout, expectedOut);
  assert.strictEqual(stderr, expectedErr);
  teardown();
}

// Check that the kGroupIndent symbol property is not enumerable
{
  const keys = Reflect.ownKeys(console)
                      .filter((val) => Object.prototype.propertyIsEnumerable.call(console, val))
                      .map((val) => val.toString());
  assert(!keys.includes('Symbol(groupIndent)'),
         'groupIndent should not be enumerable');
}

// Check custom groupIndentation.
{
  setup(3);
  const expectedOut = 'Set the groupIndentation parameter to 3\n' +
                      'This is the outer level\n' +
                      '   Level 2\n' +
                      '      Level 3\n' +
                      '   Back to level 2\n' +
                      'Back to the outer level\n' +
                      'Still at the outer level\n';


  const expectedErr = '      More of level 3\n';

  c.log('Set the groupIndentation parameter to 3');
  c.log('This is the outer level');
  c.group();
  c.log('Level 2');
  c.group();
  c.log('Level 3');
  c.warn('More of level 3');
  c.groupEnd();
  c.log('Back to level 2');
  c.groupEnd();
  c.log('Back to the outer level');
  c.groupEnd();
  c.log('Still at the outer level');

  assert.strictEqual(stdout, expectedOut);
  assert.strictEqual(stderr, expectedErr);
  teardown();
}

// Check the correctness of the groupIndentation parameter.
{
  // TypeError
  [null, 'str', [], false, true, {}].forEach((e) => {
    assert.throws(
      () => {
        new Console({ stdout: process.stdout,
                      stderr: process.stderr,
                      groupIndentation: e });
      },
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError'
      }
    );
  });

  // RangeError for integer
  [NaN, 1.01].forEach((e) => {
    assert.throws(
      () => {
        new Console({ stdout: process.stdout,
                      stderr: process.stderr,
                      groupIndentation: e });
      },
      {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
        message: /an integer/,
      }
    );
  });

  // RangeError
  [-1, 1001].forEach((e) => {
    assert.throws(
      () => {
        new Console({ stdout: process.stdout,
                      stderr: process.stderr,
                      groupIndentation: e });
      },
      {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
        message: />= 0 && <= 1000/,
      }
    );
  });
}
