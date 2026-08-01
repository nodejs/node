onconnect = function (e) {
  setTimeout(function() { e.ports[0].postMessage(''); }, 250);
  y(); // will "report the error"
  // onerror is null so it'll be "not handled", and the error should be
  // reported to the user, although we don't test that here
  // make sure we don't fire an error event on the message port or the
  // SharedWorker object
}
