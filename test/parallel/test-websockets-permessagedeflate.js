'use strict';
const PerMessageDeflate = require('../../lib/internal/websockets/PerMessageDeflate');
const Extensions = require('../../lib/internal/websockets/Extensions');
const assert = require('assert');

/*'PerMessageDeflate'*/
{
  /*'#ctor'*/
  {
    /*'throws TypeError when called without new'*/
    {
      try {
        var perMessageDeflate = PerMessageDeflate();
      }
      catch (e) {
        assert.ok(e instanceof TypeError);
      }
    }
  }

  /*'#offer'*/
  {
    /*'should create default params'*/
    {
      var perMessageDeflate = new PerMessageDeflate();
      assert.deepEqual(perMessageDeflate.offer(), { client_max_window_bits: true });
    }

    /*'should create params from options'*/
    {
      var perMessageDeflate = new PerMessageDeflate({
        serverNoContextTakeover: true,
        clientNoContextTakeover: true,
        serverMaxWindowBits: 10,
        clientMaxWindowBits: 11
      });
      assert.deepEqual(perMessageDeflate.offer(), {
        server_no_context_takeover: true,
        client_no_context_takeover: true,
        server_max_window_bits: 10,
        client_max_window_bits: 11
      });
    }
  }

  /*'#accept'*/
  {
    /*'as server'*/
    {
      /*'should accept empty offer'*/
      {
        var perMessageDeflate = new PerMessageDeflate({}, true);
        assert.deepEqual(perMessageDeflate.accept([{}]), {});
      }

      /*'should accept offer'*/
      {
        var perMessageDeflate = new PerMessageDeflate({}, true);
        var extensions = Extensions.parse('permessage-deflate; server_no_context_takeover; client_no_context_takeover; server_max_window_bits=10; client_max_window_bits=11');
        assert.deepEqual(perMessageDeflate.accept(extensions['permessage-deflate']), {
          server_no_context_takeover: true,
          client_no_context_takeover: true,
          server_max_window_bits: 10,
          client_max_window_bits: 11
        });
      }

      /*'should prefer configuration than offer'*/
      {
        var perMessageDeflate = new PerMessageDeflate({
          serverNoContextTakeover: true,
          clientNoContextTakeover: true,
          serverMaxWindowBits: 12,
          clientMaxWindowBits: 11
        }, true);
        var extensions = Extensions.parse('permessage-deflate; server_max_window_bits=14; client_max_window_bits=13');
        assert.deepEqual(perMessageDeflate.accept(extensions['permessage-deflate']), {
          server_no_context_takeover: true,
          client_no_context_takeover: true,
          server_max_window_bits: 12,
          client_max_window_bits: 11
        });
      }

      /*'should fallback'*/
      {
        var perMessageDeflate = new PerMessageDeflate({ serverMaxWindowBits: 11 }, true);
        var extensions = Extensions.parse('permessage-deflate; server_max_window_bits=10, permessage-deflate');
        assert.deepEqual(perMessageDeflate.accept(extensions['permessage-deflate']), {
          server_max_window_bits: 11
        });
      }

      /*'should throw an error if server_no_context_takeover is unsupported'*/
      {
        var perMessageDeflate = new PerMessageDeflate({ serverNoContextTakeover: false }, true);
        var extensions = Extensions.parse('permessage-deflate; server_no_context_takeover');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }))
      }

      /*'should throw an error if server_max_window_bits is unsupported'*/
      {
        var perMessageDeflate = new PerMessageDeflate({ serverMaxWindowBits: false }, true);
        var extensions = Extensions.parse('permessage-deflate; server_max_window_bits=10');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      }

      /*'should throw an error if server_max_window_bits is less than configuration'*/
      {
        var perMessageDeflate = new PerMessageDeflate({ serverMaxWindowBits: 11 }, true);
        var extensions = Extensions.parse('permessage-deflate; server_max_window_bits=10');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      }

      /*'should throw an error if client_max_window_bits is unsupported on client'*/
      {
        var perMessageDeflate = new PerMessageDeflate({ clientMaxWindowBits: 10 }, true);
        var extensions = Extensions.parse('permessage-deflate');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      }
    }

    /*'as client'*/
    {
      /*'should accept empty response'*/
      {
        var perMessageDeflate = new PerMessageDeflate({});
        assert.deepEqual(perMessageDeflate.accept([{}]), {});
      }

      /*'should accept response parameter'*/
      {
        var perMessageDeflate = new PerMessageDeflate({});
        var extensions = Extensions.parse('permessage-deflate; server_no_context_takeover; client_no_context_takeover; server_max_window_bits=10; client_max_window_bits=11');
        assert.deepEqual(perMessageDeflate.accept(extensions['permessage-deflate']), {
          server_no_context_takeover: true,
          client_no_context_takeover: true,
          server_max_window_bits: 10,
          client_max_window_bits: 11
        });
      }

      /*'should throw an error if client_no_context_takeover is unsupported'*/
      {
        var perMessageDeflate = new PerMessageDeflate({ clientNoContextTakeover: false });
        var extensions = Extensions.parse('permessage-deflate; client_no_context_takeover');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      }

      /*'should throw an error if client_max_window_bits is unsupported'*/
      {
        var perMessageDeflate = new PerMessageDeflate({ clientMaxWindowBits: false });
        var extensions = Extensions.parse('permessage-deflate; client_max_window_bits=10');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      }

      /*'should throw an error if client_max_window_bits is greater than configuration'*/
      {
        var perMessageDeflate = new PerMessageDeflate({ clientMaxWindowBits: 10 });
        var extensions = Extensions.parse('permessage-deflate; client_max_window_bits=11');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      }
    }

    /*'validate parameters'*/
    {
      /*'should throw an error if a parameter has multiple values'*/
      {
        var perMessageDeflate = new PerMessageDeflate();
        var extensions = Extensions.parse('permessage-deflate; server_no_context_takeover; server_no_context_takeover');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      }

      /*'should throw an error if a parameter is undefined'*/
      {
        var perMessageDeflate = new PerMessageDeflate();
        var extensions = Extensions.parse('permessage-deflate; foo;');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      }

      /*'should throw an error if server_no_context_takeover has a value'*/
      {
        var perMessageDeflate = new PerMessageDeflate();
        var extensions = Extensions.parse('permessage-deflate; server_no_context_takeover=10');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      }

      /*'should throw an error if client_no_context_takeover has a value'*/
      {
        var perMessageDeflate = new PerMessageDeflate();
        var extensions = Extensions.parse('permessage-deflate; client_no_context_takeover=10');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      }

      /*'should throw an error if server_max_window_bits has an invalid value'*/
      {
        var perMessageDeflate = new PerMessageDeflate();
        var extensions = Extensions.parse('permessage-deflate; server_max_window_bits=7');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      }

      /*'should throw an error if client_max_window_bits has an invalid value'*/
      {
        var perMessageDeflate = new PerMessageDeflate();
        var extensions = Extensions.parse('permessage-deflate; client_max_window_bits=16');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      }
    }
  }

  /*'#compress/#decompress'*/
  {
    /*'should compress/decompress data'*/
    {
      var perMessageDeflate = new PerMessageDeflate();
      perMessageDeflate.accept([{}]);
      perMessageDeflate.compress(new Buffer([1, 2, 3]), true, function(err, compressed) {
        if (err) return done(err);
        perMessageDeflate.decompress(compressed, true, function(err, data) {
          if (err) return;
          assert.deepEqual(data, new Buffer([1, 2, 3]));
        });
      });
    }

    /*'should compress/decompress fragments'*/
    {
      var perMessageDeflate = new PerMessageDeflate();
      perMessageDeflate.accept([{}]);

      var buf = new Buffer([1, 2, 3, 4]);
      perMessageDeflate.compress(buf.slice(0, 2), false, function(err, compressed1) {
        if (err) return done(err);
        perMessageDeflate.compress(buf.slice(2), true, function(err, compressed2) {
          if (err) return;
          perMessageDeflate.decompress(compressed1, false, function(err, data1) {
            if (err) return;
            perMessageDeflate.decompress(compressed2, true, function(err, data2) {
              if (err) return;
              assert.deepEqual(new Buffer.concat([data1, data2]), new Buffer([1, 2, 3, 4]));
            });
          });
        });
      });
    }

    /*'should compress/decompress data with parameters'*/
    {
      function done() {}
      (function () {
        var perMessageDeflate = new PerMessageDeflate({ memLevel: 5 });
        var extensions = Extensions.parse('permessage-deflate; server_no_context_takeover; client_no_context_takeover; server_max_window_bits=10; client_max_window_bits=11');
        perMessageDeflate.accept(extensions['permessage-deflate']);
        perMessageDeflate.compress(new Buffer([1, 2, 3]), true, function(err, compressed) {
          if (err) return done(err);
          perMessageDeflate.decompress(compressed, true, function(err, data) {
            if (err) return done(err);
            assert.deepEqual(data, new Buffer([1, 2, 3]));
            done()
          });
        });
      }())
    }

    /*'should compress/decompress data with no context takeover'*/
    {
      function done() {}
      (function () {
        var perMessageDeflate = new PerMessageDeflate();
        var extensions = Extensions.parse('permessage-deflate; server_no_context_takeover; client_no_context_takeover');
        perMessageDeflate.accept(extensions['permessage-deflate']);
        var buf = new Buffer('foofoo');
        perMessageDeflate.compress(buf, true, function(err, compressed1) {
          if (err) return done(err);
          perMessageDeflate.decompress(compressed1, true, function(err, data) {
            if (err) return done(err);
            perMessageDeflate.compress(data, true, function(err, compressed2) {
              if (err) return done(err);
              perMessageDeflate.decompress(compressed2, true, function(err, data) {
                if (err) return done(err);
                assert.deepEqual(compressed2.length, compressed1.length);
                assert.deepEqual(data, buf);
                done()
              });
            });
          });
        });
      }())
    }
  }
}
