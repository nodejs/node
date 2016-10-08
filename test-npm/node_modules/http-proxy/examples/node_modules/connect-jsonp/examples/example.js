// note: assumes the connect & connect-json node packages are installed 
var connect = require('connect'),
    jsonp   = require('../lib/connect-jsonp');

var response = {
  success: true,
  it: 'works!'    
};

var server = connect.createServer(
  connect.logger({ format: ':method :url' }),
  connect.bodyParser(),
  jsonp(),
  connect.router(app),
  connect.errorHandler({ dumpExceptions: true, showStack: true })

).listen(3000);

console.log('connect-jsonp example server listening on port 3000');

function app(app) {
  // simple alert, not exactly x-domain...
  app.get('/', function(req, res) {
    res.writeHead(200, {'Content-Type': 'text/html'});
    res.end('<script type="text/javascript" src="/script-tag?callback=alert"></script>');
  });

  // requested after the script tag is rendered, result is evaluated
  app.get('/script-tag', function(req, res) {
    res.writeHead(200, {'Content-Type': 'application/json'});
    res.end(JSON.stringify(response));      
  });
}