process.mixin(require("./common"));
http = require("http");

var
  PORT = 8222,

  multipart = require('multipart'),
  fixture = require('./fixtures/multipart'),

  requests = 0,
  badRequests = 0,
  partsReceived = 0,
  partsComplete = 0,

  respond = function(res, text) {
    requests++;
    if (requests == 4) {
      server.close();
    }

    res.sendHeader(200, {"Content-Type": "text/plain"});
    res.sendBody(text);
    res.finish();
  };

var server = http.createServer(function(req, res) {
  if (req.headers['x-use-simple-api']) {
    multipart.parse(req)
      .addCallback(function() {
        respond(res, 'thanks');
      })
      .addErrback(function() {
        badRequests++;
        respond(res, 'no thanks');
      });
    return;
  }


  try {
    var stream = new multipart.Stream(req);
  } catch (e) {
    badRequests++;
    respond(res, 'no thanks');
    return;
  }

  var parts = {};
  stream.addListener('part', function(part) {
    partsReceived++;

    var name = part.name;

    if (partsReceived == 1) {
      assert.equal('reply', name);
    } else if (partsReceived == 2) {
      assert.equal('fileupload', name);
    }

    parts[name] = '';
    part.addListener('body', function(chunk) {
      parts[name] += chunk;
    });
    part.addListener('complete', function(chunk) {
      assert.equal(0, part.buffer.length);
      if (partsReceived == 1) {
        assert.equal('yes', parts[name]);
      } else if (partsReceived == 2) {
        assert.equal(
          '/9j/4AAQSkZJRgABAQAAAQABAAD//gA+Q1JFQVRPUjogZ2QtanBlZyB2MS4wICh1c2luZyBJSkcg',
          parts[name]
        );
      }
      partsComplete++;
    });
  });

  stream.addListener('complete', function() {
    respond(res, 'thanks');
  });
});
server.listen(PORT);

var client = http.createClient(PORT);

var request = client.request('POST', '/', {
  'Content-Type': 'multipart/form-data; boundary=AaB03x',
  'Content-Length': fixture.reply.length
});
request.sendBody(fixture.reply, 'binary');
request.finish();

var simpleRequest = client.request('POST', '/', {
  'X-Use-Simple-Api': 'yes',
  'Content-Type': 'multipart/form-data; boundary=AaB03x',
  'Content-Length': fixture.reply.length
});
simpleRequest.sendBody(fixture.reply, 'binary');
simpleRequest.finish();

var badRequest = client.request('POST', '/', {
  'Content-Type': 'invalid!',
  'Content-Length': fixture.reply.length
});
badRequest.sendBody(fixture.reply, 'binary');
badRequest.finish();

var simpleBadRequest = client.request('POST', '/', {
  'X-Use-Simple-Api': 'yes',
  'Content-Type': 'something',
  'Content-Length': fixture.reply.length
});
simpleBadRequest.sendBody(fixture.reply, 'binary');
simpleBadRequest.finish();

process.addListener('exit', function() {
  puts("done");
  assert.equal(2, partsComplete);
  assert.equal(2, partsReceived);
  assert.equal(2, badRequests);
});