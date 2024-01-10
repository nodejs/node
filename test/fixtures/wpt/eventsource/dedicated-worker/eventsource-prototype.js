try {
  EventSource.prototype.ReturnTrue = function() { return true }
  var source = new EventSource("../resources/message.py")
  postMessage([true, source.ReturnTrue(), 'EventSource' in self])
  source.close()
} catch(e) {
  postMessage([false, String(e)])
}