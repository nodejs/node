# Class: WebSocket

> ⚠️ Warning: the WebSocket API is experimental and has known bugs.

Extends: [`EventTarget`](https://developer.mozilla.org/en-US/docs/Web/API/EventTarget)

The WebSocket object provides a way to manage a WebSocket connection to a server, allowing bidirectional communication. The API follows the [WebSocket spec](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket).

## `new WebSocket(url[, protocol])`

Arguments:

* **url** `URL | string` - The url's protocol *must* be `ws` or `wss`.
* **protocol** `string | string[]` (optional) - Subprotocol(s) to request the server use. 

## Read More

- [MDN - WebSocket](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket)
- [The WebSocket Specification](https://www.rfc-editor.org/rfc/rfc6455)
- [The WHATWG WebSocket Specification](https://websockets.spec.whatwg.org/)
