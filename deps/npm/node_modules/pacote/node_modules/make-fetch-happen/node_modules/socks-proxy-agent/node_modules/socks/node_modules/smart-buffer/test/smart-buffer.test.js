var SmartBuffer = require('../lib/smart-buffer.js');
var assert = require('chai').assert;


describe('Constructing a SmartBuffer', function () {
    describe('Constructing with an existing Buffer', function () {
        var buff = new Buffer([0xAA, 0xBB, 0xCC, 0xDD, 0xFF, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99]);
        var reader = new SmartBuffer(buff);

        it('should have the exact same internal Buffer when constructed with a Buffer', function () {
            assert.strictEqual(reader.buff, buff);
        });

        it('should return a buffer with the same content', function () {
            assert.deepEqual(reader.toBuffer(), buff);
        });
    });

    describe('Constructing with an existing Buffer and setting the encoding', function () {
        var buff = new Buffer([0xAA, 0xBB, 0xCC, 0xDD, 0xFF, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99]);
        var reader = new SmartBuffer(buff, 'ascii');

        it('should have the exact same internal Buffer', function () {
            assert.strictEqual(reader.buff, buff);
        });

        it('should have the same encoding that was set', function () {
            assert.strictEqual(reader.encoding, 'ascii');
        });
    });

    describe('Constructing with a specified size', function () {
        var size = 128;
        var reader = new SmartBuffer(size);

        it('should have an internal Buffer with the same length as the size defined in the constructor', function () {
            assert.strictEqual(reader.buff.length, size);
        });
    });

    describe('Constructing with a specified encoding', function () {
        var encoding = 'utf8';

        it('should have an internal encoding with the encoding given to the constructor (1st argument)', function () {
            var reader = new SmartBuffer(encoding);
            assert.strictEqual(reader.encoding, encoding);
        });

        it('should have an internal encoding with the encoding given to the constructor (2nd argument)', function () {
            var reader = new SmartBuffer(1024, encoding);
            assert.strictEqual(reader.encoding, encoding);
        });

    });

    describe('Constructing with invalid parameters', function () {
        it('should throw an exception when given an invalid number size', function () {
            assert.throws(function () {
                var reader = new SmartBuffer(-100);
            }, Error);
        });

        it('should throw an exception when give a invalid encoding', function () {
            assert.throws(function () {
                var reader = new SmartBuffer('invalid');
            }, Error);

            assert.throws(function () {
                var reader = new SmartBuffer(1024, 'invalid');
            }, Error);
        });

        it('should throw and exception when given an object that is not a Buffer', function () {
            assert.throws(function () {
                var reader = new SmartBuffer(null);
            }, TypeError);
        });
    });
});


describe('Reading/Writing To/From SmartBuffer', function () {
    /**
     * Technically, if one of these works, they all should. But they're all here anyways.
     */

    describe('Numeric Values', function () {
        var reader = new SmartBuffer();
        reader.writeInt8(0x44);
        reader.writeUInt8(0xFF);
        reader.writeInt16BE(0x6699);
        reader.writeInt16LE(0x6699);
        reader.writeUInt16BE(0xFFDD);
        reader.writeUInt16LE(0xFFDD);
        reader.writeInt32BE(0x77889900);
        reader.writeInt32LE(0x77889900);
        reader.writeUInt32BE(0xFFDDCCBB);
        reader.writeUInt32LE(0xFFDDCCBB);
        reader.writeFloatBE(1.234);
        reader.writeFloatLE(1.234);
        reader.writeDoubleBE(1.234567890);
        reader.writeDoubleLE(1.234567890);

        it('should equal the correct values that were written above', function () {
            assert.strictEqual(reader.readInt8(), 0x44);
            assert.strictEqual(reader.readUInt8(), 0xFF);
            assert.strictEqual(reader.readInt16BE(), 0x6699);
            assert.strictEqual(reader.readInt16LE(), 0x6699);
            assert.strictEqual(reader.readUInt16BE(), 0xFFDD);
            assert.strictEqual(reader.readUInt16LE(), 0xFFDD);
            assert.strictEqual(reader.readInt32BE(), 0x77889900);
            assert.strictEqual(reader.readInt32LE(), 0x77889900);
            assert.strictEqual(reader.readUInt32BE(), 0xFFDDCCBB);
            assert.strictEqual(reader.readUInt32LE(), 0xFFDDCCBB);
            assert.closeTo(reader.readFloatBE(), 1.234, 0.001);
            assert.closeTo(reader.readFloatLE(), 1.234, 0.001);
            assert.closeTo(reader.readDoubleBE(), 1.234567890, 0.001);
            assert.closeTo(reader.readDoubleLE(), 1.234567890, 0.001);
        });

    });

    describe('Basic String Values', function () {
        var reader = new SmartBuffer();
        reader.writeStringNT('hello');
        reader.writeString('world');
        reader.writeStringNT('✎✏✎✏✎✏');

        it('should equal the correct strings that were written above', function () {
            assert.strictEqual(reader.readStringNT(), 'hello');
            assert.strictEqual(reader.readString(5), 'world');
            assert.strictEqual(reader.readStringNT(), '✎✏✎✏✎✏');
        });
    });

    describe('Mixed Encoding Strings', function () {
        var reader = new SmartBuffer('ascii');
        reader.writeStringNT('some ascii text');
        reader.writeStringNT('ѕσмє υтƒ8 тєχт', 'utf8');

        it('should equal the correct strings that were written above', function () {
            assert.strictEqual(reader.readStringNT(), 'some ascii text');
            assert.strictEqual(reader.readStringNT('utf8'), 'ѕσмє υтƒ8 тєχт');
        });
    });

    describe('Null/non-null terminating strings', function () {
        var reader = new SmartBuffer();
        reader.writeString('hello\0test\0bleh');

        it('should equal hello', function () {
            assert.strictEqual(reader.readStringNT(), 'hello');
        });

        it('should equal: test', function () {
            assert.strictEqual(reader.readString(4), 'test');
        });

        it('should have a length of zero', function () {
            assert.strictEqual(reader.readStringNT().length, 0);
        });

        it('should equal: bleh', function () {
            assert.strictEqual(reader.readStringNT(), 'bleh');
        });


    });

    describe('Reading string without specifying length', function () {
        var str = 'hello123';
        var writer = new SmartBuffer();
        writer.writeString(str);

        var reader = new SmartBuffer(writer.toBuffer());

        assert.strictEqual(reader.readString(), str);
    });

    describe('Write string as specific position', function () {
        var str = 'hello123';
        var writer = new SmartBuffer();
        writer.writeString(str, 10);

        var reader = new SmartBuffer(writer.toBuffer());

        reader.skipTo(10);
        it('Should read the correct string from the original position it was written to.', function () {
            assert.strictEqual(reader.readString(), str);
        });


    });

    describe('Buffer Values', function () {
        describe('Writing buffer to position 0', function () {
            var buff = new SmartBuffer();
            var frontBuff = new Buffer([1, 2, 3, 4, 5, 6]);
            buff.writeStringNT('hello');
            buff.writeBuffer(frontBuff, 0);

            it('should write the buffer to the front of the smart buffer instance', function () {
                var readBuff = buff.readBuffer(frontBuff.length);
                assert.deepEqual(readBuff, frontBuff);
            });
        });

        describe('Writing null terminated buffer to position 0', function () {
            var buff = new SmartBuffer();
            var frontBuff = new Buffer([1, 2, 3, 4, 5, 6]);
            buff.writeStringNT('hello');
            buff.writeBufferNT(frontBuff, 0);

            console.log(buff);

            it('should write the buffer to the front of the smart buffer instance', function () {
                var readBuff = buff.readBufferNT();
                console.log(readBuff);
                assert.deepEqual(readBuff, frontBuff);
            });
        });

        describe('Explicit lengths', function () {
            var buff = new Buffer([0x01, 0x02, 0x04, 0x08, 0x16, 0x32, 0x64]);
            var reader = new SmartBuffer();
            reader.writeBuffer(buff);

            it('should equal the buffer that was written above.', function () {
                assert.deepEqual(reader.readBuffer(7), buff);
            });
        });

        describe('Implicit lengths', function () {
            var buff = new Buffer([0x01, 0x02, 0x04, 0x08, 0x16, 0x32, 0x64]);
            var reader = new SmartBuffer();
            reader.writeBuffer(buff);

            it('should equal the buffer that was written above.', function () {
                assert.deepEqual(reader.readBuffer(), buff);
            });
        });

        describe('Null Terminated Buffer Reading', function () {
            var buff = new SmartBuffer();
            buff.writeBuffer(new Buffer([0x01, 0x02, 0x03, 0x04, 0x00, 0x01, 0x02, 0x03]));

            var read1 = buff.readBufferNT();
            var read2 = buff.readBufferNT();

            it('Should return a length of 4 for the four bytes before the first null in the buffer.', function () {
                assert.equal(read1.length, 4);
            });

            it('Should return a length of 3 for the three bytes after the first null in the buffer after reading to end.', function () {
                assert.equal(read2.length, 3);
            });
        });

        describe('Null Terminated Buffer Writing', function () {
            var buff = new SmartBuffer();
            buff.writeBufferNT(new Buffer([0x01, 0x02, 0x03, 0x04]));

            var read1 = buff.readBufferNT();

            it('Should read the correct null terminated buffer data.', function () {
                assert.equal(read1.length, 4);
            });

        })

    });

    describe('Inserting values into specific positions', function () {
        var reader = new SmartBuffer();

        reader.writeUInt16LE(0x0060);
        reader.writeStringNT('something');
        reader.writeUInt32LE(8485934);
        reader.writeUInt16LE(6699);
        reader.writeStringNT('else');
        reader.writeUInt16LE(reader.length - 2, 2);


        it('should equal the size of the remaining data in the buffer', function () {
            reader.readUInt16LE();
            var size = reader.readUInt16LE();
            assert.strictEqual(reader.remaining(), size);
        });
    });

    describe('Adding more data to the buffer than the internal buffer currently allows.', function () {
        it('Should automatically adjust internal buffer size when needed', function () {
            var writer = new SmartBuffer();
            var largeBuff = new Buffer(10000);

            writer.writeBuffer(largeBuff);

            assert.strictEqual(writer.length, largeBuff.length);
        });
    });

});

describe('Skipping around data', function () {
    var writer = new SmartBuffer();
    writer.writeStringNT('hello');
    writer.writeUInt16LE(6699);
    writer.writeStringNT('world!');

    it('Should equal the UInt16 that was written above', function () {
        var reader = new SmartBuffer(writer.toBuffer());
        reader.skip(6);
        assert.strictEqual(reader.readUInt16LE(), 6699);
        reader.skipTo(0);
        assert.strictEqual(reader.readStringNT(), 'hello');
        reader.rewind(6);
        assert.strictEqual(reader.readStringNT(), 'hello');
    });

    it('Should throw an error when attempting to skip more bytes than actually exist.', function () {
        var reader = new SmartBuffer(writer.toBuffer());

        assert.throws(function () {
            reader.skip(10000);
        });
    });

    it('Should throw an error when attempting to skip to a position that does not exist.', function () {
        var reader = new SmartBuffer(writer.toBuffer());

        assert.throws(function () {
            reader.skipTo(10000);
        });
    });

    it('Should throw an error when attempting to rewind past the start of the buffer.', function () {
        var buff = new SmartBuffer();
        assert.throws(function () {
            buff.rewind(10000);
        });
    });
});

describe('Automatic internal buffer resizing', function () {
    var writer;

    it('Should not throw an error when adding data that is larger than current buffer size (internal resize algo fails)', function () {
        var str = 'String larger than one byte';
        writer = new SmartBuffer(1);
        writer.writeString(str);

        assert.strictEqual(writer.buff.length, str.length);

    });

    it('Should not throw an error when adding data that is larger than current buffer size (internal resize algo succeeds)', function () {
        writer = new SmartBuffer(100);
        var buff = new Buffer(105);

        writer.writeBuffer(buff);

        // Test internal array growth algo.
        assert.strictEqual(writer.buff.length, (100 * 3 / 2 + 1));
    });
});

describe('Clearing the buffer', function () {
    var writer = new SmartBuffer();
    writer.writeString('somedata');

    it('Should contain some data.', function () {
        assert.notStrictEqual(writer.length, 0);
    });

    it('Should contain zero data after being cleared.', function () {
        writer.clear();
        assert.strictEqual(writer.length, 0);
    });
});

describe('Displaying the buffer as a string', function () {
    var buff = new Buffer([1, 2, 3, 4]);
    var sbuff = new SmartBuffer(buff);

    var str = buff.toString();
    var str64 = buff.toString('base64');

    it('Should return a valid string representing the internal buffer', function () {
        assert.strictEqual(str, sbuff.toString());
    });

    it('Should return a valid base64 string representing the internal buffer', function () {
        assert.strictEqual(str64, sbuff.toString('base64'));
    });
});

describe('Destroying the buffer', function () {
    var writer = new SmartBuffer();
    writer.writeString('hello123');

    writer.destroy();

    it('Should have a length of zero when buffer is destroyed', function () {
        assert.strictEqual(0, writer.length);
    });

    it('Should have no internal buff property when buffer is destroyed', function () {
        assert.notProperty(writer, 'buff');
    });
});