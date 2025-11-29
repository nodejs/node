importScripts("/resources/testharness.js");

async_test(function() {
  var file = new File(["bits"], "dummy", { 'type': 'text/plain', lastModified: 42 });
  var reader = new FileReader();
  reader.onload = this.step_func_done(function() {
    assert_equals(file.name, "dummy", "file name");
    assert_equals(reader.result, "bits", "file content");
    assert_equals(file.lastModified, 42, "file lastModified");
  });
  reader.onerror = this.unreached_func("Unexpected error event");
  reader.readAsText(file);
}, "FileReader in Worker");

done();
