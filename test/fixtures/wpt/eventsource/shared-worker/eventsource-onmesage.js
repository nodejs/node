onconnect = function(e) {
try {
  var port = e.ports[0]
  var source = new EventSource("../resources/message.py")
  source.onmessage = function(e) {
    port.postMessage([true, e.data])
    this.close()
  }
} catch(e) {
  port.postMessage([false, String(e)])
}
}