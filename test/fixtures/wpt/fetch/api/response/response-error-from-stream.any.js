// META: global=window,worker
// META: title=Response Receives Propagated Error from ReadableStream

function newStreamWithStartError() {
  var err = new Error("Start error");
  return [new ReadableStream({
            start(controller) {
                controller.error(err);
            }
          }),
          err]
}

function newStreamWithPullError() {
  var err = new Error("Pull error");
  return [new ReadableStream({
            pull(controller) {
                controller.error(err);
            }
          }),
          err]
}

function runRequestPromiseTest([stream, err], responseReaderMethod, testDescription) {
  promise_test(test => {
    return promise_rejects_exactly(
      test,
      err,
      new Response(stream)[responseReaderMethod](),
      'CustomTestError should propagate'
    )
  }, testDescription)
}


promise_test(test => {
  var [stream, err] = newStreamWithStartError();
  return promise_rejects_exactly(test, err, stream.getReader().read(), 'CustomTestError should propagate')
}, "ReadableStreamDefaultReader Promise receives ReadableStream start() Error")

promise_test(test => {
  var [stream, err] = newStreamWithPullError();
  return promise_rejects_exactly(test, err, stream.getReader().read(), 'CustomTestError should propagate')
}, "ReadableStreamDefaultReader Promise receives ReadableStream pull() Error")


// test start() errors for all Body reader methods
runRequestPromiseTest(newStreamWithStartError(), 'arrayBuffer', 'ReadableStream start() Error propagates to Response.arrayBuffer() Promise');
runRequestPromiseTest(newStreamWithStartError(), 'blob',        'ReadableStream start() Error propagates to Response.blob() Promise');
runRequestPromiseTest(newStreamWithStartError(), 'bytes',       'ReadableStream start() Error propagates to Response.bytes() Promise');
runRequestPromiseTest(newStreamWithStartError(), 'formData',    'ReadableStream start() Error propagates to Response.formData() Promise');
runRequestPromiseTest(newStreamWithStartError(), 'json',        'ReadableStream start() Error propagates to Response.json() Promise');
runRequestPromiseTest(newStreamWithStartError(), 'text',        'ReadableStream start() Error propagates to Response.text() Promise');

// test pull() errors for all Body reader methods
runRequestPromiseTest(newStreamWithPullError(), 'arrayBuffer', 'ReadableStream pull() Error propagates to Response.arrayBuffer() Promise');
runRequestPromiseTest(newStreamWithPullError(), 'blob',        'ReadableStream pull() Error propagates to Response.blob() Promise');
runRequestPromiseTest(newStreamWithPullError(), 'bytes',       'ReadableStream pull() Error propagates to Response.bytes() Promise');
runRequestPromiseTest(newStreamWithPullError(), 'formData',    'ReadableStream pull() Error propagates to Response.formData() Promise');
runRequestPromiseTest(newStreamWithPullError(), 'json',        'ReadableStream pull() Error propagates to Response.json() Promise');
runRequestPromiseTest(newStreamWithPullError(), 'text',        'ReadableStream pull() Error propagates to Response.text() Promise');
