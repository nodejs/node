/*global Buffer*/
// Named constants with unique integer values
var C = {};
// Tokens
var LEFT_BRACE    = C.LEFT_BRACE    = 0x1;
var RIGHT_BRACE   = C.RIGHT_BRACE   = 0x2;
var LEFT_BRACKET  = C.LEFT_BRACKET  = 0x3;
var RIGHT_BRACKET = C.RIGHT_BRACKET = 0x4;
var COLON         = C.COLON         = 0x5;
var COMMA         = C.COMMA         = 0x6;
var TRUE          = C.TRUE          = 0x7;
var FALSE         = C.FALSE         = 0x8;
var NULL          = C.NULL          = 0x9;
var STRING        = C.STRING        = 0xa;
var NUMBER        = C.NUMBER        = 0xb;
// Tokenizer States
var START   = C.START   = 0x11;
var STOP    = C.STOP    = 0x12;
var TRUE1   = C.TRUE1   = 0x21;
var TRUE2   = C.TRUE2   = 0x22;
var TRUE3   = C.TRUE3   = 0x23;
var FALSE1  = C.FALSE1  = 0x31;
var FALSE2  = C.FALSE2  = 0x32;
var FALSE3  = C.FALSE3  = 0x33;
var FALSE4  = C.FALSE4  = 0x34;
var NULL1   = C.NULL1   = 0x41;
var NULL2   = C.NULL2   = 0x42;
var NULL3   = C.NULL3   = 0x43;
var NUMBER1 = C.NUMBER1 = 0x51;
var NUMBER3 = C.NUMBER3 = 0x53;
var STRING1 = C.STRING1 = 0x61;
var STRING2 = C.STRING2 = 0x62;
var STRING3 = C.STRING3 = 0x63;
var STRING4 = C.STRING4 = 0x64;
var STRING5 = C.STRING5 = 0x65;
var STRING6 = C.STRING6 = 0x66;
// Parser States
var VALUE   = C.VALUE   = 0x71;
var KEY     = C.KEY     = 0x72;
// Parser Modes
var OBJECT  = C.OBJECT  = 0x81;
var ARRAY   = C.ARRAY   = 0x82;
// Character constants
var BACK_SLASH =      "\\".charCodeAt(0);
var FORWARD_SLASH =   "\/".charCodeAt(0);
var BACKSPACE =       "\b".charCodeAt(0);
var FORM_FEED =       "\f".charCodeAt(0);
var NEWLINE =         "\n".charCodeAt(0);
var CARRIAGE_RETURN = "\r".charCodeAt(0);
var TAB =             "\t".charCodeAt(0);

var STRING_BUFFER_SIZE = 64 * 1024;

function Parser() {
  this.tState = START;
  this.value = undefined;

  this.string = undefined; // string data
  this.stringBuffer = Buffer.alloc ? Buffer.alloc(STRING_BUFFER_SIZE) : new Buffer(STRING_BUFFER_SIZE);
  this.stringBufferOffset = 0;
  this.unicode = undefined; // unicode escapes

  this.key = undefined;
  this.mode = undefined;
  this.stack = [];
  this.state = VALUE;
  this.bytes_remaining = 0; // number of bytes remaining in multi byte utf8 char to read after split boundary
  this.bytes_in_sequence = 0; // bytes in multi byte utf8 char to read
  this.temp_buffs = { "2": new Buffer(2), "3": new Buffer(3), "4": new Buffer(4) }; // for rebuilding chars split before boundary is reached

  // Stream offset
  this.offset = -1;
}

// Slow code to string converter (only used when throwing syntax errors)
Parser.toknam = function (code) {
  var keys = Object.keys(C);
  for (var i = 0, l = keys.length; i < l; i++) {
    var key = keys[i];
    if (C[key] === code) { return key; }
  }
  return code && ("0x" + code.toString(16));
};

var proto = Parser.prototype;
proto.onError = function (err) { throw err; };
proto.charError = function (buffer, i) {
  this.tState = STOP;
  this.onError(new Error("Unexpected " + JSON.stringify(String.fromCharCode(buffer[i])) + " at position " + i + " in state " + Parser.toknam(this.tState)));
};
proto.appendStringChar = function (char) {
  if (this.stringBufferOffset >= STRING_BUFFER_SIZE) {
    this.string += this.stringBuffer.toString('utf8');
    this.stringBufferOffset = 0;
  }

  this.stringBuffer[this.stringBufferOffset++] = char;
};
proto.appendStringBuf = function (buf, start, end) {
  var size = buf.length;
  if (typeof start === 'number') {
    if (typeof end === 'number') {
      if (end < 0) {
        // adding a negative end decreeses the size
        size = buf.length - start + end;
      } else {
        size = end - start;
      }
    } else {
      size = buf.length - start;
    }
  }

  if (size < 0) {
    size = 0;
  }

  if (this.stringBufferOffset + size > STRING_BUFFER_SIZE) {
    this.string += this.stringBuffer.toString('utf8', 0, this.stringBufferOffset);
    this.stringBufferOffset = 0;
  }

  buf.copy(this.stringBuffer, this.stringBufferOffset, start, end);
  this.stringBufferOffset += size;
};
proto.write = function (buffer) {
  if (typeof buffer === "string") buffer = new Buffer(buffer);
  var n;
  for (var i = 0, l = buffer.length; i < l; i++) {
    if (this.tState === START){
      n = buffer[i];
      this.offset++;
      if(n === 0x7b){ this.onToken(LEFT_BRACE, "{"); // {
      }else if(n === 0x7d){ this.onToken(RIGHT_BRACE, "}"); // }
      }else if(n === 0x5b){ this.onToken(LEFT_BRACKET, "["); // [
      }else if(n === 0x5d){ this.onToken(RIGHT_BRACKET, "]"); // ]
      }else if(n === 0x3a){ this.onToken(COLON, ":");  // :
      }else if(n === 0x2c){ this.onToken(COMMA, ","); // ,
      }else if(n === 0x74){ this.tState = TRUE1;  // t
      }else if(n === 0x66){ this.tState = FALSE1;  // f
      }else if(n === 0x6e){ this.tState = NULL1; // n
      }else if(n === 0x22){ // "
        this.string = "";
        this.stringBufferOffset = 0;
        this.tState = STRING1;
      }else if(n === 0x2d){ this.string = "-"; this.tState = NUMBER1; // -
      }else{
        if (n >= 0x30 && n < 0x40) { // 1-9
          this.string = String.fromCharCode(n); this.tState = NUMBER3;
        } else if (n === 0x20 || n === 0x09 || n === 0x0a || n === 0x0d) {
          // whitespace
        } else {
          return this.charError(buffer, i);
        }
      }
    }else if (this.tState === STRING1){ // After open quote
      n = buffer[i]; // get current byte from buffer
      // check for carry over of a multi byte char split between data chunks
      // & fill temp buffer it with start of this data chunk up to the boundary limit set in the last iteration
      if (this.bytes_remaining > 0) {
        for (var j = 0; j < this.bytes_remaining; j++) {
          this.temp_buffs[this.bytes_in_sequence][this.bytes_in_sequence - this.bytes_remaining + j] = buffer[j];
        }

        this.appendStringBuf(this.temp_buffs[this.bytes_in_sequence]);
        this.bytes_in_sequence = this.bytes_remaining = 0;
        i = i + j - 1;
      } else if (this.bytes_remaining === 0 && n >= 128) { // else if no remainder bytes carried over, parse multi byte (>=128) chars one at a time
        if (n <= 193 || n > 244) {
          return this.onError(new Error("Invalid UTF-8 character at position " + i + " in state " + Parser.toknam(this.tState)));
        }
        if ((n >= 194) && (n <= 223)) this.bytes_in_sequence = 2;
        if ((n >= 224) && (n <= 239)) this.bytes_in_sequence = 3;
        if ((n >= 240) && (n <= 244)) this.bytes_in_sequence = 4;
        if ((this.bytes_in_sequence + i) > buffer.length) { // if bytes needed to complete char fall outside buffer length, we have a boundary split
          for (var k = 0; k <= (buffer.length - 1 - i); k++) {
            this.temp_buffs[this.bytes_in_sequence][k] = buffer[i + k]; // fill temp buffer of correct size with bytes available in this chunk
          }
          this.bytes_remaining = (i + this.bytes_in_sequence) - buffer.length;
          i = buffer.length - 1;
        } else {
          this.appendStringBuf(buffer, i, i + this.bytes_in_sequence);
          i = i + this.bytes_in_sequence - 1;
        }
      } else if (n === 0x22) {
        this.tState = START;
        this.string += this.stringBuffer.toString('utf8', 0, this.stringBufferOffset);
        this.stringBufferOffset = 0;
        this.onToken(STRING, this.string);
        this.offset += Buffer.byteLength(this.string, 'utf8') + 1;
        this.string = undefined;
      }
      else if (n === 0x5c) {
        this.tState = STRING2;
      }
      else if (n >= 0x20) { this.appendStringChar(n); }
      else {
          return this.charError(buffer, i);
      }
    }else if (this.tState === STRING2){ // After backslash
      n = buffer[i];
      if(n === 0x22){ this.appendStringChar(n); this.tState = STRING1;
      }else if(n === 0x5c){ this.appendStringChar(BACK_SLASH); this.tState = STRING1;
      }else if(n === 0x2f){ this.appendStringChar(FORWARD_SLASH); this.tState = STRING1;
      }else if(n === 0x62){ this.appendStringChar(BACKSPACE); this.tState = STRING1;
      }else if(n === 0x66){ this.appendStringChar(FORM_FEED); this.tState = STRING1;
      }else if(n === 0x6e){ this.appendStringChar(NEWLINE); this.tState = STRING1;
      }else if(n === 0x72){ this.appendStringChar(CARRIAGE_RETURN); this.tState = STRING1;
      }else if(n === 0x74){ this.appendStringChar(TAB); this.tState = STRING1;
      }else if(n === 0x75){ this.unicode = ""; this.tState = STRING3;
      }else{
        return this.charError(buffer, i);
      }
    }else if (this.tState === STRING3 || this.tState === STRING4 || this.tState === STRING5 || this.tState === STRING6){ // unicode hex codes
      n = buffer[i];
      // 0-9 A-F a-f
      if ((n >= 0x30 && n < 0x40) || (n > 0x40 && n <= 0x46) || (n > 0x60 && n <= 0x66)) {
        this.unicode += String.fromCharCode(n);
        if (this.tState++ === STRING6) {
          this.appendStringBuf(Buffer(String.fromCharCode(parseInt(this.unicode, 16))));
          this.unicode = undefined;
          this.tState = STRING1;
        }
      } else {
        return this.charError(buffer, i);
      }
    } else if (this.tState === NUMBER1 || this.tState === NUMBER3) {
        n = buffer[i];

        switch (n) {
          case 0x30: // 0
          case 0x31: // 1
          case 0x32: // 2
          case 0x33: // 3
          case 0x34: // 4
          case 0x35: // 5
          case 0x36: // 6
          case 0x37: // 7
          case 0x38: // 8
          case 0x39: // 9
          case 0x2e: // .
          case 0x65: // e
          case 0x45: // E
          case 0x2b: // +
          case 0x2d: // -
            this.string += String.fromCharCode(n);
            this.tState = NUMBER3;
            break;
          default:
            this.tState = START;
            var result = Number(this.string);

            if (isNaN(result)){
              return this.charError(buffer, i);
            }

            if ((this.string.match(/[0-9]+/) == this.string) && (result.toString() != this.string)) {
              // Long string of digits which is an ID string and not valid and/or safe JavaScript integer Number
              this.onToken(STRING, this.string);
            } else {
              this.onToken(NUMBER, result);
            }

            this.offset += this.string.length - 1;
            this.string = undefined;
            i--;
            break;
        }
    }else if (this.tState === TRUE1){ // r
      if (buffer[i] === 0x72) { this.tState = TRUE2; }
      else { return this.charError(buffer, i); }
    }else if (this.tState === TRUE2){ // u
      if (buffer[i] === 0x75) { this.tState = TRUE3; }
      else { return this.charError(buffer, i); }
    }else if (this.tState === TRUE3){ // e
      if (buffer[i] === 0x65) { this.tState = START; this.onToken(TRUE, true); this.offset+= 3; }
      else { return this.charError(buffer, i); }
    }else if (this.tState === FALSE1){ // a
      if (buffer[i] === 0x61) { this.tState = FALSE2; }
      else { return this.charError(buffer, i); }
    }else if (this.tState === FALSE2){ // l
      if (buffer[i] === 0x6c) { this.tState = FALSE3; }
      else { return this.charError(buffer, i); }
    }else if (this.tState === FALSE3){ // s
      if (buffer[i] === 0x73) { this.tState = FALSE4; }
      else { return this.charError(buffer, i); }
    }else if (this.tState === FALSE4){ // e
      if (buffer[i] === 0x65) { this.tState = START; this.onToken(FALSE, false); this.offset+= 4; }
      else { return this.charError(buffer, i); }
    }else if (this.tState === NULL1){ // u
      if (buffer[i] === 0x75) { this.tState = NULL2; }
      else { return this.charError(buffer, i); }
    }else if (this.tState === NULL2){ // l
      if (buffer[i] === 0x6c) { this.tState = NULL3; }
      else { return this.charError(buffer, i); }
    }else if (this.tState === NULL3){ // l
      if (buffer[i] === 0x6c) { this.tState = START; this.onToken(NULL, null); this.offset += 3; }
      else { return this.charError(buffer, i); }
    }
  }
};
proto.onToken = function (token, value) {
  // Override this to get events
};

proto.parseError = function (token, value) {
  this.tState = STOP;
  this.onError(new Error("Unexpected " + Parser.toknam(token) + (value ? ("(" + JSON.stringify(value) + ")") : "") + " in state " + Parser.toknam(this.state)));
};
proto.push = function () {
  this.stack.push({value: this.value, key: this.key, mode: this.mode});
};
proto.pop = function () {
  var value = this.value;
  var parent = this.stack.pop();
  this.value = parent.value;
  this.key = parent.key;
  this.mode = parent.mode;
  this.emit(value);
  if (!this.mode) { this.state = VALUE; }
};
proto.emit = function (value) {
  if (this.mode) { this.state = COMMA; }
  this.onValue(value);
};
proto.onValue = function (value) {
  // Override me
};
proto.onToken = function (token, value) {
  if(this.state === VALUE){
    if(token === STRING || token === NUMBER || token === TRUE || token === FALSE || token === NULL){
      if (this.value) {
        this.value[this.key] = value;
      }
      this.emit(value);
    }else if(token === LEFT_BRACE){
      this.push();
      if (this.value) {
        this.value = this.value[this.key] = {};
      } else {
        this.value = {};
      }
      this.key = undefined;
      this.state = KEY;
      this.mode = OBJECT;
    }else if(token === LEFT_BRACKET){
      this.push();
      if (this.value) {
        this.value = this.value[this.key] = [];
      } else {
        this.value = [];
      }
      this.key = 0;
      this.mode = ARRAY;
      this.state = VALUE;
    }else if(token === RIGHT_BRACE){
      if (this.mode === OBJECT) {
        this.pop();
      } else {
        return this.parseError(token, value);
      }
    }else if(token === RIGHT_BRACKET){
      if (this.mode === ARRAY) {
        this.pop();
      } else {
        return this.parseError(token, value);
      }
    }else{
      return this.parseError(token, value);
    }
  }else if(this.state === KEY){
    if (token === STRING) {
      this.key = value;
      this.state = COLON;
    } else if (token === RIGHT_BRACE) {
      this.pop();
    } else {
      return this.parseError(token, value);
    }
  }else if(this.state === COLON){
    if (token === COLON) { this.state = VALUE; }
    else { return this.parseError(token, value); }
  }else if(this.state === COMMA){
    if (token === COMMA) {
      if (this.mode === ARRAY) { this.key++; this.state = VALUE; }
      else if (this.mode === OBJECT) { this.state = KEY; }

    } else if (token === RIGHT_BRACKET && this.mode === ARRAY || token === RIGHT_BRACE && this.mode === OBJECT) {
      this.pop();
    } else {
      return this.parseError(token, value);
    }
  }else{
    return this.parseError(token, value);
  }
};

Parser.C = C;

module.exports = Parser;
