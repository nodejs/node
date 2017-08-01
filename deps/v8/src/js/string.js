// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalString = global.String;
var matchSymbol = utils.ImportNow("match_symbol");
var searchSymbol = utils.ImportNow("search_symbol");

//-------------------------------------------------------------------

// ES6 21.1.3.11.
function StringMatchJS(pattern) {
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

// ES6 21.1.3.15.
function StringSearch(pattern) {
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


// ES6 draft, revision 26 (2014-07-18), section B.2.3.2.1
function HtmlEscape(str) {
  return %RegExpInternalReplace(/"/g, TO_STRING(str), "&quot;");
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.2
function StringAnchor(name) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.anchor");
  return "<a name=\"" + HtmlEscape(name) + "\">" + TO_STRING(this) +
         "</a>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.3
function StringBig() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.big");
  return "<big>" + TO_STRING(this) + "</big>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.4
function StringBlink() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.blink");
  return "<blink>" + TO_STRING(this) + "</blink>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.5
function StringBold() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.bold");
  return "<b>" + TO_STRING(this) + "</b>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.6
function StringFixed() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.fixed");
  return "<tt>" + TO_STRING(this) + "</tt>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.7
function StringFontcolor(color) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.fontcolor");
  return "<font color=\"" + HtmlEscape(color) + "\">" + TO_STRING(this) +
         "</font>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.8
function StringFontsize(size) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.fontsize");
  return "<font size=\"" + HtmlEscape(size) + "\">" + TO_STRING(this) +
         "</font>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.9
function StringItalics() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.italics");
  return "<i>" + TO_STRING(this) + "</i>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.10
function StringLink(s) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.link");
  return "<a href=\"" + HtmlEscape(s) + "\">" + TO_STRING(this) + "</a>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.11
function StringSmall() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.small");
  return "<small>" + TO_STRING(this) + "</small>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.12
function StringStrike() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.strike");
  return "<strike>" + TO_STRING(this) + "</strike>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.13
function StringSub() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.sub");
  return "<sub>" + TO_STRING(this) + "</sub>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.14
function StringSup() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.sup");
  return "<sup>" + TO_STRING(this) + "</sup>";
}

// ES6, section 21.1.3.13
function StringRepeat(count) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.repeat");

  var s = TO_STRING(this);
  var n = TO_INTEGER(count);

  if (n < 0 || n === INFINITY) throw %make_range_error(kInvalidCountValue);

  // Early return to allow an arbitrarily-large repeat of the empty string.
  if (s.length === 0) return "";

  // The maximum string length is stored in a smi, so a longer repeat
  // must result in a range error.
  if (n > %_MaxSmi()) throw %make_range_error(kInvalidStringLength);

  var r = "";
  while (true) {
    if (n & 1) r += s;
    n >>= 1;
    if (n === 0) return r;
    s += s;
  }
}


// ES6 Draft 05-22-2014, section 21.1.3.3
function StringCodePointAt(pos) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.codePointAt");

  var string = TO_STRING(this);
  var size = string.length;
  pos = TO_INTEGER(pos);
  if (pos < 0 || pos >= size) {
    return UNDEFINED;
  }
  var first = %_StringCharCodeAt(string, pos);
  if (first < 0xD800 || first > 0xDBFF || pos + 1 == size) {
    return first;
  }
  var second = %_StringCharCodeAt(string, pos + 1);
  if (second < 0xDC00 || second > 0xDFFF) {
    return first;
  }
  return (first - 0xD800) * 0x400 + second + 0x2400;
}

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

// ES#sec-string.prototype.padstart
// String.prototype.padStart(maxLength [, fillString])
function StringPadStart(maxLength, fillString) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.padStart");
  var thisString = TO_STRING(this);

  return StringPad(thisString, maxLength, fillString) + thisString;
}
%FunctionSetLength(StringPadStart, 1);

// ES#sec-string.prototype.padend
// String.prototype.padEnd(maxLength [, fillString])
function StringPadEnd(maxLength, fillString) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.padEnd");
  var thisString = TO_STRING(this);

  return thisString + StringPad(thisString, maxLength, fillString);
}
%FunctionSetLength(StringPadEnd, 1);

// -------------------------------------------------------------------
// String methods related to templates

// ES6 Draft 03-17-2015, section 21.1.2.4
function StringRaw(callSite) {
  "use strict";
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

// -------------------------------------------------------------------

// Set up the non-enumerable functions on the String object.
utils.InstallFunctions(GlobalString, DONT_ENUM, [
  "raw", StringRaw
]);

// Set up the non-enumerable functions on the String prototype object.
utils.InstallFunctions(GlobalString.prototype, DONT_ENUM, [
  "codePointAt", StringCodePointAt,
  "match", StringMatchJS,
  "padEnd", StringPadEnd,
  "padStart", StringPadStart,
  "repeat", StringRepeat,
  "search", StringSearch,
  "link", StringLink,
  "anchor", StringAnchor,
  "fontcolor", StringFontcolor,
  "fontsize", StringFontsize,
  "big", StringBig,
  "blink", StringBlink,
  "bold", StringBold,
  "fixed", StringFixed,
  "italics", StringItalics,
  "small", StringSmall,
  "strike", StringStrike,
  "sub", StringSub,
  "sup", StringSup
]);

})
