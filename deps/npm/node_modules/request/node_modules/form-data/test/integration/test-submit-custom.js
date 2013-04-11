var common = require('../common');
var assert = common.assert;
var http = require('http');
var path = require('path');
var mime = require('mime');
var request = require('request');
var fs = require('fs');
var FormData = require(common.dir.lib + '/form_data');
var IncomingForm = require('formidable').IncomingForm;

var remoteFile = 'http://nodejs.org/images/logo.png';

// wrap non simple values into function
// just to deal with ReadStream "autostart"
// Can't wait for 0.10
var FIELDS = [
  {name: 'my_field', value: 'my_value'},
  {name: 'my_buffer', value: function(){ return new Buffer([1, 2, 3])} },
  {name: 'my_file', value: function(){ return fs.createReadStream(common.dir.fixture + '/unicycle.jpg')} },
  {name: 'remote_file', value: function(){ return request(remoteFile)} }
];

var server = http.createServer(function(req, res) {

  // formidable is fixed on github
  // but still 7 month old in npm
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

    // 3rd field : my_file
    var field = FIELDS.shift();
    assert.ok( data.indexOf('form-data; name="'+field.name+'"') != -1 );
    assert.ok( data.indexOf('; filename="'+path.basename(field.value.path)+'"') != -1 );
    // check for unicycle.jpg traces
    assert.ok( data.indexOf('2005:06:21 01:44:12') != -1 );
    assert.ok( data.indexOf('Content-Type: '+mime.lookup(field.value.path) ) != -1 );

    // 4th field : remote_file
    var field = FIELDS.shift();
    assert.ok( data.indexOf('form-data; name="'+field.name+'"') != -1 );
    assert.ok( data.indexOf('; filename="'+path.basename(field.value.path)+'"') != -1 );
    // check for http://nodejs.org/images/logo.png traces
    assert.ok( data.indexOf('ImageReady') != -1 );
    assert.ok( data.indexOf('Content-Type: '+mime.lookup(remoteFile) ) != -1 );

    res.writeHead(200);
    res.end('done');

  });

});

server.listen(common.port, function() {

  var form = new FormData();

  FIELDS.forEach(function(field) {
    // important to append ReadStreams within the same tick
    if ((typeof field.value == 'function')) {
      field.value = field.value();
    }
    form.append(field.name, field.value);
  });

  // custom params object passed to submit
  form.submit({
    port: common.port,
    path: '/'
  }, function(err, res) {

    if (err) {
      throw err;
    }

    assert.strictEqual(res.statusCode, 200);
    server.close();
  });

});

process.on('exit', function() {
  assert.strictEqual(FIELDS.length, 0);
});
