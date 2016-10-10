
/**
 * Module dependencies.
 */

var assert = require('assert')
  , http = require('http');

var server = http.createServer(function(req, res){
    if (req.method === 'GET') {
        if (req.url === '/delay') {
            setTimeout(function(){
                res.writeHead(200, {});
                res.end('delayed');
            }, 200);
        } else {
            var body = JSON.stringify({ name: 'tj' });
            res.writeHead(200, {
                'Content-Type': 'application/json; charset=utf8',
                'Content-Length': body.length
            });
            res.end(body);
        }
    } else {
        var body = '';
        req.setEncoding('utf8');
        req.addListener('data', function(chunk){ body += chunk });
        req.addListener('end', function(){
            res.writeHead(200, {});
            res.end(req.url + ' ' + body);
        });
    }
});

module.exports = {
    'test assert.response()': function(done){
        assert.response(server, {
            url: '/',
            method: 'GET'
        },{
            body: '{"name":"tj"}',
            status: 200,
            headers: {
                'Content-Type': 'application/json; charset=utf8'
            }
        }, done);
    }
};