// This worker listens for commands from the page and messages back
// the result.

function handle(message) {
  const np = self.registration.navigationPreload;
  switch (message) {
    case 'getState':
      return np.getState();
    case 'enable':
      return np.enable();
    case 'disable':
      return np.disable();
    case 'setHeaderValue':
      return np.setHeaderValue('insightful');
  }
  return Promise.reject('bad message');
}

self.addEventListener('message', e => {
    e.waitUntil(handle(e.data).then(result => e.source.postMessage(result)));
  });
