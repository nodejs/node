var port;
var timeout;
addEventListener('error', function(e) {
  // event is not canceled, thus the error is "not handled"
  // so error should be reported to the user, but this test doesn't check
  // that.
  // just make sure that this event has the right properties
  clearTimeout(timeout);
  var log = '';
  if (!self.ErrorEvent || Object.getPrototypeOf(e) != ErrorEvent.prototype)
    log += 'event should be an ErrorEvent. ';
  if (e.bubbles)
    log += 'event should not bubble. ';
  if (!e.cancelable)
    log += 'event should be cancelable. ';
  if (!e.isTrusted)
    log += 'event should be trusted. ';
  if (typeof e.message != 'string')
    log += 'message wasn\'t a string. ';
  if (e.filename != location.href)
    log += 'filename was ' + e.filename + ', expected ' + location.href + '. ';
  if (typeof e.lineno != 'number')
    log += 'lineno wasn\'t a number. ';
  if (typeof e.colno != 'number')
    log += 'colno argument wasn\'t a number. ';
  if (e.error != 42)
    log += 'fifth argument wasn\'t the thrown exception. ';
  port.postMessage(log);
}, false);
onconnect = function (e) {
  port = e.ports[0];
  timeout = setTimeout(function() { port.postMessage('No error event fired'); }, 250);
  throw 42; // will "report the error"
}
