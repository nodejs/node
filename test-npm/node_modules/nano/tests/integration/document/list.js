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
var db = harness.locals.db;
var nano = harness.locals.nano;
var it = harness.it;

it('should insert a bunch of items', helpers.insertThree);

it('should list the three documents', function(assert) {
  db.list(function(error, docs) {
    assert.equal(error, null, 'should get list');
    assert.equal(docs['total_rows'], 3, 'with total three rows');
    assert.ok(docs.rows, 'and the rows themselves');
    assert.end();
  });
});

it('should be able to list using the `relax` function', function(assert) {
  nano.relax({
    db: 'document_list',
    doc: '_all_docs',
    method: 'GET',
    qs: {limit: 1}
  }, function(error, docs) {
    assert.equal(error, null, 'not relaxed');
    assert.ok(docs.rows, 'got meh rows');
    assert.equal(docs.rows.length, 1, 'but its only one #sadpanda');
    assert.equal(docs['total_rows'], 3, 'out of three');
    assert.end();
  });
});

it('should be able to list with a startkey', function(assert) {
  db.list({startkey: 'c'}, function(error, docs) {
    assert.equal(error, null, 'should work');
    assert.ok(docs.rows, 'get teh rows');
    assert.equal(docs.rows.length, 2, 'starts in row two');
    assert.equal(docs['total_rows'], 3, 'out of three rows');
    assert.equal(docs.offset, 1, 'offset is 1');
    assert.end();
  });
});
