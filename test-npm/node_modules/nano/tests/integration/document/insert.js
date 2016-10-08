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

it('should insert one simple document', function(assert) {
  db.insert({'foo': 'baz'}, 'foobaz', function(error, foo) {
    rev = foo.rev;
    assert.equal(error, null, 'should have stored foo');
    assert.equal(foo.ok, true, 'response should be ok');
    assert.ok(foo.rev, 'response should have rev');
    assert.end();
  });
});

it('should fail to insert again since it already exists', function(assert) {
  db.insert({}, 'foobaz', function(error) {
    assert.equal(error['statusCode'], 409, 'should be conflict');
    assert.equal(error.scope, 'couch', 'scope is couch');
    assert.equal(error.error, 'conflict', 'type is conflict');
    assert.end();
  });
});

it('should be able to use custom params in insert', function(assert) {
  db.insert({
    foo: 'baz',
    _rev: rev
  }, {
    docName: 'foobaz',
    'new_edits': false
  }, function(error, foo) {
    assert.equal(error, null, 'should have stored foo');
    assert.equal(foo.ok, true, 'response should be ok');
    assert.ok(foo.rev, 'response should have rev');
    assert.end();
  });
});

it('should be able to insert functions in docs', function(assert) {
  db.insert({
    fn: function() { return true; },
    fn2: 'function () { return true; }'
  }, function(error, fns) {
    assert.equal(error, null, 'should have stored foo');
    assert.equal(fns.ok, true, 'response should be ok');
    assert.ok(fns.rev, 'response should have rev');
    db.get(fns.id, function(error, fns) {
      assert.equal(fns.fn, fns.fn2, 'fn matches fn2');
      assert.equal(error, null, 'should get foo');
      assert.end();
    });
  });
});

it('should be able to stream an insert', function(assert) {
  var buffer = '';
  var foobar = db.insert({'foo': 'bar'});

  function runAssertions(error, foobar) {
    assert.equal(error, null, 'should have stored foobar');
    assert.ok(foobar.ok, 'this is ok');
    assert.ok(foobar.rev, 'and i got revz');
    assert.end();
  }

  foobar.on('data', function(chunk) { buffer += chunk; });
  foobar.on('end', function() { runAssertions(null, JSON.parse(buffer)); });
  foobar.on('error', runAssertions);
});
