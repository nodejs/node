// Example - generation of report on fatal error (JavaScript heap OOM)
require('node-report');
var http = require('http');

function my_listener(request, response) {
  switch (request.url) {
  case '/':
    response.writeHead(200, "OK",{'Content-Type': 'text/html'});
    response.write('<html><head><title>node-report example</title></head><body style="font-family:arial;">');
    response.write('<h2>node-report example: report triggered on fatal error (heap OOM)</h2>');
    response.write('<p>Click on button below to initiate excessive memory usage.');
    response.write('<p>The application will fail when the heap is full, and the node-report module will produce a report.');
    response.write('<form enctype="application/x-www-form-urlencoded" action="/memory" method="post">');
    response.write('<button>Use memory</button></form>');
    response.write('</form></body></html');
    response.end();
    break;
  case '/memory':
    console.log('fatalerror.js: allocating excessive JavaScript heap memory....please wait');
    var list = [];
    while (true) list.push(new my_record());
    response.end();
    break;
  }
}

function my_record() {
  this.name = 'foo';
  this.id = 128;
  this.account = 98454324;
}

var http_server = http.createServer(my_listener);
http_server.listen(8080);

console.log('fatalerror.js: Node running');
console.log('fatalerror.js: Note: heap default is 1.4Gb, use --max-old-space-size=<size in Mb> to change');
console.log('fatalerror.js: Go to http://<machine>:8080/ or http://localhost:8080/');

setTimeout(function(){
  console.log('fatalerror.js: application timeout expired, exiting.');
  process.exit(0);
}, 60000);
