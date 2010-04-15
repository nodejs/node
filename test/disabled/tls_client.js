require('../common');
var sys=require('sys');
var net=require('net');
var fs=require('fs');
var crypto=require('crypto');

//var client = net.createConnection(4443, "localhost");
var client = net.createConnection(443, "www.microsoft.com");
//var client = net.createConnection(443, "www.google.com");

var caPem = fs.readFileSync(fixturesDir+"/msca.pem");
//var caPem = fs.readFileSync("ca.pem");

var credentials = crypto.createCredentials({ca:caPem});

client.setEncoding("UTF8");
client.addListener("connect", function () {
  sys.puts("client connected.");
  client.setSecure(credentials);
});

client.addListener("secure", function () {
  sys.puts("client secure : "+JSON.stringify(client.getCipher()));
  sys.puts(JSON.stringify(client.getPeerCertificate()));
  sys.puts("verifyPeer : "+client.verifyPeer());
  client.write("GET / HTTP/1.0\r\n\r\n");
});

client.addListener("data", function (chunk) {
  sys.error(chunk);
});

client.addListener("end", function () {
  sys.puts("client disconnected.");
});

