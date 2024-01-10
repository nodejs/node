importScripts("/resources/testharness.js");

async_test(function() {
  var source = new EventSource("../resources/message.py")
  source.addEventListener("message", this.step_func_done(function(e) {
    assert_equals(e.data, 'data');
    source.close();
  }), false)
}, "dedicated worker - EventSource: addEventListener()");

done();
