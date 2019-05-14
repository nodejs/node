// Copyright 2018 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file was copied from the output of
// http://hg.mozilla.org/projects/kraken/sunspider script.

var benchmarkPath = "";
var tests = [ "ai-astar", "audio-beat-detection", "audio-dft", "audio-fft", "audio-oscillator", "imaging-gaussian-blur", "imaging-darkroom", "imaging-desaturate", "json-parse-financial", "json-stringify-tinderbox", "stanford-crypto-aes", "stanford-crypto-ccm", "stanford-crypto-pbkdf2", "stanford-crypto-sha256-iterative" ];
var categories = [ "ai", "audio", "imaging", "json", "stanford" ];
var results = new Array();

var time = 0;
var times = [];

times.length = tests.length;

for (var krakenCounter = 0; krakenCounter < tests.length; krakenCounter++) {
    var testBase = benchmarkPath + tests[krakenCounter];
    var testName = testBase + ".js";
    var testData = testBase + "-data.js";
    // load test data
    load(testData);
    var startTime = new Date;
    load(testName);
    times[krakenCounter] = new Date() - startTime;
    gc();
}

function recordResults(tests, times)
{
    var output = "";

    for (j = 0; j < tests.length; j++) {
        output += tests[j] + '-orig(RunTime): ' + times[j] + ' ms.\n';
    }
    print(output);
}

recordResults(tests, times);
