self.addEventListener('install', function(event) {
  event.waitUntil(new Promise(function(aRequest, aResponse) {
      throw new Error();
    }));
});
