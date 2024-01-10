try {
  var url = decodeURIComponent(location.hash.substr(1))
  var source = new EventSource(url)
  source.onerror = function(e) {
    postMessage([true, this.readyState, 'data' in e])
    this.close();
  }
} catch(e) {
  postMessage([false, String(e)])
}