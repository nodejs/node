// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


/**
 * Creates a CSV lines parser.
 */
export class CsvParser {
  /**
   * Converts \x00 and \u0000 escape sequences in the given string.
   *
   * @param {string} input field.
   **/
  escapeField(string) {
    let nextPos = string.indexOf("\\");
    if (nextPos === -1) return string;
    let result = [string.substring(0, nextPos)];
    // Escape sequences of the form \x00 and \u0000;
    let pos = 0;
    while (nextPos !== -1) {
      const escapeIdentifier = string[nextPos + 1];
      pos = nextPos + 2;
      if (escapeIdentifier === 'n') {
        result.push('\n');
        nextPos = pos;
      } else if (escapeIdentifier === '\\') {
        result.push('\\');
        nextPos = pos;
      } else {
        if (escapeIdentifier === 'x') {
          // \x00 ascii range escapes consume 2 chars.
          nextPos = pos + 2;
        } else {
          // \u0000 unicode range escapes consume 4 chars.
          nextPos = pos + 4;
        }
        // Convert the selected escape sequence to a single character.
        const escapeChars = string.substring(pos, nextPos);
        if (escapeChars === '2C') {
            result.push(',');
        } else {
          result.push(String.fromCharCode(parseInt(escapeChars, 16)));
        }
      }

      // Continue looking for the next escape sequence.
      pos = nextPos;
      nextPos = string.indexOf("\\", pos);
      // If there are no more escape sequences consume the rest of the string.
      if (nextPos === -1) {
        result.push(string.substr(pos));
        break;
      } else if (pos !== nextPos) {
        result.push(string.substring(pos, nextPos));
      }
    }
    return result.join('');
  }

  /**
   * Parses a line of CSV-encoded values. Returns an array of fields.
   *
   * @param {string} line Input line.
   */
  parseLine(line) {
    let pos = 0;
    const endPos = line.length;
    const fields = [];
    if (endPos == 0) return fields;
    let nextPos = 0;
    while(nextPos !== -1) {
      nextPos = line.indexOf(',', pos);
      let field;
      if (nextPos === -1) {
        field = line.substr(pos);
      } else {
        field = line.substring(pos, nextPos);
      }
      fields.push(this.escapeField(field));
      pos = nextPos + 1;
    };
    return fields
  }
}
