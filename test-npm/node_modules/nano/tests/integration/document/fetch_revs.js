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
var it = harness.it;

it('should insert a bunch of items', helpers.insertThree);

it('should be able to fetch with one key', function(assert) {
  db.fetchRevs({keys:['foobar']}, function(error, docs) {
    assert.equal(error, null, 'should work');
    assert.equal(docs.rows.length, 1, 'and get one row');
    assert.equal(docs['total_rows'], 3, 'out of 3');
    assert.equal(docs.rows[0].doc, undefined, 'rev should not return key');
    assert.end();
  });
});

it('should be able to fetch with multiple keys', function(assert) {
  db.fetchRevs({keys:['foobar', 'barfoo']}, function(error, docs) {
    assert.equal(error, null, 'it works');
    assert.equal(docs.rows.length, 2, 'two rows');
    assert.equal(docs['total_rows'], 3, 'out of 3');
    assert.equal(docs.rows[0].doc, undefined, 'no doc in 1');
    assert.equal(docs.rows[1].doc, undefined, 'no doc in 2');
    assert.end();
  });
});

it('should be able to fetch with params', function(assert) {
  db.fetchRevs({keys:['foobar']}, {still: 'no'}, function(error, docs) {
    assert.equal(error, null, 'should work');
    assert.equal(docs.rows.length, 1, 'and get one row');
    assert.equal(docs['total_rows'], 3, 'out of 3');
    assert.equal(docs.rows[0].doc, undefined, 'rev should not return key');
    assert.end();
  });
});
