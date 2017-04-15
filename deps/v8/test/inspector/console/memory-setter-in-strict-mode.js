// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print("Tests checks that console.memory property can be set in strict mode (crbug.com/468611).")

Protocol.Runtime.evaluate({ expression: "\"use strict\"\nconsole.memory = {};undefined" }).then(dumpResult);

function dumpResult(result)
{
  InspectorTest.logMessage(result);
  InspectorTest.completeTest();
}
