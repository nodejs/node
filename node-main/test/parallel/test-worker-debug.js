'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const EventEmitter = require('events');
const { Session } = require('inspector');
const {
  Worker, isMainThread, parentPort, workerData
} = require('worker_threads');


const workerMessage = 'This is a message from a worker';

function waitForMessage() {
  return new Promise((resolve) => {
    parentPort.once('message', resolve);
  });
}

// This is at the top so line numbers change less often
if (!isMainThread) {
  if (workerData === 1) {
    console.log(workerMessage);
    debugger;  // eslint-disable-line no-debugger
  } else if (workerData === 2) {
    parentPort.postMessage('running');
    waitForMessage();
  }
  return;
}

function doPost(session, method, params) {
  return new Promise((resolve, reject) => {
    session.post(method, params, (error, result) => {
      if (error)
        reject(JSON.stringify(error));
      else
        resolve(result);
    });
  });
}

function waitForEvent(emitter, event) {
  return new Promise((resolve) => emitter.once(event, resolve));
}

function waitForWorkerAttach(session) {
  return waitForEvent(session, 'NodeWorker.attachedToWorker')
      .then(({ params }) => params);
}

async function waitForWorkerDetach(session, id) {
  let sessionId;
  do {
    const { params } =
        await waitForEvent(session, 'NodeWorker.detachedFromWorker');
    sessionId = params.sessionId;
  } while (sessionId !== id);
}

function runWorker(id, workerCallback = () => {}) {
  return new Promise((resolve, reject) => {
    const worker = new Worker(__filename, { workerData: id });
    workerCallback(worker);
    worker.on('error', reject);
    worker.on('exit', resolve);
  });
}

class WorkerSession extends EventEmitter {
  constructor(parentSession, id) {
    super();
    this._parentSession = parentSession;
    this._id = id;
    this._requestCallbacks = new Map();
    this._nextCommandId = 1;
    this._parentSession.on('NodeWorker.receivedMessageFromWorker',
                           ({ params }) => {
                             if (params.sessionId === this._id)
                               this._processMessage(JSON.parse(params.message));
                           });
  }

  _processMessage(message) {
    if (message.id === undefined) {
      // console.log(JSON.stringify(message));
      this.emit('inspectorNotification', message);
      this.emit(message.method, message);
      return;
    }
    if (!this._requestCallbacks.has(message.id))
      return;
    const [ resolve, reject ] = this._requestCallbacks.get(message.id);
    this._requestCallbacks.delete(message.id);
    if (message.error)
      reject(new Error(message.error.message));
    else
      resolve(message.result);
  }

  async waitForBreakAfterCommand(command, script, line) {
    const notificationPromise = waitForEvent(this, 'Debugger.paused');
    this.post(command);
    const notification = await notificationPromise;
    const callFrame = notification.params.callFrames[0];
    assert.strictEqual(callFrame.location.lineNumber, line);
  }

  post(method, parameters) {
    const msg = {
      id: this._nextCommandId++,
      method
    };
    if (parameters)
      msg.params = parameters;

    return new Promise((resolve, reject) => {
      this._requestCallbacks.set(msg.id, [resolve, reject]);
      this._parentSession.post('NodeWorker.sendMessageToWorker', {
        sessionId: this._id, message: JSON.stringify(msg)
      });
    });
  }
}

async function testBasicWorkerDebug(session, post) {
  // 1. Do 'enable' with waitForDebuggerOnStart = true
  // 2. Run worker. It should break on start.
  // 3. Enable Runtime (to get console message) and Debugger. Resume.
  // 4. Breaks on the 'debugger' statement. Resume.
  // 5. Console message received, worker runs to a completion.
  // 6. contextCreated/contextDestroyed had been properly dispatched
  console.log('Test basic debug scenario');
  await post('NodeWorker.enable', { waitForDebuggerOnStart: true });
  const attached = waitForWorkerAttach(session);
  const worker = runWorker(1);
  const { sessionId, waitingForDebugger } = await attached;
  assert.strictEqual(waitingForDebugger, true);
  const detached = waitForWorkerDetach(session, sessionId);
  const workerSession = new WorkerSession(session, sessionId);
  const contextEventPromises = Promise.all([
    waitForEvent(workerSession, 'Runtime.executionContextCreated'),
    waitForEvent(workerSession, 'Runtime.executionContextDestroyed'),
  ]);
  const consolePromise = waitForEvent(workerSession, 'Runtime.consoleAPICalled')
      .then((notification) => notification.params.args[0].value);
  await workerSession.post('Debugger.enable');
  await workerSession.post('Runtime.enable');
  await workerSession.waitForBreakAfterCommand(
    'Runtime.runIfWaitingForDebugger', __filename, 1);
  await workerSession.waitForBreakAfterCommand(
    'Debugger.resume', __filename, 25);  // V8 line number is zero-based
  const msg = await consolePromise;
  assert.strictEqual(msg, workerMessage);
  workerSession.post('Debugger.resume');
  await Promise.all([worker, detached, contextEventPromises]);
}

async function testNoWaitOnStart(session, post) {
  console.log('Test disabled waitForDebuggerOnStart');
  await post('NodeWorker.enable', { waitForDebuggerOnStart: false });
  let worker;
  const promise = waitForWorkerAttach(session);
  const exitPromise = runWorker(2, (w) => { worker = w; });
  const { waitingForDebugger } = await promise;
  assert.strictEqual(waitingForDebugger, false);
  worker.postMessage('resume');
  await exitPromise;
}

async function testTwoWorkers(session, post) {
  console.log('Test attach to a running worker and then start a new one');
  await post('NodeWorker.disable');
  let okToAttach = false;
  const worker1attached = waitForWorkerAttach(session).then((notification) => {
    assert.strictEqual(okToAttach, true);
    return notification;
  });

  let worker1Exited;
  const worker = await new Promise((resolve, reject) => {
    worker1Exited = runWorker(2, resolve);
  }).then((worker) => new Promise(
    (resolve) => worker.once('message', () => resolve(worker))));
  okToAttach = true;
  await post('NodeWorker.enable', { waitForDebuggerOnStart: true });
  const { waitingForDebugger: worker1Waiting } = await worker1attached;
  assert.strictEqual(worker1Waiting, false);

  const worker2Attached = waitForWorkerAttach(session);
  let worker2Done = false;
  const worker2Exited = runWorker(1)
      .then(() => assert.strictEqual(worker2Done, true));
  const worker2AttachInfo = await worker2Attached;
  assert.strictEqual(worker2AttachInfo.waitingForDebugger, true);
  worker2Done = true;

  const workerSession = new WorkerSession(session, worker2AttachInfo.sessionId);
  workerSession.post('Runtime.runIfWaitingForDebugger');
  worker.postMessage('resume');
  await Promise.all([worker1Exited, worker2Exited]);
}

async function testWaitForDisconnectInWorker(session, post) {
  console.log('Test NodeRuntime.waitForDisconnect in worker');

  const sessionWithoutWaiting = new Session();
  sessionWithoutWaiting.connect();
  const sessionWithoutWaitingPost = doPost.bind(null, sessionWithoutWaiting);

  await sessionWithoutWaitingPost('NodeWorker.enable', {
    waitForDebuggerOnStart: true
  });
  await post('NodeWorker.enable', { waitForDebuggerOnStart: true });

  const attached = [
    waitForWorkerAttach(session),
    waitForWorkerAttach(sessionWithoutWaiting),
  ];

  let worker = null;
  const exitPromise = runWorker(2, (w) => worker = w);

  const [{ sessionId: sessionId1 }, { sessionId: sessionId2 }] =
      await Promise.all(attached);

  const workerSession1 = new WorkerSession(session, sessionId1);
  const workerSession2 = new WorkerSession(sessionWithoutWaiting, sessionId2);

  await workerSession2.post('Runtime.enable');
  await workerSession1.post('Runtime.enable');
  await workerSession1.post('NodeRuntime.notifyWhenWaitingForDisconnect', {
    enabled: true
  });
  await workerSession1.post('Runtime.runIfWaitingForDebugger');

  // Create the promises before sending the exit message to the Worker in order
  // to avoid race conditions.
  const disconnectPromise =
    waitForEvent(workerSession1, 'NodeRuntime.waitingForDisconnect');
  const executionContextDestroyedPromise =
    waitForEvent(workerSession2, 'Runtime.executionContextDestroyed');
  worker.postMessage('resume');

  await disconnectPromise;
  post('NodeWorker.detach', { sessionId: sessionId1 });
  await executionContextDestroyedPromise;

  await exitPromise;

  await post('NodeWorker.disable');
  await sessionWithoutWaitingPost('NodeWorker.disable');
  sessionWithoutWaiting.disconnect();
}

(async function test() {
  const session = new Session();
  session.connect();
  const post = doPost.bind(null, session);

  await testBasicWorkerDebug(session, post);

  console.log('Test disabling attach to workers');
  await post('NodeWorker.disable');
  await runWorker(1);

  await testNoWaitOnStart(session, post);

  await testTwoWorkers(session, post);

  await testWaitForDisconnectInWorker(session, post);

  session.disconnect();
  console.log('Test done');
})().then(common.mustCall()).catch((err) => {
  console.error(err);
  process.exitCode = 1;
});
