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
var nano = harness.locals.nano;

it('should list the correct databases', function(assert) {
  nano.db.list(function(error, list) {
    assert.equal(error, null, 'should list databases');
    var filtered = list.filter(function(e) {
      return e === 'database_list' || e === '_replicator' || e === '_users';
    });
    assert.equal(filtered.length, 3, 'should have exactly 3 dbs');
    assert.end();
  });
});
