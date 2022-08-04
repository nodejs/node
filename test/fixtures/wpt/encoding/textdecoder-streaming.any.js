// META: title=Encoding API: Streaming decode
// META: global=window,worker
// META: script=resources/encodings.js
// META: script=/common/sab.js

var string = '\x00123ABCabc\x80\xFF\u0100\u1000\uFFFD\uD800\uDC00\uDBFF\uDFFF';
var octets = {
    'utf-8':    [0x00,0x31,0x32,0x33,0x41,0x42,0x43,0x61,0x62,0x63,0xc2,0x80,
                 0xc3,0xbf,0xc4,0x80,0xe1,0x80,0x80,0xef,0xbf,0xbd,0xf0,0x90,
                 0x80,0x80,0xf4,0x8f,0xbf,0xbf],
    'utf-16le': [0x00,0x00,0x31,0x00,0x32,0x00,0x33,0x00,0x41,0x00,0x42,0x00,
                 0x43,0x00,0x61,0x00,0x62,0x00,0x63,0x00,0x80,0x00,0xFF,0x00,
                 0x00,0x01,0x00,0x10,0xFD,0xFF,0x00,0xD8,0x00,0xDC,0xFF,0xDB,
                 0xFF,0xDF],
    'utf-16be': [0x00,0x00,0x00,0x31,0x00,0x32,0x00,0x33,0x00,0x41,0x00,0x42,
                 0x00,0x43,0x00,0x61,0x00,0x62,0x00,0x63,0x00,0x80,0x00,0xFF,
                 0x01,0x00,0x10,0x00,0xFF,0xFD,0xD8,0x00,0xDC,0x00,0xDB,0xFF,
                 0xDF,0xFF]
};

["ArrayBuffer", "SharedArrayBuffer"].forEach((arrayBufferOrSharedArrayBuffer) => {
    Object.keys(octets).forEach(function(encoding) {
        for (var len = 1; len <= 5; ++len) {
            test(function() {
                var encoded = octets[encoding];

                var out = '';
                var decoder = new TextDecoder(encoding);
                for (var i = 0; i < encoded.length; i += len) {
                    var sub = [];
                    for (var j = i; j < encoded.length && j < i + len; ++j) {
                        sub.push(encoded[j]);
                    }
                    var uintArray = new Uint8Array(createBuffer(arrayBufferOrSharedArrayBuffer, sub.length));
                    uintArray.set(sub);
                    out += decoder.decode(uintArray, {stream: true});
                }
                out += decoder.decode();
                assert_equals(out, string);
            }, 'Streaming decode: ' + encoding + ', ' + len + ' byte window (' + arrayBufferOrSharedArrayBuffer + ')');
        }
    });

    test(() => {
        function bytes(byteArray) {
            const view = new Uint8Array(createBuffer(arrayBufferOrSharedArrayBuffer, byteArray.length));
            view.set(byteArray);
            return view;
        }

        const decoder = new TextDecoder();

        assert_equals(decoder.decode(bytes([0xC1]), {stream: true}), "\uFFFD");
        assert_equals(decoder.decode(), "");

        assert_equals(decoder.decode(bytes([0xF5]), {stream: true}), "\uFFFD");
        assert_equals(decoder.decode(), "");

        assert_equals(decoder.decode(bytes([0xE0, 0x41]), {stream: true}), "\uFFFDA");
        assert_equals(decoder.decode(bytes([0x42])), "B");

        assert_equals(decoder.decode(bytes([0xE0, 0x80]), {stream: true}), "\uFFFD\uFFFD");
        assert_equals(decoder.decode(bytes([0x80])), "\uFFFD");

        assert_equals(decoder.decode(bytes([0xED, 0xA0]), {stream: true}), "\uFFFD\uFFFD");
        assert_equals(decoder.decode(bytes([0x80])), "\uFFFD");

        assert_equals(decoder.decode(bytes([0xF0, 0x41]), {stream: true}), "\uFFFDA");
        assert_equals(decoder.decode(bytes([0x42]), {stream: true}), "B");
        assert_equals(decoder.decode(bytes([0x43])), "C");

        assert_equals(decoder.decode(bytes([0xF0, 0x80]), {stream: true}), "\uFFFD\uFFFD");
        assert_equals(decoder.decode(bytes([0x80]), {stream: true}), "\uFFFD");
        assert_equals(decoder.decode(bytes([0x80])), "\uFFFD");

        assert_equals(decoder.decode(bytes([0xF4, 0xA0]), {stream: true}), "\uFFFD\uFFFD");
        assert_equals(decoder.decode(bytes([0x80]), {stream: true}), "\uFFFD");
        assert_equals(decoder.decode(bytes([0x80])), "\uFFFD");

        assert_equals(decoder.decode(bytes([0xF0, 0x90, 0x41]), {stream: true}), "\uFFFDA");
        assert_equals(decoder.decode(bytes([0x42])), "B");

        // 4-byte UTF-8 sequences always correspond to non-BMP characters. Here
        // we make sure that, although the first 3 bytes are enough to emit the
        // lead surrogate, it only gets emitted when the fourth byte is read.
        assert_equals(decoder.decode(bytes([0xF0, 0x9F, 0x92]), {stream: true}), "");
        assert_equals(decoder.decode(bytes([0xA9])), "\u{1F4A9}");
    }, `Streaming decode: UTF-8 chunk tests (${arrayBufferOrSharedArrayBuffer})`);
})
