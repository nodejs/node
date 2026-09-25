const isSharedWorker =
  "SharedWorkerGlobalScope" in self && self instanceof SharedWorkerGlobalScope;

function setMessageHandler(response) {
  onmessage = e => {
    e.ports[0].postMessage(response);
  };

  if (isSharedWorker) {
    onconnect = e => {
      e.ports[0].onmessage = onmessage;
    };
  }
}

setMessageHandler("handler-before-throw");

throw new Error("uncaught-exception");

// This should never be called because of the uncaught exception above.
setMessageHandler("handler-after-throw");
