// META: title=EventSource always UTF-8
async_test().step(function() {
  var source = new EventSource("resources/message.py?mime=text/event-stream%3bcharset=windows-1252&message=data%3Aok%E2%80%A6")
  source.onmessage = this.step_func(function(e) {
    assert_equals('ok…', e.data, 'decoded data')
    source.close()
    this.done()
  })
  source.onerror = this.step_func(function() {
    assert_unreached("Got error event")
  })
})
