'use strict';

const assert = require('assert');
const { duplexPair } = require('stream');

const [sideA, sideB] = duplexPair();

let sideAErrorReceived = false;
let sideBErrorReceived = false;

// Add error handlers
sideA.on('error', (err) => {
  sideAErrorReceived = true;
});
sideB.on('error', (err) => {
  sideBErrorReceived = true;
});

// Ensure the streams are flowing
sideA.resume();
sideB.resume();

// Destroy sideB with an error
sideB.destroy(new Error('Simulated error'));

// Wait for event loop to process error events
setImmediate(() => {
  assert.strictEqual(
    sideAErrorReceived,
    true,
    'sideA should receive the error when sideB is destroyed with an error'
  );
  assert.strictEqual(
    sideBErrorReceived,
    true,
    'sideB should emit its own error when destroyed'
  );
});

