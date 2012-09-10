var vm = require('vm');
var fs = require('fs');
var browserify = require('../');
var test = require('tap').test;
var crypto = require('crypto');

test('cache', function (t) {
    t.plan(2);
    
    var file = '/tmp/' + Math.floor(Math.random() * (1<<30)).toString(16) + '.json';
    browserify({ cache : file }).require('seq').bundle();
    setTimeout(function () {
        fs.readFile(file, function (err, body) {
            if (err) t.fail(err);
            var cache = JSON.parse(body);
            var tfile = require.resolve('seq');
            fs.readFile(tfile, function (err, body) {
                var hash = new crypto.Hash('md5').update(body).digest('hex');
                t.ok(cache[hash]);
                t.deepEqual(
                    cache[hash].strings.sort(),
                    [ 'events', 'hashish', 'chainsaw' ].sort()
                );
                t.end();
            });
        });
    }, 200);
});
