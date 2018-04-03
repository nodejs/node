var SmartBuffer = (function () {

    /**
     * Constructor for SmartBuffer.
     * @param arg1 {Buffer || Number || String} Buffer to read from, or expected size to write to, or encoding to use.
     * @param arg2 {String} Encoding to use for writing and reading strings. Defaults to utf8. If encoding is given in arg1, this is ignored.
     * @constructor
     *
     * There are a few ways to construct a SmartBuffer:
     *
     * SmartBuffer() - Defaults to utf8, 4096 pre-set internal Buffer length.
     * SmartBuffer(size) - Defaults to utf8, sets internal Buffer length to the size given.
     * SmartBuffer(encoding) - Sets the given encoding, defaults to 4096 pre-set internal Buffer length.
     * SmartBuffer(Buffer) - Defaults to utf8, sets the internal Buffer to the given buffer (same memory).
     * SmartBuffer(Buffer, encoding) - Sets the given encoding, sets the internal Buffer to the given buffer (same memory).
     *
     */
    function SmartBuffer(arg1, arg2) {
        var type;
        switch (type = typeof arg1) {
            case 'number':
                if (isFinite(arg1) && arg1 > 0) {
                    this.buff = new Buffer(Math.ceil(arg1));
                    this.length = 0;
                } else {
                    throw new Error('When specifying a size, it must be a valid number above zero.');
                }
                break;

            case 'string':
                if (Buffer.isEncoding(arg1)) {
                    this.buff = new Buffer(4096);
                    this.length = 0;
                    this.encoding = arg1;
                } else {
                    throw new Error('Invalid Encoding');
                }
                break;

            case 'object':
                if (Buffer.isBuffer(arg1)) {
                    this.buff = arg1;
                    this.length = arg1.length;
                } else {
                    throw new TypeError('First argument must be a Buffer, Number representing the size, or a String representing the encoding.');
                }
                break;

            default:
                this.buff = new Buffer(4096);
                this.length = 0;
                break;
        }

        if (typeof this.encoding === 'undefined') {
            if (typeof arg2 === 'string') {
                if (Buffer.isEncoding(arg2)) {
                    this.encoding = arg2;
                } else {
                    throw new Error('Invalid Encoding');
                }
            }
        }

        this._readOffset = 0;
        this._writeOffset = 0;
    }


    SmartBuffer.prototype._ensureWritable = function (len, offset) {
        this._ensureCapacity(this.length + len + (typeof offset === 'number' ? offset : 0));

        if (typeof offset === 'number') {
            this.buff.copy(this.buff, offset + len, offset, this.buff.length);
        }
        this.length = Math.max(this.length + len, (typeof offset === 'number' ?  offset : 0) + len);
    };

    SmartBuffer.prototype._ensureCapacity = function (minlen) {
        var oldlen = this.buff.length;

        if (minlen > oldlen) {
            var data = this.buff;
            var newlen = (oldlen * 3) / 2 + 1;
            if (newlen < minlen)
                newlen = minlen;
            this.buff = new Buffer(newlen);
            data.copy(this.buff, 0, 0, oldlen);
        }
    };


    var makeReader = function (func, size) {
        return function () {
            var ret = func.call(this.buff, this._readOffset);
            this._readOffset += size;
            return ret;
        }
    };

    var makeWriter = function (func, size) {
        return function (value, offset) {
            this._ensureWritable(size, offset);
            func.call(this.buff, value, typeof offset === 'number' ? offset : this._writeOffset);
            this._writeOffset += size;
            return this;
        }
    };


    /*
     Read Operations
     */

    SmartBuffer.prototype.readInt8 = makeReader(Buffer.prototype.readInt8, 1);
    SmartBuffer.prototype.readInt16BE = makeReader(Buffer.prototype.readInt16BE, 2);
    SmartBuffer.prototype.readInt16LE = makeReader(Buffer.prototype.readInt16LE, 2);
    SmartBuffer.prototype.readInt32BE = makeReader(Buffer.prototype.readInt32BE, 4);
    SmartBuffer.prototype.readInt32LE = makeReader(Buffer.prototype.readInt32LE, 4);

    SmartBuffer.prototype.readUInt8 = makeReader(Buffer.prototype.readUInt8, 1);
    SmartBuffer.prototype.readUInt16BE = makeReader(Buffer.prototype.readUInt16BE, 2);
    SmartBuffer.prototype.readUInt16LE = makeReader(Buffer.prototype.readUInt16LE, 2);
    SmartBuffer.prototype.readUInt32BE = makeReader(Buffer.prototype.readUInt32BE, 4);
    SmartBuffer.prototype.readUInt32LE = makeReader(Buffer.prototype.readUInt32LE, 4);

    SmartBuffer.prototype.readFloatBE = makeReader(Buffer.prototype.readFloatBE, 4);
    SmartBuffer.prototype.readFloatLE = makeReader(Buffer.prototype.readFloatLE, 4);

    SmartBuffer.prototype.readDoubleBE = makeReader(Buffer.prototype.readDoubleBE, 8);
    SmartBuffer.prototype.readDoubleLE = makeReader(Buffer.prototype.readDoubleLE, 8);


    /**
     * Reads a string of the given length.
     * @param length {Number} The length of the string to read. (Defaults to the length of the remaining data)
     * @param encoding {String} The encoding to use. (Defaults to encoding set in constructor, or utf8)
     * @returns {string} The string.
     */
    SmartBuffer.prototype.readString = function (length, encoding) {
        var len = Math.min(length, this.length - this._readOffset) || (this.length - this._readOffset);
        var ret = this.buff.slice(this._readOffset, this._readOffset + len).toString(encoding || this.encoding);
        this._readOffset += len;
        return ret;
    };

    /**
     * Reads a null terminated string from the underlying buffer.
     * @param encoding {String} Encoding to use. Defaults to encoding set in constructor, or utf8.
     * @returns {string}
     */
    SmartBuffer.prototype.readStringNT = function (encoding) {
        var nullpos = this.length;
        for (var i = this._readOffset; i < this.length; i++) {
            if (this.buff[i] == 0x00) {
                nullpos = i;
                break;
            }
        }

        var result = this.buff.slice(this._readOffset, nullpos);
        this._readOffset = nullpos + 1;

        return result.toString(encoding || this.encoding);
    };


    /**
     * Reads a specified number of bytes.
     * @param len {Number} Numbers of bytes to read. (Defaults to the remaining data length)
     * @returns {Buffer} Buffer containing the read bytes.
     */
    SmartBuffer.prototype.readBuffer = function (len) {
        var endpoint = Math.min(this.length, this._readOffset + (typeof len === 'number' ? len : this.length));
        var ret = this.buff.slice(this._readOffset, endpoint);
        this._readOffset = endpoint;
        return ret;
    };

    /**
     * Reads a null terminated sequence of bytes from the underlying buffer.
     * @returns {Buffer} Buffer containing the read bytes.
     */
    SmartBuffer.prototype.readBufferNT = function () {
        var nullpos = this.length;
        for (var i = this._readOffset; i < this.length; i++) {
            if (this.buff[i] == 0x00) {
                nullpos = i;
                break;
            }
        }

        var ret = this.buff.slice(this._readOffset, nullpos);
        this._readOffset = nullpos + 1;

        return ret;
    };


    /*
     Write Operations
     */


    SmartBuffer.prototype.writeInt8 = makeWriter(Buffer.prototype.writeInt8, 1);
    SmartBuffer.prototype.writeInt16BE = makeWriter(Buffer.prototype.writeInt16BE, 2);
    SmartBuffer.prototype.writeInt16LE = makeWriter(Buffer.prototype.writeInt16LE, 2);
    SmartBuffer.prototype.writeInt32BE = makeWriter(Buffer.prototype.writeInt32BE, 4);
    SmartBuffer.prototype.writeInt32LE = makeWriter(Buffer.prototype.writeInt32LE, 4);

    SmartBuffer.prototype.writeUInt8 = makeWriter(Buffer.prototype.writeUInt8, 1);
    SmartBuffer.prototype.writeUInt16BE = makeWriter(Buffer.prototype.writeUInt16BE, 2);
    SmartBuffer.prototype.writeUInt16LE = makeWriter(Buffer.prototype.writeUInt16LE, 2);
    SmartBuffer.prototype.writeUInt32BE = makeWriter(Buffer.prototype.writeUInt32BE, 4);
    SmartBuffer.prototype.writeUInt32LE = makeWriter(Buffer.prototype.writeUInt32LE, 4);

    SmartBuffer.prototype.writeFloatBE = makeWriter(Buffer.prototype.writeFloatBE, 4);
    SmartBuffer.prototype.writeFloatLE = makeWriter(Buffer.prototype.writeFloatLE, 4);

    SmartBuffer.prototype.writeDoubleBE = makeWriter(Buffer.prototype.writeDoubleBE, 8);
    SmartBuffer.prototype.writeDoubleLE = makeWriter(Buffer.prototype.writeDoubleLE, 8);


    /**
     * Writes a string to the underlying buffer.
     * @param value {String} The string to write.
     * @param offset {Number} The offset to write the string to. (Encoding can also be set here in place of offset)
     * @param encoding {String} The encoding to use. (Defaults to encoding set in constructor, or to utf8)
     * @returns {*}
     */
    SmartBuffer.prototype.writeString = function (value, offset, encoding) {
        var len, _offset, type = typeof offset;

        if (type === 'number') {
            _offset = offset;
        } else if (type === 'string') {
            encoding = offset;
            offset = this._writeOffset;
        } else {
            encoding = undefined;
            offset = this._writeOffset;
        }

        len = Buffer.byteLength(value, encoding || this.encoding);
        this._ensureWritable(len, _offset);

        this.buff.write(value, offset, len, encoding || this.encoding);
        this._writeOffset += len;
        return this;
    };

    /**
     * Writes a null terminated string to the underlying buffer.
     * @param value {String} The string to write.
     * @param offset {Number} The offset to write the string to. (Encoding can also be set here in place of offset)
     * @param encoding {String} The encoding to use. (Defaults to encoding set in constructor, or to utf8)
     * @returns {*}
     */
    SmartBuffer.prototype.writeStringNT = function (value, offset, encoding) {
        this.writeString(value, offset, encoding);
        this.writeUInt8(0x00, (typeof offset === 'number' ? offset + value.length : this._writeOffset));
        return this;
    };

    /**
     * Writes a Buffer to the underlying buffer.
     * @param value {Buffer} The buffer to write.
     * @param offset {Number} The offset to write the Buffer to.
     * @returns {*}
     */
    SmartBuffer.prototype.writeBuffer = function (value, offset) {
        var len = value.length;
        this._ensureWritable(len, offset);
        value.copy(this.buff, typeof offset === 'number' ? offset : this._writeOffset);
        this._writeOffset += len;
        return this;
    };

    /**
     * Writes a null terminated Buffer to the underlying buffer.
     * @param value {Buffer} The buffer to write.
     * @param offset {Number} The offset to write the Buffer to.
     * @returns {*}
     */
    SmartBuffer.prototype.writeBufferNT = function (value, offset) {
        this.writeBuffer(value, offset);
        this.writeUInt8(0x00, (typeof offset === 'number' ? offset + value.length : this._writeOffset));

        return this;
    };


    /**
     * Resets the Endless Buffer.
     */
    SmartBuffer.prototype.clear = function () {
        this._writeOffset = 0;
        this._readOffset = 0;
        this.length = 0;
    };

    /**
     * Gets the remaining number of bytes to be read from the existing Buffer.
     * @returns {number} The number of bytes remaining.
     */
    SmartBuffer.prototype.remaining = function () {
        return this.length - this._readOffset;
    };

    /**
     * Skips the read position forward by the amount of given.
     * @param amount {Number} The amount of bytes to skip forward.
     */
    SmartBuffer.prototype.skip = function (amount) {
        if (this._readOffset + amount > this.length)
            throw new Error('Target position is beyond the bounds of the data.');

        this._readOffset += amount;
    };

    /**
     * Rewinds the read position backward by the amount given.
     * @param amount {Number} The amount of bytes to reverse backward.
     */
    SmartBuffer.prototype.rewind = function (amount) {
        if (this._readOffset - amount < 0)
            throw new Error('Target position is beyond the bounds of the data.');

        this._readOffset -= amount;
    };

    /**
     * Skips the read position to the given position.
     * @param position {Number} The position to skip to.
     */
    SmartBuffer.prototype.skipTo = function (position) {
        if (position < 0 || position > this.length)
            throw new Error('Target position is beyond the bounds of the data.');

        this._readOffset = position;
    };

    /**
     * Gets the underlying Buffer.
     * @returns {*}
     */
    SmartBuffer.prototype.toBuffer = function () {
        return this.buff.slice(0, this.length);
    };

    /**
     * Gets a string representation of the underlying Buffer.
     * @param encoding {String} Encoding to use. (Defaults to encoding set in constructor, or utf8.)
     * @returns {*}
     */
    SmartBuffer.prototype.toString = function (encoding) {
        return this.buff.toString(encoding || this.encoding, 0, this.length);
    };

    /**
     * Destroys the underlying Buffer, and resets the SmartBuffer.
     */
    SmartBuffer.prototype.destroy = function () {
        delete this.buff;
        this.clear();
    };

    return SmartBuffer;
})();

module.exports = SmartBuffer;