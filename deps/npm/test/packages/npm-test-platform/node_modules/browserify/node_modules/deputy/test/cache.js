var test = require('tap').test;
var deputy = require('../');
var fs = require('fs');
var crypto = require('crypto');

test('cache with new file', function (t) {
    t.plan(4);
    
    var ps = []; for (var i = 0; i < 10; i++) {
        ps.push(Math.floor(Math.random() * (1<<30)).toString(16));
    }
    var file = '/tmp/' + ps.join('/') + '.json';
    var detective = deputy(file);
    
    var src = [
        [
            'require("a"); require("b")',
            { strings : ['a','b'], expressions : [] }
        ],
        [
            'require("x"); require(x+1)',
            { strings : [ 'x' ], expressions : [ 'x+1' ] }
        ]
    ];
    
    t.deepEqual(detective.find(src[0][0]), src[0][1]);
    setTimeout(firstFile, 100);
    
    function compareSources (ix, cb) {
        fs.readFile(file, function (err, body) {
            t.deepEqual(
                src.slice(0,ix).reduce(function (acc,s) {
                    acc[hash(s[0])] = s[1];
                    return acc;
                }, {}),
                JSON.parse(body)
            );
            cb();
        });
    }
    
    function firstFile () {
        compareSources(1, function () {
            t.deepEqual(detective.find(src[1][0]), src[1][1]);
            setTimeout(secondFile, 100);
        });
    }
    
    function secondFile () {
        compareSources(2, function () {
            t.end();
        });
    }
    
    function hash (src) {
        return new crypto.Hash('md5').update(src).digest('hex');
    }
});
