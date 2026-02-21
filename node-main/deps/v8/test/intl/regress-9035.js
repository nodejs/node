// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These Numbering Systems (nu) are listed in
// http://www.unicode.org/repos/cldr/tags/latest/common/bcp47/number.xml
// with " — algorithmic" in the description but are NOT listed in
// https://www.ecma-international.org/ecma-402/#table-numbering-system-digits
// So there are no mandate to support these nu in a particular way.
// Therefore in here we are only assure it won't throw exception in the
// constructor and format method.
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-armn')).format(0));
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-armnlow')).format(0));
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-cyrl')).format(0));
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-ethi')).format(0));
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-geor')).format(0));
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-grek')).format(0));
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-greklow')).format(0));
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-hans')).format(0));
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-hansfin')).format(0));
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-hant')).format(0));
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-hantfin')).format(0));
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-hebr')).format(0));
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-jpanfin')).format(0));
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-roman')).format(0));
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-romanlow')).format(0));
assertDoesNotThrow(() => (new Intl.NumberFormat('en-u-nu-taml')).format(0));
