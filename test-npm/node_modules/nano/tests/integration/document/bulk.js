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

it('should be able to bulk insert two docs', function(assert) {
  db.bulk({
    'docs': [
      {'key':'baz', 'name':'bazzel'},
      {'key':'bar', 'name':'barry'}
    ]
  },
  function(error, response) {
    assert.equal(error, null, 'no error');
    assert.equal(response.length, 2, 'has two docs');
    assert.ok(response[0].id, 'first got id');
    assert.ok(response[1].id, 'other also got id');
    assert.end();
  });
});
