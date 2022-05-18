'use strict';
require('../common');
const assert = require('assert');

function bar() {}

// It should call once the bar function with 1 as first argument
{
  const tracker = new assert.CallTracker();
  const callsNoop = tracker.callsWith(bar, [1], 1);
  callsNoop(1);
  tracker.verify();
}

// It should call once the bar function with 'a' and 'b' as arguments
{
  const tracker = new assert.CallTracker();
  const callsNoop = tracker.callsWith(bar, ['a', 'b'], 1);
  callsNoop('a', 'b');
  tracker.verify();
}

// It should call twice the bar function with 'a' and 'b' as arguments
{
  const tracker = new assert.CallTracker();
  const callsNoop = tracker.callsWith(bar, ['a', 'b'], 2);
  callsNoop('a', 'b');
  callsNoop('a', 'b');
  tracker.verify();
}

// When calling the watching function with incorrect params it should show an error message
{
  const tracker = new assert.CallTracker();
  const callsNoop = tracker.callsWith(['a', 'b'], 1);
  callsNoop('c', 'd');
  const expectedMessage = 'Expected the calls function to be executed 1 time(s)' +
  ' with args (a,b) but was executed 1 time(s) with args (c,d).';
  const [{ message }] = tracker.report();
  assert.deepStrictEqual(message, expectedMessage);
}

// It should show an error with calling the function wrongly once and rightly once
{
  const tracker = new assert.CallTracker();
  const callsNoop = tracker.callsWith(['a', 'b'], 1);
  callsNoop('c', 'd');
  callsNoop('a', 'b');
  const expectedMessage = 'Expected the calls function to be executed 1 time(s)' +
  ' with args (a,b) but was executed 2 time(s) with args (a,b).';
  const [{ message }] = tracker.report();
  assert.deepStrictEqual(message, expectedMessage);
}

// It should verify once the given args
{
  const tracker = new assert.CallTracker();
  const callsNoop = tracker.callsWith([bar]);
  callsNoop(bar);
  tracker.verify();
}

// It should call 100 times with the same args
{
  const tracker = new assert.CallTracker();
  const callCount = 100;
  const callsNoop = tracker.callsWith([ { abc: 1 }, [1], bar], callCount);
  for (let i = 0; i < callCount; i++)
    callsNoop({ abc: 1 }, [1], bar);

  tracker.verify();
}

// Known behavior: it should validate two different function instances as not equal
{
  const tracker = new assert.CallTracker();
  const callsNoop = tracker.callsWith([function() {}]);
  callsNoop(function() {});

  const [{ message }] = tracker.report();
  const expectedMsg = 'Expected the calls function to be executed 1 time(s)' +
  ' with args (function() {}) but was executed 1 time(s) with args (function() {}).';
  assert.deepStrictEqual(message, expectedMsg);
}

// It should not use the same signature of tracker.call
{
  const tracker = new assert.CallTracker();
  assert.throws(
    () => tracker.callsWith(bar),
    {
      code: 'ERR_ASSERTION',
    }
  );
}

// It should not use the same signature of tracker.call
{
  const tracker = new assert.CallTracker();
  assert.throws(
    () => tracker.callsWith(1),
    {
      code: 'ERR_ASSERTION',
    }
  );
}

// It should make sure that the objects' instances have different values
{
  const tracker = new assert.CallTracker();
  const callsNoop = tracker.callsWith([ new Map([[ 'a', '1' ]]) ]);
  callsNoop(new Map([[ 'a', '2']]));

  const [{ message }] = tracker.report();
  const expectedMsg = 'Expected the calls function to be executed 1 time(s)' +
  ' with args ([object Map]) but was executed 1 time(s) with args ([object Map]).';
  assert.deepStrictEqual(message, expectedMsg);
}

// It should validate two complex objects
{
  const tracker = new assert.CallTracker();
  const callsNoop = tracker.callsWith([ new Map([[ 'a', '1' ]]), new Set([ 'a', 2]) ]);
  callsNoop(new Map([[ 'a', '1']]), new Set([ 'a', 2]));
  tracker.verify();
}
