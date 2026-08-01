var port;
var timeout;
onerror = function(a,b,c,d,e) {
  // will return undefined, thus the error is "not handled"
  // so error should be reported to the user, but this test doesn't check
  // that.
  // just make sure that this method is invoked with five arguments
  clearTimeout(timeout);
  var log = '';
  if (arguments.length != 5)
    log += 'got ' + arguments.length + ' arguments, expected 5. ';
  if (typeof a != 'string')
    log += 'first argument wasn\'t a string. ';
  if (b != location.href)
    log += 'second argument was ' + b + ', expected ' + location.href + '. ';
  if (typeof c != 'number')
    log += 'third argument wasn\'t a number. ';
  if (typeof d != 'number')
    log += 'fourth argument wasn\'t a number. ';
  if (e != 42)
    log += 'fifth argument wasn\'t the thrown exception. ';
  port.postMessage(log);
}
onconnect = function (e) {
  port = e.ports[0];
  timeout = setTimeout(function() { port.postMessage('self.onerror was not invoked'); }, 250);
  throw 42; // will "report the error"
}
