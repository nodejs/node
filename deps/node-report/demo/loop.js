// Example - generation of report via signal for a looping application
// Note: node-report signal trigger is not supported on Windows
require('node-report');
var http = require("http");

function my_listener(request, response) {
  switch (request.url) {
  case '/loop':
    console.log("loop.js: going to busy loop now, use 'kill -USR2 " + process.pid + "' to trigger report");
    var list = [];
    for (var i=0; i<10000000000; i++) {
      for (var j=0; i<1000; i++) {
        list.push(new MyRecord());
      }
      for (var j=0; i<1000; i++) {
        list[j].id += 1;
        list[j].account += 2;
      }
      for (var j=0; i<1000; i++) {
        list.pop();
      }
    }
    // drop through to refresh page
  case '/':
    response.writeHead(200, "OK",{'Content-Type': 'text/html'});
    response.write('<html><head><title>node-report example</title></head><body style="font-family:arial;">');
    response.write('<h2>node-report example: report triggered using USR2 signal</h2>');
    response.write('<p>Click on button below to enter JavaScript busy loop, then');
    response.write(' use "kill -USR2 ' + process.pid + '" in a terminal window to trigger report');
    response.write('<form enctype="application/x-www-form-urlencoded" action="/loop" method="post">');
    response.write('<button>Enter busy loop</button></form>');
    response.write('<p>When busy loop completes, click on button below to terminate the application.');
    response.write('<form enctype="application/x-www-form-urlencoded" action="/end" method="post">');
    response.write('<button>End application</button></form>');
    response.write('</form></body></html');
    response.end();
    break;
  case '/end':
    process.exit(0);
  }
}

function MyRecord() {
  this.name = 'foo';
  this.id = 128;
  this.account = 98454324;
}

var http_server = http.createServer(my_listener);
http_server.listen(8080);

console.log('loop.js: Node running');
console.log('loop.js: Go to http://<machine>:8080/ or http://localhost:8080/');

setTimeout(function() {
  console.log('loop.js: timeout expired, exiting.');
  process.exit(0);
}, 60000);
