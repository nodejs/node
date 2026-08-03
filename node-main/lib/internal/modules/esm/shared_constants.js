// This file contains the definition for the constant values that must be
// available to both the main thread and the loader thread.

'use strict';

/*
The shared memory area is divided in 1 32-bit long section. It has to be 32-bit long as
`Atomics.notify` only works with `Int32Array` objects.

--32-bits--
    ^
    |
    |
WORKER_TO_MAIN_THREAD_NOTIFICATION

WORKER_TO_MAIN_THREAD_NOTIFICATION is only used to send notifications, its value is going to
increase every time the worker sends a notification to the main thread.

*/

module.exports = {
  WORKER_TO_MAIN_THREAD_NOTIFICATION: 0,

  SHARED_MEMORY_BYTE_LENGTH: 1 * 4,
};
