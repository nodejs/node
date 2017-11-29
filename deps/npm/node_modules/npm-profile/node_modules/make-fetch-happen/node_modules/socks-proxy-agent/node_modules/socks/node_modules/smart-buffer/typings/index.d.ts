// Type definitions for smart-buffer
// Project: https://github.com/JoshGlazebrook/smart-buffer
// Definitions by: Josh Glazebrook <https://github.com/JoshGlazebrook>



declare class SmartBuffer {
    /**
     * Creates a new SmartBuffer instance (defaults to utf8 encoding)
     */
    constructor();

    /**
     * Creates a new SmartBuffer instance
     *
     * @param arg1 { Number } The size the underlying buffer instance should be instantiated to (defaults to 4096)
     * @param arg2 { String } The string encoding to use for reading/writing strings (defaults to utf8)
     */
    constructor(size: number, encoding?: string);

    /**
     * Creates a new SmartBuffer instance
     *
     * @param arg1 { String } The string encoding to use for reading/writing strings (defaults to utf8)
     */
    constructor(encoding?: string);

    /**
     * Creates a new SmartBuffer instance
     *
     * @param arg1 { Buffer } An existing buffer instance to copy to this smart buffer instance
     * @param arg2 { String } The string encoding to use for reading/writing strings (defaults to utf8)
     */
    constructor(buffer: Buffer, encoding?: string)



    // Signed number readers

    /**
     * Reads a 8-bit signed integer
     */
    readInt8(): number;

    /**
     * Reads a 16-bit signed integer (big endian)
     */
    readInt16BE(): number;

    /**
     * Reads a 16-bit signed integer (little endian)
     */
    readInt16LE(): number;

    /**
     * Reads a 32-bit signed integer (big endian)
     */
    readInt32BE(): number;

    /**
     * Reads a 32-bit signed integer (little endian)
     */
    readInt32LE(): number;

    // Unsigned number readers

    /**
     * Reads a 8-bit unsigned integer
     */
    readUInt8(): number;

    /**
     * Reads a 16-bit unsigned integer (big endian)
     */
    readUInt16BE(): number;

    /**
     * Reads a 16-bit unsigned integer (little endian)
     */
    readUInt16LE(): number;

    /**
     * Reads a 32-bit unsigned integer (big endian)
     */
    readUInt32BE(): number;

    /**
     * Reads a 32-bit unsigned integer (little endian)
     */
    readUInt32LE(): number;

    // Floating point readers

    /**
     * Reads a float (big endian)
     */
    readFloatBE(): number;

    /**
     * Reads a float (little endian)
     */
    readFloatLE(): number;

    /**
     * Reads a double (big endian)
     */
    readDoubleBE(): number;

    /**
     * Reads a double (little endian)
     */
    readDoubleLE(): number;

    // String readers

    /**
     * Reads a string
     *
     * @param length { Number } The length of the string to read
     * @param encoding { Number} The encoding to use (defaults to instance level encoding)
     */
    readString(length?: number, encoding?: string): string;

    /**
     * Reads a null terminated string
     *
     * @param encoding The encoding to use (defaults to instance level encoding)
     */
    readStringNT(encoding?: string): string;

    // Buffer readers

    /**
     * Reads binary data into a Buffer
     *
     * @param len { Number } The amount of data to read
     */
    readBuffer(len?: number): Buffer;

    /**
     * Reads null terminated binary data into a Buffer
     */
    readBufferNT(): Buffer;


    // Signed number writers

    /**
     * Writes a 8-bit signed integer value
     *
     * @param value { Number } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeInt8(value: number, offset?: number): this;

    /**
     * Writes a 16-bit signed integer (big endian) value
     *
     * @param value { Number } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeInt16BE(value: number, offset?: number): this;

    /**
     * Writes a 16-bit signed integer (little endian) value
     *
     * @param value { Number } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeInt16LE(value: number, offset?: number): this;

    /**
     * Writes a 32-bit signed integer (big endian) value
     *
     * @param value { Number } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeInt32BE(value: number, offset?: number): this;

    /**
     * Writes a 32-bit signed integer (little endian) value
     *
     * @param value { Number } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeInt32LE(value: number, offset?: number): this;

    // Unsigned number writers

    /**
     * Writes a 8-bit unsigned integer value
     *
     * @param value { Number } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeUInt8(value: number, offset?: number): this;

    /**
     * Writes a 16-bit unsigned integer (big endian) value
     *
     * @param value { Number } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeUInt16BE(value: number, offset?: number): this;

    /**
     * Writes a 16-bit unsigned integer (little endian) value
     *
     * @param value { Number } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeUInt16LE(value: number, offset?: number): this;

    /**
     * Writes a 32-bit unsigned integer (big endian) value
     *
     * @param value { Number } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeUInt32BE(value: number, offset?: number): this;

    /**
     * Writes a 32-bit unsigned integer (little endian) value
     *
     * @param value { Number } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeUInt32LE(value: number, offset?: number): this;

    // Floating point writers

    /**
     * Writes a float (big endian) value
     *
     * @param value { Number } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeFloatBE(value: number, offset?: number): this;

    /**
     * Writes a float (little endian) value
     *
     * @param value { Number } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeFloatLE(value: number, offset?: number): this;

    /**
     * Writes a double (big endian) value
     *
     * @param value { Number } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeDoubleBE(value: number, offset?: number): this;

    /**
     * Writes a double (little endian) value
     *
     * @param value { Number } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeDoubleLE(value: number, offset?: number): this;

    // String writers

    /**
     * Writes a string
     *
     * @param value { String } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    /**
     * Writes a string
     *
     * @param value { String } The value to write to the buffer
     * @param offset { String } The encoding to use when writing the string (defaults to instance level encoding)
     */
    /**
     * Writes a string
     *
     * @param value { String } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     * @param encoding { String } The encoding to use when writing the string (defaults to instance level encoding)
     */
    writeString(value: string, offset?: number | string, encoding?: string): this;

    /**
     * Writes a null terminated string
     *
     * @param value { String } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     */
    /**
     * Writes a null terminated string
     *
     * @param value { String } The value to write to the buffer
     * @param offset { String } The encoding to use when writing the string (defaults to instance level encoding)
     */
    /**
     * Writes a null terminated string
     *
     * @param value { String } The value to write to the buffer
     * @param offset { Number } The offset position to write the value to
     * @param encoding { String } The encoding to use when writing the string (defaults to instance level encoding)
     */
    writeStringNT(value: string, offset?: number | string, encoding?: string): this;

    // Buffer writers

    /**
     * Writes a Buffer
     *
     * @param value { Buffer } The Buffer to write to the smart buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeBuffer(value: Buffer, offset?: number): this;

    /**
     * Writes a Buffer with null termination
     *
     * @param value { Buffer } The buffer to write to the smart buffer
     * @param offset { Number } The offset position to write the value to
     */
    writeBufferNT(value: Buffer, offset?: number): this;


    // Misc Functions

    /**
     * Clears the smart buffer
     */
    clear();

    /**
     * Gets the number of bytes that remain to be read
     */
    remaining(): number;

    /**
     * Increases the read offset position
     *
     * @param amount { Number } The amount to increase the read offset position by
     */
    skip(amount: number);

    /**
     * Changes the read offset position
     *
     * @param position { Number } The position to change the read offset to
     */
    skipTo(position: number);

    /**
     * Decreases the read offset position
     *
     * @param amount { Number } The amount to decrease the read offset position by
     */
    rewind(amount: number);

    /**
     * Gets the underlying Buffer instance
     */
    toBuffer(): Buffer;

    /**
     * Gets the string representation of the underlying Buffer
     *
     * @param encoding { String } The encoding to use (defaults to instance level encoding)
     */
    toString(encoding?: string): string;

    /**
     * Destroys the smart buffer instance
     */
    destroy();

    /**
     * Gets the current length of the smart buffer instance
     */
    length: number;
}

export = SmartBuffer;