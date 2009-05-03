HTTP Parser
===========

This is a parser for HTTP messages written in C. It parses both requests
and responses. The parser is designed to be used in performance HTTP
applications. It does not make any allocations, it does not buffer data, and
it can be interrupted at anytime. It only requires about 100 bytes of data
per message stream (in a web server that is per connection). 

Features:

  * No dependencies 
  * Parses both requests and responses.
  * Handles keep-alive streams.
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

Usage
-----

One `http_parser` object is used per TCP connection. Initialize the struct
using `http_parser_init()` and set the callbacks. That might look something
like this:

    http_parser *parser = malloc(sizeof(http_parser));
    http_parser_init(parser, HTTP_REQUEST);
    parser->on_path = my_path_callback;
    parser->on_header_field = my_header_field_callback;
    parser->data = my_socket;

When data is received on the socket execute the parser and check for errors.

    size_t len = 80*1024;
    char buf[len];
    ssize_t recved;

    recved = read(fd, buf, len);
    if (recved != 0) // handle error

    http_parser_execute(parser, buf, recved);

    if (http_parser_has_error(parser)) {
      // handle error. usually just close the connection
    }

During the `http_parser_execute()` call, the callbacks set in `http_parser`
will be executed. The parser maintains state and never looks behind, so
buffering the data is not necessary. If you need to save certain data for
later usage, you can do that from the callbacks. (You can also `read()` into
a heap allocated buffer to avoid copying memory around if this fits your
application.)
  
The parser decodes the transfer-encoding for both requests and responses
transparently. That is, a chunked encoding is decoded before being sent to
the on_body callback.

It does not decode the content-encoding (gzip). Not all HTTP applications
need to inspect the body. Decoding gzip is non-neglagable amount of
processing (and requires making allocations). HTTP proxies using this
parser, for example, would not want such a feature.

Releases
--------

  * [0.1](http://s3.amazonaws.com/four.livejournal/20090427/http_parser-0.1.tar.gz)

The source repo is at [github](http://github.com/ry/http-parser).
