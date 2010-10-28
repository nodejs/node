## Synopsis

An example of a [web server](http.html) written with Node which responds with 'Hello
World':

    var http = require('http');

    http.createServer(function (request, response) {
      response.writeHead(200, {'Content-Type': 'text/plain'});
      response.end('Hello World\n');
    }).listen(8124);

    console.log('Server running at http://127.0.0.1:8124/');

To run the server, put the code into a file called `example.js` and execute
it with the node program

    > node example.js
    Server running at http://127.0.0.1:8124/

All of the examples in the documentation can be run similarly.
