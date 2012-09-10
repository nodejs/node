var connect = require('connect');
var http = require('http');
var vm = require('vm');
var browserify = require('../');

var test = require('tap').test;

test('alias', function (t) {
    t.plan(5);
    var port = 10000 + Math.floor(Math.random() * (Math.pow(2,16) - 10000));
    var server = connect.createServer();
    
    server.use(browserify({
        mount : '/bundle.js',
        require : { moo : 'seq' }
    }));
    
    server.listen(port, makeRequest);
    
    function makeRequest () {
        
        var req = { host : 'localhost', port : port, path : '/bundle.js' };
        http.get(req, function (res) {
            t.equal(res.statusCode, 200);
            server.close();
            
            var context = {
                setTimeout : setTimeout,
                console : console
            };
            
            var src = '';
            res.on('data', function (buf) {
                src += buf.toString();
            });
            
            res.on('end', function () {
                vm.runInNewContext(src, context);
                t.ok(context.require('moo'));
                t.ok(context.require('seq'));
                t.equal(context.require('seq'), context.require('moo'));
                
                context.require('moo')([1,2,3])
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
