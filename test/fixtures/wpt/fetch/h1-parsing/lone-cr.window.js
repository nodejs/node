// These tests expect that a network error is returned if there's a CR that is not immediately
// followed by LF before reaching message-body.
//
// No browser does this currently, but Firefox does treat it equivalently to a space which gives
// hope.

[
  "HTTP/1.1\r200 OK\n\nBODY",
  "HTTP/1.1 200\rOK\n\nBODY",
  "HTTP/1.1 200 OK\n\rHeader: Value\n\nBODY",
  "HTTP/1.1 200 OK\nHeader\r: Value\n\nBODY",
  "HTTP/1.1 200 OK\nHeader:\r Value\n\nBODY",
  "HTTP/1.1 200 OK\nHeader: Value\r\n\nBody",
  "HTTP/1.1 200 OK\nHeader: Value\r\r\nBODY",
  "HTTP/1.1 200 OK\nHeader: Value\rHeader2: Value2\n\nBODY",
  "HTTP/1.1 200 OK\nHeader: Value\n\rBODY",
  "HTTP/1.1 200 OK\nHeader: Value\n\r"
].forEach(input => {
  promise_test(t => {
    const message = encodeURIComponent(input);
    return promise_rejects_js(t, TypeError, fetch(`resources/message.py?message=${message}`));
  }, `Parsing response with a lone CR before message-body (${input})`);
});
