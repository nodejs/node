'use strict';

const installEventFired = new Promise(resolve => {
  self.fireInstallEvent = resolve;
});

const installFinished = new Promise(resolve => {
  self.finishInstall = resolve;
});

addEventListener('install', event => {
  fireInstallEvent();
  event.waitUntil(installFinished);
});

addEventListener('message', event => {
  let resolveWaitUntil;
  event.waitUntil(new Promise(resolve => { resolveWaitUntil = resolve; }));

  // Use a dedicated MessageChannel for every request so senders can wait for
  // individual requests to finish, and concurrent requests (to different
  // workers) don't cause race conditions.
  const port = event.data;
  port.onmessage = (event) => {
    switch (event.data) {
      case 'awaitInstallEvent':
        installEventFired.then(() => {
            port.postMessage('installEventFired');
        }).finally(resolveWaitUntil);
        break;

      case 'finishInstall':
        installFinished.then(() => {
            port.postMessage('installFinished');
        }).finally(resolveWaitUntil);
        finishInstall();
        break;

      case 'callUpdate': {
        const channel = new MessageChannel();
        registration.update().then(() => {
            channel.port2.postMessage({
                success: true,
            });
        }).catch((exception) => {
            channel.port2.postMessage({
                success: false,
                exception: exception.name,
            });
        }).finally(resolveWaitUntil);
        port.postMessage(channel.port1, [channel.port1]);
        break;
      }

      default:
        port.postMessage('Unexpected command ' + event.data);
        resolveWaitUntil();
        break;
    }
  };
});
