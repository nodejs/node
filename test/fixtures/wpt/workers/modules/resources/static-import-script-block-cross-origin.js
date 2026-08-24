import * as module from './export-block-cross-origin.js';

if ('DedicatedWorkerGlobalScope' in self &&
    self instanceof DedicatedWorkerGlobalScope) {
  self.onmessage = e => {
    e.target.postMessage(module.importedModules);
  };
} else if (
    'SharedWorkerGlobalScope' in self &&
    self instanceof SharedWorkerGlobalScope) {
  self.onconnect = e => {
    e.ports[0].postMessage(module.importedModules);
  };
}
