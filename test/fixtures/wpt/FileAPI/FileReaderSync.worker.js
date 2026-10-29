importScripts("/resources/testharness.js");

var blob, empty_blob, readerSync;
setup(() => {
    readerSync = new FileReaderSync();
    blob = new Blob(["test"]);
    empty_blob = new Blob();
});

test(() => {
    assert_true(readerSync instanceof FileReaderSync);
}, "Interface");

test(() => {
    var text = readerSync.readAsText(blob);
    assert_equals(text, "test");
}, "readAsText");

test(() => {
    var text = readerSync.readAsText(empty_blob);
    assert_equals(text, "");
}, "readAsText with empty blob");

test(() => {
    var data = readerSync.readAsDataURL(blob);
    assert_equals(data.indexOf("data:"), 0);
}, "readAsDataURL");

test(() => {
    var data = readerSync.readAsDataURL(empty_blob);
    assert_equals(data.indexOf("data:"), 0);
}, "readAsDataURL with empty blob");

test(() => {
    var data = readerSync.readAsBinaryString(blob);
    assert_equals(data, "test");
}, "readAsBinaryString");

test(() => {
    var data = readerSync.readAsBinaryString(empty_blob);
    assert_equals(data, "");
}, "readAsBinaryString with empty blob");

test(() => {
    var data = readerSync.readAsArrayBuffer(blob);
    assert_true(data instanceof ArrayBuffer);
    assert_equals(data.byteLength, "test".length);
}, "readAsArrayBuffer");

test(() => {
    var data = readerSync.readAsArrayBuffer(empty_blob);
    assert_true(data instanceof ArrayBuffer);
    assert_equals(data.byteLength, 0);
}, "readAsArrayBuffer with empty blob");

done();
