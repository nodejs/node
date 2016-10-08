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

var helpers = require('../../helpers/integration');
var pixel = helpers.pixel;
var harness = helpers.harness(__filename);
var db = harness.locals.db;
var it = harness.it;

var rev;

it('should be able to insert and update attachments', function(assert) {
  var buffer = new Buffer(pixel, 'base64');
  db.attachment.insert('new', 'att', 'Hello', 'text/plain',
  function(error, hello) {
    assert.equal(error, null, 'should store hello');
    assert.equal(hello.ok, true, 'response ok');
    assert.ok(hello.rev, 'should have a revision');
    db.attachment.insert('new', 'att', buffer, 'image/bmp',
    {rev: hello.rev}, function(error, bmp) {
      assert.equal(error, null, 'should store the pixel');
      assert.ok(bmp.rev, 'should store a revision');
      rev = bmp.rev;
      assert.end();
    });
  });
});

it('should be able to fetch the updated pixel', function(assert) {
  db.get('new', function(error, newDoc) {
    assert.equal(error, null, 'should get new');
    newDoc.works = true;
    db.insert(newDoc, 'new', function(error, response) {
      assert.equal(error, null, 'should update doc');
      assert.equal(response.ok, true, 'response ok');
      assert.end();
    });
  });
});
