setTimeout(() => {
    navigator.serviceWorker.onmessage = e => self.postMessage(e.data);
}, 500);
setTimeout(() => {
    self.postMessage("No message received");
}, 5000);
