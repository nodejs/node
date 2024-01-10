try {
  var source = new EventSource("../resources/message.py")
  postMessage([true, source.url])
  source.close()
} catch(e) {
  postMessage([false, String(e)])
}