// META: title=Encoding API: Legacy encodings
// META: script=resources/encodings.js

encodings_table.forEach(function(section) {
    section.encodings.forEach(function(encoding) {
        if (encoding.name !== 'replacement') {
            test(function() {
                assert_equals(new TextDecoder(encoding.name).encoding, encoding.name.toLowerCase()); // ASCII names only, so safe
            }, 'Encoding argument supported for decode: ' + encoding.name);
        }

        test(function() {
            assert_equals(new TextEncoder(encoding.name).encoding, 'utf-8');
        }, 'Encoding argument not considered for encode: ' + encoding.name);
    });
});
