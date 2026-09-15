onmessage = e => Atomics.store(e.data, 0, 1);
postMessage('ready');
