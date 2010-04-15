HTTP Parser
===========

This is a parser for HTTP messages written in C. It parses both requests
and responses. The parser is designed to be used in performance HTTP
applications. It does not make any allocations, it does not buffer data, and
it can be interrupted at anytime. Depending on your architecture, it only
requires between 100 and 200 bytes of data per message stream (in a web
server that is per connection).

Features:

  * No dependencies
  * Parses both requests and responses.
  * Handles persistent streams.
  * Decodes chunked encoding.
  * Extracts the following data from a message
    * header fields and values
    * content-length
    * request method
    * response status code
    * transfer-encoding
    * http version
    * request path, query string, fragment
    * message body
  * Defends against buffer overflow attacks.
  * Upgrade support

Usage
-----

One `http_parser` object is used per TCP connection. Initialize the struct
using `http_parser_init()` and set the callbacks. That might look something
like this for a request parser:

    http_parser_settings settings;
    settings.on_path = my_path_callback;
    settings.on_header_field = my_header_field_callback;
    /* ... */
    settings.data = my_socket;

    http_parser *parser = malloc(sizeof(http_parser));
    http_parser_init(parser, HTTP_REQUEST);

When data is received on the socket execute the parser and check for errors.

    size_t len = 80*1024, nparsed;
    char buf[len];
    ssize_t recved;

    recved = recv(fd, buf, len, 0);

    if (recved < 0) {
      /* Handle error. */
    }

    /* Start up / continue the parser.
     * Note we pass the recved==0 to http_parse_requests to signal
     * that EOF has been recieved.
     */
    nparsed = http_parser_execute(parser, settings, buf, recved);

    if (parser->upgrade) {
      /* handle new protocol */
    } else if (nparsed != recved) {
      /* Handle error. Usually just close the connection. */
    }

HTTP needs to know where the end of the stream is. For example, sometimes
servers send responses without Content-Length and expect the client to
consume input (for the body) until EOF. To tell http_parser about EOF, give
`0` as the forth parameter to `http_parser_execute()`. Callbacks and errors
can still be encountered during an EOF, so one must still be prepared
to receive them.

Scalar valued message information such as `status_code`, `method`, and the
HTTP version are stored in the parser structure. This data is only
temporally stored in `http_parser` and gets reset on each new message. If
this information is needed later, copy it out of the structure during the
`headers_complete` callback.

The parser decodes the transfer-encoding for both requests and responses
transparently. That is, a chunked encoding is decoded before being sent to
the on_body callback.

It does not decode the content-encoding (gzip). Not all HTTP applications
need to inspect the body. Decoding gzip is non-neglagable amount of
processing (and requires making allocations). HTTP proxies using this
parser, for example, would not want such a feature.

The Special Problem of Upgrade
------------------------------

HTTP supports upgrading the connection to a different protocol. An
increasingly common example of this is the Web Socket protocol which sends
a request like

        GET /demo HTTP/1.1
        Upgrade: WebSocket
        Connection: Upgrade
        Host: example.com
        Origin: http://example.com
        WebSocket-Protocol: sample

followed by non-HTTP data.

(See http://tools.ietf.org/html/draft-hixie-thewebsocketprotocol-75 for more
information the Web Socket protocol.)

To support this, the parser will treat this as a normal HTTP message without a
body. Issuing both on_headers_complete and on_message_complete callbacks. However
http_parser_execute() may finish without parsing the entire supplied buffer.

The user needs to check if parser->upgrade has been set to 1 after
http_parser_execute() returns to determine if a premature exit was due to an
upgrade or an error.


Callbacks
---------

During the `http_parser_execute()` call, the callbacks set in
`http_parser_settings` will be executed. The parser maintains state and
never looks behind, so buffering the data is not necessary. If you need to
save certain data for later usage, you can do that from the callbacks.

There are two types of callbacks:

* notification `typedef int (*http_cb) (http_parser*);`
    Callbacks: on_message_begin, on_headers_complete, on_message_complete.
* data `typedef int (*http_data_cb) (http_parser*, const char *at, size_t length);`
    Callbacks: (requests only) on_path, on_query_string, on_uri, on_fragment,
               (common) on_header_field, on_header_value, on_body;

Callbacks must return 0 on success. Returning a non-zero value indicates
error to the parser, making it exit immediately.

In case you parse HTTP message in chunks (i.e. `read()` request line
from socket, parse, read half headers, parse, etc) your data callbacks
may be called more than once. Http-parser guarantees that data pointer is only
valid for the lifetime of callback. You can also `read()` into a heap allocated
buffer to avoid copying memory around if this fits your application.

Reading headers may be a tricky task if you read/parse headers partially.
Basically, you need to remember whether last header callback was field or value
and apply following logic:

    (on_header_field and on_header_value shortened to on_h_*)
     ------------------------ ------------ --------------------------------------------
    | State (prev. callback) | Callback   | Description/action                         |
     ------------------------ ------------ --------------------------------------------
    | nothing (first call)   | on_h_field | Allocate new buffer and copy callback data |
    |                        |            | into it                                    |
     ------------------------ ------------ --------------------------------------------
    | value                  | on_h_field | New header started.                        |
    |                        |            | Copy current name,value buffers to headers |
    |                        |            | list and allocate new buffer for new name  |
     ------------------------ ------------ --------------------------------------------
    | field                  | on_h_field | Previous name continues. Reallocate name   |
    |                        |            | buffer and append callback data to it      |
     ------------------------ ------------ --------------------------------------------
    | field                  | on_h_value | Value for current header started. Allocate |
    |                        |            | new buffer and copy callback data to it    |
     ------------------------ ------------ --------------------------------------------
    | value                  | on_h_value | Value continues. Reallocate value buffer   |
    |                        |            | and append callback data to it             |
     ------------------------ ------------ --------------------------------------------

See examples of reading in headers:

* [partial example](http://gist.github.com/155877) in C
* [from http-parser tests](http://github.com/ry/http-parser/blob/37a0ff8928fb0d83cec0d0d8909c5a4abcd221af/test.c#L403) in C
* [from Node library](http://github.com/ry/node/blob/842eaf446d2fdcb33b296c67c911c32a0dabc747/src/http.js#L284) in Javascript
