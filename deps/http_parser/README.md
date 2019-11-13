HTTP Parser
===========

[![Build Status](https://api.travis-ci.org/nodejs/http-parser.svg?branch=master)](https://travis-ci.org/nodejs/http-parser)

This is a parser for HTTP messages written in C. It parses both requests and
responses. The parser is designed to be used in performance HTTP
applications. It does not make any syscalls nor allocations, it does not
buffer data, it can be interrupted at anytime. Depending on your
architecture, it only requires about 40 bytes of data per message
stream (in a web server that is per connection).

Features:

  * No dependencies
  * Handles persistent streams (keep-alive).
  * Decodes chunked encoding.
  * Upgrade support
  * Defends against buffer overflow attacks.

The parser extracts the following information from HTTP messages:

  * Header fields and values
  * Content-Length
  * Request method
  * Response status code
  * Transfer-Encoding
  * HTTP version
  * Request URL
  * Message body


Usage
-----

One `http_parser` object is used per TCP connection. Initialize the struct
using `http_parser_init()` and set the callbacks. That might look something
like this for a request parser:
```c
http_parser_settings settings;
settings.on_url = my_url_callback;
settings.on_header_field = my_header_field_callback;
/* ... */

http_parser *parser = malloc(sizeof(http_parser));
http_parser_init(parser, HTTP_REQUEST);
parser->data = my_socket;
```

When data is received on the socket execute the parser and check for errors.

```c
size_t len = 80*1024, nparsed;
char buf[len];
ssize_t recved;

recved = recv(fd, buf, len, 0);

if (recved < 0) {
  /* Handle error. */
}

/* Start up / continue the parser.
 * Note we pass recved==0 to signal that EOF has been received.
 */
nparsed = http_parser_execute(parser, &settings, buf, recved);

if (parser->upgrade) {
  /* handle new protocol */
} else if (nparsed != recved) {
  /* Handle error. Usually just close the connection. */
}
```

`http_parser` needs to know where the end of the stream is. For example, sometimes
servers send responses without Content-Length and expect the client to
consume input (for the body) until EOF. To tell `http_parser` about EOF, give
`0` as the fourth parameter to `http_parser_execute()`. Callbacks and errors
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


The Special Problem of Upgrade
------------------------------

`http_parser` supports upgrading the connection to a different protocol. An
increasingly common example of this is the WebSocket protocol which sends
a request like

        GET /demo HTTP/1.1
        Upgrade: WebSocket
        Connection: Upgrade
        Host: example.com
        Origin: http://example.com
        WebSocket-Protocol: sample

followed by non-HTTP data.

(See [RFC6455](https://tools.ietf.org/html/rfc6455) for more information the
WebSocket protocol.)

To support this, the parser will treat this as a normal HTTP message without a
body, issuing both on_headers_complete and on_message_complete callbacks. However
http_parser_execute() will stop parsing at the end of the headers and return.

The user is expected to check if `parser->upgrade` has been set to 1 after
`http_parser_execute()` returns. Non-HTTP data begins at the buffer supplied
offset by the return value of `http_parser_execute()`.


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
    Callbacks: (requests only) on_url,
               (common) on_header_field, on_header_value, on_body;

Callbacks must return 0 on success. Returning a non-zero value indicates
error to the parser, making it exit immediately.

For cases where it is necessary to pass local information to/from a callback,
the `http_parser` object's `data` field can be used.
An example of such a case is when using threads to handle a socket connection,
parse a request, and then give a response over that socket. By instantiation
of a thread-local struct containing relevant data (e.g. accepted socket,
allocated memory for callbacks to write into, etc), a parser's callbacks are
able to communicate data between the scope of the thread and the scope of the
callback in a threadsafe manner. This allows `http_parser` to be used in
multi-threaded contexts.

Example:
```c
 typedef struct {
  socket_t sock;
  void* buffer;
  int buf_len;
 } custom_data_t;


int my_url_callback(http_parser* parser, const char *at, size_t length) {
  /* access to thread local custom_data_t struct.
  Use this access save parsed data for later use into thread local
  buffer, or communicate over socket
  */
  parser->data;
  ...
  return 0;
}

...

void http_parser_thread(socket_t sock) {
 int nparsed = 0;
 /* allocate memory for user data */
 custom_data_t *my_data = malloc(sizeof(custom_data_t));

 /* some information for use by callbacks.
 * achieves thread -> callback information flow */
 my_data->sock = sock;

 /* instantiate a thread-local parser */
 http_parser *parser = malloc(sizeof(http_parser));
 http_parser_init(parser, HTTP_REQUEST); /* initialise parser */
 /* this custom data reference is accessible through the reference to the
 parser supplied to callback functions */
 parser->data = my_data;

 http_parser_settings settings; /* set up callbacks */
 settings.on_url = my_url_callback;

 /* execute parser */
 nparsed = http_parser_execute(parser, &settings, buf, recved);

 ...
 /* parsed information copied from callback.
 can now perform action on data copied into thread-local memory from callbacks.
 achieves callback -> thread information flow */
 my_data->buffer;
 ...
}

```

In case you parse HTTP message in chunks (i.e. `read()` request line
from socket, parse, read half headers, parse, etc) your data callbacks
may be called more than once. `http_parser` guarantees that data pointer is only
valid for the lifetime of callback. You can also `read()` into a heap allocated
buffer to avoid copying memory around if this fits your application.

Reading headers may be a tricky task if you read/parse headers partially.
Basically, you need to remember whether last header callback was field or value
and apply the following logic:

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


Parsing URLs
------------

A simplistic zero-copy URL parser is provided as `http_parser_parse_url()`.
Users of this library may wish to use it to parse URLs constructed from
consecutive `on_url` callbacks.

See examples of reading in headers:

* [partial example](http://gist.github.com/155877) in C
* [from http-parser tests](http://github.com/joyent/http-parser/blob/37a0ff8/test.c#L403) in C
* [from Node library](http://github.com/joyent/node/blob/842eaf4/src/http.js#L284) in Javascript
