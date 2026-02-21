// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following test are not part of the comformance. Just some output in
// Chinese to verify the format does return something reasonable for Chinese.
// It may be changed when we update the CLDR data.
// NOTE: These are UNSPECIFIED behavior in
// http://tc39.github.io/proposal-intl-list-time/

let zhLongConjunction = new Intl.ListFormat(
    ["zh"], {style: "long", type: 'conjunction'});

var parts;
parts = zhLongConjunction.formatToParts();
assertEquals(0, parts.length);

parts = zhLongConjunction.formatToParts([]);
assertEquals(0, parts.length);

parts = zhLongConjunction.formatToParts(['譚永鋒']);
assertEquals(1, parts.length);
assertEquals('譚永鋒', parts[0].value);
assertEquals('element', parts[0].type);

parts = zhLongConjunction.formatToParts(['譚永鋒', '劉新宇']);
assertEquals(3, parts.length);
assertEquals('譚永鋒', parts[0].value);
assertEquals('element', parts[0].type);
assertEquals('和', parts[1].value);
assertEquals('literal', parts[1].type);
assertEquals('劉新宇', parts[2].value);
assertEquals('element', parts[2].type);

parts = zhLongConjunction.formatToParts(['黄子容', '譚永鋒', '劉新宇']);
assertEquals(5, parts.length);
assertEquals('黄子容', parts[0].value);
assertEquals('element', parts[0].type);
assertEquals('、', parts[1].value);
assertEquals('literal', parts[1].type);
assertEquals('譚永鋒', parts[2].value);
assertEquals('element', parts[2].type);
assertEquals('和', parts[3].value);
assertEquals('literal', parts[3].type);
assertEquals('劉新宇', parts[4].value);
assertEquals('element', parts[4].type);

parts = zhLongConjunction.formatToParts(['黄子容', '譚永鋒', '劉新宇', '朱君毅']);
assertEquals(7, parts.length);
assertEquals('黄子容', parts[0].value);
assertEquals('element', parts[0].type);
assertEquals('、', parts[1].value);
assertEquals('literal', parts[1].type);
assertEquals('譚永鋒', parts[2].value);
assertEquals('element', parts[2].type);
assertEquals('、', parts[3].value);
assertEquals('literal', parts[3].type);
assertEquals('劉新宇', parts[4].value);
assertEquals('element', parts[4].type);
assertEquals('和', parts[5].value);
assertEquals('literal', parts[5].type);
assertEquals('朱君毅', parts[6].value);
assertEquals('element', parts[6].type);

let zhShortDisjunction = new Intl.ListFormat(
    ["zh"], {style: "short", type: 'disjunction'});
parts = zhShortDisjunction.formatToParts();
assertEquals(0, parts.length);

parts = zhShortDisjunction.formatToParts([]);
assertEquals(0, parts.length);

parts = zhShortDisjunction.formatToParts(['譚永鋒']);
assertEquals(1, parts.length);
assertEquals('譚永鋒', parts[0].value);
assertEquals('element', parts[0].type);

parts = zhShortDisjunction.formatToParts(['譚永鋒', '劉新宇']);
assertEquals(3, parts.length);
assertEquals('譚永鋒', parts[0].value);
assertEquals('element', parts[0].type);
assertEquals('或', parts[1].value);
assertEquals('literal', parts[1].type);
assertEquals('劉新宇', parts[2].value);
assertEquals('element', parts[2].type);

parts = zhShortDisjunction.formatToParts(['黄子容', '譚永鋒', '劉新宇']);
assertEquals(5, parts.length);
assertEquals('黄子容', parts[0].value);
assertEquals('element', parts[0].type);
assertEquals('、', parts[1].value);
assertEquals('literal', parts[1].type);
assertEquals('譚永鋒', parts[2].value);
assertEquals('element', parts[2].type);
assertEquals('或', parts[3].value);
assertEquals('literal', parts[3].type);
assertEquals('劉新宇', parts[4].value);
assertEquals('element', parts[4].type);

parts = zhShortDisjunction.formatToParts(['黄子容', '譚永鋒', '劉新宇', '朱君毅']);
assertEquals(7, parts.length);
assertEquals('黄子容', parts[0].value);
assertEquals('element', parts[0].type);
assertEquals('、', parts[1].value);
assertEquals('literal', parts[1].type);
assertEquals('譚永鋒', parts[2].value);
assertEquals('element', parts[2].type);
assertEquals('、', parts[3].value);
assertEquals('literal', parts[3].type);
assertEquals('劉新宇', parts[4].value);
assertEquals('element', parts[4].type);
assertEquals('或', parts[5].value);
assertEquals('literal', parts[5].type);
assertEquals('朱君毅', parts[6].value);

let zhNarrowUnit = new Intl.ListFormat(
    ["zh"], {style: "narrow", type: 'unit'});

parts = zhNarrowUnit.formatToParts();
assertEquals(0, parts.length);

parts = zhNarrowUnit.formatToParts([]);
assertEquals(0, parts.length);

parts = zhNarrowUnit.formatToParts(['3英哩']);
assertEquals(1, parts.length);
assertEquals('3英哩', parts[0].value);
assertEquals('element', parts[0].type);

parts = zhNarrowUnit.formatToParts(['3英哩', '4碼']);
assertEquals(2, parts.length);
assertEquals('3英哩', parts[0].value);
assertEquals('element', parts[0].type);
assertEquals('4碼', parts[1].value);
assertEquals('element', parts[1].type);

parts = zhNarrowUnit.formatToParts(['3英哩', '4碼', '5英尺']);
assertEquals(3, parts.length);
assertEquals('3英哩', parts[0].value);
assertEquals('element', parts[0].type);
assertEquals('4碼', parts[1].value);
assertEquals('element', parts[1].type);
assertEquals('5英尺', parts[2].value);
assertEquals('element', parts[2].type);

parts = zhNarrowUnit.formatToParts(['3英哩', '4碼', '5英尺','7英吋']);
assertEquals(4, parts.length);
assertEquals('3英哩', parts[0].value);
assertEquals('element', parts[0].type);
assertEquals('4碼', parts[1].value);
assertEquals('element', parts[1].type);
assertEquals('5英尺', parts[2].value);
assertEquals('element', parts[2].type);
assertEquals('7英吋', parts[3].value);
assertEquals('element', parts[3].type);
