// META: title=FileAPI Test: readAsText uses the Blob type's charset when no encoding argument is given

// Per the File API "read as text" algorithm, the encoding is determined in this
// order: (1) the explicit `encoding` argument if it is a supported encoding;
// (2) otherwise, if the blob's `type` has a `charset` parameter, that charset;
// (3) otherwise UTF-8. These tests exercise step (2) with BOM-free data so that
// the charset parameter is the only thing that can select the right decoder
// (i.e. they are not accidentally satisfied by BOM sniffing).

async_test(function() {
    // 0x80 is the Euro sign (U+20AC) in windows-1252. As UTF-8 it is a lone
    // continuation byte and decodes to U+FFFD.
    var blob = new Blob([new Uint8Array([0x80])], { type: "text/plain;charset=windows-1252" });
    var reader = new FileReader();

    reader.onloadend = this.step_func_done(function() {
        assert_equals(reader.result, "€",
            "readAsText should honor the windows-1252 charset from the Blob type and decode 0x80 as U+20AC, not UTF-8 U+FFFD.");
    });

    reader.readAsText(blob);
}, "readAsText() uses the charset parameter of the Blob's type (windows-1252)");

async_test(function() {
    // "héllo" in windows-1252: 0xE9 is 'é'. As UTF-8, the lone 0xE9 is an
    // invalid lead byte and decodes to U+FFFD.
    var blob = new Blob([new Uint8Array([0x68, 0xE9, 0x6C, 0x6C, 0x6F])], { type: "text/plain;charset=windows-1252" });
    var reader = new FileReader();

    reader.onloadend = this.step_func_done(function() {
        assert_equals(reader.result, "héllo",
            "readAsText should decode the body using the windows-1252 charset declared in the Blob type.");
    });

    reader.readAsText(blob);
}, "readAsText() decodes a windows-1252 Blob body using the type's charset");

async_test(function() {
    // An explicit encoding argument must still win over the Blob type charset
    // (step 1 of the algorithm). This already works in WebKit and guards against
    // a fix that over-corrects by always preferring the type charset.
    var blob = new Blob([new Uint8Array([0x80])], { type: "text/plain;charset=UTF-8" });
    var reader = new FileReader();

    reader.onloadend = this.step_func_done(function() {
        assert_equals(reader.result, "€",
            "An explicit encoding argument takes precedence over the Blob type's charset.");
    });

    reader.readAsText(blob, "windows-1252");
}, "readAsText() encoding argument overrides the Blob type's charset");
