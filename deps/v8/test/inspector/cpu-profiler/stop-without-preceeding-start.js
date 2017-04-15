// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print("Test that profiler doesn't crash when we call stop without preceeding start.");

Protocol.Profiler.stop().then(didStopProfile);
function didStopProfile(messageObject)
{
  InspectorTest.expectedError("ProfileAgent.stop", messageObject);
  InspectorTest.completeTest();
}
