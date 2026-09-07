self.onmessageerror = e => { e.source.postMessage("received error event"); };
self.onmessage = e => { e.source.postMessage("received message event"); };
