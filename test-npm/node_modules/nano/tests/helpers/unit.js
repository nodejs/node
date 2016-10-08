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

var helpers = require('./');
var Client = require('../../lib/nano');
var test  = require('tape');
var _ = require('underscore');

helpers.unit = function(method, error) {
  var unitName = 'nano/tests/unit/' + method.join('/');
  var debug = require('debug')(unitName);

  function log(data) {
    debug({ got: data.body });
  }

  var cli = helpers.mockClientOk(log, error);

  //
  // allow database creation and other server stuff
  //
  if(method[1].match(/follow/)) {
    if(method[0] === 'database') {
      cli.server = helpers.mockClientFollow(log, error);
    } else {
      cli = helpers.mockClientFollow(log, error);
    }
  } else {
    cli.server = helpers.mockClientDb(log, error);
  }

  var testNr = 1;

  return function() {
    var args = Array.prototype.slice.call(arguments);
    var stub = args.pop();

    test(unitName + ':' + testNr++,
    function(assert) {
      var f;
      assert.ok(typeof stub, 'object');

      //
      // document functions and database functions
      // are at the top level in nano
      //
      if(method[0] === 'database') {
        f = cli.server.db[method[1]];
      } else if(method[0] === 'view' && method[1] === 'compact') {
        f = cli.view.compact;
      } else if(!~['multipart', 'attachment'].indexOf(method[0])) {
        f = cli[method[1]];
      } else {
        f = cli[method[0]][method[1]];
      }
      assert.ok(typeof f, 'function');

      args.push(function(err, req, response) {
        if (error) {
          assert.ok(err);
          return assert.end();
        }

        assert.equal(response.statusCode, 200);
        if(stub.uri) {
          stub.uri = helpers.couch + stub.uri;
        } else {
          stub.db = helpers.couch + stub.db;
        }

        assert.deepEqual(req, stub);
        assert.end();
      });

      f.apply(null, args);
    });
  };
};

function mockClient(code, path, extra) {
  return function(debug, error) {
    extra = extra || {};
    var opts = _.extend(extra, {
      url: helpers.couch + path,
      log: debug,
      request: function(req, cb) {
        if(error) {
          return cb(error);
        }

        if(code === 500) {
          cb(new Error('omg connection failed'));
        } else {
          cb(null, {
            statusCode: code,
            headers: {}
          }, req); }
        }
    });

    return Client(opts);
  };
}

function mockClientUnparsedError() {
  return function(debug, body) {
    return Client({
      url: helpers.couch,
      log: debug,
      request: function(_, cb) {
        return cb(null, {statusCode: 500}, body || '<b> Error happened </b>');
      }
    });
  };
}

function mockClientFollow() {
  return function(debug, error) {
    return Client({
      url: helpers.couch,
      log: debug,
      follow: function(qs, cb) {
        if(error) {
          return cb(error);
        }

        return cb(null, qs,  {statusCode: 200});
      }
    });
  };
}


helpers.mockClientFollow = mockClientFollow();
helpers.mockClientUnparsedError = mockClientUnparsedError();
helpers.mockClientDb = mockClient(200, '');
helpers.mockClientOk = mockClient(200, '/mock');
helpers.mockClientFail = mockClient(500, '');
helpers.mockClientJar = mockClient(300, '', {jar: 'is set'});
module.exports = helpers;
