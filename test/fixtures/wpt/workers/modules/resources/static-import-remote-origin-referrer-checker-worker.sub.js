// Import a remote origin script.
import * as module from 'https://{{hosts[alt][]}}:{{ports[https][0]}}/workers/modules/resources/export-referrer-checker.py';
if ('DedicatedWorkerGlobalScope' in self &&
    self instanceof DedicatedWorkerGlobalScope) {
  postMessage(module.referrer);
} else if (
    'SharedWorkerGlobalScope' in self &&
    self instanceof SharedWorkerGlobalScope) {
  onconnect = e => {
    e.ports[0].postMessage(module.referrer);
  };
}
