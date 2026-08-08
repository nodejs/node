self.caches.keys().then(() => postMessage('PASS'), () => postMessage('FAIL'));
