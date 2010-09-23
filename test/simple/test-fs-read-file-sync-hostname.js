common = require("../common");
assert = common.assert
var fs = require('fs');

// test reading from hostname
if (process.platform === "linux2") {
  var hostname = fs.readFileSync('/proc/sys/kernel/hostname');
  assert.ok(hostname.length > 0);
}
