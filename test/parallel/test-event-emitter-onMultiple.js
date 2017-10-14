'use strict';
const common = require('../common');

// This test ensures that it is possible to add a listener to multiple events at once

const assert = require('assert');
const EventEmitter = require('../../lib/events');
const myEE = new EventEmitter();

async function test_goodInput_1() {

    const input = {
        event1 : [ common.mustCall() ],
        event2 : [ common.mustCall(), common.mustCall()]
      };

    await myEE.onMultiple(input);

    await myEE.emit("event1");
    await myEE.emit("event2");;

}


async function test_goodInput_2() {

    const input = {
        event3() { common.mustCall() },
        event4() { common.mustCall() }
    }

    await myEE.onMultiple(input);

    await myEE.emit("event3");
    await myEE.emit("event4");;

}


async function test_badInputs() {

    await assert.throws(function() { myEE.onMultiple(undefined) } );
    await assert.throws(function() { myEE.onMultiple(true) } );
    await assert.throws(function() { myEE.onMultiple(1) } );
    await assert.throws(function() { myEE.onMultiple("foo") } );
    await assert.throws(function() { myEE.onMultiple(Symbol("Foo")) } );
    await assert.throws(function() { myEE.onMultiple(function() {}) } );
    await assert.throws(function() { myEE.onMultiple([]) } );    
       
}

test_goodInput_1();
test_goodInput_2();
test_badInputs();