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

var rev;

it('should insert one doc', function(assert) {
  db.insert({'foo': 'baz'}, 'foobar', function(error, foo) {
    assert.equal(error, null, 'stored foo');
    assert.equal(foo.ok, true, 'response ok');
    assert.ok(foo.rev, 'withs rev');
    rev = foo.rev;
    assert.end();
  });
});

it('should update the document', function(assert) {
  db.insert({foo: 'bar', '_rev': rev}, 'foobar', function(error, response) {
    assert.equal(error, null, 'should have deleted foo');
    assert.equal(response.ok, true, 'response should be ok');
    assert.end();
  });
});
