// Licensed under the Apache License, Version 2.0 (the 'License'); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

'use strict';

var fs = require('fs');
var path = require('path');
var helpers = require('../../helpers/integration');
var harness = helpers.harness(__filename);
var db = harness.locals.db;
var it = harness.it;
var pixel = helpers.pixel;

it('should be able to pipe to a writeStream', function(assert) {
  var buffer = new Buffer(pixel, 'base64');
  var filename = path.join(__dirname, '.temp.bmp');
  var ws = fs.createWriteStream(filename);

  ws.on('close', function() {
    assert.equal(fs.readFileSync(filename).toString('base64'), pixel);
    fs.unlinkSync(filename);
    assert.end();
  });

  db.attachment.insert('new', 'att', buffer, 'image/bmp',
  function(error, bmp) {
    assert.equal(error, null, 'Should store the pixel');
    db.attachment.get('new', 'att', {rev: bmp.rev}).pipe(ws);
  });
});

it('should be able to pipe from a readStream', function(assert) {
  var logo = path.join(__dirname, '..', '..', 'fixtures', 'logo.png');
  var rs = fs.createReadStream(logo);
  var is = db.attachment.insert('nodejs', 'logo.png', null, 'image/png');

  is.on('end', function() {
    db.attachment.get('nodejs', 'logo.png', function(err, buffer) {
      assert.equal(err, null, 'should get the logo');
      assert.equal(
        fs.readFileSync(logo).toString('base64'), buffer.toString('base64'),
        'logo should remain unchanged');
      assert.end();
    });
  });

  rs.pipe(is);
});
