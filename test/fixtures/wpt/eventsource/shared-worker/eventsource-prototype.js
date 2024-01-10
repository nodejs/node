onconnect = function(e) {
try {
  var port = e.ports[0]
  EventSource.prototype.ReturnTrue = function() { return true }
  var source = new EventSource("../resources/message.py")
  port.postMessage([true, source.ReturnTrue(), 'EventSource' in self])
  source.close()
} catch(e) {
  port.postMessage([false, String(e)])
}
}