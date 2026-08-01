self.onmessage = function(e) {
  var port = e.data.port;
  var options = e.data.options;

  e.waitUntil(self.clients.matchAll(options)
    .then(function(clients) {
        var message = [];
        clients.forEach(function(client) {
            var frame_type = client.frameType;
            if (client.url.indexOf('clients-matchall-include-uncontrolled.https.html') > -1 &&
                client.frameType == 'auxiliary') {
              // The test tab might be opened using window.open() by the test framework.
              // In that case, just pretend it's top-level!
              frame_type = 'top-level';
            }
            if (e.data.includeLifecycleState) {
              message.push({visibilityState: client.visibilityState,
                            focused: client.focused,
                            url: client.url,
                            lifecycleState: client.lifecycleState,
                            type: client.type,
                            frameType: frame_type});
            } else {
              message.push([client.visibilityState,
                            client.focused,
                            client.url,
                            client.type,
                            frame_type]);
            }
          });
        // Sort by url
        if (!e.data.disableSort) {
          message.sort(function(a, b) { return a[2] > b[2] ? 1 : -1; });
        }
        port.postMessage(message);
      })
    .catch(e => {
        port.postMessage('clients.matchAll() rejected: ' + e);
      }));
};
