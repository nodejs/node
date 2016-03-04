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

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
});

// -------------------------------------------------------------------

// ES6 21.2.5.15.
function RegExpGetUnicode() {
  if (!IS_REGEXP(this)) {
    // TODO(littledan): Remove this RegExp compat workaround
    if (this === GlobalRegExpPrototype) {
      %IncrementUseCounter(kRegExpPrototypeUnicodeGetter);
      return UNDEFINED;
    }
    throw MakeTypeError(kRegExpNonRegExp, "RegExp.prototype.unicode");
  }
  return !!REGEXP_UNICODE(this);
}
%FunctionSetName(RegExpGetUnicode, "RegExp.prototype.unicode");
%SetNativeFlag(RegExpGetUnicode);

utils.InstallGetter(GlobalRegExp.prototype, 'unicode', RegExpGetUnicode);

})
