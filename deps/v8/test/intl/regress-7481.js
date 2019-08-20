// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
assertEquals(
    "en-u-hc-h11-nu-arab",
    new Intl.DateTimeFormat(["en-u-hc-h11-nu-arab"]).resolvedOptions().locale
);
assertEquals(
    "en-u-hc-h12-nu-arab",
    new Intl.DateTimeFormat(["en-u-hc-h12-nu-arab"]).resolvedOptions().locale
);
assertEquals(
    "en-u-hc-h23-nu-arab",
    new Intl.DateTimeFormat(["en-u-hc-h23-nu-arab"]).resolvedOptions().locale
);
assertEquals(
    "en-u-hc-h24-nu-arab",
    new Intl.DateTimeFormat(["en-u-hc-h24-nu-arab"]).resolvedOptions().locale
);

// https://tc39.github.io/ecma402/#sec-intl.datetimeformat-internal-slots
// invalid hc should be removed
// [[LocaleData]][locale].hc must be « null, "h11", "h12", "h23", "h24" » for all locale values.
assertEquals(
    "en-u-nu-arab",
    new Intl.DateTimeFormat(["en-u-hc-h10-nu-arab"]).resolvedOptions().locale
);
assertEquals(
    "en-u-nu-arab",
    new Intl.DateTimeFormat(["en-u-hc-h13-nu-arab"]).resolvedOptions().locale
);
assertEquals(
    "en-u-nu-arab",
    new Intl.DateTimeFormat(["en-u-hc-h22-nu-arab"]).resolvedOptions().locale
);
assertEquals(
    "en-u-nu-arab",
    new Intl.DateTimeFormat(["en-u-hc-h25-nu-arab"]).resolvedOptions().locale
);
