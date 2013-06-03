/*
test custom filename and content-type:
re: https://github.com/felixge/node-form-data/issues/29
*/

var common       = require('../common');
var assert       = common.assert;
var http         = require('http');
var fs           = require('fs');

var FormData     = require(common.dir.lib + '/form_data');
var IncomingForm = require('formidable').IncomingForm;

var options = {
  filename: 'test.png',
  contentType: 'image/gif'
};

var server = http.createServer(function(req, res) {

  var form = new IncomingForm({uploadDir: common.dir.tmp});

  form.parse(req);

  form
    .on('file', function(name, file) {
      assert.strictEqual(name, 'my_file');
      assert.strictEqual(file.name, options.filename);
      assert.strictEqual(file.type, options.contentType);
    })
    .on('end', function() {
      res.writeHead(200);
      res.end('done');
    });
});


server.listen(common.port, function() {
  var form = new FormData();

  form.append('my_file', fs.createReadStream(common.dir.fixture + '/unicycle.jpg'), options);

  form.submit('http://localhost:' + common.port + '/', function(err, res) {
    if (err) {
      throw err;
    }

    assert.strictEqual(res.statusCode, 200);
    server.close();
  });

});
