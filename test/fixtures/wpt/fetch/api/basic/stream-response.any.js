// META: global=window,worker
// META: script=../resources/utils.js

function streamBody(reader, test, count = 0) {
  return reader.read().then(function(data) {
    if (!data.done && count < 2) {
      count += 1;
      return streamBody(reader, test, count);
    } else {
      test.step(function() {
        assert_true(count >= 2, "Retrieve body progressively");
      });
    }
  });
}

//simulate streaming:
//count is large enough to let the UA deliver the body before it is completely retrieved
promise_test(function(test) {
  return fetch(RESOURCES_DIR + "trickle.py?ms=30&count=100").then(function(resp) {
    if (resp.body)
      return streamBody(resp.body.getReader(), test);
    else
      test.step(function() {
        assert_unreached( "Body does not exist in response");
      });
  });
}, "Stream response's body when content-type is present");

// This test makes sure that the response body is not buffered if no content type is provided.
promise_test(function(test) {
  return fetch(RESOURCES_DIR + "trickle.py?ms=300&count=10&notype=true").then(function(resp) {
    if (resp.body)
      return streamBody(resp.body.getReader(), test);
    else
      test.step(function() {
        assert_unreached( "Body does not exist in response");
      });
  });
}, "Stream response's body when content-type is not present");
