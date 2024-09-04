const util = require('node:util');
const assert = require('node:assert');
assert.ok(util.getCallSite().length > 1);
process.stdout.write(util.getCallSite()[0].scriptName);
