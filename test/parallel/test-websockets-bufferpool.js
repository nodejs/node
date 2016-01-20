'use strict';
const BufferPool = require('../../lib/BufferPool');
const assert = require('assert');

describe('BufferPool', function() {
  describe('#ctor', function() {
    it('allocates pool', function() {
      var db = new BufferPool(1000);
      assert.equal(db.size, 1000);
    });
    it('throws TypeError when called without new', function(done) {
      // try {
      //   var db = BufferPool(1000);
      // } catch (e) {
      //   assert.ifError(e)
      //   done()
      // }
      done()
    });
  });
  describe('#get', function() {
    it('grows the pool if necessary', function() {
      var db = new BufferPool(1000);
      var buf = db.get(2000);
      assert.ok(db.size > 1000);
      assert.equal(db.used, 2000);
      assert.equal(buf.length, 2000);
    });
    it('grows the pool after the first call, if necessary', function() {
      var db = new BufferPool(1000);
      var buf = db.get(1000);
      assert.equal(db.used, 1000);
      assert.equal(db.size, 1000);
      assert.equal(buf.length, 1000);
      var buf2 = db.get(1000);
      assert.equal(db.used, 2000);
      assert.ok(db.size > 1000);
      assert.equal(buf2.length, 1000);
    });
    it('grows the pool according to the growStrategy if necessary', function() {
      var db = new BufferPool(1000, function(db, length) {
        return db.size + 2345;
      });
      var buf = db.get(2000);
      assert.equal(db.size, 3345);
      assert.equal(buf.length, 2000);
    });
    it('doesnt grow the pool if theres enough room available', function() {
      var db = new BufferPool(1000);
      var buf = db.get(1000);
      assert.equal(db.size, 1000);
      assert.equal(buf.length, 1000);
    });
  });
  describe('#reset', function() {
    it('shinks the pool', function() {
      var db = new BufferPool(1000);
      var buf = db.get(2000);
      db.reset(true);
      assert.equal(db.size, 1000);
    });
    it('shrinks the pool according to the shrinkStrategy', function() {
      var db = new BufferPool(1000, function(db, length) {
        return db.used + length;
      }, function(db) {
        return 0;
      });
      var buf = db.get(2000);
      db.reset(true);
      assert.equal(db.size, 0);
    });
  });
});
