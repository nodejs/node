// This is a service worker script used by the client-url-creation-url test.
// It exists only to look up the client URL of the test iframe and send it back
// to the test page.
addEventListener('message', message_event => {
  const port = message_event.data.port;

  const async_work = async () => {
    try {
      const clients = await self.clients.matchAll();

      // In our test there should be exactly one client that is our test
      // navigation iframe.
      if (clients.length == 1) {
        const client = clients[0];
        port.postMessage(client.url);
      } else {
        port.postMessage(`error: expected 1 client, not ${clients.length}`);
      }
    } catch (error) {
      port.postMessage(`error: ${error.message}`);
    }
  };
  message_event.waitUntil(async_work());
});