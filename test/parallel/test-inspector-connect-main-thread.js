'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { Session } = require('inspector');
const path = require('path');
const { pathToFileURL } = require('url');
const { isMainThread, parentPort, Worker, workerData } =
    require('worker_threads');

if (!workerData) {
  common.skipIfWorker();
}

function toDebug() {
  let a = 1;
  a = a + 1;
  return a * 200;
}

const messagesSent = [];

async function post(session, method, params) {
  return new Promise((resolve, reject) => {
    session.post(method, params, (error, success) => {
      messagesSent.push(method);
      if (error) {
        process._rawDebug(`Message ${method} produced an error`);
        reject(error);
      } else {
        process._rawDebug(`Message ${method} was sent`);
        resolve(success);
      }
    });
  });
}

async function waitForNotification(session, notification) {
  return new Promise((resolve) => session.once(notification, resolve));
}

function startWorker(skipChild, sharedBuffer) {
  return new Promise((resolve) => {
    const worker = new Worker(__filename, {
      workerData: { skipChild, sharedBuffer }
    });
    worker.on('error', (e) => {
      console.error(e);
      throw e;
    });
    worker.once('message', (m) => {
      resolve(worker);
    });
  });
}

function waitForConsoleRequest(worker) {
  return new Promise((resolve) => {
    worker.on('message', ({ doConsoleLog }) => {
      if (doConsoleLog) {
        resolve();
      }
    });
  });
}

function waitForMessagesSent(worker) {
  return new Promise((resolve) => {
    worker.on('message', ({ messagesSent }) => {
      if (messagesSent) {
        resolve(messagesSent);
      }
    });
  });
}

function doConsoleLog(arrayBuffer) {
  console.log('Message for a test');
  arrayBuffer[0] = 128;
}

// This tests that inspector callbacks are called in a microtask
// and do not interrupt the main code. Interrupting the code flow
// can lead to unexpected behaviors.
async function ensureListenerDoesNotInterrupt(session) {
  const currentTime = Date.now();
  let consoleLogHappened = false;
  session.once('Runtime.consoleAPICalled',
               () => { consoleLogHappened = true; });
  const buf = new Uint8Array(workerData.sharedBuffer);
  parentPort.postMessage({ doConsoleLog: true });
  while (buf[0] === 1) {
    // Making sure the console.log was executed
  }
  while ((Date.now() - currentTime) < 50) {
    // Spin wait for 50ms, assume that was enough to get inspector message
  }
  assert.strictEqual(consoleLogHappened, false);
  await new Promise(queueMicrotask);
  assert.strictEqual(consoleLogHappened, true);
}

async function main() {
  const sharedBuffer = new SharedArrayBuffer(1);
  const arrayBuffer = new Uint8Array(sharedBuffer);
  arrayBuffer[0] = 1;
  const worker = await startWorker(false, sharedBuffer);
  waitForConsoleRequest(worker).then(doConsoleLog.bind(null, arrayBuffer));
  const workerDonePromise = waitForMessagesSent(worker);
  assert.strictEqual(toDebug(), 400);
  assert.deepStrictEqual(await workerDonePromise, [
    'Debugger.enable',
    'Runtime.enable',
    'Debugger.setBreakpointByUrl',
    'Debugger.evaluateOnCallFrame',
    'Debugger.resume'
  ]);
}

async function childMain() {
  // Ensures the worker does not terminate too soon
  parentPort.on('message', () => { });
  await waitForMessagesSent(await startWorker(true));
  const session = new Session();
  session.connectToMainThread();
  await post(session, 'Debugger.enable');
  await post(session, 'Runtime.enable');
  await post(session, 'Debugger.setBreakpointByUrl', {
    'lineNumber': 18,
    'url': pathToFileURL(path.resolve(__dirname, __filename)).toString(),
    'columnNumber': 0,
    'condition': ''
  });
  const pausedPromise = waitForNotification(session, 'Debugger.paused');
  parentPort.postMessage('Ready');
  const callFrameId = (await pausedPromise).params.callFrames[0].callFrameId;

  // Delay to ensure main thread is truly suspended
  await new Promise((resolve) => setTimeout(resolve, 50));

  const { result: { value } } =
      await post(session,
                 'Debugger.evaluateOnCallFrame',
                 { callFrameId, expression: 'a * 100' });
  assert.strictEqual(value, 100);
  await post(session, 'Debugger.resume');
  await ensureListenerDoesNotInterrupt(session);
  parentPort.postMessage({ messagesSent });
  parentPort.close();
  console.log('Worker is done');
}

async function skipChildMain() {
  // Ensures the worker does not terminate too soon
  parentPort.on('message', () => { });

  const session = new Session();
  session.connectToMainThread();
  const notifications = [];
  session.on('NodeWorker.attachedToWorker', (n) => notifications.push(n));
  await post(session, 'NodeWorker.enable', { waitForDebuggerOnStart: false });
  // 2 notifications mean there are 2 workers so we are connected to a main
  // thread
  assert.strictEqual(notifications.length, 2);
  parentPort.postMessage('Ready');
  parentPort.postMessage({ messagesSent });
  parentPort.close();
  console.log('Skip child is done');
}

if (isMainThread) {
  main().then(common.mustCall());
} else if (workerData.skipChild) {
  skipChildMain().then(common.mustCall());
} else {
  childMain().then(common.mustCall());
}
