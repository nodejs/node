// META: global=window,worker
// META: title=Response consume
// META: script=../resources/utils.js

promise_test(function(test) {
    var body = "";
    var response = new Response("");
    return validateStreamFromString(response.body.getReader(), "");
}, "Read empty text response's body as readableStream");

promise_test(function(test) {
    var response = new Response(new Blob([], { "type" : "text/plain" }));
    return validateStreamFromString(response.body.getReader(), "");
}, "Read empty blob response's body as readableStream");

var formData = new FormData();
formData.append("name", "value");
var textData = JSON.stringify("This is response's body");
var blob = new Blob([textData], { "type" : "text/plain" });
var urlSearchParamsData = "name=value";
var urlSearchParams = new URLSearchParams(urlSearchParamsData);

for (const mode of [undefined, "byob"]) {
    promise_test(function(test) {
        var response = new Response(blob);
        return validateStreamFromString(response.body.getReader({ mode }), textData);
    }, `Read blob response's body as readableStream with mode=${mode}`);

    promise_test(function(test) {
        var response = new Response(textData);
        return validateStreamFromString(response.body.getReader({ mode }), textData);
    }, `Read text response's body as readableStream with mode=${mode}`);

    promise_test(function(test) {
        var response = new Response(urlSearchParams);
        return validateStreamFromString(response.body.getReader({ mode }), urlSearchParamsData);
    }, `Read URLSearchParams response's body as readableStream with mode=${mode}`);

    promise_test(function(test) {
        var arrayBuffer = new ArrayBuffer(textData.length);
        var int8Array = new Int8Array(arrayBuffer);
        for (var cptr = 0; cptr < textData.length; cptr++)
            int8Array[cptr] = textData.charCodeAt(cptr);

        return validateStreamFromString(new Response(arrayBuffer).body.getReader({ mode }), textData);
    }, `Read array buffer response's body as readableStream with mode=${mode}`);

    promise_test(function(test) {
        var response = new Response(formData);
        return validateStreamFromPartialString(response.body.getReader({ mode }),
        "Content-Disposition: form-data; name=\"name\"\r\n\r\nvalue");
    }, `Read form data response's body as readableStream with mode=${mode}`);
}

test(function() {
    assert_equals(Response.error().body, null);
}, "Getting an error Response stream");

test(function() {
    assert_equals(Response.redirect("/").body, null);
}, "Getting a redirect Response stream");

promise_test(async function(test) {
  var buffer = new ArrayBuffer(textData.length);

  var body = new Response(textData).body;
  const reader = body.getReader( {mode: 'byob'} );

  let offset = 3;
  while (offset < textData.length) {
    const {done, value} = await reader.read(new Uint8Array(buffer, offset));
    if (done) {
      break;
    }
    buffer = value.buffer;
    offset += value.byteLength;
  }

  validateBufferFromString(buffer, `\0\0\0\"This is response's bo`, 'Buffer should be validated');
}, `Reading with offset from Response stream`);
