// Import a remote origin script.
const importUrl =
    'https://{{domains[www1]}}:{{ports[https][0]}}/workers/modules/resources/export-on-load-script.js';
if ('DedicatedWorkerGlobalScope' in self &&
    self instanceof DedicatedWorkerGlobalScope) {
  import(importUrl)
      .then(module => postMessage(module.importedModules))
      .catch(e => postMessage(['ERROR']));
} else if (
    'SharedWorkerGlobalScope' in self &&
    self instanceof SharedWorkerGlobalScope) {
  onconnect = e => {
    import(importUrl)
        .then(module => e.ports[0].postMessage(module.importedModules))
        .catch(error => e.ports[0].postMessage(['ERROR']));
  };
}
