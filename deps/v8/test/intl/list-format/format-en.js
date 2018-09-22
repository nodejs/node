// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-list-format

// The following test are not part of the comformance. Just some output in
// English to verify the format does return something reasonable for English.
// It may be changed when we update the CLDR data.
// NOTE: These are UNSPECIFIED behavior in
// http://tc39.github.io/proposal-intl-list-time/

let enLongConjunction = new Intl.ListFormat(
    ["en"], {style: "long", type: 'conjunction'});

assertEquals('', enLongConjunction.format());
 assertEquals('', enLongConjunction.format([]));
assertEquals('a', enLongConjunction.format(['a']));
assertEquals('b', enLongConjunction.format(['b']));
assertEquals('a and b', enLongConjunction.format(['a', 'b']));
assertEquals('a, b, and c', enLongConjunction.format(['a', 'b', 'c']));
assertEquals('a, b, c, and d', enLongConjunction.format(['a', 'b', 'c', 'd']));
assertEquals('a, b, c, d, and and', enLongConjunction.format(['a', 'b', 'c', 'd', 'and']));

let enLongDisjunction = new Intl.ListFormat(
    ["en"], {style: "long", type: 'disjunction'});

assertEquals('', enLongDisjunction.format());
assertEquals('', enLongDisjunction.format([]));
assertEquals('a', enLongDisjunction.format(['a']));
assertEquals('b', enLongDisjunction.format(['b']));
assertEquals('a or b', enLongDisjunction.format(['a', 'b']));
assertEquals('a, b, or c', enLongDisjunction.format(['a', 'b', 'c']));
assertEquals('a, b, c, or d', enLongDisjunction.format(['a', 'b', 'c', 'd']));
assertEquals('a, b, c, d, or or', enLongDisjunction.format(['a', 'b', 'c', 'd', 'or']));

let enLongUnit = new Intl.ListFormat(
    ["en"], {style: "long", type: 'unit'});

assertEquals('', enLongUnit.format());
assertEquals('', enLongUnit.format([]));
assertEquals('a', enLongUnit.format(['a']));
assertEquals('b', enLongUnit.format(['b']));
assertEquals('a, b', enLongUnit.format(['a', 'b']));
assertEquals('a, b, c', enLongUnit.format(['a', 'b', 'c']));
assertEquals('a, b, c, d', enLongUnit.format(['a', 'b', 'c', 'd']));
assertEquals('a, b, c, d, or', enLongUnit.format(['a', 'b', 'c', 'd', 'or']));

let enShortConjunction = new Intl.ListFormat(
    ["en"], {style: "short", type: 'conjunction'});

assertEquals('', enShortConjunction.format());
assertEquals('', enShortConjunction.format([]));
assertEquals('a', enShortConjunction.format(['a']));
assertEquals('b', enShortConjunction.format(['b']));
assertEquals('a and b', enShortConjunction.format(['a', 'b']));
assertEquals('a, b, and c', enShortConjunction.format(['a', 'b', 'c']));
assertEquals('a, b, c, and d', enShortConjunction.format(['a', 'b', 'c', 'd']));
assertEquals('a, b, c, d, and and', enShortConjunction.format(['a', 'b', 'c', 'd', 'and']));

let enShortDisjunction = new Intl.ListFormat(
    ["en"], {style: "short", type: 'disjunction'});

assertEquals('', enShortDisjunction.format());
assertEquals('', enShortDisjunction.format([]));
assertEquals('a', enShortDisjunction.format(['a']));
assertEquals('b', enShortDisjunction.format(['b']));
assertEquals('a or b', enShortDisjunction.format(['a', 'b']));
assertEquals('a, b, or c', enShortDisjunction.format(['a', 'b', 'c']));
assertEquals('a, b, c, or d', enShortDisjunction.format(['a', 'b', 'c', 'd']));
assertEquals('a, b, c, d, or or', enShortDisjunction.format(['a', 'b', 'c', 'd', 'or']));

let enShortUnit = new Intl.ListFormat(
    ["en"], {style: "short", type: 'unit'});

assertEquals('', enShortUnit.format());
assertEquals('', enShortUnit.format([]));
assertEquals('a', enShortUnit.format(['a']));
assertEquals('b', enShortUnit.format(['b']));
assertEquals('a, b', enShortUnit.format(['a', 'b']));
assertEquals('a, b, c', enShortUnit.format(['a', 'b', 'c']));
assertEquals('a, b, c, d', enShortUnit.format(['a', 'b', 'c', 'd']));
assertEquals('a, b, c, d, or', enShortUnit.format(['a', 'b', 'c', 'd', 'or']));

let enNarrowConjunction = new Intl.ListFormat(
    ["en"], {style: "narrow", type: 'conjunction'});

assertEquals('', enNarrowConjunction.format());
assertEquals('', enNarrowConjunction.format([]));
assertEquals('a', enNarrowConjunction.format(['a']));
assertEquals('b', enNarrowConjunction.format(['b']));
assertEquals('a and b', enNarrowConjunction.format(['a', 'b']));
assertEquals('a, b, and c', enNarrowConjunction.format(['a', 'b', 'c']));
assertEquals('a, b, c, and d', enNarrowConjunction.format(['a', 'b', 'c', 'd']));
assertEquals('a, b, c, d, and and', enNarrowConjunction.format(['a', 'b', 'c', 'd', 'and']));

let enNarrowDisjunction = new Intl.ListFormat(
    ["en"], {style: "narrow", type: 'disjunction'});

assertEquals('', enNarrowDisjunction.format());
assertEquals('', enNarrowDisjunction.format([]));
assertEquals('a', enNarrowDisjunction.format(['a']));
assertEquals('b', enNarrowDisjunction.format(['b']));
assertEquals('a or b', enNarrowDisjunction.format(['a', 'b']));
assertEquals('a, b, or c', enNarrowDisjunction.format(['a', 'b', 'c']));
assertEquals('a, b, c, or d', enNarrowDisjunction.format(['a', 'b', 'c', 'd']));
assertEquals('a, b, c, d, or or', enNarrowDisjunction.format(['a', 'b', 'c', 'd', 'or']));

let enNarrowUnit = new Intl.ListFormat(
    ["en"], {style: "narrow", type: 'unit'});

assertEquals('', enNarrowUnit.format());
assertEquals('', enNarrowUnit.format([]));
assertEquals('a', enNarrowUnit.format(['a']));
assertEquals('b', enNarrowUnit.format(['b']));
assertEquals('a b', enNarrowUnit.format(['a', 'b']));
assertEquals('a b c', enNarrowUnit.format(['a', 'b', 'c']));
assertEquals('a b c d', enNarrowUnit.format(['a', 'b', 'c', 'd']));
assertEquals('a b c d or', enNarrowUnit.format(['a', 'b', 'c', 'd', 'or']));
