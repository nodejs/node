navigator.serviceWorker.onmessage = e => self.postMessage(e.data);
setTimeout(() => {
    self.postMessage("No message received");
}, 5000);
