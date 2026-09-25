const childWorkerScript = `
  self.onmessage = async (e) => {
    const response = await fetch(e.data);
    const text = await response.text();
    self.postMessage(text);
  };
`;
const blob = new Blob([childWorkerScript], { type: 'text/javascript' });
const blobUrl = URL.createObjectURL(blob);
const childWorker = new Worker(blobUrl);

// When a message comes from the parent frame, sends a resource url to the child
// worker.
self.onmessage = (e) => {
  childWorker.postMessage(e.data);
};

// When a message comes from the child worker, sends a content of fetch() to the
// parent frame.
childWorker.onmessage = (e) => {
  self.postMessage(e.data);
};
