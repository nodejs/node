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
var harness = helpers.harness(__filename);
var it = harness.it;
var db = harness.locals.db;

it('should be able to insert a new plain text attachment', function(assert) {
  db.attachment.insert('new',
  'att', 'Hello World!', 'text/plain', function(error, att) {
    assert.equal(error, null, 'store the attachment');
    assert.equal(att.ok, true, 'response ok');
    assert.ok(att.rev, 'have a revision number');
    db.attachment.destroy('new', 'att', {rev: att.rev},
    function(error, response) {
      assert.equal(error, null, 'delete the attachment');
      assert.equal(response.ok, true, 'response ok');
      assert.equal(response.id, 'new', '`id` should be `new`');
      assert.end();
    });
  });
});

it('should fail destroying with a bad filename', function(assert) {
  db.attachment.destroy('new', false, true, function(error, response) {
    assert.equal(response, undefined, 'no response should be given');
    assert.end();
  });
});
