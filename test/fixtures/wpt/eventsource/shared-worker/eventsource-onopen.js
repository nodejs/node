onconnect = function(e) {
try {
  var port = e.ports[0]
  var source = new EventSource("../resources/message.py")
  source.onopen = function(e) {
    port.postMessage([true, source.readyState, 'data' in e, e.bubbles, e.cancelable])
    this.close()
  }
} catch(e) {
  port.postMessage([false, String(e)])
}
}