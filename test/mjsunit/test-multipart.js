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
sys.puts("test "+emails.length+" emails");
(function testEmails () {
  
  var email = emails.pop(),
    curr = 0;

  if (!email) {
    sys.puts("done testing emails");
    firstPart.emitSuccess();
    return;
  }
  sys.puts("testing email "+emails.length);
  var expect = email.expect;

  var message  = new (events.EventEmitter);
  message.headers = email.headers;

  var mp = multipart.parse(message);
  mp.addListener("partBegin", function (part) {
    sys.puts(">> testing part #"+curr);
    testPart(email.expect[curr ++], part);
  });
  mp.addListener("complete", function () {
    sys.puts("done with email "+emails.length);
    process.nextTick(testEmails);
  });
  // stream it through in chunks.
  var emailBody = email.body;
  process.nextTick(function s () {
    if (emailBody) {
      message.emit("data", emailBody.substr(0, chunkSize));
      emailBody = emailBody.substr(chunkSize);
      process.nextTick(s);
    } else {
      message.emit("end");
    }
  });
})();

// run good HTTP messages test after previous test ends.
var secondPart = new (events.Promise),
  server = http.createServer(function (req, res) {
    sys.puts("HTTP mp request");
    var mp = multipart.parse(req),
      curr = 0;
    req.setBodyEncoding("binary");
    if (req.url !== "/bad") {
      sys.puts("expected to be good");
      mp.addListener("partBegin", function (part) {
        sys.puts(">> testing part #"+curr);
        testPart(message.expect[curr ++], part);
      });
    } else {
      sys.puts("expected to be bad");
    }
    mp.addListener("error", function (er) {
      sys.puts("!! error occurred");
      res.sendHeader(400, {});
      res.write("bad");
      res.close();
    });
    mp.addListener("complete", function () {
      res.sendHeader(200, {});
      res.write("ok");
      res.close();
    });
  }),
  message,
  client = http.createClient(PORT);
server.listen(PORT);

// could dry these two up a bit.
firstPart.addCallback(function testGoodMessages () {
  var httpMessages = fixture.messages.slice(0);
  sys.puts("testing "+httpMessages.length+" good messages");
  (function testHTTP () {
    message = httpMessages.pop();
    if (!message) {
      secondPart.emitSuccess();
      return;
    }
    sys.puts("test message "+httpMessages.length);
    var req = client.request("POST", "/", message.headers);
    req.write(message.body, "binary");
    req.addListener('response', function (res) {
      var buff = "";
      res.addListener("data", function (chunk) { buff += chunk });
      res.addListener("end", function () {
        assert.equal(buff, "ok");
        process.nextTick(testHTTP);
      });
    });
    req.close();
  })();
});
secondPart.addCallback(function testBadMessages () {
  var httpMessages = fixture.badMessages.slice(0);
  sys.puts("testing "+httpMessages.length+" bad messages");
  (function testHTTP () {
    message = httpMessages.pop();
    if (!message) {
      server.close()
      return;
    }
    sys.puts("test message "+httpMessages.length);
    var req = client.request("POST", "/bad", message.headers);
    req.write(message.body, "binary");
    req.addListener('response', function (res) {
      var buff = "";
      res.addListener("data", function (chunk) { buff += chunk });
      res.addListener("end", function () {
        assert.equal(buff, "bad");
        process.nextTick(testHTTP);
      });
    });
    req.close();
  })();
});
