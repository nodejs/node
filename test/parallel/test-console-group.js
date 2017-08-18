'use strict';
const common = require('../common');

const assert = require('assert');
const Console = require('console').Console;

let stdout, stderr;

function setup() {
  stdout = '';
  common.hijackStdout(function(data) {
    stdout += data;
  });

  stderr = '';
  common.hijackStderr(function(data) {
    stderr += data;
  });
}

function teardown() {
  common.restoreStdout();
  common.restoreStderr();
}

{
  setup();
  const expectedOut = 'This is the outer level\n' +
                      '  Level 2\n' +
                      '    Level 3\n' +
                      '  Back to level 2\n' +
                      'Back to the outer level\n' +
                      'Still at the outer level\n';


  const expectedErr = '    More of level 3\n';

  console.log('This is the outer level');
  console.group();
  console.log('Level 2');
  console.group();
  console.log('Level 3');
  console.warn('More of level 3');
  console.groupEnd();
  console.log('Back to level 2');
  console.groupEnd();
  console.log('Back to the outer level');
  console.groupEnd();
  console.log('Still at the outer level');

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

  const c1 = new Console(process.stdout, process.stderr);
  const c2 = new Console(process.stdout, process.stderr);
  c1.log('No indentation');
  c2.log('None here either');
  c1.group();
  c1.log('Now the first console is indenting');
  c2.log('But the second one does not');

  assert.strictEqual(stdout, expectedOut);
  assert.strictEqual(stderr, expectedErr);
  teardown();
}
