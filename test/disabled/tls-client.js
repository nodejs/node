var common = require('../common');
var tls = require('tls');
var fs = require('fs');


// most servers don't require certificates

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
};


var s = tls.connect(443, "google.com", options, function() {
  console.error("CONNECTED");
  s.pipe(process.stdout);
  process.openStdin().pipe(s);
});

