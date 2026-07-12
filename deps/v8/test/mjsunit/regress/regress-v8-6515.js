// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These patterns shouldn't generate code of excessive size.
assertNull(/\b\B\b\B\b\B\b\B\b\B\b\B\b\B\b\B\b\B/.exec(" aa "));
assertNull(/\b\b\b\b\b\b\b\b\b\B\B\B\B\B\B\B\B\B/.exec(" aa "));
assertNull(/\b\B$\b\B$\b\B$\b\B$\b\B$\b\B$\b\B$/.exec(" aa "));
