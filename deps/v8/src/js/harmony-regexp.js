// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

'use strict';

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalRegExp = global.RegExp;
var GlobalRegExpPrototype = GlobalRegExp.prototype;
var MakeTypeError;
var regExpFlagsSymbol = utils.ImportNow("regexp_flags_symbol");

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
});

// -------------------------------------------------------------------

// ES6 draft 12-06-13, section 21.2.5.3
// + https://bugs.ecmascript.org/show_bug.cgi?id=3423
function RegExpGetFlags() {
  if (!IS_RECEIVER(this)) {
    throw MakeTypeError(
        kRegExpNonObject, "RegExp.prototype.flags", TO_STRING(this));
  }
  var result = '';
  if (this.global) result += 'g';
  if (this.ignoreCase) result += 'i';
  if (this.multiline) result += 'm';
  if (this.unicode) result += 'u';
  if (this.sticky) result += 'y';
  return result;
}

// ES6 21.2.5.12.
function RegExpGetSticky() {
  if (!IS_REGEXP(this)) {
    // Compat fix: RegExp.prototype.sticky == undefined; UseCounter tracks it
    // TODO(littledan): Remove this workaround or standardize it
    if (this === GlobalRegExpPrototype) {
      %IncrementUseCounter(kRegExpPrototypeStickyGetter);
      return UNDEFINED;
    }
    throw MakeTypeError(kRegExpNonRegExp, "RegExp.prototype.sticky");
  }
  return !!REGEXP_STICKY(this);
}
%FunctionSetName(RegExpGetSticky, "RegExp.prototype.sticky");
%SetNativeFlag(RegExpGetSticky);

utils.InstallGetter(GlobalRegExp.prototype, 'flags', RegExpGetFlags);
utils.InstallGetter(GlobalRegExp.prototype, 'sticky', RegExpGetSticky);

})
