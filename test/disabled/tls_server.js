require('../common');

var sys=require('sys');
var net=require('net');
var fs=require('fs');
var crypto=require('crypto');

var keyPem = fs.readFileSync(fixturesDir + "/cert.pem");
var certPem = fs.readFileSync(fixturesDir + "/cert.pem");

var credentials = crypto.createCredentials({key:keyPem, cert:certPem});
var i = 0;
var server = net.createServer(function (connection) {
  connection.setSecure(credentials);
  connection.setEncoding("binary");

  connection.addListener("secure", function () {
    //sys.puts("Secure");
  });

  connection.addListener("data", function (chunk) {
    sys.puts("recved: " + JSON.stringify(chunk));
    connection.write("HTTP/1.0 200 OK\r\nContent-type: text/plain\r\nContent-length: 9\r\n\r\nOK : "+i+"\r\n\r\n");
    i=i+1;
    connection.end();
  });

  connection.addListener("end", function () {
    connection.end();
  });

});
server.listen(4443);


