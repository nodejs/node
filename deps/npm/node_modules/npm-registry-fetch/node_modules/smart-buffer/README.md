smart-buffer  [![Build Status](https://travis-ci.org/JoshGlazebrook/smart-buffer.svg?branch=master)](https://travis-ci.org/JoshGlazebrook/smart-buffer)  [![Coverage Status](https://coveralls.io/repos/github/JoshGlazebrook/smart-buffer/badge.svg?branch=master)](https://coveralls.io/github/JoshGlazebrook/smart-buffer?branch=master)
=============

smart-buffer is a light Buffer wrapper that takes away the need to keep track of what position to read and write data to and from the underlying Buffer. It also adds null terminating string operations and **grows** as you add more data.


### What it's useful for:

I created smart-buffer because I wanted to simplify the process of using Buffer for building and reading network packets to send over a socket. Rather than having to keep track of which position I need to write a UInt16 to after adding a string of variable length, I simply don't have to.

Key Features:
* Proxies all of the Buffer write and read functions.
* Keeps track of read and write positions for you.
* Grows the internal Buffer as you add data to it.
* Useful string operations. (Null terminating strings)
* Allows for inserting values at specific points in the internal Buffer.

#### Note:
smart-buffer can be used for writing to an underlying buffer as well as reading from it. It however does not function correctly if you're mixing both read and write operations with each other.

## Installing:

`npm install smart-buffer`

## Using smart-buffer

### Example

Say you were building a packet that had to conform to the following protocol:

`[PacketType:2][PacketLength:2][Data:XX]`

To build this packet using the vanilla Buffer class, you would have to count up the length of the data payload beforehand. You would also need to keep track of the current "cursor" position in your Buffer so you write everything in the right places. With smart-buffer you don't have to do either of those things.

```javascript
function createLoginPacket(username, password, age, country) {
    var packet = new SmartBuffer();
    packet.writeUInt16LE(0x0060); // Login Packet Type/ID
    packet.writeStringNT(username);
    packet.writeStringNT(password);
    packet.writeUInt8(age);
    packet.writeStringNT(country);
    packet.writeUInt16LE(packet.length - 2, 2);

    return packet.toBuffer();
}
```
With the above function, you now can do this:
```javascript
var login = createLoginPacket("Josh", "secret123", 22, "United States");

// <Buffer 60 00 1e 00 4a 6f 73 68 00 73 65 63 72 65 74 31 32 33 00 16 55 6e 69 74 65 64 20 53 74 61 74 65 73 00>
```
Notice that the `[PacketLength:2]` part of the packet was inserted after we had added everything else, and as shown in the Buffer dump above, is in the correct location along with everything else.

Reading back the packet we created above is just as easy:
```javascript

var reader = new SmartBuffer(login);

var logininfo = {
    packetType: reader.readUInt16LE(),
    packetLength: reader.readUInt16LE(),
    username: reader.readStringNT(),
    password: reader.readStringNT(),
    age: reader.readUInt8(),
    country: reader.readStringNT()
};

/*
{
    packetType: 96, (0x0060)
    packetLength: 30,
    username: 'Josh',
    password: 'secret123',
    age: 22,
    country: 'United States'
};
*/
```

# Api Reference:

### Constructing a smart-buffer

smart-buffer has a few different constructor signatures you can use. By default, utf8 encoding is used, and the internal Buffer length will be 4096. When reading from a Buffer, smart-buffer does NOT make a copy of the Buffer. It reads from the Buffer it was given.

```javascript
var SmartBuffer = require('smart-buffer');

// Reading from an existing Buffer:
var reader = new SmartBuffer(buffer);
var reader = new SmartBuffer(buffer, 'ascii');

// Writing to a new Buffer:
var writer = new SmartBuffer();               // Defaults to utf8, 4096 length internal Buffer.
var writer = new SmartBuffer(1024);           // Defaults to utf8, 1024 length internal Buffer.
var writer = new SmartBuffer('ascii');         // Sets to ascii encoding, 4096 length internal buffer.
var writer = new SmartBuffer(1024, 'ascii');  // Sets to ascii encoding, 1024 length internal buffer.
```

## Reading Data

smart-buffer supports all of the common read functions you will find in the vanilla Buffer class. The only difference is, you do not need to specify which location to start reading from. This is possible because as you read data out of a smart-buffer, it automatically progresses an internal read offset/position to know where to pick up from on the next read.

## Reading Numeric Values

When numeric values, you simply need to call the function you want, and the data is returned.

Supported Operations:
* readInt8
* readInt16BE
* readInt16LE
* readInt32BE
* readInt32LE
* readUInt8
* readUInt16BE
* readUInt16LE
* readUInt32BE
* readUInt32LE
* readFloatBE
* readFloatLE
* readDoubleBE
* readDoubleLE

```javascript
var reader = new SmartBuffer(somebuffer);
var num = reader.readInt8();
```

## Reading String Values

When reading String values, you can either choose to read a null terminated string, or a string of a specified length.

### SmartBuffer.readStringNT( [encoding] )
> `String` **String encoding to use**  - Defaults to the encoding set in the constructor, or utf8.

returns `String`

> Note: When readStringNT is called and there is no null character found, smart-buffer will read to the end of the internal Buffer.

### SmartBuffer.readString( [length], [encoding] )
### SmartBuffer.readString( [length] )
### SmartBuffer.readString( [encoding] )
> `Number` **Length of the string to read**

> `String` **String encoding to use** - Defaults to the encoding set in the constructor, or utf8.

returns `String`

> Note: When readString is called without a specified length, smart-buffer will read to the end of the internal Buffer.



## Reading Buffer Values

### SmartBuffer.readBuffer( length )
> `Number` **Length of data to read into a Buffer**

returns `Buffer`

> Note: This function uses `slice` to retrieve the Buffer.


### SmartBuffer.readBufferNT()

returns `Buffer`

> Note: This reads the next sequence of bytes in the buffer until a null (0x00) value is found. (Null terminated buffer)
> Note: This function uses `slice` to retrieve the Buffer.


## Writing Data

smart-buffer supports all of the common write functions you will find in the vanilla Buffer class. The only difference is, you do not need to specify which location to write to in your Buffer by default. You do however have the option of **inserting** a piece of data into your smart-buffer at a given location.


## Writing Numeric Values


For numeric values, you simply need to call the function you want, and the data is written at the end of the internal Buffer's current write position. You can specify a offset/position to **insert** the given value at, but keep in mind this does not override data at the given position. This feature also does not work properly when inserting a value beyond the current internal length of the smart-buffer (length being the .length property of the smart-buffer instance you're writing to)

Supported Operations:
* writeInt8
* writeInt16BE
* writeInt16LE
* writeInt32BE
* writeInt32LE
* writeUInt8
* writeUInt16BE
* writeUInt16LE
* writeUInt32BE
* writeUInt32LE
* writeFloatBE
* writeFloatLE
* writeDoubleBE
* writeDoubleLE

The following signature is the same for all the above functions:

### SmartBuffer.writeInt8( value, [offset] )
> `Number` **A valid Int8 number**

> `Number` **The position to insert this value at**

returns this

> Note: All write operations return `this` to allow for chaining.

## Writing String Values

When reading String values, you can either choose to write a null terminated string, or a non null terminated string.

### SmartBuffer.writeStringNT( value, [offset], [encoding] )
### SmartBuffer.writeStringNT( value, [offset] )
### SmartBuffer.writeStringNT( value, [encoding] )
> `String` **String value to write**

> `Number` **The position to insert this String at**

> `String` **The String encoding to use.** - Defaults to the encoding set in the constructor, or utf8.

returns this

### SmartBuffer.writeString( value, [offset], [encoding] )
### SmartBuffer.writeString( value, [offset] )
### SmartBuffer.writeString( value, [encoding] )
> `String` **String value to write**

> `Number` **The position to insert this String at**

> `String` **The String encoding to use** - Defaults to the encoding set in the constructor, or utf8.

returns this


## Writing Buffer Values

### SmartBuffer.writeBuffer( value, [offset] )
> `Buffer` **Buffer value to write**

> `Number` **The position to insert this Buffer's content at**

returns this

### SmartBuffer.writeBufferNT( value, [offset] )
> `Buffer` **Buffer value to write**

> `Number` **The position to insert this Buffer's content at**

returns this


## Utility Functions

### SmartBuffer.clear()
Resets the SmartBuffer to its default state where it can be reused for reading or writing.

### SmartBuffer.remaining()

returns `Number` The amount of data left to read based on the current read Position.

### SmartBuffer.skip( value )
> `Number` **The amount of bytes to skip ahead**

Skips the read position ahead by the given value.

returns this

### SmartBuffer.rewind( value )
> `Number` **The amount of bytes to reward backwards**

Rewinds the read position backwards by the given value.

returns this

### SmartBuffer.skipTo( position )
> `Number` **The point to skip the read position to**

Moves the read position to the given point.
returns this

### SmartBuffer.toBuffer()

returns `Buffer` A Buffer containing the contents of the internal Buffer.

> Note: This uses the slice function.

### SmartBuffer.toString( [encoding] )
> `String` **The String encoding to use** - Defaults to the encoding set in the constructor, or utf8.

returns `String` The internal Buffer in String representation.

### SmartBuffer.destroy()
Attempts to destroy the smart-buffer.

returns this

## Properties

### SmartBuffer.length

returns `Number` **The length of the data that is being tracked in the internal Buffer** - Does NOT return the absolute length of the internal Buffer being written to.

## License

This work is licensed under the [MIT license](http://en.wikipedia.org/wiki/MIT_License).