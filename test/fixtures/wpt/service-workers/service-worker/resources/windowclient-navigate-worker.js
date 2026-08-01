importScripts('/resources/testharness.js');

function matchQuery(queryString) {
  return self.location.search.substr(1) === queryString;
}

async function navigateTest(t, e) {
  const port = e.data.port;
  const url = e.data.url;
  const expected = e.data.expected;

  let p = clients.matchAll({ includeUncontrolled : true })
    .then(function(clients) {
      for (const client of clients) {
        if (client.url === e.data.clientUrl) {
          assert_equals(client.frameType, e.data.frameType);
          return client.navigate(url);
        }
      }
      throw 'Could not locate window client.';
    }).then(function(newClient) {
      // If we didn't reject, we better get resolved with the right thing.
      if (newClient === null) {
        assert_equals(newClient, expected);
      } else {
        assert_equals(newClient.url, expected);
      }
    });

  if (typeof self[expected] === "function") {
    // It's a JS error type name.  We are expecting our promise to be rejected
    // with that error.
    p = promise_rejects_js(t, self[expected], p);
  }

  // Let our caller know we are done.
  return p.finally(() => port.postMessage(null));
}

function getTestClient() {
  return clients.matchAll({ includeUncontrolled: true })
    .then(function(clients) {
      for (const client of clients) {
        if (client.url.includes('windowclient-navigate.https.html')) {
          return client;
        }
      }

      throw new Error('Service worker was unable to locate test client.');
    });
}

function waitForMessage(client) {
  const channel = new MessageChannel();
  client.postMessage({ port: channel.port2 }, [channel.port2]);

  return new Promise(function(resolve) {
    channel.port1.onmessage = resolve;
  });
}

// The worker must remain in the "installing" state for the duration of some
// sub-tests. In order to achieve this coordination without relying on global
// state, the worker must create a message channel with the client from within
// the "install" event handler.
if (matchQuery('installing')) {
  self.addEventListener('install', function(e) {
    e.waitUntil(getTestClient().then(waitForMessage));
  });
}

self.addEventListener('message', function(e) {
  e.waitUntil(promise_test(t => navigateTest(t, e),
                           e.data.description + " worker side"));
});
