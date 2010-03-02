process.mixin(require('../common.js'));

var request_count = 1000;
var response_body = '{"ok": true}';

var server = process.http.createServer(function(req, res) {
 res.sendHeader(200, {'Content-Type': 'text/javascript'});
 res.sendBody(response_body);
 res.finish();
});
server.listen(PORT, 4024);

var requests_ok = 0;
var requests_complete = 0;

function onLoad () {
 for (var i = 0; i < request_count; i++) {
   process.http.cat('http://localhost:'+PORT+'/', 'utf8')
     .addCallback(function (content) {
       assert.equal(response_body, content)
       print(".");
       requests_ok++;
       requests_complete++;
       if (requests_ok == request_count) {
         puts("\nrequests ok: " + requests_ok);
         server.close();
       }
     })
     .addErrback(function() {
       print("-");
       requests_complete++;
       //process.debug("error " + i);
     });
 }
}

process.addListener("exit", function () {
  assert.equal(request_count, requests_complete); 
  assert.equal(request_count, requests_ok); 
});
