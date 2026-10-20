const worker2 = new Worker("cache-api-nested-worker2.js");
worker2.onmessage = e => self.postMessage(e.data);

