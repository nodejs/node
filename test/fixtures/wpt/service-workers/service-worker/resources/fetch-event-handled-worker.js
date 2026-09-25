// This worker reports back the final state of FetchEvent.handled (RESOLVED or
// REJECTED) to the test.

self.addEventListener('message', function(event) {
  self.port = event.data.port;
});

self.addEventListener('fetch', function(event) {
  try {
    event.handled.then(() => {
      self.port.postMessage('RESOLVED');
    }, () => {
      self.port.postMessage('REJECTED');
    });
  } catch (e) {
    self.port.postMessage('FAILED');
    return;
  }

  const search = new URL(event.request.url).search;
  switch (search) {
    case '?respondWith-not-called':
      break;
    case '?respondWith-not-called-and-event-canceled':
      event.preventDefault();
      break;
    case '?respondWith-called-and-promise-resolved':
      event.respondWith(Promise.resolve(new Response('body')));
      break;
    case '?respondWith-called-and-promise-resolved-to-invalid-response':
      event.respondWith(Promise.resolve('invalid response'));
      break;
    case '?respondWith-called-and-promise-rejected':
      event.respondWith(Promise.reject(new Error('respondWith rejected')));
      break;
  }
});
