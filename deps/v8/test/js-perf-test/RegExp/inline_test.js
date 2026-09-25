// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function ConstTest() {
  /[Cz]/.test("abCdefgzabCdefgzabCdefgzabCdefgzabCdefgzabCdefgzabCdefgzabCdefgz");
}

const cre = /[Cz]/;
function GlobalConstTest() {
  cre.test("abCdefgzabCdefgzabCdefgzabCdefgzabCdefgzabCdefgzabCdefgzabCdefgz");
}

var benchmarks = [ [ConstTest, () => {}],
                   [GlobalConstTest, () => {}],
                 ];
createBenchmarkSuite("InlineTest");
