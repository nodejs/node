process.mixin(require("./common"));
http = require("http");
url = require("url");
PORT = 8888;

HOST = "localhost";

var have_tls;
try {
  var dummy_server = http.createServer();
  dummy_server.setSecure();
  have_tls=true;
} catch (e) {
  have_tls=false;
  puts("Not compiled with TLS support.");
  process.exit();
} 


var responses_sent = 0;
var responses_recvd = 0;
var body0 = "";
var body1 = "";
var caPem = fs.readFileSync(fixturesDir+"/test_ca.pem");
var certPem = fs.readFileSync(fixturesDir+"/test_cert.pem");
var keyPem = fs.readFileSync(fixturesDir+"/test_key.pem");


var http_server=http.createServer(function (req, res) {
  var verified = req.connection.verifyPeer();
  var peerDN = req.connection.getPeerCertificate("DNstring");
  assert.equal(verified, 1);
  assert.equal(peerDN, "C=UK,ST=Acknack Ltd,L=Rhys Jones,O=node.js,"
                        + "OU=Test TLS Certificate,CN=localhost");
  
  if (responses_sent == 0) {
    assert.equal("GET", req.method);
    assert.equal("/hello", url.parse(req.url).pathname);

    p(req.headers);
    assert.equal(true, "accept" in req.headers);
    assert.equal("*/*", req.headers["accept"]);

    assert.equal(true, "foo" in req.headers);
    assert.equal("bar", req.headers["foo"]);
  }

  if (responses_sent == 1) {
    assert.equal("POST", req.method);
    assert.equal("/world", url.parse(req.url).pathname);
    this.close();
  }

  req.addListener('end', function () {
    res.sendHeader(200, {"Content-Type": "text/plain"});
    res.write("The path was " + url.parse(req.url).pathname);
    res.close();
    responses_sent += 1;
  });

  //assert.equal("127.0.0.1", res.connection.remoteAddress);
});
http_server.setSecure("X509_PEM", caPem, 0, keyPem, certPem);
http_server.listen(PORT);

var client = http.createClient(PORT, HOST);
client.setSecure("x509_PEM", caPem, 0, keyPem, certPem);
var req = client.request("/hello", {"Accept": "*/*", "Foo": "bar"});
req.addListener('response', function (res) {
  var verified = res.connection.verifyPeer();
  var peerDN = res.connection.getPeerCertificate("DNstring");
  assert.equal(verified, 1);
  assert.equal(peerDN, "C=UK,ST=Acknack Ltd,L=Rhys Jones,O=node.js,"
                        + "OU=Test TLS Certificate,CN=localhost");
  assert.equal(200, res.statusCode);
  responses_recvd += 1;
  res.setBodyEncoding("ascii");
  res.addListener('data', function (chunk) { body0 += chunk; });
  debug("Got /hello response");
});
req.close();

setTimeout(function () {
  req = client.request("POST", "/world");
  req.addListener('response', function (res) {
    var verified = res.connection.verifyPeer();
    var peerDN = res.connection.getPeerCertificate("DNstring");
    assert.equal(verified, 1);
    assert.equal(peerDN, "C=UK,ST=Acknack Ltd,L=Rhys Jones,O=node.js,"
                          + "OU=Test TLS Certificate,CN=localhost");
    assert.equal(200, res.statusCode);
    responses_recvd += 1;
    res.setBodyEncoding("utf8");
    res.addListener('data', function (chunk) { body1 += chunk; });
    debug("Got /world response");
  });
  req.close();
}, 1);

process.addListener("exit", function () {
  debug("responses_recvd: " + responses_recvd);
  assert.equal(2, responses_recvd);

  debug("responses_sent: " + responses_sent);
  assert.equal(2, responses_sent);

  assert.equal("The path was /hello", body0);
  assert.equal("The path was /world", body1);
});

