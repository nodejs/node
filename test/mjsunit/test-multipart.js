process.mixin(require("./common"));

var http = require("http"),
  multipart = require("multipart"),
  sys = require("sys"),
  PORT = 8222,
  fixture = require("./fixtures/multipart"),
  events = require("events"),
  testPart = function (expect, part) {
    if (!expect) {
      throw new Error("Got more parts than expected: "+
        JSON.stringify(part.headers));
    }
    for (var i in expect) {
      assert.equal(expect[i], part[i]);
    }
  };

var emails = fixture.messages.slice(0),
  chunkSize = 1, // set to minimum to forcibly expose boundary conditions.
                 // in a real scenario, this would be much much bigger.
  firstPart = new (events.Promise);

// test streaming messages through directly, as if they were in a file or something.
(function testEmails () {
  var email = emails.pop(),
    curr = 0;
  if (!email) {
    firstPart.emitSuccess();
    return;
  }
  var expect = email.expect;

  var message  = new (events.EventEmitter);
  message.headers = email.headers;

  var mp = multipart.parse(message);
  mp.addListener("partBegin", function (part) {
    testPart(email.expect[curr ++], part);
  });
  mp.addListener("complete", function () {
    process.nextTick(testEmails);
  });
  // stream it through in chunks.
  var emailBody = email.body;
  process.nextTick(function s () {
    if (emailBody) {
      message.emit("body", emailBody.substr(0, chunkSize));
      emailBody = emailBody.substr(chunkSize);
      process.nextTick(s);
    } else {
      message.emit("complete");
    }
  });
})();

// run good HTTP messages test after previous test ends.
var secondPart = new (events.Promise),
  server = http.createServer(function (req, res) {
    var mp = multipart.parse(req),
      curr = 0;
    req.setBodyEncoding("binary");
    if (req.url !== "/bad") {
      mp.addListener("partBegin", function (part) {
        testPart(message.expect[curr ++], part);
      });
    }
    mp.addListener("error", function (er) {
      res.sendHeader(400, {});
      res.sendBody("bad");
      res.finish();
    });
    mp.addListener("complete", function () {
      res.sendHeader(200, {});
      res.sendBody("ok");
      res.finish();
    });
  }),
  message,
  client = http.createClient(PORT);
server.listen(PORT);

// could dry these two up a bit.
firstPart.addCallback(function testGoodMessages () {
  var httpMessages = fixture.messages.slice(0);
  process.nextTick(function testHTTP () {
    message = httpMessages.pop();
    if (!message) {
      secondPart.emitSuccess();
      return;
    }
    var req = client.request("POST", "/", message.headers);
    req.sendBody(message.body, "binary");
    req.finish(function (res) {
      var buff = "";
      res.addListener("body", function (chunk) { buff += chunk });
      res.addListener("complete", function () {
        assert.equal(buff, "ok");
        process.nextTick(testHTTP);
      });
    });
  });
});
secondPart.addCallback(function testBadMessages () {
  var httpMessages = fixture.badMessages.slice(0);
  process.nextTick(function testHTTP () {
    message = httpMessages.pop();
    if (!message) {
      server.close()
      return;
    }
    var req = client.request("POST", "/bad", message.headers);
    req.sendBody(message.body, "binary");
    req.finish(function (res) {
      var buff = "";
      res.addListener("body", function (chunk) { buff += chunk });
      res.addListener("complete", function () {
        assert.equal(buff, "bad");
        process.nextTick(testHTTP);
      });
    });
  });
});