// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

'use strict';

%CheckIsBootstrapping();

var GlobalRegExp = global.RegExp;

// -------------------------------------------------------------------

// ES6 draft 12-06-13, section 21.2.5.3
// + https://bugs.ecmascript.org/show_bug.cgi?id=3423
function RegExpGetFlags() {
  if (!IS_SPEC_OBJECT(this)) {
    throw MakeTypeError(kFlagsGetterNonObject, $toString(this));
  }
  var result = '';
  if (this.global) result += 'g';
  if (this.ignoreCase) result += 'i';
  if (this.multiline) result += 'm';
  if (this.unicode) result += 'u';
  if (this.sticky) result += 'y';
  return result;
}

%DefineAccessorPropertyUnchecked(GlobalRegExp.prototype, 'flags',
                                 RegExpGetFlags, null, DONT_ENUM);
%SetNativeFlag(RegExpGetFlags);

})
