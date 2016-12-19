var Parser = require('../jsonparse');
var Http = require('http');
require('./colors');
var p = new Parser();
var cred = require('./credentials');
var client = Http.createClient(80, "stream.twitter.com");
var request = client.request("GET", "/1/statuses/sample.json", {
  "Host": "stream.twitter.com",
  "Authorization": (new Buffer(cred.username + ":" + cred.password)).toString("base64")
});
request.on('response', function (response) {
  console.log(response.statusCode);
  console.dir(response.headers);
  response.on('data', function (chunk) {
    p.write(chunk);
  });
  response.on('end', function () {
    console.log("END");
  });
});
request.end();
var text = "", name = "";
p.onValue = function (value) {
  if (this.stack.length === 1 && this.key === 'text') { text = value; }
  if (this.stack.length === 2 && this.key === 'name' && this.stack[1].key === 'user') { name = value; }
  if (this.stack.length === 0) {
    console.log(text.blue + " - " + name.yellow);
    text = name = "";
  }
};
