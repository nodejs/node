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

var rev;

it('should insert a document', function(assert) {
  db.insert({'foo': 'baz'}, 'foobaz', function(error, foo) {
    assert.equal(error, null, 'stores foo');
    assert.equal(foo.ok, true, 'ok response');
    assert.ok(foo.rev, 'response with rev');
    rev = foo.rev;
    assert.end();
  });
});

it('should delete a document', function(assert) {
  db.destroy('foobaz', rev, function(error, response) {
    assert.equal(error, null, 'deleted foo');
    assert.equal(response.ok, true, 'ok!');
    assert.end();
  });
});
