try {
  var source = new EventSource("../resources/message.py")
  source.onopen = function(e) {
    postMessage([true, source.readyState, 'data' in e, e.bubbles, e.cancelable])
    this.close()
  }
} catch(e) {
  postMessage([false, String(e)])
}