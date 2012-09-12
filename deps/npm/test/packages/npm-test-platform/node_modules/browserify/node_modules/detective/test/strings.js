var test = require('tap').test;
var detective = require('../');
var fs = require('fs');
var src = fs.readFileSync(__dirname + '/files/strings.js');

test('single', function (t) {
    t.deepEqual(detective(src), [ 'a', 'b', 'c', 'events', 'doom', 'y', 'events2' ]);
    t.end();
});
