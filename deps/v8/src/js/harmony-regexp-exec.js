// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalRegExp = global.RegExp;
var RegExpSubclassExecJS = utils.ImportNow("RegExpSubclassExecJS");
var RegExpSubclassMatch = utils.ImportNow("RegExpSubclassMatch");
var RegExpSubclassReplace = utils.ImportNow("RegExpSubclassReplace");
var RegExpSubclassSearch = utils.ImportNow("RegExpSubclassSearch");
var RegExpSubclassSplit = utils.ImportNow("RegExpSubclassSplit");
var RegExpSubclassTest = utils.ImportNow("RegExpSubclassTest");
var matchSymbol = utils.ImportNow("match_symbol");
var replaceSymbol = utils.ImportNow("replace_symbol");
var searchSymbol = utils.ImportNow("search_symbol");
var splitSymbol = utils.ImportNow("split_symbol");

utils.OverrideFunction(GlobalRegExp.prototype, "exec",
                       RegExpSubclassExecJS, true);
utils.OverrideFunction(GlobalRegExp.prototype, matchSymbol,
                       RegExpSubclassMatch, true);
utils.OverrideFunction(GlobalRegExp.prototype, replaceSymbol,
                       RegExpSubclassReplace, true);
utils.OverrideFunction(GlobalRegExp.prototype, searchSymbol,
                       RegExpSubclassSearch, true);
utils.OverrideFunction(GlobalRegExp.prototype, splitSymbol,
                       RegExpSubclassSplit, true);
utils.OverrideFunction(GlobalRegExp.prototype, "test",
                       RegExpSubclassTest, true);

})
