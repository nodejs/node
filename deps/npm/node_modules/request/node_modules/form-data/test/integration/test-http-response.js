var common = require('../common');
var assert = common.assert;
var http = require('http');
var path = require('path');
var mime = require('mime');
var request = require('request');
var parseUrl = require('url').parse;
var fs = require('fs');
var FormData = require(common.dir.lib + '/form_data');
var IncomingForm = require('formidable').IncomingForm;

var remoteFile = 'http://nodejs.org/images/logo.png';

var FIELDS;
var server;

var parsedUrl = parseUrl(remoteFile)
  , options = {
      method: 'get',
      port: parsedUrl.port || 80,
      path: parsedUrl.pathname,
      host: parsedUrl.hostname
    }
  ;

http.request(options, function(res) {

  FIELDS = [
    {name: 'my_field', value: 'my_value'},
    {name: 'my_buffer', value: new Buffer([1, 2, 3])},
    {name: 'remote_file', value: res }
  ];

  var form = new FormData();
  FIELDS.forEach(function(field) {
    form.append(field.name, field.value);
  });

  server.listen(common.port, function() {

    form.submit('http://localhost:' + common.port + '/', function(err, res) {

      if (err) {
        throw err;
      }

      assert.strictEqual(res.statusCode, 200);
      server.close();
    });

  });


}).end();

server = http.createServer(function(req, res) {

  // formidable is broken so let's do it manual way
  //
  // var form = new IncomingForm();
  // form.uploadDir = common.dir.tmp;
  //  form.parse(req);
  // form
  //   .on('field', function(name, value) {
  //     var field = FIELDS.shift();
  //     assert.strictEqual(name, field.name);
  //     assert.strictEqual(value, field.value+'');
  //   })
  //   .on('file', function(name, file) {
  //     var field = FIELDS.shift();
  //     assert.strictEqual(name, field.name);
  //     assert.strictEqual(file.name, path.basename(field.value.path));
  //     // mime.lookup file.NAME == 'my_file' ?
  //     assert.strictEqual(file.type, mime.lookup(file.name));
  //   })
  //   .on('end', function() {
  //     res.writeHead(200);
  //     res.end('done');
  //   });

  // temp workaround
  var data = '';
  req.setEncoding('utf8');

  req.on('data', function(d) {
    data += d;
  });

  req.on('end', function() {

    // check for the fields' traces

    // 1st field : my_field
    var field = FIELDS.shift();
    assert.ok( data.indexOf('form-data; name="'+field.name+'"') != -1 );
    assert.ok( data.indexOf(field.value) != -1 );

    // 2nd field : my_buffer
    var field = FIELDS.shift();
    assert.ok( data.indexOf('form-data; name="'+field.name+'"') != -1 );
    assert.ok( data.indexOf(field.value) != -1 );

    // 3rd field : remote_file
    var field = FIELDS.shift();
    assert.ok( data.indexOf('form-data; name="'+field.name+'"') != -1 );
    assert.ok( data.indexOf('; filename="'+path.basename(remoteFile)+'"') != -1 );
    // check for http://nodejs.org/images/logo.png traces
    assert.ok( data.indexOf('ImageReady') != -1 );
    assert.ok( data.indexOf('Content-Type: '+mime.lookup(remoteFile) ) != -1 );

    res.writeHead(200);
    res.end('done');

  });

});


process.on('exit', function() {
  assert.strictEqual(FIELDS.length, 0);
});
