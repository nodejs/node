onconnect = function(e) {
try {
  var port = e.ports[0]
  var source = new EventSource("../resources/message.py")
  source.onopen = function(e) {
    this.close()
    port.postMessage([true, this.readyState])
  }
} catch(e) {
  port.postMessage([false, String(e)])
}
}