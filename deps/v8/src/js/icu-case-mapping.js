// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {
"use strict";

%CheckIsBootstrapping();

var GlobalString = global.String;
var OverrideFunction = utils.OverrideFunction;
var ToLowerCaseI18N = utils.ImportNow("ToLowerCaseI18N");
var ToUpperCaseI18N = utils.ImportNow("ToUpperCaseI18N");
var ToLocaleLowerCaseI18N = utils.ImportNow("ToLocaleLowerCaseI18N");
var ToLocaleUpperCaseI18N = utils.ImportNow("ToLocaleUpperCaseI18N");

OverrideFunction(GlobalString.prototype, 'toLowerCase', ToLowerCaseI18N, true);
OverrideFunction(GlobalString.prototype, 'toUpperCase', ToUpperCaseI18N, true);
OverrideFunction(GlobalString.prototype, 'toLocaleLowerCase',
                 ToLocaleLowerCaseI18N, true);
OverrideFunction(GlobalString.prototype, 'toLocaleUpperCase',
                 ToLocaleUpperCaseI18N, true);

})
