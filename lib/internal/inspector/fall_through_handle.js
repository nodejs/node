'use strict';
const { addFallThroughListener } = internalBinding('inspector');
const { InternalWorker } = require('internal/worker');

function spawnLoadNetworkResourceWorker() {
  const worker = new InternalWorker('internal/inspector/load_network_resource_worker', {
    stderr: false,
    stdin: false,
    stdout: false,
    trackUnmanagedFds: false,
  });

  worker.unref();

  addFallThroughListener((sessionId, callId, method, message) => {
    if (method === 'Network.loadNetworkResource') {
      worker.postMessage({
        sessionId,
        callId,
        method,
        message,
      });
    }
  });
}

module.exports = {
  spawnLoadNetworkResourceWorker,
};
