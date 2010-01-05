process.mixin(require("./common"));
http = require("http");

var multipart = require('multipart');
var fixture = require('./fixtures/multipart');

var port = 8222;
var parts_reveived = 0;
var parts_complete = 0;
var requests = 0;
var badRequests = 0;
var parts = {};
var respond = function(res, text) {
  requests++;
  if (requests == 2) {
    server.close();
  }

  res.sendHeader(200, {"Content-Type": "text/plain"});
  res.sendBody(text);
  res.finish();
};

var server = http.createServer(function(req, res) {
  try {
    var stream = new multipart.Stream(req);
  } catch (e) {
    badRequests++;
    respond(res, 'no thanks');
    return;
  }

  stream.addListener('part', function(part) {
    parts_reveived++;

    var name = part.name;

    if (parts_reveived == 1) {
      assert.equal('reply', name);
    } else if (parts_reveived == 2) {
      assert.equal('fileupload', name);
    }

    parts[name] = '';
    part.addListener('body', function(chunk) {
      parts[name] += chunk;
    });
    part.addListener('complete', function(chunk) {
      assert.equal(0, part.buffer.length);
      if (parts_reveived == 1) {
        assert.equal('yes', parts[name]);
      } else if (parts_reveived == 2) {
        assert.equal('/9j/4AAQSkZJRgABAQAAAQABAAD//gA+Q1JFQVRPUjogZ2QtanBlZyB2MS4wICh1c2luZyBJSkcg', parts[name]);
      }
      parts_complete++;
    });
  });

  stream.addListener('complete', function() {
    respond(res, 'thanks');
  });
});
server.listen(port);

var client = http.createClient(port);
var request = client.request('POST', '/', {'Content-Type': 'multipart/form-data; boundary=AaB03x', 'Content-Length': fixture.reply.length});
request.sendBody(fixture.reply, 'binary');
request.finish();

var badRequest = client.request('POST', '/', {'Content-Type': 'something', 'Content-Length': fixture.reply.length});
badRequest.sendBody(fixture.reply, 'binary');
badRequest.finish();

process.addListener('exit', function() {
  puts("done");
  assert.equal(2, parts_complete);
  assert.equal(2, parts_reveived);
  assert.equal(1, badRequests);
});