onconnect = function(e) {
try {
  var port = e.ports[0]
  var source = new EventSource("../resources/message.py")
  port.postMessage([true, source.url])
  source.close()
} catch(e) {
  port.postMessage([false, String(e)])
}
}