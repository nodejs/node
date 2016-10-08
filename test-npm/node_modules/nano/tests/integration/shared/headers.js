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
var nano = harness.locals.nano;
var db = harness.locals.db;
var it = harness.it;

it('should get headers', function(assert) {
  db.attachment.insert('new', 'att', 'Hello', 'text/plain',
  function(error, hello) {
    assert.equal(error, null, 'should store hello');
    assert.equal(hello.ok, true, 'response should be ok');
    assert.ok(hello.rev, 'should have a revision number');
    nano.request({
      db: 'shared_headers',
      doc: 'new',
      headers: {'If-None-Match': JSON.stringify(hello.rev)}
    }, function(error, helloWorld, rh) {
      assert.equal(error, null, 'should get the hello');
      assert.equal(rh['statusCode'], 304, 'status is not modified');
      nano.request({
        db: 'shared_headers',
        doc: 'new',
        att: 'att'
      }, function(error, helloWorld, rh) {
        assert.equal(error, null, 'should get the hello');
        assert.equal(rh['statusCode'], 200, 'status is `ok`');
        assert.end();
      });
    });
  });
});
