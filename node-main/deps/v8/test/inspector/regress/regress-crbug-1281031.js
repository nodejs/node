// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const contextGroupId = utils.createContextGroup();
const sessionId = utils.connectSession(contextGroupId, '0', () => {});
utils.disconnectSession(sessionId);
utils.print('Did not crash upon invalid non-dictionary state passed to utils.connectSession()');
utils.quit();
