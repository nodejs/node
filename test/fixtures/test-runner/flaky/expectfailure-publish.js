'use strict';
// A flaky + expectFailure test that throws (an expected failure, so the test
// passes) must still publish its error to the `node.test` error channel exactly
// as a non-flaky expectFailure test does. The fixture counts error publishes and
// writes the total to the state file.
const dc = require('node:diagnostics_channel');
const { test } = require('node:test');
const { writeFileSync } = require('node:fs');

const stateFile = process.env.FLAKY_STATE;
const channel = dc.tracingChannel('node.test');
let errors = 0;
channel.error.subscribe(() => { errors++; });

test('flaky expectFailure throws', { flaky: 3, expectFailure: true }, () => {
  throw new Error('this failure is expected');
});

process.on('exit', () => {
  writeFileSync(stateFile, String(errors));
});
