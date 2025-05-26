'use strict';

/**
 * Byte offset where the actual data begins in the shared memory
 * @type {number}
 */
const OFFSET = 64;

/**
 * Index in the Int32Array for signaling from worker to main thread
 * 0: writing from worker, reading from main
 * @type {number}
 */
const TO_WORKER = 0;

/**
 * Index in the Int32Array for signaling from main to worker thread
 * 1: writing from main, reading from worker
 * @type {number}
 */
const TO_MAIN = 1;

module.exports = {
  OFFSET,
  TO_WORKER,
  TO_MAIN,
};
