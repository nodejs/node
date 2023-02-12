// This file contains the definition for the constant values that must be
// available to both the main thread and the loader thread.

'use strict';

module.exports = {
  WORKER_TO_MAIN_THREAD_NOTIFICATION: 0,

  BOREDOM_OF_THREADS: 1,
  WORKER_THREAD_IS_BORED: 0b01,
  MAIN_THREAD_IS_BORED: 0b10,

  NUMBER_OF_INCOMING_MESSAGES: 2,
  NUMBER_OF_INCOMING_ASYNC_RESPONSES: 3,
};
