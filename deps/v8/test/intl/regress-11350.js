// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test Long Locale handle minimize and maximize correctly.

let ext = "-u-cu-eur-em-default-hc-h23-ks-level1-lb-strict-lw-normal-" +
          "ms-metric-nu-latn-rg-atzzzz-sd-atat1-ss-none-tz-atvie-va-posix";

// Test maximize()
assertEquals ("de-Latn-DE" + ext,
    (new Intl.Locale("de" + ext)).maximize().toString());

assertEquals ("de-Latn-DE" + ext,
    (new Intl.Locale("de-DE" + ext)).maximize().toString());

assertEquals ("de-Latn-DE" + ext,
    (new Intl.Locale("de-Latn" + ext)).maximize().toString());

assertEquals ("de-Latn-DE" + ext,
    (new Intl.Locale("de-Latn-DE" + ext)).maximize().toString());

assertEquals ("de-Hant-DE" + ext,
    (new Intl.Locale("de-Hant" + ext)).maximize().toString());

assertEquals ("de-Hant-AT" + ext,
    (new Intl.Locale("de-Hant-AT" + ext)).maximize().toString());

assertEquals ("de-Latn-AT" + ext,
    (new Intl.Locale("de-AT" + ext)).maximize().toString());

// Test minimize()
assertEquals ("de" + ext,
    (new Intl.Locale("de-Latn-DE" + ext)).minimize().toString());

assertEquals ("de" + ext,
    (new Intl.Locale("de-Latn" + ext)).minimize().toString());

assertEquals ("de" + ext,
    (new Intl.Locale("de-DE" + ext)).minimize().toString());

assertEquals ("de-AT" + ext,
    (new Intl.Locale("de-Latn-AT" + ext)).minimize().toString());

assertEquals ("de-Hant" + ext,
    (new Intl.Locale("de-Hant" + ext)).minimize().toString());

assertEquals ("de-Hant-AT" + ext,
    (new Intl.Locale("de-Hant-AT" + ext)).minimize().toString());
