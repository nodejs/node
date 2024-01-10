onconnect = function(e) {
try {
  var port = e.ports[0]
  var source = new EventSource("http://this is invalid/")
  port.postMessage([false, 'no exception thrown'])
  source.close()
} catch(e) {
  port.postMessage([true, e.code])
}
}