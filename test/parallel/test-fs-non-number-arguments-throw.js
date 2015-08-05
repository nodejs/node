const assert = require('assert');
const fs = require('fs');
var saneEmitter,
    sanity = 'ire(\'assert\')';

saneEmitter = fs.createReadStream(__filename, { start: 19, end: 31 });

assert.throws(function () {
  fs.createReadStream(__filename, { start: "17", end: 29 });
}, "start as string didn't throw an error for createReadStream");

assert.throws(function () {
  fs.createReadStream(__filename, { start: 17, end: "29" });
}, "end as string didn't throw an error");

assert.throws(function () {
  fs.createWriteStream(__filename, { start: "17" });
}, "start as string didn't throw an error for createWriteStream");

saneEmitter.on('data', function (data) {
  // a sanity check when using numbers instead of strings
  assert.strictEqual(sanity, data.toString('utf8'), 'read ' +
                     data.toString('utf8') + ' instead of ' + sanity);
});
