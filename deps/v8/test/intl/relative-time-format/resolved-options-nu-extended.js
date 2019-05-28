// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Testing 'bn' locale with -u-nu.
// Split from test/intl/relative-time-format/resolved-options-nu.js
// because Android not yet include bn locale data.

// For locale default the numberingSystem to other than 'latn'
assertEquals(
    "beng",
    new Intl.RelativeTimeFormat("bn").resolvedOptions().numberingSystem
);

// For locale which default others but  use -u-nu-latn to change to 'latn' numberingSystem
assertEquals(
    "latn",
    new Intl.RelativeTimeFormat("bn-u-nu-latn").resolvedOptions()
        .numberingSystem
);
// For locale use -u-nu- with invalid value still back to default.
assertEquals(
    "beng",
    new Intl.RelativeTimeFormat("bn-u-nu-abcd").resolvedOptions()
        .numberingSystem
);
