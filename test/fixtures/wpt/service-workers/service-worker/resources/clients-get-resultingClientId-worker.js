let savedPort = null;
let savedResultingClientId = null;

async function getTestingPage() {
  const clientList = await self.clients.matchAll({ type: 'window', includeUncontrolled: true });
  for (let c of clientList) {
    if (c.url.endsWith('clients-get.https.html')) {
      c.focus();
      return c;
    }
  }
  return null;
}

async function destroyResultingClient(testingPage) {
  const destroyedPromise = new Promise(resolve => {
    self.addEventListener('message', e => {
      if (e.data.msg == 'resultingClientDestroyed') {
        resolve();
      }
    }, {once: true});
  });
  testingPage.postMessage({ msg: 'destroyResultingClient' });
  return destroyedPromise;
}

self.addEventListener('fetch', async (e) => {
  let { resultingClientId } = e;
  savedResultingClientId = resultingClientId;

  if (e.request.url.endsWith('simple.html?fail')) {
    e.waitUntil((async () => {
      const testingPage = await getTestingPage();
      await destroyResultingClient(testingPage);
      testingPage.postMessage({ msg: 'resultingClientDestroyedAck',
                                resultingDestroyedClientId: savedResultingClientId });
    })());
    return;
  }

  e.respondWith(fetch(e.request));
});

self.addEventListener('message', (e) => {
  let { msg, resultingClientId } = e.data;
  e.waitUntil((async () => {
    if (msg == 'getIsResultingClientUndefined') {
      const client = await self.clients.get(resultingClientId);
      let isUndefined = typeof client == 'undefined';
      e.source.postMessage({ msg: 'getIsResultingClientUndefined',
        isResultingClientUndefined: isUndefined });
      return;
    }
    if (msg == 'getResultingClientId') {
      e.source.postMessage({ msg: 'getResultingClientId',
                             resultingClientId: savedResultingClientId });
      return;
    }
  })());
});
