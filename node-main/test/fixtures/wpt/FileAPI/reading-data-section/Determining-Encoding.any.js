// META: title=FileAPI Test: Blob Determining Encoding

var t = async_test("Blob Determing Encoding with encoding argument");
t.step(function() {
    // string 'hello'
    var data = [0xFE,0xFF,0x00,0x68,0x00,0x65,0x00,0x6C,0x00,0x6C,0x00,0x6F];
    var blob = new Blob([new Uint8Array(data)]);
    var reader = new FileReader();

    reader.onloadend = t.step_func_done (function(event) {
        assert_equals(this.result, "hello", "The FileReader should read the ArrayBuffer through UTF-16BE.")
    }, reader);

    reader.readAsText(blob, "UTF-16BE");
});

var t = async_test("Blob Determing Encoding with type attribute");
t.step(function() {
    var data = [0xFE,0xFF,0x00,0x68,0x00,0x65,0x00,0x6C,0x00,0x6C,0x00,0x6F];
    var blob = new Blob([new Uint8Array(data)], {type:"text/plain;charset=UTF-16BE"});
    var reader = new FileReader();

    reader.onloadend = t.step_func_done (function(event) {
        assert_equals(this.result, "hello", "The FileReader should read the ArrayBuffer through UTF-16BE.")
    }, reader);

    reader.readAsText(blob);
});


var t = async_test("Blob Determing Encoding with UTF-8 BOM");
t.step(function() {
    var data = [0xEF,0xBB,0xBF,0x68,0x65,0x6C,0x6C,0xC3,0xB6];
    var blob = new Blob([new Uint8Array(data)]);
    var reader = new FileReader();

    reader.onloadend = t.step_func_done (function(event) {
        assert_equals(this.result, "hellö", "The FileReader should read the blob with UTF-8.");
    }, reader);

    reader.readAsText(blob);
});

var t = async_test("Blob Determing Encoding without anything implying charset.");
t.step(function() {
    var data = [0x68,0x65,0x6C,0x6C,0xC3,0xB6];
    var blob = new Blob([new Uint8Array(data)]);
    var reader = new FileReader();

    reader.onloadend = t.step_func_done (function(event) {
        assert_equals(this.result, "hellö", "The FileReader should read the blob by default with UTF-8.");
    }, reader);

    reader.readAsText(blob);
});

var t = async_test("Blob Determing Encoding with UTF-16BE BOM");
t.step(function() {
    var data = [0xFE,0xFF,0x00,0x68,0x00,0x65,0x00,0x6C,0x00,0x6C,0x00,0x6F];
    var blob = new Blob([new Uint8Array(data)]);
    var reader = new FileReader();

    reader.onloadend = t.step_func_done (function(event) {
        assert_equals(this.result, "hello", "The FileReader should read the ArrayBuffer through UTF-16BE.");
    }, reader);

    reader.readAsText(blob);
});

var t = async_test("Blob Determing Encoding with UTF-16LE BOM");
t.step(function() {
    var data = [0xFF,0xFE,0x68,0x00,0x65,0x00,0x6C,0x00,0x6C,0x00,0x6F,0x00];
    var blob = new Blob([new Uint8Array(data)]);
    var reader = new FileReader();

    reader.onloadend = t.step_func_done (function(event) {
        assert_equals(this.result, "hello", "The FileReader should read the ArrayBuffer through UTF-16LE.");
    }, reader);

    reader.readAsText(blob);
});
