// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For locale default the numberingSystem to 'latn'
assertEquals(
    "latn",
    new Intl.RelativeTimeFormat("ar").resolvedOptions().numberingSystem
);
assertEquals(
    "latn",
    new Intl.RelativeTimeFormat("en").resolvedOptions().numberingSystem
);
assertEquals(
    "latn",
    new Intl.RelativeTimeFormat("fr").resolvedOptions().numberingSystem
);
assertEquals(
    "latn",
    new Intl.RelativeTimeFormat("hi").resolvedOptions().numberingSystem
);
assertEquals(
    "latn",
    new Intl.RelativeTimeFormat("th").resolvedOptions().numberingSystem
);
assertEquals(
    "latn",
    new Intl.RelativeTimeFormat("zh-Hant").resolvedOptions().numberingSystem
);

// For locale default the numberingSystem to other than 'latn'
assertEquals(
    "arab",
    new Intl.RelativeTimeFormat("ar-TD").resolvedOptions().numberingSystem
);
assertEquals(
    "arabext",
    new Intl.RelativeTimeFormat("fa").resolvedOptions().numberingSystem
);

// For locale use -u-nu- to change to other numberingSystem
assertEquals(
    "thai",
    new Intl.RelativeTimeFormat("en-u-nu-thai").resolvedOptions()
        .numberingSystem
);
assertEquals(
    "arab",
    new Intl.RelativeTimeFormat("en-u-nu-arab").resolvedOptions()
        .numberingSystem
);

// For locale which default others but  use -u-nu-latn to change to 'latn' numberingSystem
assertEquals(
    "latn",
    new Intl.RelativeTimeFormat("fa-u-nu-latn").resolvedOptions()
        .numberingSystem
);
assertEquals(
    "latn",
    new Intl.RelativeTimeFormat("ar-TD-u-nu-latn").resolvedOptions()
        .numberingSystem
);
assertEquals(
    "latn",
    new Intl.RelativeTimeFormat("fa-u-nu-latn").resolvedOptions()
        .numberingSystem
);

// For locale use -u-nu- with invalid value still back to default.
assertEquals(
    "latn",
    new Intl.RelativeTimeFormat("en-u-nu-abcd").resolvedOptions()
        .numberingSystem
);

assertEquals(
    "arabext",
    new Intl.RelativeTimeFormat("fa-u-nu-abcd").resolvedOptions()
        .numberingSystem
);
