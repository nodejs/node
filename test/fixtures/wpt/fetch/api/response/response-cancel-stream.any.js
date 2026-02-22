// META: global=window,worker
// META: title=Response consume blob and http bodies
// META: script=../resources/utils.js

promise_test(function(test) {
    return new Response(new Blob([], { "type" : "text/plain" })).body.cancel();
}, "Cancelling a starting blob Response stream");

promise_test(function(test) {
    var response = new Response(new Blob(["This is data"], { "type" : "text/plain" }));
    var reader = response.body.getReader();
    reader.read();
    return reader.cancel();
}, "Cancelling a loading blob Response stream");

promise_test(function(test) {
    var response = new Response(new Blob(["T"], { "type" : "text/plain" }));
    var reader = response.body.getReader();

    var closedPromise = reader.closed.then(function() {
        return reader.cancel();
    });
    reader.read().then(function readMore({done, value}) {
      if (!done) return reader.read().then(readMore);
    });
    return closedPromise;
}, "Cancelling a closed blob Response stream");

promise_test(function(test) {
    return fetch(RESOURCES_DIR + "trickle.py?ms=30&count=100").then(function(response) {
        return response.body.cancel();
    });
}, "Cancelling a starting Response stream");

promise_test(function() {
    return fetch(RESOURCES_DIR + "trickle.py?ms=30&count=100").then(function(response) {
        var reader = response.body.getReader();
        return reader.read().then(function() {
            return reader.cancel();
        });
    });
}, "Cancelling a loading Response stream");

promise_test(function() {
    async function readAll(reader) {
      while (true) {
        const {value, done} = await reader.read();
        if (done)
          return;
      }
    }

    return fetch(RESOURCES_DIR + "top.txt").then(function(response) {
        var reader = response.body.getReader();
        return readAll(reader).then(() => reader.cancel());
    });
}, "Cancelling a closed Response stream");

promise_test(async () => {
    const response = await fetch(RESOURCES_DIR + "top.txt");
    const { body } = response;
    await body.cancel();
    assert_equals(body, response.body, ".body should not change after cancellation");
}, "Accessing .body after canceling it");
