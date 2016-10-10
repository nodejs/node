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
var Nano = helpers.Nano;
var it = harness.it;

var admin = Nano(helpers.admin);
var cookie;
var server;

it('should be able to setup admin and login', function(assert) {
  nano.relax({
    method : 'PUT',
    path: '_config/admins/' + helpers.username,
    body: helpers.password
  }, function(err) {
    assert.equal(err, null, 'should create admin');
    nano.auth(helpers.username, helpers.password, function(err, _, headers) {
      assert.equal(err, null, 'should have logged in successfully');
      assert.ok(headers['set-cookie'],
        'response should have a set-cookie header');
      cookie = headers['set-cookie'];
      assert.end();
    });
  });
});

it('should be able to insert with a cookie', function(assert) {
  server = Nano({
    url: helpers.couch,
    cookie: cookie
  });
  var db = server.use('shared_cookie');

  db.insert({'foo': 'baz'}, null, function(error, response) {
    assert.equal(error, null, 'should have stored value');
    assert.equal(response.ok, true, 'response should be ok');
    assert.ok(response.rev, 'response should have rev');
    assert.end();
  });
});

it('should be able to get the session', function(assert) {
  server.session(function(error, session) {
    assert.equal(error, null, 'should have gotten the session');
    assert.equal(session.userCtx.name, helpers.username);
    assert.end();
  });
});

it('must restore admin parteh mode for other tests', function(assert) {
  admin.relax({
    method: 'DELETE',
    path: '_config/admins/' + helpers.username
  }, function(err) {
    assert.equal(err, null, 'should have deleted admin user');
    assert.end();
  });
});
