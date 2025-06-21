// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// "let" in non-strict mode can be a label, even if composed of unicode escape
// sequences.

var wasTouched = false;
l\u0065t:
do {
  break l\u0065t;
  wasTouched = true;
} while (false);
// Verify that in addition to no exception thrown, breaking to the label also
// works.
assertFalse(wasTouched);
