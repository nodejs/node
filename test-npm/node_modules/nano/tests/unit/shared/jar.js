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

var helpers = require('../../helpers/unit');
var test  = require('tape');
var debug = require('debug')('nano/tests/unit/shared/jar');

var cli = helpers.mockClientJar(debug);

test('it should be able to set a jar box', function(assert) {
  assert.equal(cli.config.jar, 'is set');
  cli.relax({}, function(_, req) {
    assert.equal(req.jar, 'is set');
    cli.relax({jar: 'changed'}, function(_, req) {
      assert.equal(req.jar, 'changed');
      assert.end();
    });
  });
});
