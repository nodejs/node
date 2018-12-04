// META: title=Encoding API: replacement encoding
// META: script=resources/encodings.js

encodings_table.forEach(function(section) {
    section.encodings.filter(function(encoding) {
        return encoding.name === 'replacement';
    }).forEach(function(encoding) {
        encoding.labels.forEach(function(label) {
            test(function() {
                assert_throws(new RangeError(), function() { new TextDecoder(label); });
            }, 'Label for "replacement" should be rejected by API: ' + label);
        });
    });
});

