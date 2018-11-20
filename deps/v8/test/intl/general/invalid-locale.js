// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure that invalid locales throw RangeError

var invalid_locales = ["arcdefghl-Latn", "fil-Latn-kxx", "fr-Latn-CAK",
                       "en-Latin-US", "en-a-foo-9charlong", "en-a-b",
                      ];

for (let locale  of invalid_locales) {
  assertThrows("var nf = new Intl.NumberFormat('" + locale + "')", RangeError);
}

var not_so_long_locales = [
  "bs-u-nu-bzcu-cab-cabs-avnlubs-avnihu-zcu-cab-cbs-avnllubs-avnihq-zcu-cab-cbs-ubs-avnihu-cabs-flus-xxd-vnluy",
  "bs-u-nu-bzcu-cab-cabs-avnlubs-avnihu-zcu-cab-cbs-avnllubs-avnihq-zcu-cab-cbs-ubs-avnihu-cabs-flus-xxd",
  "bs-u-nu-bzcu-cab-cabs-avnlubs-avnihu-zcu",
];

for (let locale  of not_so_long_locales) {
  assertEquals((new Intl.NumberFormat(locale)).resolvedOptions().numberingSystem,
      "latn");
}

// The point of this test is to make sure that there's no ill-effect with too
// long a locale name. Because, thhere's no provision in the Ecma 402 on the
// length limit of a locale ID and BCP 47 (RFC 5646 section 2.1). So, it's
// a spec violation to treat this as invalid. See TODO(jshin) comment
// in Runtime_CanonicalizeLanguageTag in runtime-intl.cc .
var overlong_locales = [
   "he-up-a-caiaup-araup-ai-pdu-sp-bs-up-arscna-zeieiaup-araup-arscia-rews-us-up-arscna-zeieiaup-araup-arsciap-arscna-zeieiaup-araup-arscie-u-sp-bs-uaup-arscia",
   "he-up-a-caiaup-araup-ai-pdu-sp-bs-up-arscna-zeieiaup-araup-arscia-rews-us-up-arscna-zeieiaup-araup-arsciap-arscna-zeieiaup-araup-arscie-u-sp-bs-uaup-arscia-xyza",
   "bs-u-nu-bzcu-cab-cabs-avnlubs-avnihu-zcu-cab-cbs-avnllubs-avnihq-zcu-cab-cbs-ubs-avnihu-cabs-flus-xxd-vnluy-abcd",
];

for (let locale  of overlong_locales) {
  assertThrows("var nf = new Intl.NumberFormat('" + locale + "')", RangeError)
}
