const sourcePromise = new Promise(resolve => {
  if ('DedicatedWorkerGlobalScope' in self &&
      self instanceof DedicatedWorkerGlobalScope) {
    self.onmessage = e => {
      resolve(e.target);
    };
  } else if (
      'SharedWorkerGlobalScope' in self &&
      self instanceof SharedWorkerGlobalScope) {
    self.onconnect = e => {
      resolve(e.ports[0]);
    };
  }
});

const importedModulesPromise =
  import('./export-block-cross-origin.js')
    .then(module => module.importedModules)
    .catch(() => ['ERROR']);

Promise.all([sourcePromise, importedModulesPromise]).then(results => {
  const [source, importedModules] = results;
  source.postMessage(importedModules);
});
