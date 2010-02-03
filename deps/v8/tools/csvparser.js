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


// Initlialize namespaces.
var devtools = devtools || {};
devtools.profiler = devtools.profiler || {};


/**
 * Creates a CSV lines parser.
 */
devtools.profiler.CsvParser = function() {
};


/**
 * A regex for matching a CSV field.
 * @private
 */
devtools.profiler.CsvParser.CSV_FIELD_RE_ = /^"((?:[^"]|"")*)"|([^,]*)/;


/**
 * A regex for matching a double quote.
 * @private
 */
devtools.profiler.CsvParser.DOUBLE_QUOTE_RE_ = /""/g;


/**
 * Parses a line of CSV-encoded values. Returns an array of fields.
 *
 * @param {string} line Input line.
 */
devtools.profiler.CsvParser.prototype.parseLine = function(line) {
  var fieldRe = devtools.profiler.CsvParser.CSV_FIELD_RE_;
  var doubleQuoteRe = devtools.profiler.CsvParser.DOUBLE_QUOTE_RE_;
  var pos = 0;
  var endPos = line.length;
  var fields = [];
  if (endPos > 0) {
    do {
      var fieldMatch = fieldRe.exec(line.substr(pos));
      if (typeof fieldMatch[1] === "string") {
        var field = fieldMatch[1];
        pos += field.length + 3;  // Skip comma and quotes.
        fields.push(field.replace(doubleQuoteRe, '"'));
      } else {
        // The second field pattern will match anything, thus
        // in the worst case the match will be an empty string.
        var field = fieldMatch[2];
        pos += field.length + 1;  // Skip comma.
        fields.push(field);
      }
    } while (pos <= endPos);
  }
  return fields;
};
