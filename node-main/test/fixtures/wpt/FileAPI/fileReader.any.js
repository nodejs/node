// META: title=FileReader States

'use strict';

test(function () {
  assert_true(
    "FileReader" in globalThis,
    "globalThis should have a FileReader property.",
  );
}, "FileReader interface object");

test(function () {
  var fileReader = new FileReader();
  assert_true(fileReader instanceof FileReader);
}, "no-argument FileReader constructor");

var t_abort = async_test("FileReader States -- abort");
t_abort.step(function () {
  var fileReader = new FileReader();
  assert_equals(fileReader.readyState, 0);
  assert_equals(fileReader.readyState, FileReader.EMPTY);

  var blob = new Blob();
  fileReader.readAsArrayBuffer(blob);
  assert_equals(fileReader.readyState, 1);
  assert_equals(fileReader.readyState, FileReader.LOADING);

  fileReader.onabort = this.step_func(function (e) {
    assert_equals(fileReader.readyState, 2);
    assert_equals(fileReader.readyState, FileReader.DONE);
    t_abort.done();
  });
  fileReader.abort();
  fileReader.onabort = this.unreached_func("abort event should fire sync");
});

var t_event = async_test("FileReader States -- events");
t_event.step(function () {
  var fileReader = new FileReader();

  var blob = new Blob();
  fileReader.readAsArrayBuffer(blob);

  fileReader.onloadstart = this.step_func(function (e) {
    assert_equals(fileReader.readyState, 1);
    assert_equals(fileReader.readyState, FileReader.LOADING);
  });

  fileReader.onprogress = this.step_func(function (e) {
    assert_equals(fileReader.readyState, 1);
    assert_equals(fileReader.readyState, FileReader.LOADING);
  });

  fileReader.onloadend = this.step_func(function (e) {
    assert_equals(fileReader.readyState, 2);
    assert_equals(fileReader.readyState, FileReader.DONE);
    t_event.done();
  });
});
