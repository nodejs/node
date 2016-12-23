// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {
"use strict";

%CheckIsBootstrapping();

var GlobalIntl = global.Intl;
var FormatDateToParts = utils.ImportNow("FormatDateToParts");

utils.InstallFunctions(GlobalIntl.DateTimeFormat.prototype,  DONT_ENUM, [
    'formatToParts', FormatDateToParts
]);
})
