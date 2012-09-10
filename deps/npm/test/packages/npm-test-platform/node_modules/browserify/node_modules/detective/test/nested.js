var test = require('tap').test;
var detective = require('../');
var fs = require('fs');
var src = fs.readFileSync(__dirname + '/files/nested.js');

test('nested', function (t) {
    t.deepEqual(detective(src), [ 'a', 'b', 'c' ]);
    t.end();
});
