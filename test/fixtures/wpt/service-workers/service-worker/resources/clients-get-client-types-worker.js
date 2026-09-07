onmessage = function(e) {
  if (e.data.cmd == 'GetClientId') {
    fetch('clientId')
        .then(function(response) {
          return response.text();
        })
        .then(function(text) {
          e.data.port.postMessage({clientId: text});
        });
  }
};
