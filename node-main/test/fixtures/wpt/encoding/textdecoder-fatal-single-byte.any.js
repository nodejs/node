// META: timeout=long
// META: title=Encoding API: Fatal flag for single byte encodings
// META: timeout=long
// META: variant=?1-1000
// META: variant=?1001-2000
// META: variant=?2001-3000
// META: variant=?3001-4000
// META: variant=?4001-5000
// META: variant=?5001-6000
// META: variant=?6001-7000
// META: variant=?7001-last
// META: script=/common/subset-tests.js

var singleByteEncodings = [
     {encoding: 'IBM866', bad: []},
     {encoding: 'ISO-8859-2', bad: []},
     {encoding: 'ISO-8859-3', bad: [0xA5, 0xAE, 0xBE, 0xC3, 0xD0, 0xE3, 0xF0]},
     {encoding: 'ISO-8859-4', bad: []},
     {encoding: 'ISO-8859-5', bad: []},
     {encoding: 'ISO-8859-6', bad: [0xA1, 0xA2, 0xA3, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBC, 0xBD, 0xBE, 0xC0, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF]},
     {encoding: 'ISO-8859-7', bad: [0xAE, 0xD2, 0xFF]},
     {encoding: 'ISO-8859-8', bad: [0xA1, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xFB, 0xFC, 0xFF]},
     {encoding: 'ISO-8859-8-I', bad: [0xA1, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xFB, 0xFC, 0xFF]},
     {encoding: 'ISO-8859-10', bad: []},
     {encoding: 'ISO-8859-13', bad: []},
     {encoding: 'ISO-8859-14', bad: []},
     {encoding: 'ISO-8859-15', bad: []},
     {encoding: 'ISO-8859-16', bad: []},
     {encoding: 'KOI8-R', bad: []},
     {encoding: 'KOI8-U', bad: []},
     {encoding: 'macintosh', bad: []},
     {encoding: 'windows-874', bad: [0xDB, 0xDC, 0xDD, 0xDE, 0xFC, 0xFD, 0xFE, 0xFF]},
     {encoding: 'windows-1250', bad: []},
     {encoding: 'windows-1251', bad: []},
     {encoding: 'windows-1252', bad: []},
     {encoding: 'windows-1253', bad: [0xAA, 0xD2, 0xFF]},
     {encoding: 'windows-1254', bad: []},
     {encoding: 'windows-1255', bad: [0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xFB, 0xFC, 0xFF]},
     {encoding: 'windows-1256', bad: []},
     {encoding: 'windows-1257', bad: [0xA1, 0xA5]},
     {encoding: 'windows-1258', bad: []},
     {encoding: 'x-mac-cyrillic', bad: []},
];

singleByteEncodings.forEach(function(t) {
    for (var i = 0; i < 256; ++i) {
        if (t.bad.indexOf(i) != -1) {
            subsetTest(test, function() {
                assert_throws_js(TypeError, function() {
                    new TextDecoder(t.encoding, {fatal: true}).decode(new Uint8Array([i]));
                });
            }, 'Throw due to fatal flag: ' + t.encoding + ' doesn\'t have a pointer ' + i);
        }
        else {
            subsetTest(test, function() {
                assert_equals(typeof new TextDecoder(t.encoding, {fatal: true}).decode(new Uint8Array([i])), "string");
            }, 'Not throw: ' + t.encoding + ' has a pointer ' + i);
        }
    }
});
