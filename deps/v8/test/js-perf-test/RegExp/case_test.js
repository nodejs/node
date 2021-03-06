// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function ConstCaseInsensitiveTest() {
  /[a-t]/i.test("abCdefgzabCdefgzabCdefgzabCdefgzabCdefgzabCdefgzabCdefgzabCdefgz");
}

function ConstCaseInsensitiveNonLatin1Test() {
  /[α-ζ]/i.test("βκςΑφabcdeβκEFGαοςερκ");
}

const icre = /[a-t]/i;
function GlobalConstCaseInsensitiveRegExpTest() {
  icre.test("abCdefgzabCdefgzabCdefgzabCdefgzabCdefgzabCdefgzabCdefgzabCdefgz");
}

const icre2 = /[α-ζ]/i;
function GlobalConstCaseInsensitiveNonLatin1RegExpTest() {
  icre2.test("βκςΑφabcdeβκEFGαοςερκ");
}

benchmarks = [ [ConstCaseInsensitiveTest, () => {}],
               [ConstCaseInsensitiveNonLatin1Test, () => {}],
               [GlobalConstCaseInsensitiveRegExpTest, () => {}],
               [GlobalConstCaseInsensitiveNonLatin1RegExpTest, () => {}],
             ];

createBenchmarkSuite("CaseInsensitiveTest");
