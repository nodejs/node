// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalString = global.String;
var matchSymbol = utils.ImportNow("match_symbol");
var searchSymbol = utils.ImportNow("search_symbol");

//-------------------------------------------------------------------

// Set up the non-enumerable functions on the String prototype object.
DEFINE_METHODS(
  GlobalString.prototype,
  {
    /* ES#sec-string.prototype.match */
    match(pattern) {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.match");

      if (!IS_NULL_OR_UNDEFINED(pattern)) {
        var matcher = pattern[matchSymbol];
        if (!IS_UNDEFINED(matcher)) {
          return %_Call(matcher, pattern, this);
        }
      }

      var subject = TO_STRING(this);

      // Equivalent to RegExpCreate (ES#sec-regexpcreate)
      var regexp = %RegExpCreate(pattern);
      return regexp[matchSymbol](subject);
    }

    /* ES#sec-string.prototype.search */
    search(pattern) {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.search");

      if (!IS_NULL_OR_UNDEFINED(pattern)) {
        var searcher = pattern[searchSymbol];
        if (!IS_UNDEFINED(searcher)) {
          return %_Call(searcher, pattern, this);
        }
      }

      var subject = TO_STRING(this);

      // Equivalent to RegExpCreate (ES#sec-regexpcreate)
      var regexp = %RegExpCreate(pattern);
      return %_Call(regexp[searchSymbol], regexp, subject);
    }
  }
);

function StringPad(thisString, maxLength, fillString) {
  maxLength = TO_LENGTH(maxLength);
  var stringLength = thisString.length;

  if (maxLength <= stringLength) return "";

  if (IS_UNDEFINED(fillString)) {
    fillString = " ";
  } else {
    fillString = TO_STRING(fillString);
    if (fillString === "") {
      // If filler is the empty String, return S.
      return "";
    }
  }

  var fillLength = maxLength - stringLength;
  var repetitions = (fillLength / fillString.length) | 0;
  var remainingChars = (fillLength - fillString.length * repetitions) | 0;

  var filler = "";
  while (true) {
    if (repetitions & 1) filler += fillString;
    repetitions >>= 1;
    if (repetitions === 0) break;
    fillString += fillString;
  }

  if (remainingChars) {
    filler += %_SubString(fillString, 0, remainingChars);
  }

  return filler;
}

DEFINE_METHODS_LEN(
  GlobalString.prototype,
  {
    /* ES#sec-string.prototype.padstart */
    /* String.prototype.padStart(maxLength [, fillString]) */
    padStart(maxLength, fillString) {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.padStart");
      var thisString = TO_STRING(this);

      return StringPad(thisString, maxLength, fillString) + thisString;
    }

    /* ES#sec-string.prototype.padend */
    /* String.prototype.padEnd(maxLength [, fillString]) */
    padEnd(maxLength, fillString) {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.padEnd");
      var thisString = TO_STRING(this);

      return thisString + StringPad(thisString, maxLength, fillString);
    }
  },
  1  /* Set functions length */
);

// -------------------------------------------------------------------
// String methods related to templates

// Set up the non-enumerable functions on the String object.
DEFINE_METHOD(
  GlobalString,

  /* ES#sec-string.raw */
  raw(callSite) {
    var numberOfSubstitutions = arguments.length;
    var cooked = TO_OBJECT(callSite);
    var raw = TO_OBJECT(cooked.raw);
    var literalSegments = TO_LENGTH(raw.length);
    if (literalSegments <= 0) return "";

    var result = TO_STRING(raw[0]);

    for (var i = 1; i < literalSegments; ++i) {
      if (i < numberOfSubstitutions) {
        result += TO_STRING(arguments[i]);
      }
      result += TO_STRING(raw[i]);
    }

    return result;
  }
);

// -------------------------------------------------------------------

})
