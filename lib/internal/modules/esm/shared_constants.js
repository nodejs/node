// This file contains the definition for the constant values that must be
// available to both the main thread and the loader thread.

'use strict';

/*
The shared memory area is divided in 4 32-bit long sections. It has to be 32-bit
long as `Atomics.notify` only works with `Int32Array` objects.

--32-bits-- --32-bits-- --32-bits-- --32-bits--
    ^           ^           ^           ^
    |           |           |           |
    |           |           | NUMBER_OF_INCOMING_ASYNC_RESPONSES
    |           |           |
    |           | NUMBER_OF_INCOMING_MESSAGES
    |           |
    |   BOREDOM_OF_THREADS
    |
WORKER_TO_MAIN_THREAD_NOTIFICATION

WORKER_TO_MAIN_THREAD_NOTIFICATION is only used to send notifications, its value should always be 0.

BOREDOM_OF_THREADS does not represent a number, but an array of booleans.
Currently, only two flags are used: WORKER_THREAD_IS_BORED, and MAIN_THREAD_IS_BORED

xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx00: WORKER_THREAD_IS_BORED is false, and MAIN_THREAD_IS_BORED is false.
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx01: WORKER_THREAD_IS_BORED is true, and MAIN_THREAD_IS_BORED is false.
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx10: WORKER_THREAD_IS_BORED is false, and MAIN_THREAD_IS_BORED is true.
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx11: WORKER_THREAD_IS_BORED is true, and MAIN_THREAD_IS_BORED is true.

NUMBER_OF_INCOMING_MESSAGES is used to communicate the number of messages sent
by the main thread and not yet handled by the worker thread. It is also used to
wake up the worker thread when it locks itself.

NUMBER_OF_INCOMING_ASYNC_RESPONSES is used to let the main thread keep count of
how many async responses to expect.

*/

module.exports = {
  WORKER_TO_MAIN_THREAD_NOTIFICATION: 0,

  BOREDOM_OF_THREADS: 1,
  WORKER_THREAD_IS_BORED: 0b01,
  MAIN_THREAD_IS_BORED: 0b10,

  NUMBER_OF_INCOMING_MESSAGES: 2,
  NUMBER_OF_INCOMING_ASYNC_RESPONSES: 3,
};
