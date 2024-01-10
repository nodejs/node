onconnect = function(e) {
try {
  var port = e.ports[0]
  var url = decodeURIComponent(location.hash.substr(1))
  var source = new EventSource(url)
  source.onerror = function(e) {
    port.postMessage([true, this.readyState, 'data' in e])
    this.close();
  }
} catch(e) {
  port.postMessage([false, String(e)])
}
}