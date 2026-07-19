const { Session } = require('inspector');
const { parentPort } = require('worker_threads');

const session = new Session();

parentPort.once('message', () => {}); // Prevent the worker from exiting.

session.connectToMainThread();

session.on(
  'NodeWorker.attachedToWorker',
  ({ params: { workerInfo } }) => {
    // send the worker title to the main thread
    parentPort.postMessage(workerInfo.title);
  }
);
session.post('NodeWorker.enable', { waitForDebuggerOnStart: false });
