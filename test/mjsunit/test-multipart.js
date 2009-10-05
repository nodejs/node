node.mixin(require("common.js"));
http = require("/http.js");

var multipart = require('/multipart.js');
var port = 8222;
var parts_reveived = 0;
var parts_complete = 0;
var parts = {};

var server = http.createServer(function(req, res) {
  var stream = new multipart.Stream(req);

  stream.addListener('part', function(part) {
    parts_reveived++;

    var name = part.headers['Content-Disposition'].name;

    if (parts_reveived == 1) {
      assertEquals('test-field', name);
    } else if (parts_reveived == 2) {
      assertEquals('test-file', name);
    }

    parts[name] = '';
    part.addListener('body', function(chunk) {
      parts[name] += chunk;
    });
    part.addListener('complete', function(chunk) {
      if (parts_reveived == 1) {
        assertEquals('foobar', parts[name]);
      } else if (parts_reveived == 2) {
        assertEquals(node.fs.cat(__filename).wait(), parts[name]);
      }
      parts_complete++;
    });
  });

  stream.addListener('complete', function() {
    res.sendHeader(200, {"Content-Type": "text/plain"});
    res.sendBody('thanks');
    res.finish();
    server.close();
  });
});
server.listen(port);

var cmd = 'curl -H "Expect:" -F "test-field=foobar" -F test-file=@'+__filename+' http://localhost:'+port+'/';
var result = exec(cmd).wait();

process.addListener('exit', function() {
  assertEquals(2, parts_complete);
});
