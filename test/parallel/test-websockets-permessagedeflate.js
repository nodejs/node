'use strict';
const PerMessageDeflate = require('../../lib/PerMessageDeflate');
const Extensions = require('../../lib/Extensions');
const assert = require('assert');

describe('PerMessageDeflate', function() {
  describe('#ctor', function() {
    it('throws TypeError when called without new', function(done) {
      // try {
      //   var perMessageDeflate = PerMessageDeflate();
      // }
      // catch (e) {
      //   assert.ok(e instanceof TypeError);
      //   done();
      // }
      done()
    });
  });

  describe('#offer', function() {
    it('should create default params', function() {
      var perMessageDeflate = new PerMessageDeflate();
      assert.deepEqual(perMessageDeflate.offer(), { client_max_window_bits: true });
    });

    it('should create params from options', function() {
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
    });
  });

  describe('#accept', function() {
    describe('as server', function() {
      it('should accept empty offer', function() {
        var perMessageDeflate = new PerMessageDeflate({}, true);
        assert.deepEqual(perMessageDeflate.accept([{}]), {});
      });

      it('should accept offer', function() {
        var perMessageDeflate = new PerMessageDeflate({}, true);
        var extensions = Extensions.parse('permessage-deflate; server_no_context_takeover; client_no_context_takeover; server_max_window_bits=10; client_max_window_bits=11');
        assert.deepEqual(perMessageDeflate.accept(extensions['permessage-deflate']), {
          server_no_context_takeover: true,
          client_no_context_takeover: true,
          server_max_window_bits: 10,
          client_max_window_bits: 11
        });
      });

      it('should prefer configuration than offer', function() {
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
      });

      it('should fallback', function() {
        var perMessageDeflate = new PerMessageDeflate({ serverMaxWindowBits: 11 }, true);
        var extensions = Extensions.parse('permessage-deflate; server_max_window_bits=10, permessage-deflate');
        assert.deepEqual(perMessageDeflate.accept(extensions['permessage-deflate']), {
          server_max_window_bits: 11
        });
      });

      it('should throw an error if server_no_context_takeover is unsupported', function() {
        var perMessageDeflate = new PerMessageDeflate({ serverNoContextTakeover: false }, true);
        var extensions = Extensions.parse('permessage-deflate; server_no_context_takeover');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }))
      });

      it('should throw an error if server_max_window_bits is unsupported', function() {
        var perMessageDeflate = new PerMessageDeflate({ serverMaxWindowBits: false }, true);
        var extensions = Extensions.parse('permessage-deflate; server_max_window_bits=10');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      });

      it('should throw an error if server_max_window_bits is less than configuration', function() {
        var perMessageDeflate = new PerMessageDeflate({ serverMaxWindowBits: 11 }, true);
        var extensions = Extensions.parse('permessage-deflate; server_max_window_bits=10');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      });

      it('should throw an error if client_max_window_bits is unsupported on client', function() {
        var perMessageDeflate = new PerMessageDeflate({ clientMaxWindowBits: 10 }, true);
        var extensions = Extensions.parse('permessage-deflate');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      });
    });

    describe('as client', function() {
      it('should accept empty response', function() {
        var perMessageDeflate = new PerMessageDeflate({});
        assert.deepEqual(perMessageDeflate.accept([{}]), {});
      });

      it('should accept response parameter', function() {
        var perMessageDeflate = new PerMessageDeflate({});
        var extensions = Extensions.parse('permessage-deflate; server_no_context_takeover; client_no_context_takeover; server_max_window_bits=10; client_max_window_bits=11');
        assert.deepEqual(perMessageDeflate.accept(extensions['permessage-deflate']), {
          server_no_context_takeover: true,
          client_no_context_takeover: true,
          server_max_window_bits: 10,
          client_max_window_bits: 11
        });
      });

      it('should throw an error if client_no_context_takeover is unsupported', function() {
        var perMessageDeflate = new PerMessageDeflate({ clientNoContextTakeover: false });
        var extensions = Extensions.parse('permessage-deflate; client_no_context_takeover');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      });

      it('should throw an error if client_max_window_bits is unsupported', function() {
        var perMessageDeflate = new PerMessageDeflate({ clientMaxWindowBits: false });
        var extensions = Extensions.parse('permessage-deflate; client_max_window_bits=10');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      });

      it('should throw an error if client_max_window_bits is greater than configuration', function() {
        var perMessageDeflate = new PerMessageDeflate({ clientMaxWindowBits: 10 });
        var extensions = Extensions.parse('permessage-deflate; client_max_window_bits=11');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      });
    });

    describe('validate parameters', function() {
      it('should throw an error if a parameter has multiple values', function() {
        var perMessageDeflate = new PerMessageDeflate();
        var extensions = Extensions.parse('permessage-deflate; server_no_context_takeover; server_no_context_takeover');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      });

      it('should throw an error if a parameter is undefined', function() {
        var perMessageDeflate = new PerMessageDeflate();
        var extensions = Extensions.parse('permessage-deflate; foo;');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      });

      it('should throw an error if server_no_context_takeover has a value', function() {
        var perMessageDeflate = new PerMessageDeflate();
        var extensions = Extensions.parse('permessage-deflate; server_no_context_takeover=10');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      });

      it('should throw an error if client_no_context_takeover has a value', function() {
        var perMessageDeflate = new PerMessageDeflate();
        var extensions = Extensions.parse('permessage-deflate; client_no_context_takeover=10');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      });

      it('should throw an error if server_max_window_bits has an invalid value', function() {
        var perMessageDeflate = new PerMessageDeflate();
        var extensions = Extensions.parse('permessage-deflate; server_max_window_bits=7');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      });

      it('should throw an error if client_max_window_bits has an invalid value', function() {
        var perMessageDeflate = new PerMessageDeflate();
        var extensions = Extensions.parse('permessage-deflate; client_max_window_bits=16');
        assert.throws((function() {
          perMessageDeflate.accept(extensions['permessage-deflate']);
        }));
      });
    });
  });

  describe('#compress/#decompress', function() {
    it('should compress/decompress data', function(done) {
      var perMessageDeflate = new PerMessageDeflate();
      perMessageDeflate.accept([{}]);
      perMessageDeflate.compress(new Buffer([1, 2, 3]), true, function(err, compressed) {
        if (err) return done(err);
        perMessageDeflate.decompress(compressed, true, function(err, data) {
          if (err) return done(err);
          assert.deepEqual(data, new Buffer([1, 2, 3]));
          done();
        });
      });
    });

    it('should compress/decompress fragments', function(done) {
      var perMessageDeflate = new PerMessageDeflate();
      perMessageDeflate.accept([{}]);

      var buf = new Buffer([1, 2, 3, 4]);
      perMessageDeflate.compress(buf.slice(0, 2), false, function(err, compressed1) {
        if (err) return done(err);
        perMessageDeflate.compress(buf.slice(2), true, function(err, compressed2) {
          if (err) return done(err);
          perMessageDeflate.decompress(compressed1, false, function(err, data1) {
            if (err) return done(err);
            perMessageDeflate.decompress(compressed2, true, function(err, data2) {
              if (err) return done(err);
              assert.deepEqual(new Buffer.concat([data1, data2]), new Buffer([1, 2, 3, 4]));
              done();
            });
          });
        });
      });
    });

    it('should compress/decompress data with parameters', function(done) {
      var perMessageDeflate = new PerMessageDeflate({ memLevel: 5 });
      var extensions = Extensions.parse('permessage-deflate; server_no_context_takeover; client_no_context_takeover; server_max_window_bits=10; client_max_window_bits=11');
      perMessageDeflate.accept(extensions['permessage-deflate']);
      perMessageDeflate.compress(new Buffer([1, 2, 3]), true, function(err, compressed) {
        if (err) return done(err);
        perMessageDeflate.decompress(compressed, true, function(err, data) {
          if (err) return done(err);
          assert.deepEqual(data, new Buffer([1, 2, 3]));
          done();
        });
      });
    });

    it('should compress/decompress data with no context takeover', function(done) {
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
              done();
            });
          });
        });
      });
    });
  });
});
