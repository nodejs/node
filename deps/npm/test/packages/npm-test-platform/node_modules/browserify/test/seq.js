var connect = require('connect');
var http = require('http');
var vm = require('vm');
var browserify = require('../');
var test = require('tap').test;

test('seq', function (t) {
    t.plan(2);
    
    var port = 10000 + Math.floor(Math.random() * (Math.pow(2,16) - 10000));
    var server = connect.createServer();
    
    server.use(browserify({
        mount : '/bundle.js',
        require : [ 'seq' ],
    }));
    
    server.listen(port, makeRequest);
    
    function makeRequest () {
        var req = { host : 'localhost', port : port, path : '/bundle.js' };
        http.get(req, function (res) {
            t.equal(res.statusCode, 200);
            server.close();
            
            var context = { setTimeout : setTimeout };
            
            var src = '';
            res.on('data', function (buf) {
                src += buf.toString();
            });
            
            res.on('end', function () {
                vm.runInNewContext(src, context);
                context.require('seq')([1,2,3])
                    .parMap(function (x) {
                        this(null, x * 100)
                    })
                    .seq(function () {
                        t.deepEqual(this.stack, [100,200,300]);
                        t.end();
                    })
                ;
            });
        });
    }
});
