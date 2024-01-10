try {
  var source = new EventSource("http://this is invalid/")
  postMessage([false, 'no exception thrown'])
  source.close()
} catch(e) {
  postMessage([true, e.code])
}