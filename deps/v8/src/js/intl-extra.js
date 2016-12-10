// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

var GlobalIntl = global.Intl;

var AddBoundMethod = utils.ImportNow("AddBoundMethod");
var IntlParseDate = utils.ImportNow("IntlParseDate");
var IntlParseNumber = utils.ImportNow("IntlParseNumber");

AddBoundMethod(GlobalIntl.DateTimeFormat, 'v8Parse', IntlParseDate, 1,
               'dateformat');
AddBoundMethod(GlobalIntl.NumberFormat, 'v8Parse', IntlParseNumber, 1,
               'numberformat');

})
