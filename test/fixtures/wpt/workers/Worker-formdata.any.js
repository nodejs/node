// META: global=dedicatedworker
test(() => {
  assert_own_property(self, 'FormData');
  assert_equals(FormData.length, 0);

  var formData = new FormData();
  assert_not_equals(formData, null);
  assert_own_property(FormData.prototype, 'append');
  formData.append('key', 'value');

  var blob = new Blob([]);
  assert_not_equals(blob, null);
  formData.append('key', blob);
  formData.append('key', blob, 'filename');

  assert_throws_dom("DataCloneError",
                    function() { postMessage(formData) },
                    "Trying to clone formdata inside a postMessage results in an exception." );
},'Test FormData interface object');
