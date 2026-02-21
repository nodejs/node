// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, Protocol } = InspectorTest.start('Don\'t crash when disconnecting the session while message tasks are still in-flight.');

Protocol.Runtime.evaluate({ expression: '1+1' });
session.reconnect();  // Reconnect the session while the respone for Runtime#evaluate is still pending.

InspectorTest.completeTest();
