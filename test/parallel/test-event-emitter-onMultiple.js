'use strict';
const common = require('../common');

// This test ensures that it is possible to add a listener to multiple events
// at once

const assert = require('assert');

async function test_goodInput_1() {

  const EventEmitter = require('../../lib/events');
  const myEE = new EventEmitter();

  const input = {
    event1: [common.mustCall()],
    event2: [common.mustCall(), common.mustCall()]
  };

  await myEE.onMultiple(input);

  await myEE.emit('event1');
  await myEE.emit('event2');

}


async function test_goodInput_2() {

  const EventEmitter = require('../../lib/events');
  const myEE = new EventEmitter();

  const input = {
    event3() { common.mustCall(); },
    event4() { common.mustCall(); }
  };

  await myEE.onMultiple(input);

  await myEE.emit('event3');
  await myEE.emit('event4');

}


async function test_badInputs() {

  const EventEmitter = require('../../lib/events');
  const myEE = new EventEmitter();

  await assert.throws(
    function() { myEE.onMultiple(undefined); },
    Error
  );
  await assert.throws(
    function() { myEE.onMultiple(true); },
    Error);
  await assert.throws(
    function() { myEE.onMultiple(1); },
    Error
  );
  await assert.throws(
    function() { myEE.onMultiple('foo'); },
    Error
  );
  await assert.throws(
    function() { myEE.onMultiple(Symbol('Foo')); },
    Error
  );
  await assert.throws(
    function() { myEE.onMultiple(function() {}); },
    Error
  );
  await assert.throws(
    function() { myEE.onMultiple([]); },
    Error
  );

}

test_goodInput_1();
test_goodInput_2();
test_badInputs();
