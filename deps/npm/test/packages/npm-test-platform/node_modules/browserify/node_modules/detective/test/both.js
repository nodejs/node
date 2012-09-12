var test = require('tap').test;
var detective = require('../');
var fs = require('fs');
var src = fs.readFileSync(__dirname + '/files/both.js');

test('both', function (t) {
    var modules = detective.find(src);
    t.deepEqual(modules.strings, [ 'a', 'b' ]);
    t.deepEqual(modules.expressions, [ '"c"+x', '"d"+y' ]);
    t.end();
});
