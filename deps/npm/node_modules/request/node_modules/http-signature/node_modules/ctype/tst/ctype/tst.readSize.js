/*
 * Testing to ensure we're reading the expected number bytes
 */
var mod_ctype = require('../../ctype');
var ASSERT = require('assert');

function testUint8()
{
	var parser, result, buffer;
	parser = new mod_ctype.Parser({ endian: 'little' });
	buffer = new Buffer('80', 'hex');
	result = parser.readStruct([ { item: { type: 'uint8_t' } } ], buffer,
	    0);
	ASSERT.equal(result['size'], 1);
}

function testSint8()
{
	var parser, result, buffer;
	parser = new mod_ctype.Parser({ endian: 'little' });
	buffer = new Buffer('80', 'hex');
	result = parser.readStruct([ { item: { type: 'int8_t' } } ], buffer, 0);
	ASSERT.equal(result['size'], 1);
}

function testUint16()
{
	var parser, result, buffer;
	parser = new mod_ctype.Parser({ endian: 'little' });
	buffer = new Buffer('8000', 'hex');
	result = parser.readStruct([ { item: { type: 'uint16_t' } } ], buffer,
	    0);
	ASSERT.equal(result['size'], 2);
}

function testSint16()
{
	var parser, result, buffer;
	parser = new mod_ctype.Parser({ endian: 'little' });
	buffer = new Buffer('8000', 'hex');
	result = parser.readStruct([ { item: { type: 'int16_t' } } ], buffer,
	    0);
	ASSERT.equal(result['size'], 2);
}

function testUint32()
{
	var parser, result, buffer;
	parser = new mod_ctype.Parser({ endian: 'little' });
	buffer = new Buffer('80000000', 'hex');
	result = parser.readStruct([ { item: { type: 'uint32_t' } } ], buffer,
	    0);
	ASSERT.equal(result['size'], 4);
}

function testSint32()
{
	var parser, result, buffer;
	parser = new mod_ctype.Parser({ endian: 'little' });
	buffer = new Buffer('80000000', 'hex');
	result = parser.readStruct([ { item: { type: 'int32_t' } } ], buffer,
	    0);
	ASSERT.equal(result['size'], 4);
}

function testUint64()
{
	var parser, result, buffer;
	parser = new mod_ctype.Parser({ endian: 'little' });
	buffer = new Buffer('8000000000000000', 'hex');
	result = parser.readStruct([ { item: { type: 'uint64_t' } } ], buffer,
	    0);
	ASSERT.equal(result['size'], 8);
}

function testSint64()
{
	var parser, result, buffer;
	parser = new mod_ctype.Parser({ endian: 'little' });
	buffer = new Buffer('8000000000000000', 'hex');
	result = parser.readStruct([ { item: { type: 'int64_t' } } ], buffer,
	    0);
	ASSERT.equal(result['size'], 8);
}

function testFloat()
{
	var parser, result, buffer;
	parser = new mod_ctype.Parser({ endian: 'little' });
	buffer = new Buffer('ABAAAA3E', 'hex');
	result = parser.readStruct([ { item: { type: 'float' } } ], buffer, 0);
	ASSERT.equal(result['size'], 4);
}

function testDouble()
{
	var parser, result, buffer;
	parser = new mod_ctype.Parser({ endian: 'little' });
	buffer = new Buffer('000000000000F03F', 'hex');
	result = parser.readStruct([ { item: { type: 'double' } } ], buffer, 0);
	ASSERT.equal(result['size'], 8);
}

function testChar()
{
	var parser, result, buffer;
	parser = new mod_ctype.Parser({ endian: 'little' });
	buffer = new Buffer('t');
	result = parser.readStruct([ { item: { type: 'char' } } ], buffer, 0);
	ASSERT.equal(result['size'], 1);
}

function test()
{
	testSint8();
	testUint8();
	testSint16();
	testUint16();
	testSint32();
	testUint32();
	testSint64();
	testUint64();
	testFloat();
	testDouble();
	testChar();
}

test();
