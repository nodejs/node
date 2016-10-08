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

it('should be able to insert three documents', helpers.insertThree);

it('should be able to receive changes since seq:2', function(assert) {
  db.changes({since:2}, function(error, response) {
    assert.equal(error, null, 'gets response from changes');
    assert.equal(response.results.length, 1, 'gets one result');
    assert.equal(response['last_seq'], 3, 'seq is 3');
    assert.end();
  });
});
