// Example - generation of report via node-report API call
var nodereport = require('node-report');
var http = require("http");

function my_listener(request, response) {
  switch (request.url) {
  case '/trigger':
    nodereport.triggerReport();
    // drop through to refresh page
  case '/':
    response.writeHead(200, "OK",{'Content-Type': 'text/html'});
    response.write('<html><head><title>node-report example</title></head><body style="font-family:arial;">');
    response.write('<h2>node-report example: report triggered by JavaScript API call</h2>');
    response.write('<p>Click on button below to trigger a report. The application will continue.');
    response.write('<form enctype="application/x-www-form-urlencoded" action="/trigger" method="post">');
    response.write('<button>Trigger report</button></form>');
    response.write('<p>Click on button below to terminate the application.');
    response.write('<form enctype="application/x-www-form-urlencoded" action="/end" method="post">');
    response.write('<button>End application</button></form>');
    response.write('</form></body></html');
    response.end();
    break;
  case '/end':
    process.exit(0);
  }
}

var http_server = http.createServer(my_listener);
http_server.listen(8080);

console.log('api_call.js: Node running');
console.log('api_call.js: Go to http://<machine>:8080/ or http://localhost:8080/');

setTimeout(function(){
  console.log('api_call.js: application timeout expired, exiting.');
  process.exit(0);
}, 60000);
