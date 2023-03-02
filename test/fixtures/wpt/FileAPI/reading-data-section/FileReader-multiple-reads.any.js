// META: title=FileReader: starting new reads while one is in progress

test(function() {
  var blob_1 = new Blob(['TEST000000001'])
  var blob_2 = new Blob(['TEST000000002'])
  var reader = new FileReader();
  reader.readAsText(blob_1)
  assert_equals(reader.readyState, FileReader.LOADING, "readyState Must be LOADING")
  assert_throws_dom("InvalidStateError", function () {
    reader.readAsText(blob_2)
  })
}, 'test FileReader InvalidStateError exception for readAsText');

test(function() {
  var blob_1 = new Blob(['TEST000000001'])
  var blob_2 = new Blob(['TEST000000002'])
  var reader = new FileReader();
  reader.readAsDataURL(blob_1)
  assert_equals(reader.readyState, FileReader.LOADING, "readyState Must be LOADING")
  assert_throws_dom("InvalidStateError", function () {
    reader.readAsDataURL(blob_2)
  })
}, 'test FileReader InvalidStateError exception for readAsDataURL');

test(function() {
  var blob_1 = new Blob(['TEST000000001'])
  var blob_2 = new Blob(['TEST000000002'])
  var reader = new FileReader();
  reader.readAsArrayBuffer(blob_1)
  assert_equals(reader.readyState, FileReader.LOADING, "readyState Must be LOADING")
  assert_throws_dom("InvalidStateError", function () {
    reader.readAsArrayBuffer(blob_2)
  })
}, 'test FileReader InvalidStateError exception for readAsArrayBuffer');

async_test(function() {
  var blob_1 = new Blob(['TEST000000001'])
  var blob_2 = new Blob(['TEST000000002'])
  var reader = new FileReader();
  var triggered = false;
  reader.onloadstart = this.step_func_done(function() {
    assert_false(triggered, "Only one loadstart event should be dispatched");
    triggered = true;
    assert_equals(reader.readyState, FileReader.LOADING,
                  "readyState must be LOADING")
    assert_throws_dom("InvalidStateError", function () {
      reader.readAsArrayBuffer(blob_2)
    })
  });
  reader.readAsArrayBuffer(blob_1)
  assert_equals(reader.readyState, FileReader.LOADING, "readyState Must be LOADING")
}, 'test FileReader InvalidStateError exception in onloadstart event for readAsArrayBuffer');

async_test(function() {
  var blob_1 = new Blob(['TEST000000001'])
  var blob_2 = new Blob(['TEST000000002'])
  var reader = new FileReader();
  reader.onloadend = this.step_func_done(function() {
    assert_equals(reader.readyState, FileReader.DONE,
                  "readyState must be DONE")
    reader.readAsArrayBuffer(blob_2)
    assert_equals(reader.readyState, FileReader.LOADING, "readyState Must be LOADING")
  });
  reader.readAsArrayBuffer(blob_1)
  assert_equals(reader.readyState, FileReader.LOADING, "readyState Must be LOADING")
}, 'test FileReader no InvalidStateError exception in loadend event handler for readAsArrayBuffer');

async_test(function() {
  var blob_1 = new Blob([new Uint8Array(0x414141)]);
  var blob_2 = new Blob(['TEST000000002']);
  var reader = new FileReader();
  reader.onloadstart = this.step_func(function() {
    reader.abort();
    reader.onloadstart = null;
    reader.onloadend = this.step_func_done(function() {
      assert_equals('TEST000000002', reader.result);
    });
    reader.readAsText(blob_2);
  });
  reader.readAsText(blob_1);
}, 'test abort and restart in onloadstart event for readAsText');
