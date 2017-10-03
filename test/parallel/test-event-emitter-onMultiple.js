'use strict';
const common = require('../common');

// This test ensures that it is possible to add a listener to multiple events at once

const assert = require('assert');
const EventEmitter = require('../../lib/events');
const myEE = new EventEmitter();

async function main() {

    // Input
    const inputArray = ["a", "b", "c"];
    const emptyFunction = function() {};

    // Test
    await myEE.onMultiple(["a", "b", "c"], emptyFunction);
    const outputArray = await myEE.eventNames();
    
    // Assert
    await assert.deepStrictEqual(inputArray, outputArray, "The listener did not get binded to multiple events");
}

main()