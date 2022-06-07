'use strict';
require('../common');
const assert = require('assert');

// This test ensures that assert.CallTracker.calls() works as intended.

const tracker = new assert.CallTracker();

function bar() {}

const err = {
  code: 'ERR_INVALID_ARG_TYPE',
};

// Ensures calls() throws on invalid input types.
assert.throws(() => {
  const callsbar = tracker.calls({
    fn: bar,
    exact: '1'
  });
  callsbar();
}, err
);

assert.throws(() => {
  const callsbar = tracker.calls({
    fn: bar,
    exact: 0.1
  });
  callsbar();
}, { code: 'ERR_OUT_OF_RANGE' }
);

assert.throws(() => {
  const callsbar = tracker.calls({
    fn: bar,
    exact: true
  });
  callsbar();
}, err
);

assert.throws(() => {
  const callsbar = tracker.calls({
    fn: bar,
    exact: () => {}
  });
  callsbar();
}, err
);

assert.throws(() => {
  const callsbar = tracker.calls({
    fn: bar,
    exact: null
  });
  callsbar();
}, err
);

// Expects an error as tracker.calls() cannot be called within a process exit
// handler.
process.on('exit', () => {
  assert.throws(() => tracker.calls({
    fn: bar,
    exact: 1
  }), {
    code: 'ERR_UNAVAILABLE_DURING_EXIT',
  });
});

const msg = 'Expected to throw';

function func() {
  throw new Error(msg);
}

const callsfunc = tracker.calls({
  fn: func,
  exact: 1
});

// Expects callsfunc() to call func() which throws an error.
assert.throws(
  () => callsfunc(),
  { message: msg }
);

{
  const tracker = new assert.CallTracker();
  const callsNoop = tracker.calls({
    exact: 1
  });
  callsNoop();
  tracker.verify();
}

{
  const tracker = new assert.CallTracker();
  const callsNoop = tracker.calls({
    fn: bar
  });
  callsNoop('abc');
  tracker.verify();
}

{
  const tracker = new assert.CallTracker();
  const callsNoop = tracker.calls({
    fn: undefined,
    exact: 1
  });
  callsNoop();
  tracker.verify();
}

{
  const tracker = new assert.CallTracker();
  const callsNoop = tracker.calls();
  callsNoop();
  tracker.verify();
}
