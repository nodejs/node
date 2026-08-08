try {
  importScripts('empty-worker.js');
  if ('DedicatedWorkerGlobalScope' in self &&
      self instanceof DedicatedWorkerGlobalScope) {
    postMessage('LOADED');
  } else if (
      'SharedWorkerGlobalScope' in self &&
      self instanceof SharedWorkerGlobalScope) {
    onconnect = e => {
      e.ports[0].postMessage('LOADED');
    };
  }
} catch (e) {
  // Post a message instead of propagating an ErrorEvent to the page because
  // propagated event loses an error name.
  //
  // Step 1. "Let notHandled be the result of firing an event named error at the
  // Worker object associated with the worker, using ErrorEvent, with the
  // cancelable attribute initialized to true, the message, filename, lineno,
  // and colno attributes initialized appropriately, and the error attribute
  // initialized to null."
  // https://html.spec.whatwg.org/multipage/workers.html#runtime-script-errors-2
  if ('DedicatedWorkerGlobalScope' in self &&
      self instanceof DedicatedWorkerGlobalScope) {
    postMessage(e.name);
  } else if (
      'SharedWorkerGlobalScope' in self &&
      self instanceof SharedWorkerGlobalScope) {
    onconnect = connectEvent => {
      connectEvent.ports[0].postMessage(e.name);
    };
  }
}
