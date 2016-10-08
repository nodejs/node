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

it('should store and delete `goofy`', function(assert) {
  db.insert({'foo': 'baz'}, 'goofy', function(error, foo) {
    assert.equal(error, null, 'should have stored foo');
    assert.equal(foo.ok, true, 'response should be ok');
    db.destroy('goofy', foo.rev, function(error, response) {
      assert.equal(error, null, 'should have deleted foo');
      assert.equal(response.ok, true, 'response ok');
      assert.end();
    });
  });
});

it('should have run the compaction', function(assert) {
  db.compact(function(error) {
    assert.equal(error, null, 'compact should respond');
    db.info(function(error, info) {
      assert.equal(error, null, 'info should respond');
      assert.equal(info['doc_count'], 0, 'document count is not 3');
      assert.equal(info['doc_del_count'], 1, 'document should be deleted');
      assert.equal(info['update_seq'], 2, 'seq is two');
      assert.end();
    });
  });
});

it('should finish compaction before ending', function(assert) {
  function nextWhenFinished() {
    db.info(function(err, info) {
      if (err) {
        return;
      }
      if (info['compact_running']) {
        return;
      }
      clearTimeout(task);
      assert.pass('compaction is complete');
      assert.end();
    });
  }

  var task = setInterval(nextWhenFinished, 100);
});
