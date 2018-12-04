// META: title=Encoding API: invalid label
// META: timeout=long
// META: script=resources/encodings.js

var tests = ["invalid-invalidLabel"];
setup(function() {
  encodings_table.forEach(function(section) {
    section.encodings.forEach(function(encoding) {
      encoding.labels.forEach(function(label) {
        ["\u0000", "\u000b", "\u00a0", "\u2028", "\u2029"].forEach(function(ws) {
          tests.push(ws + label);
          tests.push(label + ws);
          tests.push(ws + label + ws);
        });
      });
    });
  });
});

tests.forEach(function(input) {
  test(function() {
    assert_throws(new RangeError(), function() { new TextDecoder(input); });
  }, 'Invalid label ' + format_value(input) + ' should be rejected by TextDecoder.');
});
