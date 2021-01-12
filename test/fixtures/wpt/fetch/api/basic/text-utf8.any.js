// META: title=Fetch: Request and Response text() should decode as UTF-8
// META: global=window,worker
// META: script=../resources/utils.js

function testTextDecoding(body, expectedText, urlParameter, title)
{
    var arrayBuffer = stringToArray(body);

    promise_test(function(test) {
        var request = new Request("", {method: "POST", body: arrayBuffer});
        return request.text().then(function(value) {
            assert_equals(value, expectedText, "Request.text() should decode data as UTF-8");
        });
    }, title + " with Request.text()");

    promise_test(function(test) {
        var response = new Response(arrayBuffer);
        return response.text().then(function(value) {
            assert_equals(value, expectedText, "Response.text() should decode data as UTF-8");
        });
    }, title + " with Response.text()");

    promise_test(function(test) {
        return fetch("../resources/status.py?code=200&type=text%2Fplain%3Bcharset%3DUTF-8&content=" + urlParameter).then(function(response) {
            return response.text().then(function(value) {
                assert_equals(value, expectedText, "Fetched Response.text() should decode data as UTF-8");
            });
        });
    }, title + " with fetched data (UTF-8 charset)");

    promise_test(function(test) {
        return fetch("../resources/status.py?code=200&type=text%2Fplain%3Bcharset%3DUTF-16&content=" + urlParameter).then(function(response) {
            return response.text().then(function(value) {
                assert_equals(value, expectedText, "Fetched Response.text() should decode data as UTF-8");
            });
        });
    }, title + " with fetched data (UTF-16 charset)");

    promise_test(function(test) {
        return new Response(body).arrayBuffer().then(function(buffer) {
            assert_array_equals(new Uint8Array(buffer), encode_utf8(body), "Response.arrayBuffer() should contain data encoded as UTF-8");
        });
    }, title + " (Response object)");

    promise_test(function(test) {
        return new Request("", {method: "POST", body: body}).arrayBuffer().then(function(buffer) {
            assert_array_equals(new Uint8Array(buffer), encode_utf8(body), "Request.arrayBuffer() should contain data encoded as UTF-8");
        });
    }, title + " (Request object)");

}

var utf8WithBOM = "\xef\xbb\xbf\xe4\xb8\x89\xe6\x9d\x91\xe3\x81\x8b\xe3\x81\xaa\xe5\xad\x90";
var utf8WithBOMAsURLParameter = "%EF%BB%BF%E4%B8%89%E6%9D%91%E3%81%8B%E3%81%AA%E5%AD%90";
var utf8WithoutBOM = "\xe4\xb8\x89\xe6\x9d\x91\xe3\x81\x8b\xe3\x81\xaa\xe5\xad\x90";
var utf8WithoutBOMAsURLParameter = "%E4%B8%89%E6%9D%91%E3%81%8B%E3%81%AA%E5%AD%90";
var utf8Decoded = "三村かな子";
testTextDecoding(utf8WithBOM, utf8Decoded, utf8WithBOMAsURLParameter, "UTF-8 with BOM");
testTextDecoding(utf8WithoutBOM, utf8Decoded, utf8WithoutBOMAsURLParameter, "UTF-8 without BOM");

var utf16BEWithBOM = "\xfe\xff\x4e\x09\x67\x51\x30\x4b\x30\x6a\x5b\x50";
var utf16BEWithBOMAsURLParameter = "%fe%ff%4e%09%67%51%30%4b%30%6a%5b%50";
var utf16BEWithBOMDecodedAsUTF8 = "��N\tgQ0K0j[P";
testTextDecoding(utf16BEWithBOM, utf16BEWithBOMDecodedAsUTF8, utf16BEWithBOMAsURLParameter, "UTF-16BE with BOM decoded as UTF-8");

var utf16LEWithBOM = "\xff\xfe\x09\x4e\x51\x67\x4b\x30\x6a\x30\x50\x5b";
var utf16LEWithBOMAsURLParameter = "%ff%fe%09%4e%51%67%4b%30%6a%30%50%5b";
var utf16LEWithBOMDecodedAsUTF8 = "��\tNQgK0j0P[";
testTextDecoding(utf16LEWithBOM, utf16LEWithBOMDecodedAsUTF8, utf16LEWithBOMAsURLParameter, "UTF-16LE with BOM decoded as UTF-8");

var utf16WithoutBOM = "\xe6\x00\xf8\x00\xe5\x00\x0a\x00\xc6\x30\xb9\x30\xc8\x30\x0a\x00";
var utf16WithoutBOMAsURLParameter = "%E6%00%F8%00%E5%00%0A%00%C6%30%B9%30%C8%30%0A%00";
var utf16WithoutBOMDecoded = "\ufffd\u0000\ufffd\u0000\ufffd\u0000\u000a\u0000\ufffd\u0030\ufffd\u0030\ufffd\u0030\u000a\u0000";
testTextDecoding(utf16WithoutBOM, utf16WithoutBOMDecoded, utf16WithoutBOMAsURLParameter, "UTF-16 without BOM decoded as UTF-8");
