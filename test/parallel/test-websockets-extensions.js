'use strict';
const Extensions = require('../../lib/internal/websockets/Extensions');
const assert = require('assert');

/*'Extensions'*/
{
  /*'parse'*/
  {
    /*'should parse'*/
    {
      var extensions = Extensions.parse('foo');
      assert.deepEqual(extensions, { foo: [{}] });
    }

    /*'should parse params'*/
    {
      var extensions = Extensions.parse('foo; bar; baz=1; bar=2');
      assert.deepEqual(extensions, {
        foo: [{ bar: [true, '2'], baz: ['1'] }]
      });
    }

    /*'should parse multiple extensions'*/
    {
      var extensions = Extensions.parse('foo, bar; baz, foo; baz');
      assert.deepEqual(extensions, {
        foo: [{}, { baz: [true] }],
        bar: [{ baz: [true] }]
      });
    }

    /*'should parse quoted params'*/
    {
      var extensions = Extensions.parse('foo; bar="hi"');
      assert.deepEqual(extensions, {
        foo: [{ bar: ['hi'] }]
      });
    }
  }

  /*'format'*/
  {
    /*'should format'*/
    {
      var extensions = Extensions.format({ foo: {} });
      assert.deepEqual(extensions, 'foo');
    }

    /*'should format params'*/
    {
      var extensions = Extensions.format({ foo: { bar: [true, 2], baz: 1 } });
      assert.deepEqual(extensions, 'foo; bar; bar=2; baz=1');
    }

    /*'should format multiple extensions'*/
    {
      var extensions = Extensions.format({
        foo: [{}, { baz: true }],
        bar: { baz: true }
      });
      assert.deepEqual(extensions, 'foo, foo; baz, bar; baz');
    }
  }
}
