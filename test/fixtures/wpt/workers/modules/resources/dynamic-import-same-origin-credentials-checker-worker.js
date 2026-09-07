const import_url = './export-credentials.py';
if ('DedicatedWorkerGlobalScope' in self &&
    self instanceof DedicatedWorkerGlobalScope) {
  import(import_url)
      .then(module => postMessage(module.cookie));
} else if (
    'SharedWorkerGlobalScope' in self &&
    self instanceof SharedWorkerGlobalScope) {
  onconnect = e => {
    import(import_url)
        .then(module => e.ports[0].postMessage(module.cookie));
  };
}
