// Example - generation of report on uncaught exception
require('node-report');
var http = require("http");

function my_listener(request, response) {
  switch (request.url) {
  case '/':
    response.writeHead(200, "OK",{'Content-Type': 'text/html'});
    response.write('<html><head><title>node-report example</title></head><body style="font-family:arial;">');
    response.write('<h2>node-report example: report triggered on uncaught exception</h2>');
    response.write('<p>Click on button below to throw an exception. The exception is not caught by the application.');
    response.write('<p>The node-report module will produce a report then terminate the application.');
    response.write('<form enctype="application/x-www-form-urlencoded" action="/exception" method="post">');
    response.write('<button>Throw exception</button></form>');
    response.write('</form></body></html');
    response.end();
    break;
  case '/exception':
    throw new Error('*** exception.js: uncaught exception thrown from function my_listener()');
  }
}

var http_server = http.createServer(my_listener);
http_server.listen(8080);

console.log('exception.js: Node running');
console.log('exception.js: Go to http://<machine>:8080/ or http://localhost:8080/');

setTimeout(function() {
  console.log('exception.js: application timeout expired, exiting.');
  process.exit(0);
}, 60000);
