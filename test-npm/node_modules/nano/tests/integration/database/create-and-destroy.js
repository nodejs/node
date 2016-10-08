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

var async = require('async');
var helpers = require('../../helpers/integration');
var harness = helpers.harness(__filename);
var it = harness.it;
var nano = harness.locals.nano;

it('should be able to create `az09_$()+-/` database', function(assert) {
  nano.db.create('az09_$()+-/', function(err) {
    assert.equal(err, null, 'should create database');
    assert.end();
  });
});

it('should be able to use config from a nano object to create a db',
function(assert) {
  var config = helpers.Nano(
    helpers.couch + '/' + encodeURIComponent('with/slash')).config;
  helpers.Nano(config.url).db.create(config.db, function(err) {
    assert.equal(err, null, 'should create database');
    assert.end();
  });
});

it('must destroy the databases we created', function(assert) {
  async.forEach(['az09_$()+-/', 'with/slash'], nano.db.destroy, function(err) {
    assert.equal(err, undefined, 'should destroy all dbs');
    assert.end();
  });
});
