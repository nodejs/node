async function messageClient(clientId, message) {
  const client = await clients.get(clientId);
  client.postMessage(message);
}

addEventListener('fetch', event => {
  let resolve;
  const promise = new Promise(r => resolve = r);

  function onAborted() {
     messageClient(event.clientId, event.request.signal.reason);
     resolve();
  }

  messageClient(event.clientId, 'fetch event has arrived');

  event.respondWith(promise.then(() => new Response('hello')));
  event.request.signal.addEventListener('abort', onAborted);
});
