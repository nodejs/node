'use strict';
const common = require('../common');

// This test ensures that it is possible to add a listener to multiple events at once

const assert = require('assert');
const EventEmitter = require('../../lib/events');
const myEE = new EventEmitter();

async function test1() {

    const input = {
        event1 : [ common.mustCall() ],
        event2 : [ common.mustCall(), common.mustCall()]
      };

    await myEE.onMultiple(input);

    await myEE.emit("event1");
    await myEE.emit("event2");;

}
    
async function test2() {

    const input1 = "String";
    const input2 = 3;
    const input3 = [];

    await assert.throws( async function() { await myEE.onMultiple(input1) } );
    await assert.throws( async function() { await myEE.onMultiple(input2) } );
    await assert.throws( async function() { await myEE.onMultiple(input3) } );
}

async function test3() {

    let ok = 0;

    const input = {
        event3() { ok++; },
        event4() { ok++; }
    }

    await myEE.onMultiple(input);

    await myEE.emit("event3");
    await myEE.emit("event4");;

    await assert.deepStrictEqual(ok, 2, "Not all events were emitted")
}

test1();