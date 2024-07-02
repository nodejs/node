'use strict';

const {
  AtomicsNotify,
  AtomicsStore,
  AtomicsWaitAsync,
  Int32Array,
  Number,
  SafeMap,
  globalThis,
} = primordials;

const {
  SharedArrayBuffer,
} = globalThis;

const {
  isMainThread,
  threadId,
} = internalBinding('worker');

const {
  codes: {
    ERR_WORKER_INVALID_ID,
    ERR_WORKER_SAME_THREAD,
  },
} = require('internal/errors');

const { MessageChannel } = require('internal/worker/io');

const { validateNumber } = require('internal/validators');

const messageTypes = {
  REGISTER_MAIN_THREAD_PORT: 'registerMainThreadPort',
  UNREGISTER_MAIN_THREAD_PORT: 'unregisterMainThreadPort',
  SEND_MESSAGE_TO_WORKER: 'sendMessageToWorker',
  RECEIVE_MESSAGE_FROM_WORKER: 'receiveMessageFromWorker',
};

// This is only populated by main thread and always empty in other threads
const threadsPorts = new SafeMap();

// This is only populated in child threads and always undefined in main thread
let mainThreadPort;

// SharedArrayBuffer must always be Int32, so it's * 4.
// We need one for the operation status (performing / performed) and one for the result (success / failure).
const WORKER_MESSAGING_SHARED_DATA = 2 * 4;
const WORKER_MESSAGING_STATUS_INDEX = 0;
const WORKER_MESSAGING_RESULT_INDEX = 1;

// This event handler is always executed on the main thread only
function handleMessageFromThread(message) {
  switch (message.type) {
    case messageTypes.REGISTER_MAIN_THREAD_PORT:
      {
        const { thread, port } = message;

        // Register the port
        threadsPorts.set(thread, port);

        // Handle messages on this port
        // When a new thread wants to register a children
        // this take care of doing that.
        // This way any thread can be linked to the main one.
        port.on('message', handleMessageFromThread);

        // Never block the thread on this port
        port.unref();
      }

      break;
    case messageTypes.UNREGISTER_MAIN_THREAD_PORT:
      threadsPorts.get(message.thread).close();
      threadsPorts.delete(message.thread);
      break;
    case messageTypes.SEND_MESSAGE_TO_WORKER:
      {
        // Send the message to the target thread
        const { source, destination, value, transferList, memory } = message;
        sendMessageToWorker(source, destination, value, transferList, memory);
      }
      break;
  }
}

function handleMessageFromMainThread(message) {
  switch (message.type) {
    case messageTypes.RECEIVE_MESSAGE_FROM_WORKER:
      receiveMessageFromWorker(message.source, message.value, message.memory);
      break;
  }
}

function sendMessageToWorker(source, destination, value, transferList, memory) {
  // We are on the main thread, we can directly process the message
  if (destination === 0) {
    receiveMessageFromWorker(source, value, memory);
    return;
  }

  // Search the port to the target thread
  const port = threadsPorts.get(destination);

  if (!port) {
    const status = new Int32Array(memory);
    AtomicsStore(status, WORKER_MESSAGING_RESULT_INDEX, 0);
    AtomicsStore(status, WORKER_MESSAGING_STATUS_INDEX, 1);
    AtomicsNotify(status, WORKER_MESSAGING_STATUS_INDEX, 1);
    return;
  }

  port.postMessage(
    {
      type: messageTypes.RECEIVE_MESSAGE_FROM_WORKER,
      source,
      destination,
      value,
      memory,
    },
    transferList,
  );
}

function receiveMessageFromWorker(source, value, memory) {
  const status = new Int32Array(memory);
  const deliverable = Number(process.listenerCount('workerMessage') > 0);

  // If somebody is listening, emit the message
  if (deliverable) {
    process.emit('workerMessage', value, source);
  }

  // Populate the result
  AtomicsStore(status, WORKER_MESSAGING_RESULT_INDEX, deliverable);
  AtomicsStore(status, WORKER_MESSAGING_STATUS_INDEX, 1);
  AtomicsNotify(status, WORKER_MESSAGING_STATUS_INDEX, 1);
}

function createMainThreadPort(thread) {
  // Create a channel that links the new thread to the main thread
  const {
    port1: mainThreadPortToMain,
    port2: mainThreadPortToThread,
  } = new MessageChannel();

  const registrationMessage = {
    type: messageTypes.REGISTER_MAIN_THREAD_PORT,
    thread,
    port: mainThreadPortToMain,
  };

  if (isMainThread) {
    handleMessageFromThread(registrationMessage);
  } else {
    mainThreadPort.postMessage(registrationMessage, [mainThreadPortToMain]);
  }

  return mainThreadPortToThread;
}

function destroyMainThreadPort(thread) {
  const unregistrationMessage = {
    type: messageTypes.UNREGISTER_MAIN_THREAD_PORT,
    thread,
  };

  if (isMainThread) {
    handleMessageFromThread(unregistrationMessage);
  } else {
    mainThreadPort.postMessage(unregistrationMessage);
  }
}

function setupMainThreadPort(port) {
  mainThreadPort = port;
  mainThreadPort.on('message', handleMessageFromMainThread);

  // Never block the process on this port
  mainThreadPort.unref();
}

async function postMessageToWorker(destination, value, transferList, timeout) {
  if (typeof transferList === 'number' && typeof timeout === 'undefined') {
    timeout = transferList;
    transferList = [];
  }

  if (typeof timeout !== 'undefined') {
    validateNumber(timeout, 'timeout', 0);
  }

  if (destination === threadId) {
    throw new ERR_WORKER_SAME_THREAD();
  }

  const memory = new SharedArrayBuffer(WORKER_MESSAGING_SHARED_DATA);
  const status = new Int32Array(memory);
  const promise = AtomicsWaitAsync(status, WORKER_MESSAGING_STATUS_INDEX, 0, timeout).value;

  const message = {
    type: messageTypes.SEND_MESSAGE_TO_WORKER,
    source: threadId,
    destination,
    value,
    memory,
    transferList,
  };

  if (isMainThread) {
    handleMessageFromThread(message);
  } else {
    mainThreadPort.postMessage(message, transferList);
  }

  // Wait for the response
  const response = await promise;

  if (response === 'timed-out' || status[WORKER_MESSAGING_RESULT_INDEX] === 0) {
    throw new ERR_WORKER_INVALID_ID(destination);
  }
}

module.exports = {
  createMainThreadPort,
  destroyMainThreadPort,
  setupMainThreadPort,
  postMessageToWorker,
};
