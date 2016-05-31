'use strict';
const BufferPool = require('../../lib/internal/websockets/BufferPool');
const assert = require('assert');

/*'BufferPool'*/
{
  /*'#ctor'*/
  {
    /* 'allocates pool'*/
    {
      var db = new BufferPool(1000);
      assert.equal(db.size, 1000);
    }
    /*'throws TypeError when called without new'*/
    {
      try {
        var db = BufferPool(1000);
      } catch (e) {
        assert.ok(e instanceof TypeError);
      }
    }
  }
  /*'#get'*/
  {
    /*'grows the pool if necessary'*/
    {
      var db = new BufferPool(1000);
      var buf = db.get(2000);
      assert.ok(db.size > 1000);
      assert.equal(db.used, 2000);
      assert.equal(buf.length, 2000);
    }
    /*'grows the pool after the first call, if necessary'*/
    {
      var db = new BufferPool(1000);
      var buf = db.get(1000);
      assert.equal(db.used, 1000);
      assert.equal(db.size, 1000);
      assert.equal(buf.length, 1000);
      var buf2 = db.get(1000);
      assert.equal(db.used, 2000);
      assert.ok(db.size > 1000);
      assert.equal(buf2.length, 1000);
    }
    /*'grows the pool according to the growStrategy if necessary'*/
    {
      var db = new BufferPool(1000, function(db, length) {
        return db.size + 2345;
      });
      var buf = db.get(2000);
      assert.equal(db.size, 3345);
      assert.equal(buf.length, 2000);
    }
    /*'doesnt grow the pool if theres enough room available'*/
    {
      var db = new BufferPool(1000);
      var buf = db.get(1000);
      assert.equal(db.size, 1000);
      assert.equal(buf.length, 1000);
    }
  }
  /*'#reset'*/
  {
    /*'shinks the pool'*/
    {
      var db = new BufferPool(1000);
      var buf = db.get(2000);
      db.reset(true);
      assert.equal(db.size, 1000);
    }
    /*'shrinks the pool according to the shrinkStrategy'*/
    {
      var db = new BufferPool(1000, function(db, length) {
        return db.used + length;
      }, function(db) {
        return 0;
      });
      var buf = db.get(2000);
      db.reset(true);
      assert.equal(db.size, 0);
    }
  }
}
