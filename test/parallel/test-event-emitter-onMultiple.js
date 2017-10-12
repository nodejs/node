'use strict';
const common = require('../common');

// This test ensures that it is possible to add a listener to multiple events at once

const assert = require('assert');
const EventEmitter = require('../../lib/events');
const myEE = new EventEmitter();

async function main() {

    let ok = 0;

    const input = {
        "event1" : [function() { ok++; }],
        "event2" : [
            function() { ok++; },
            function() { ok++; }
        ]
    }

    await myEE.onMultiple(input);

    await myEE.emit("event1");
    await myEE.emit("event2");
    await myEE.emit("event3");

    assert.deepStrictEqual(ok, 3, "Some handlers were not executed");

}

main()