// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertEquals("id", (new Intl.Locale("in")).toString());
assertEquals("he", (new Intl.Locale("iw")).toString());
assertEquals("yi", (new Intl.Locale("ji")).toString());
assertEquals("jv", (new Intl.Locale("jw")).toString());
assertEquals("ro", (new Intl.Locale("mo")).toString());
assertEquals("sr", (new Intl.Locale("scc")).toString());
assertEquals("hr", (new Intl.Locale("scr")).toString());

assertEquals("sr-Latn", (new Intl.Locale("sh")).toString());
assertEquals("sr-ME", (new Intl.Locale("cnr")).toString());
assertEquals("nb", (new Intl.Locale("no")).toString());
assertEquals("fil", (new Intl.Locale("tl")).toString());

assertEquals("hy-AM", (new Intl.Locale("hy-SU")).toString());
assertEquals("lv-LV", (new Intl.Locale("lv-SU")).toString());
