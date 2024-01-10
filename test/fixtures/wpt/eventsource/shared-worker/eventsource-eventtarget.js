onconnect = function(e) {
try {
  var port = e.ports[0]
  var source = new EventSource("../resources/message.py")
  source.addEventListener("message", listener, false)
  function listener(e) {
    port.postMessage([true, e.data])
    this.close()
  }
} catch(e) {
  port.postMessage([false, String(e)])
}
}