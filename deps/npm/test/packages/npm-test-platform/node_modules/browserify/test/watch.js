var connect = require('connect');
var http = require('http');
var vm = require('vm');
var fs = require('fs');
var path = require('path');
var test = require('tap').test;

test('watch', function (t) {
    t.plan(8);
    
    var port = 10000 + Math.floor(Math.random() * (Math.pow(2,16) - 10000));
    var server = connect.createServer();
    
    var filters = 0;
    
    var bundle = require('../')({
        require : path.resolve(__dirname, 'watch/a.js'),
        mount : '/bundle.js',
        filter : function (src) {
            filters ++;
            if (filters === 2) t.ok(true, 'has 2');
            return src;
        },
        watch : { interval : 100 },
    });
    server.use(bundle);
    
    server.use(connect.static(path.resolve(__dirname, 'watch')));
    
    server.listen(port, function () {
        setTimeout(compareSources, 1000);
    });
    
    function getBundle (cb) {
        var req = { host : 'localhost', port : port, path : '/bundle.js' };
        setTimeout(function () {
            http.get(req, function (res) {
                t.equal(res.statusCode, 200);
                
                var src = '';
                res.on('data', function (buf) {
                    src += buf.toString();
                });
                
                res.on('end', function () {
                    cb(src)
                });
            });
        }, 50);
    }
    
    function compareSources () {
        getBundle(function (s1) {
            var m0 = bundle.modified;
            t.ok(m0);
             
            var c1 = {};
            vm.runInNewContext(s1, c1);
            var a1 = c1.require('./a');
            
            var a2 = Math.floor(Math.random() * 10000);
            var s2_ = bundle.bundle();
            
            getBundle(function (s2) {
                t.notEqual(s1, s2, 'sources are equal');
                
                var c2 = {};
                vm.runInNewContext(s2, c2);
                var a2_ = c2.require('./a');
                
                var m1 = bundle.modified;
                t.ok(m1);
                t.ok(m1 > m0);
                
                fs.writeFileSync(
                    path.resolve(__dirname, 'watch/a.js'),
                    'module.exports = ' + a1
                );
                
                server.close();
                t.deepEqual(a2, a2_);
                t.end();
            });
            
            fs.writeFileSync(
                path.resolve(__dirname, 'watch/a.js'),
                'module.exports = ' + a2
            );
        });
    }
});
