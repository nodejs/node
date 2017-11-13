/*
 * The Original Code is Mozilla Universal charset detector code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   António Afonso (antonio.afonso gmail.com) - port to JavaScript
 *   Mark Pilgrim - port to Python
 *   Shy Shalom - original C code
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

!function(jschardet) {

jschardet.CharSetProber = function() {
    this.reset = function() {
        this._mState = jschardet.Constants.detecting;
    }

    this.getCharsetName = function() {
        return null;
    }

    this.feed = function(aBuf) {
    }

    this.getState = function() {
        return this._mState;
    }

    this.getConfidence = function() {
        return 0.0;
    }

    this.filterHighBitOnly = function(aBuf) {
        aBuf = aBuf.replace(/[\x00-\x7F]+/g, " ");
        return aBuf;
    }

    this.filterWithoutEnglishLetters = function(aBuf) {
        aBuf = aBuf.replace(/[A-Za-z]+/g, " ");
        return aBuf;
    }

    // Input: aBuf is a string containing all different types of characters
    // Output: a string that contains all alphabetic letters, high-byte characters, and word immediately preceding `>`, but nothing else within `<>`
    // Ex: input - '¡£º <div blah blah> abcdef</div> apples! * and oranges 9jd93jd>'
    //     output - '¡£º blah div apples and oranges jd jd '
    this.filterWithEnglishLetters = function(aBuf) {
        var result = '';
        var inTag = false;
        var prev = 0;

        for (var curr = 0; curr < aBuf.length; curr++) {
          var c = aBuf[curr];

          if (c == '>') {
            inTag = false;
          } else if (c == '<') {
            inTag = true;
          }

          var isAlpha = /[a-zA-Z]/.test(c);
          var isASCII = /^[\x00-\x7F]*$/.test(c);

          if (isASCII && !isAlpha) {
            if (curr > prev && !inTag) {
              result = result + aBuf.substring(prev, curr) + ' ';
            }

            prev = curr + 1;
          }
        }

        if (!inTag) {
          result = result + aBuf.substring(prev);
        }

        return result;
    }
}

}(require('./init'));
