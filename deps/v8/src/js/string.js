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

// ES#sec-createhtml
function HtmlEscape(str) {
  return %RegExpInternalReplace(/"/g, TO_STRING(str), "&quot;");
}

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

    /* ES#sec-string.prototype.anchor */
    anchor(name) {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.anchor");
      return "<a name=\"" + HtmlEscape(name) + "\">" + TO_STRING(this) +
             "</a>";
    }

    /* ES#sec-string.prototype.big */
    big() {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.big");
      return "<big>" + TO_STRING(this) + "</big>";
    }

    /* ES#sec-string.prototype.blink */
    blink() {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.blink");
      return "<blink>" + TO_STRING(this) + "</blink>";
    }

    /* ES#sec-string.prototype.bold */
    bold() {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.bold");
      return "<b>" + TO_STRING(this) + "</b>";
    }

    /* ES#sec-string.prototype.fixed */
    fixed() {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.fixed");
      return "<tt>" + TO_STRING(this) + "</tt>";
    }

    /* ES#sec-string.prototype.fontcolor */
    fontcolor(color) {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.fontcolor");
      return "<font color=\"" + HtmlEscape(color) + "\">" + TO_STRING(this) +
             "</font>";
    }

    /* ES#sec-string.prototype.fontsize */
    fontsize(size) {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.fontsize");
      return "<font size=\"" + HtmlEscape(size) + "\">" + TO_STRING(this) +
             "</font>";
    }

    /* ES#sec-string.prototype.italics */
    italics() {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.italics");
      return "<i>" + TO_STRING(this) + "</i>";
    }

    /* ES#sec-string.prototype.link */
    link(s) {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.link");
      return "<a href=\"" + HtmlEscape(s) + "\">" + TO_STRING(this) + "</a>";
    }

    /* ES#sec-string.prototype.small */
    small() {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.small");
      return "<small>" + TO_STRING(this) + "</small>";
    }

    /* ES#sec-string.prototype.strike */
    strike() {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.strike");
      return "<strike>" + TO_STRING(this) + "</strike>";
    }

    /* ES#sec-string.prototype.sub */
    sub() {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.sub");
      return "<sub>" + TO_STRING(this) + "</sub>";
    }

    /* ES#sec-string.prototype.sup */
    sup() {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.sup");
      return "<sup>" + TO_STRING(this) + "</sup>";
    }

    /* ES#sec-string.prototype.repeat */
    repeat(count) {
      CHECK_OBJECT_COERCIBLE(this, "String.prototype.repeat");

      var s = TO_STRING(this);
      var n = TO_INTEGER(count);

      if (n < 0 || n === INFINITY) throw %make_range_error(kInvalidCountValue);

      // Early return to allow an arbitrarily-large repeat of the empty string.
      if (s.length === 0) return "";

      // The maximum string length is stored in a smi, so a longer repeat
      // must result in a range error.
      if (n > %_StringMaxLength()) %ThrowInvalidStringLength();

      var r = "";
      while (true) {
        if (n & 1) r += s;
        n >>= 1;
        if (n === 0) return r;
        s += s;
      }
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
