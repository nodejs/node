var test = require('tape');
var tryit = require('../tryit');


test('basic functionality', function (t) {
    var count = 0;

    var noOp = function () {};
    var throwsError = function () {
        throw new Error('whammo');
    }

    tryit(noOp, function (e) {
        t.ok(e == null, 'should be called without an error');
    });

    tryit(throwsError, function (e) {
        t.ok('should be called');
        t.ok(e instanceof Error);
    });

    t.end();
});

test('handle case where callback throws', function (t) {
    var count = 0;

    t.throws(function () {
        tryit(function () {}, function(e) {
            count++;
            t.equal(count, 1, 'should be called once');
            throw new Error('kablowie');
        });
    }, 'should throw once');

    t.end();
});
