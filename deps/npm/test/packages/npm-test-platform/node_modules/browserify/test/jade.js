var browserify = require('../');
var vm = require('vm');
var jade = require('jade');
var test = require('tap').test;

test('jade', function (t) {
    t.plan(5);
    var b = browserify({
        require : 'jade',
        ignore : [
            'coffee-script', 'less', 'sass', 'stylus', 'markdown', 'discount',
            'markdown-js'
        ]
    });
    var src = b.bundle();
    
    t.ok(typeof src === 'string');
    t.ok(src.length > 0);
    
    var c = { console : console };
    vm.runInNewContext(src, c);
    var j = c.require('jade');
    t.deepEqual(
        Object.keys(jade),
        Object.keys(j)
    );
    
    jade.render('div #{x}\n  span moo', { x : 42 }, function (err, r0) {
        t.equal(r0, '<div>42<span>moo</span></div>');
        jade.render('div #{x}\n  span moo', { x : 42 }, function (err, r1) {
            t.equal(r0, r1);
            t.end();
        });
    });
});
