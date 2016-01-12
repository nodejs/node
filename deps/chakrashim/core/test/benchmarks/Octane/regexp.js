////////////////////////////////////////////////////////////////////////////////
// base.js
////////////////////////////////////////////////////////////////////////////////

// Copyright 2013 the V8 project authors. All rights reserved.
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

if(typeof(WScript) === "undefined")
{
    var WScript = {
        Echo: print
    }
}

// Performance.now is used in latency benchmarks, the fallback is Date.now.
var performance = performance || {};
performance.now = (function() {
  return performance.now       ||
         performance.mozNow    ||
         performance.msNow     ||
         performance.oNow      ||
         performance.webkitNow ||
         Date.now;
})();

// Simple framework for running the benchmark suites and
// computing a score based on the timing measurements.


// A benchmark has a name (string) and a function that will be run to
// do the performance measurement. The optional setup and tearDown
// arguments are functions that will be invoked before and after
// running the benchmark, but the running time of these functions will
// not be accounted for in the benchmark score.
function Benchmark(name, doWarmup, doDeterministic, run, setup, tearDown, rmsResult, minIterations) {
  this.name = name;
  this.doWarmup = doWarmup;
  this.doDeterministic = doDeterministic;
  this.run = run;
  this.Setup = setup ? setup : function() { };
  this.TearDown = tearDown ? tearDown : function() { };
  this.rmsResult = rmsResult ? rmsResult : null; 
  this.minIterations = minIterations ? minIterations : 32;
}


// Benchmark results hold the benchmark and the measured time used to
// run the benchmark. The benchmark score is computed later once a
// full benchmark suite has run to completion. If latency is set to 0
// then there is no latency score for this benchmark.
function BenchmarkResult(benchmark, time, latency) {
  this.benchmark = benchmark;
  this.time = time;
  this.latency = latency;
}


// Automatically convert results to numbers. Used by the geometric
// mean computation.
BenchmarkResult.prototype.valueOf = function() {
  return this.time;
}


// Suites of benchmarks consist of a name and the set of benchmarks in
// addition to the reference timing that the final score will be based
// on. This way, all scores are relative to a reference run and higher
// scores implies better performance.
function BenchmarkSuite(name, reference, benchmarks) {
  this.name = name;
  this.reference = reference;
  this.benchmarks = benchmarks;
  BenchmarkSuite.suites.push(this);
}


// Keep track of all declared benchmark suites.
BenchmarkSuite.suites = [];

// Scores are not comparable across versions. Bump the version if
// you're making changes that will affect that scores, e.g. if you add
// a new benchmark or change an existing one.
BenchmarkSuite.version = '9';

// Override the alert function to throw an exception instead.
alert = function(s) {
  throw "Alert called with argument: " + s;
};


// To make the benchmark results predictable, we replace Math.random
// with a 100% deterministic alternative.
BenchmarkSuite.ResetRNG = function() {
  Math.random = (function() {
    var seed = 49734321;
    return function() {
      // Robert Jenkins' 32 bit integer hash function.
      seed = ((seed + 0x7ed55d16) + (seed << 12))  & 0xffffffff;
      seed = ((seed ^ 0xc761c23c) ^ (seed >>> 19)) & 0xffffffff;
      seed = ((seed + 0x165667b1) + (seed << 5))   & 0xffffffff;
      seed = ((seed + 0xd3a2646c) ^ (seed << 9))   & 0xffffffff;
      seed = ((seed + 0xfd7046c5) + (seed << 3))   & 0xffffffff;
      seed = ((seed ^ 0xb55a4f09) ^ (seed >>> 16)) & 0xffffffff;
      return (seed & 0xfffffff) / 0x10000000;
    };
  })();
}


// Runs all registered benchmark suites and optionally yields between
// each individual benchmark to avoid running for too long in the
// context of browsers. Once done, the final score is reported to the
// runner.
BenchmarkSuite.RunSuites = function(runner) {
  var continuation = null;
  var suites = BenchmarkSuite.suites;
  var length = suites.length;
  BenchmarkSuite.scores = [];
  var index = 0;
  function RunStep() {
    while (continuation || index < length) {
      if (continuation) {
        continuation = continuation();
      } else {
        var suite = suites[index++];
        if (runner.NotifyStart) runner.NotifyStart(suite.name);
        continuation = suite.RunStep(runner);
      }
      if (continuation && typeof window != 'undefined' && window.setTimeout) {
        window.setTimeout(RunStep, 25);
        return;
      }
    }

    // show final result
    if (runner.NotifyScore) {
      var score = BenchmarkSuite.GeometricMean(BenchmarkSuite.scores);
      var formatted = BenchmarkSuite.FormatScore(100 * score);
      runner.NotifyScore(formatted);
    }
  }
  RunStep();
}


// Counts the total number of registered benchmarks. Useful for
// showing progress as a percentage.
BenchmarkSuite.CountBenchmarks = function() {
  var result = 0;
  var suites = BenchmarkSuite.suites;
  for (var i = 0; i < suites.length; i++) {
    result += suites[i].benchmarks.length;
  }
  return result;
}


// Computes the geometric mean of a set of numbers.
BenchmarkSuite.GeometricMean = function(numbers) {
  var log = 0;
  for (var i = 0; i < numbers.length; i++) {
    log += Math.log(numbers[i]);
  }
  return Math.pow(Math.E, log / numbers.length);
}


// Computes the geometric mean of a set of throughput time measurements.
BenchmarkSuite.GeometricMeanTime = function(measurements) {
  var log = 0;
  for (var i = 0; i < measurements.length; i++) {
    log += Math.log(measurements[i].time);
  }
  return Math.pow(Math.E, log / measurements.length);
}


// Computes the geometric mean of a set of rms measurements.
BenchmarkSuite.GeometricMeanLatency = function(measurements) {
  var log = 0;
  var hasLatencyResult = false;
  for (var i = 0; i < measurements.length; i++) {
    if (measurements[i].latency != 0) {
      log += Math.log(measurements[i].latency);
      hasLatencyResult = true;
    }
  }
  if (hasLatencyResult) {
    return Math.pow(Math.E, log / measurements.length);
  } else {
    return 0;
  }
}


// Converts a score value to a string with at least three significant
// digits.
BenchmarkSuite.FormatScore = function(value) {
  if (value > 100) {
    return value.toFixed(0);
  } else {
    return value.toPrecision(3);
  }
}

// Notifies the runner that we're done running a single benchmark in
// the benchmark suite. This can be useful to report progress.
BenchmarkSuite.prototype.NotifyStep = function(result) {
  this.results.push(result);
  if (this.runner.NotifyStep) this.runner.NotifyStep(result.benchmark.name);
}


// Notifies the runner that we're done with running a suite and that
// we have a result which can be reported to the user if needed.
BenchmarkSuite.prototype.NotifyResult = function() {
  var mean = BenchmarkSuite.GeometricMeanTime(this.results);
  var score = this.reference[0] / mean;
  BenchmarkSuite.scores.push(score);
  if (this.runner.NotifyResult) {
    var formatted = BenchmarkSuite.FormatScore(100 * score);
    this.runner.NotifyResult(this.name, formatted);
  }
  if (this.reference.length == 2) {
    var meanLatency = BenchmarkSuite.GeometricMeanLatency(this.results);
    if (meanLatency != 0) {
      var scoreLatency = this.reference[1] / meanLatency;
      BenchmarkSuite.scores.push(scoreLatency);
      if (this.runner.NotifyResult) {
        var formattedLatency = BenchmarkSuite.FormatScore(100 * scoreLatency)
        this.runner.NotifyResult(this.name + "Latency", formattedLatency);
      }
    }
  }
}


// Notifies the runner that running a benchmark resulted in an error.
BenchmarkSuite.prototype.NotifyError = function(error) {
  if (this.runner.NotifyError) {
    this.runner.NotifyError(this.name, error);
  }
  if (this.runner.NotifyStep) {
    this.runner.NotifyStep(this.name);
  }
}


// Runs a single benchmark for at least a second and computes the
// average time it takes to run a single iteration.
BenchmarkSuite.prototype.RunSingleBenchmark = function(benchmark, data) {
  function Measure(data) {
    var elapsed = 0;
    var start = new Date();
  
  // Run either for 1 second or for the number of iterations specified
  // by minIterations, depending on the config flag doDeterministic.
    for (var i = 0; (benchmark.doDeterministic ? 
      i<benchmark.minIterations : elapsed < 1000); i++) {
      benchmark.run();
      elapsed = new Date() - start;
    }
    if (data != null) {
      data.runs += i;
      data.elapsed += elapsed;
    }
  }

  // Sets up data in order to skip or not the warmup phase.
  if (!benchmark.doWarmup && data == null) {
    data = { runs: 0, elapsed: 0 };
  }

  if (data == null) {
    Measure(null);
    return { runs: 0, elapsed: 0 };
  } else {
    Measure(data);
    // If we've run too few iterations, we continue for another second.
    if (data.runs < benchmark.minIterations) return data;
    var usec = (data.elapsed * 1000) / data.runs;
    var rms = (benchmark.rmsResult != null) ? benchmark.rmsResult() : 0;
    this.NotifyStep(new BenchmarkResult(benchmark, usec, rms));
    return null;
  }
}


// This function starts running a suite, but stops between each
// individual benchmark in the suite and returns a continuation
// function which can be invoked to run the next benchmark. Once the
// last benchmark has been executed, null is returned.
BenchmarkSuite.prototype.RunStep = function(runner) {
  BenchmarkSuite.ResetRNG();
  this.results = [];
  this.runner = runner;
  var length = this.benchmarks.length;
  var index = 0;
  var suite = this;
  var data;

  // Run the setup, the actual benchmark, and the tear down in three
  // separate steps to allow the framework to yield between any of the
  // steps.

  function RunNextSetup() {
    if (index < length) {
      try {
        suite.benchmarks[index].Setup();
      } catch (e) {
        suite.NotifyError(e);
        return null;
      }
      return RunNextBenchmark;
    }
    suite.NotifyResult();
    return null;
  }

  function RunNextBenchmark() {
    try {
      data = suite.RunSingleBenchmark(suite.benchmarks[index], data);
    } catch (e) {
      suite.NotifyError(e);
      return null;
    }
    // If data is null, we're done with this benchmark.
    return (data == null) ? RunNextTearDown : RunNextBenchmark();
  }

  function RunNextTearDown() {
    try {
      suite.benchmarks[index++].TearDown();
    } catch (e) {
      suite.NotifyError(e);
      return null;
    }
    return RunNextSetup;
  }

  // Start out running the setup.
  return RunNextSetup();
}

/////////////////////////////////////////////////////////////
// regexp.js
/////////////////////////////////////////////////////////////

// Copyright 2010 the V8 project authors. All rights reserved.
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

// Automatically generated on 2009-01-30. Manually updated on 2010-09-17.

// This benchmark is generated by loading 50 of the most popular pages
// on the web and logging all regexp operations performed.  Each
// operation is given a weight that is calculated from an estimate of
// the popularity of the pages where it occurs and the number of times
// it is executed while loading each page.  Furthermore the literal
// letters in the data are encoded using ROT13 in a way that does not
// affect how the regexps match their input.  Finally the strings are
// scrambled to exercise the regexp engine on different input strings.


var RegExpSuite = new BenchmarkSuite('RegExp', [910985], [
  new Benchmark("RegExp", true, false, 
    RegExpRun, RegExpSetup, RegExpTearDown, null, 16)
]);

var regExpBenchmark = null;

function RegExpSetup() {
  regExpBenchmark = new RegExpBenchmark();
  RegExpRun(); // run once to get system initialized
}

function RegExpRun() {
  regExpBenchmark.run();
}

function RegExpTearDown() {
  regExpBenchmark = null;
}

// Returns an array of n different variants of the input string str.
// The variants are computed by randomly rotating one random
// character.
function computeInputVariants(str, n) {
  var variants = [ str ];
  for (var i = 1; i < n; i++) {
    var pos = Math.floor(Math.random() * str.length);
    var chr = String.fromCharCode((str.charCodeAt(pos) + Math.floor(Math.random() * 128)) % 128);
    variants[i] = str.substring(0, pos) + chr + str.substring(pos + 1, str.length);
  }
  return variants;
}

function RegExpBenchmark() {
  function Exec(re, string) {
    var sum = 0;
    re.lastIndex = 0;
    var array = re.exec(string);
    if (array) {
      for (var i = 0; i < array.length; i++) {
        var substring = array[i];
        if (substring) sum += substring.length;
      }
    }
    return sum;
  }
  var re0 = /^ba/;
  var re1 = /(((\w+):\/\/)([^\/:]*)(:(\d+))?)?([^#?]*)(\?([^#]*))?(#(.*))?/;
  var re2 = /^\s*|\s*$/g;
  var re3 = /\bQBZPbageby_cynprubyqre\b/;
  var re4 = /,/;
  var re5 = /\bQBZPbageby_cynprubyqre\b/g;
  var re6 = /^[\s\xa0]+|[\s\xa0]+$/g;
  var re7 = /(\d*)(\D*)/g;
  var re8 = /=/;
  var re9 = /(^|\s)lhv\-h(\s|$)/;
  var str0 = 'Zbmvyyn/5.0 (Jvaqbjf; H; Jvaqbjf AG 5.1; ra-HF) NccyrJroXvg/528.9 (XUGZY, yvxr Trpxb) Puebzr/2.0.157.0 Fnsnev/528.9';
  var re10 = /\#/g;
  var re11 = /\./g;
  var re12 = /'/g;
  var re13 = /\?[\w\W]*(sevraqvq|punaaryvq|tebhcvq)=([^\&\?#]*)/i;
  var str1 = 'Fubpxjnir Synfu 9.0  e115';
  var re14 = /\s+/g;
  var re15 = /^\s*(\S*(\s+\S+)*)\s*$/;
  var re16 = /(-[a-z])/i;

  var s0 = computeInputVariants('pyvpx', 6511);
  var s1 = computeInputVariants('uggc://jjj.snprobbx.pbz/ybtva.cuc', 1844);
  var s2 = computeInputVariants('QBZPbageby_cynprubyqre', 739);
  var s3 = computeInputVariants('uggc://jjj.snprobbx.pbz/', 598);
  var s4 = computeInputVariants('uggc://jjj.snprobbx.pbz/fepu.cuc', 454);
  var s5 = computeInputVariants('qqqq, ZZZ q, llll', 352);
  var s6 = computeInputVariants('vachggrkg QBZPbageby_cynprubyqre', 312);
  var s7 = computeInputVariants('/ZlFcnprUbzrcntr/Vaqrk-FvgrUbzr,10000000', 282);
  var s8 = computeInputVariants('vachggrkg', 177);
  var s9 = computeInputVariants('528.9', 170);
  var s10 = computeInputVariants('528', 170);
  var s11 = computeInputVariants('VCPhygher=ra-HF', 156);
  var s12 = computeInputVariants('CersreerqPhygher=ra-HF', 156);
  var s13 = computeInputVariants('xrlcerff', 144);
  var s14 = computeInputVariants('521', 139);
  var s15 = computeInputVariants(str0, 139);
  var s16 = computeInputVariants('qvi .so_zrah', 137);
  var s17 = computeInputVariants('qvi.so_zrah', 137);
  var s18 = computeInputVariants('uvqqra_ryrz', 117);
  var s19 = computeInputVariants('sevraqfgre_naba=nvq%3Qn6ss9p85n868ro9s059pn854735956o3%26ers%3Q%26df%3Q%26vpgl%3QHF', 95);
  var s20 = computeInputVariants('uggc://ubzr.zlfcnpr.pbz/vaqrk.psz', 93);
  var s21 = computeInputVariants(str1, 92);
  var s22 = computeInputVariants('svefg', 85);
  var s23 = computeInputVariants('uggc://cebsvyr.zlfcnpr.pbz/vaqrk.psz', 85);
  var s24 = computeInputVariants('ynfg', 85);
  var s25 = computeInputVariants('qvfcynl', 85);

  function runBlock0() {
    var sum = 0;
    for (var i = 0; i < 525; i++) {
      sum += Exec(re0, s0[i]);
    }
    for (var i = 0; i < 1844; i++) {
      sum += Exec(re0, s0[i + 525]);
      sum += Exec(re1, s1[i]);
    }
    for (var i = 0; i < 739; i++) {
      sum += Exec(re0, s0[i + 2369]);
      sum += s2[i].replace(re2, '').length;
    }
    for (var i = 0; i < 598; i++) {
      sum += Exec(re0, s0[i + 3108]);
      sum += Exec(re1, s3[i]);
    }
    for (var i = 0; i < 454; i++) {
      sum += Exec(re0, s0[i + 3706]);
      sum += Exec(re1, s4[i]);
    }
    for (var i = 0; i < 352; i++) {
      sum += Exec(re0, s0[i + 4160]);
      sum += Exec(/qqqq|qqq|qq|q|ZZZZ|ZZZ|ZZ|Z|llll|ll|l|uu|u|UU|U|zz|z|ff|f|gg|g|sss|ss|s|mmm|mm|m/g, s5[i]);
    }
    for (var i = 0; i < 312; i++) {
      sum += Exec(re0, s0[i + 4512]);
      sum += Exec(re3, s6[i]);
    }
    for (var i = 0; i < 282; i++) {
      sum += Exec(re0, s0[i + 4824]);
      sum += Exec(re4, s7[i]);
    }
    for (var i = 0; i < 177; i++) {
      sum += Exec(re0, s0[i + 5106]);
      sum += s8[i].replace(re5, '').length;
    }
    for (var i = 0; i < 170; i++) {
      sum += Exec(re0, s0[i + 5283]);
      sum += s9[i].replace(re6, '').length;
      sum += Exec(re7, s10[i]);
    }
    for (var i = 0; i < 156; i++) {
      sum += Exec(re0, s0[i + 5453]);
      sum += Exec(re8, s11[i]);
      sum += Exec(re8, s12[i]);
    }
    for (var i = 0; i < 144; i++) {
      sum += Exec(re0, s0[i + 5609]);
      sum += Exec(re0, s13[i]);
    }
    for (var i = 0; i < 139; i++) {
      sum += Exec(re0, s0[i + 5753]);
      sum += s14[i].replace(re6, '').length;
      sum += Exec(re7, s14[i]);
      sum += Exec(re9, '');
      sum += Exec(/JroXvg\/(\S+)/, s15[i]);
    }
    for (var i = 0; i < 137; i++) {
      sum += Exec(re0, s0[i + 5892]);
      sum += s16[i].replace(re10, '').length;
      sum += s16[i].replace(/\[/g, '').length;
      sum += s17[i].replace(re11, '').length;
    }
    for (var i = 0; i < 117; i++) {
      sum += Exec(re0, s0[i + 6029]);
      sum += s18[i].replace(re2, '').length;
    }
    for (var i = 0; i < 95; i++) {
      sum += Exec(re0, s0[i + 6146]);
      sum += Exec(/(?:^|;)\s*sevraqfgre_ynat=([^;]*)/, s19[i]);
    }
    for (var i = 0; i < 93; i++) {
      sum += Exec(re0, s0[i + 6241]);
      sum += s20[i].replace(re12, '').length;
      sum += Exec(re13, s20[i]);
    }
    for (var i = 0; i < 92; i++) {
      sum += Exec(re0, s0[i + 6334]);
      sum += s21[i].replace(/([a-zA-Z]|\s)+/, '').length;
    }
    for (var i = 0; i < 85; i++) {
      sum += Exec(re0, s0[i + 6426]);
      sum += s22[i].replace(re14, '').length;
      sum += s22[i].replace(re15, '').length;
      sum += s23[i].replace(re12, '').length;
      sum += s24[i].replace(re14, '').length;
      sum += s24[i].replace(re15, '').length;
      sum += Exec(re16, s25[i]);
      sum += Exec(re13, s23[i]);
    }
    return sum;
  }
  var re17 = /(^|[^\\])\"\\\/Qngr\((-?[0-9]+)\)\\\/\"/g;
  var str2 = '{"anzr":"","ahzoreSbezng":{"PheeraplQrpvznyQvtvgf":2,"PheeraplQrpvznyFrcnengbe":".","VfErnqBayl":gehr,"PheeraplTebhcFvmrf":[3],"AhzoreTebhcFvmrf":[3],"CrepragTebhcFvmrf":[3],"PheeraplTebhcFrcnengbe":",","PheeraplFlzoby":"\xa4","AnAFlzoby":"AnA","PheeraplArtngvirCnggrea":0,"AhzoreArtngvirCnggrea":1,"CrepragCbfvgvirCnggrea":0,"CrepragArtngvirCnggrea":0,"ArtngvirVasvavglFlzoby":"-Vasvavgl","ArtngvirFvta":"-","AhzoreQrpvznyQvtvgf":2,"AhzoreQrpvznyFrcnengbe":".","AhzoreTebhcFrcnengbe":",","PheeraplCbfvgvirCnggrea":0,"CbfvgvirVasvavglFlzoby":"Vasvavgl","CbfvgvirFvta":"+","CrepragQrpvznyQvtvgf":2,"CrepragQrpvznyFrcnengbe":".","CrepragTebhcFrcnengbe":",","CrepragFlzoby":"%","CreZvyyrFlzoby":"\u2030","AngvirQvtvgf":["0","1","2","3","4","5","6","7","8","9"],"QvtvgFhofgvghgvba":1},"qngrGvzrSbezng":{"NZQrfvtangbe":"NZ","Pnyraqne":{"ZvaFhccbegrqQngrGvzr":"@-62135568000000@","ZnkFhccbegrqQngrGvzr":"@253402300799999@","NytbevguzGlcr":1,"PnyraqneGlcr":1,"Renf":[1],"GjbQvtvgLrneZnk":2029,"VfErnqBayl":gehr},"QngrFrcnengbe":"/","SvefgQnlBsJrrx":0,"PnyraqneJrrxEhyr":0,"ShyyQngrGvzrCnggrea":"qqqq, qq ZZZZ llll UU:zz:ff","YbatQngrCnggrea":"qqqq, qq ZZZZ llll","YbatGvzrCnggrea":"UU:zz:ff","ZbaguQnlCnggrea":"ZZZZ qq","CZQrfvtangbe":"CZ","ESP1123Cnggrea":"qqq, qq ZZZ llll UU\':\'zz\':\'ff \'TZG\'","FubegQngrCnggrea":"ZZ/qq/llll","FubegGvzrCnggrea":"UU:zz","FbegnoyrQngrGvzrCnggrea":"llll\'-\'ZZ\'-\'qq\'G\'UU\':\'zz\':\'ff","GvzrFrcnengbe":":","HavirefnyFbegnoyrQngrGvzrCnggrea":"llll\'-\'ZZ\'-\'qq UU\':\'zz\':\'ff\'M\'","LrneZbaguCnggrea":"llll ZZZZ","NooerivngrqQnlAnzrf":["Fha","Zba","Ghr","Jrq","Guh","Sev","Fng"],"FubegrfgQnlAnzrf":["Fh","Zb","Gh","Jr","Gu","Se","Fn"],"QnlAnzrf":["Fhaqnl","Zbaqnl","Ghrfqnl","Jrqarfqnl","Guhefqnl","Sevqnl","Fngheqnl"],"NooerivngrqZbaguAnzrf":["Wna","Sro","Zne","Nce","Znl","Wha","Why","Nht","Frc","Bpg","Abi","Qrp",""],"ZbaguAnzrf":["Wnahnel","Sroehnel","Znepu","Ncevy","Znl","Whar","Whyl","Nhthfg","Frcgrzore","Bpgbore","Abirzore","Qrprzore",""],"VfErnqBayl":gehr,"AngvirPnyraqneAnzr":"Tertbevna Pnyraqne","NooerivngrqZbaguTravgvirAnzrf":["Wna","Sro","Zne","Nce","Znl","Wha","Why","Nht","Frc","Bpg","Abi","Qrp",""],"ZbaguTravgvirAnzrf":["Wnahnel","Sroehnel","Znepu","Ncevy","Znl","Whar","Whyl","Nhthfg","Frcgrzore","Bpgbore","Abirzore","Qrprzore",""]}}';
  var str3 = '{"anzr":"ra-HF","ahzoreSbezng":{"PheeraplQrpvznyQvtvgf":2,"PheeraplQrpvznyFrcnengbe":".","VfErnqBayl":snyfr,"PheeraplTebhcFvmrf":[3],"AhzoreTebhcFvmrf":[3],"CrepragTebhcFvmrf":[3],"PheeraplTebhcFrcnengbe":",","PheeraplFlzoby":"$","AnAFlzoby":"AnA","PheeraplArtngvirCnggrea":0,"AhzoreArtngvirCnggrea":1,"CrepragCbfvgvirCnggrea":0,"CrepragArtngvirCnggrea":0,"ArtngvirVasvavglFlzoby":"-Vasvavgl","ArtngvirFvta":"-","AhzoreQrpvznyQvtvgf":2,"AhzoreQrpvznyFrcnengbe":".","AhzoreTebhcFrcnengbe":",","PheeraplCbfvgvirCnggrea":0,"CbfvgvirVasvavglFlzoby":"Vasvavgl","CbfvgvirFvta":"+","CrepragQrpvznyQvtvgf":2,"CrepragQrpvznyFrcnengbe":".","CrepragTebhcFrcnengbe":",","CrepragFlzoby":"%","CreZvyyrFlzoby":"\u2030","AngvirQvtvgf":["0","1","2","3","4","5","6","7","8","9"],"QvtvgFhofgvghgvba":1},"qngrGvzrSbezng":{"NZQrfvtangbe":"NZ","Pnyraqne":{"ZvaFhccbegrqQngrGvzr":"@-62135568000000@","ZnkFhccbegrqQngrGvzr":"@253402300799999@","NytbevguzGlcr":1,"PnyraqneGlcr":1,"Renf":[1],"GjbQvtvgLrneZnk":2029,"VfErnqBayl":snyfr},"QngrFrcnengbe":"/","SvefgQnlBsJrrx":0,"PnyraqneJrrxEhyr":0,"ShyyQngrGvzrCnggrea":"qqqq, ZZZZ qq, llll u:zz:ff gg","YbatQngrCnggrea":"qqqq, ZZZZ qq, llll","YbatGvzrCnggrea":"u:zz:ff gg","ZbaguQnlCnggrea":"ZZZZ qq","CZQrfvtangbe":"CZ","ESP1123Cnggrea":"qqq, qq ZZZ llll UU\':\'zz\':\'ff \'TZG\'","FubegQngrCnggrea":"Z/q/llll","FubegGvzrCnggrea":"u:zz gg","FbegnoyrQngrGvzrCnggrea":"llll\'-\'ZZ\'-\'qq\'G\'UU\':\'zz\':\'ff","GvzrFrcnengbe":":","HavirefnyFbegnoyrQngrGvzrCnggrea":"llll\'-\'ZZ\'-\'qq UU\':\'zz\':\'ff\'M\'","LrneZbaguCnggrea":"ZZZZ, llll","NooerivngrqQnlAnzrf":["Fha","Zba","Ghr","Jrq","Guh","Sev","Fng"],"FubegrfgQnlAnzrf":["Fh","Zb","Gh","Jr","Gu","Se","Fn"],"QnlAnzrf":["Fhaqnl","Zbaqnl","Ghrfqnl","Jrqarfqnl","Guhefqnl","Sevqnl","Fngheqnl"],"NooerivngrqZbaguAnzrf":["Wna","Sro","Zne","Nce","Znl","Wha","Why","Nht","Frc","Bpg","Abi","Qrp",""],"ZbaguAnzrf":["Wnahnel","Sroehnel","Znepu","Ncevy","Znl","Whar","Whyl","Nhthfg","Frcgrzore","Bpgbore","Abirzore","Qrprzore",""],"VfErnqBayl":snyfr,"AngvirPnyraqneAnzr":"Tertbevna Pnyraqne","NooerivngrqZbaguTravgvirAnzrf":["Wna","Sro","Zne","Nce","Znl","Wha","Why","Nht","Frc","Bpg","Abi","Qrp",""],"ZbaguTravgvirAnzrf":["Wnahnel","Sroehnel","Znepu","Ncevy","Znl","Whar","Whyl","Nhthfg","Frcgrzore","Bpgbore","Abirzore","Qrprzore",""]}}';
  var str4 = 'HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str5 = 'HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=';
  var re18 = /^\s+|\s+$/g;
  var str6 = 'uggc://jjj.snprobbx.pbz/vaqrk.cuc';
  var re19 = /(?:^|\s+)ba(?:\s+|$)/;
  var re20 = /[+, ]/;
  var re21 = /ybnqrq|pbzcyrgr/;
  var str7 = ';;jvaqbj.IjPurpxZbhfrCbfvgvbaNQ_VQ=shapgvba(r){vs(!r)ine r=jvaqbj.rirag;ine c=-1;vs(d1)c=d1.EbyybssCnary;ine bo=IjTrgBow("IjCnayNQ_VQ_"+c);vs(bo&&bo.fglyr.ivfvovyvgl=="ivfvoyr"){ine fns=IjFns?8:0;ine pheK=r.pyvragK+IjBOFpe("U")+fns,pheL=r.pyvragL+IjBOFpe("I")+fns;ine y=IjBOEC(NQ_VQ,bo,"Y"),g=IjBOEC(NQ_VQ,bo,"G");ine e=y+d1.Cnaryf[c].Jvqgu,o=g+d1.Cnaryf[c].Urvtug;vs((pheK<y)||(pheK>e)||(pheL<g)||(pheL>o)){vs(jvaqbj.IjBaEbyybssNQ_VQ)IjBaEbyybssNQ_VQ(c);ryfr IjPybfrNq(NQ_VQ,c,gehr,"");}ryfr erghea;}IjPnapryZbhfrYvfgrareNQ_VQ();};;jvaqbj.IjFrgEbyybssCnaryNQ_VQ=shapgvba(c){ine z="zbhfrzbir",q=qbphzrag,s=IjPurpxZbhfrCbfvgvbaNQ_VQ;c=IjTc(NQ_VQ,c);vs(d1&&d1.EbyybssCnary>-1)IjPnapryZbhfrYvfgrareNQ_VQ();vs(d1)d1.EbyybssCnary=c;gel{vs(q.nqqRiragYvfgrare)q.nqqRiragYvfgrare(z,s,snyfr);ryfr vs(q.nggnpuRirag)q.nggnpuRirag("ba"+z,s);}pngpu(r){}};;jvaqbj.IjPnapryZbhfrYvfgrareNQ_VQ=shapgvba(){ine z="zbhfrzbir",q=qbphzrag,s=IjPurpxZbhfrCbfvgvbaNQ_VQ;vs(d1)d1.EbyybssCnary=-1;gel{vs(q.erzbirRiragYvfgrare)q.erzbirRiragYvfgrare(z,s,snyfr);ryfr vs(q.qrgnpuRirag)q.qrgnpuRirag("ba"+z,s);}pngpu(r){}};;d1.IjTc=d2(n,c){ine nq=d1;vs(vfAnA(c)){sbe(ine v=0;v<nq.Cnaryf.yratgu;v++)vs(nq.Cnaryf[v].Anzr==c)erghea v;erghea 0;}erghea c;};;d1.IjTpy=d2(n,c,p){ine cn=d1.Cnaryf[IjTc(n,c)];vs(!cn)erghea 0;vs(vfAnA(p)){sbe(ine v=0;v<cn.Pyvpxguehf.yratgu;v++)vs(cn.Pyvpxguehf[v].Anzr==p)erghea v;erghea 0;}erghea p;};;d1.IjGenpr=d2(n,f){gel{vs(jvaqbj["Ij"+"QtQ"])jvaqbj["Ij"+"QtQ"](n,1,f);}pngpu(r){}};;d1.IjYvzvg1=d2(n,f){ine nq=d1,vh=f.fcyvg("/");sbe(ine v=0,p=0;v<vh.yratgu;v++){vs(vh[v].yratgu>0){vs(nq.FzV.yratgu>0)nq.FzV+="/";nq.FzV+=vh[v];nq.FtZ[nq.FtZ.yratgu]=snyfr;}}};;d1.IjYvzvg0=d2(n,f){ine nq=d1,vh=f.fcyvg("/");sbe(ine v=0;v<vh.yratgu;v++){vs(vh[v].yratgu>0){vs(nq.OvC.yratgu>0)nq.OvC+="/";nq.OvC+=vh[v];}}};;d1.IjRVST=d2(n,c){jvaqbj["IjCnayNQ_VQ_"+c+"_Bow"]=IjTrgBow("IjCnayNQ_VQ_"+c+"_Bow");vs(jvaqbj["IjCnayNQ_VQ_"+c+"_Bow"]==ahyy)frgGvzrbhg("IjRVST(NQ_VQ,"+c+")",d1.rvsg);};;d1.IjNavzSHC=d2(n,c){ine nq=d1;vs(c>nq.Cnaryf.yratgu)erghea;ine cna=nq.Cnaryf[c],nn=gehr,on=gehr,yn=gehr,en=gehr,cn=nq.Cnaryf[0],sf=nq.ShF,j=cn.Jvqgu,u=cn.Urvtug;vs(j=="100%"){j=sf;en=snyfr;yn=snyfr;}vs(u=="100%"){u=sf;nn=snyfr;on=snyfr;}vs(cn.YnY=="Y")yn=snyfr;vs(cn.YnY=="E")en=snyfr;vs(cn.GnY=="G")nn=snyfr;vs(cn.GnY=="O")on=snyfr;ine k=0,l=0;fjvgpu(nq.NshP%8){pnfr 0:oernx;pnfr 1:vs(nn)l=-sf;oernx;pnfr 2:k=j-sf;oernx;pnfr 3:vs(en)k=j;oernx;pnfr 4:k=j-sf;l=u-sf;oernx;pnfr 5:k=j-sf;vs(on)l=u;oernx;pnfr 6:l=u-sf;oernx;pnfr 7:vs(yn)k=-sf;l=u-sf;oernx;}vs(nq.NshP++ <nq.NshG)frgGvzrbhg(("IjNavzSHC(NQ_VQ,"+c+")"),nq.NshC);ryfr{k=-1000;l=k;}cna.YrsgBssfrg=k;cna.GbcBssfrg=l;IjNhErcb(n,c);};;d1.IjTrgErnyCbfvgvba=d2(n,b,j){erghea IjBOEC.nccyl(guvf,nethzragf);};;d1.IjPnapryGvzrbhg=d2(n,c){c=IjTc(n,c);ine cay=d1.Cnaryf[c];vs(cay&&cay.UgU!=""){pyrneGvzrbhg(cay.UgU);}};;d1.IjPnapryNyyGvzrbhgf=d2(n){vs(d1.YbpxGvzrbhgPunatrf)erghea;sbe(ine c=0;c<d1.bac;c++)IjPnapryGvzrbhg(n,c);};;d1.IjFgnegGvzrbhg=d2(n,c,bG){c=IjTc(n,c);ine cay=d1.Cnaryf[c];vs(cay&&((cay.UvqrGvzrbhgInyhr>0)||(nethzragf.yratgu==3&&bG>0))){pyrneGvzrbhg(cay.UgU);cay.UgU=frgGvzrbhg(cay.UvqrNpgvba,(nethzragf.yratgu==3?bG:cay.UvqrGvzrbhgInyhr));}};;d1.IjErfrgGvzrbhg=d2(n,c,bG){c=IjTc(n,c);IjPnapryGvzrbhg(n,c);riny("IjFgnegGvzrbhg(NQ_VQ,c"+(nethzragf.yratgu==3?",bG":"")+")");};;d1.IjErfrgNyyGvzrbhgf=d2(n){sbe(ine c=0;c<d1.bac;c++)IjErfrgGvzrbhg(n,c);};;d1.IjQrgnpure=d2(n,rig,sap){gel{vs(IjQVR5)riny("jvaqbj.qrgnpuRirag(\'ba"+rig+"\',"+sap+"NQ_VQ)");ryfr vs(!IjQVRZnp)riny("jvaqbj.erzbirRiragYvfgrare(\'"+rig+"\',"+sap+"NQ_VQ,snyfr)");}pngpu(r){}};;d1.IjPyrnaHc=d2(n){IjCvat(n,"G");ine nq=d1;sbe(ine v=0;v<nq.Cnaryf.yratgu;v++){IjUvqrCnary(n,v,gehr);}gel{IjTrgBow(nq.gya).vaareUGZY="";}pngpu(r){}vs(nq.gya!=nq.gya2)gel{IjTrgBow(nq.gya2).vaareUGZY="";}pngpu(r){}gel{d1=ahyy;}pngpu(r){}gel{IjQrgnpure(n,"haybnq","IjHayNQ_VQ");}pngpu(r){}gel{jvaqbj.IjHayNQ_VQ=ahyy;}pngpu(r){}gel{IjQrgnpure(n,"fpebyy","IjFeNQ_VQ");}pngpu(r){}gel{jvaqbj.IjFeNQ_VQ=ahyy;}pngpu(r){}gel{IjQrgnpure(n,"erfvmr","IjEmNQ_VQ");}pngpu(r){}gel{jvaqbj.IjEmNQ_VQ=ahyy;}pngpu(r){}gel{IjQrgnpure(n';
  var str8 = ';;jvaqbj.IjPurpxZbhfrCbfvgvbaNQ_VQ=shapgvba(r){vs(!r)ine r=jvaqbj.rirag;ine c=-1;vs(jvaqbj.IjNqNQ_VQ)c=jvaqbj.IjNqNQ_VQ.EbyybssCnary;ine bo=IjTrgBow("IjCnayNQ_VQ_"+c);vs(bo&&bo.fglyr.ivfvovyvgl=="ivfvoyr"){ine fns=IjFns?8:0;ine pheK=r.pyvragK+IjBOFpe("U")+fns,pheL=r.pyvragL+IjBOFpe("I")+fns;ine y=IjBOEC(NQ_VQ,bo,"Y"),g=IjBOEC(NQ_VQ,bo,"G");ine e=y+jvaqbj.IjNqNQ_VQ.Cnaryf[c].Jvqgu,o=g+jvaqbj.IjNqNQ_VQ.Cnaryf[c].Urvtug;vs((pheK<y)||(pheK>e)||(pheL<g)||(pheL>o)){vs(jvaqbj.IjBaEbyybssNQ_VQ)IjBaEbyybssNQ_VQ(c);ryfr IjPybfrNq(NQ_VQ,c,gehr,"");}ryfr erghea;}IjPnapryZbhfrYvfgrareNQ_VQ();};;jvaqbj.IjFrgEbyybssCnaryNQ_VQ=shapgvba(c){ine z="zbhfrzbir",q=qbphzrag,s=IjPurpxZbhfrCbfvgvbaNQ_VQ;c=IjTc(NQ_VQ,c);vs(jvaqbj.IjNqNQ_VQ&&jvaqbj.IjNqNQ_VQ.EbyybssCnary>-1)IjPnapryZbhfrYvfgrareNQ_VQ();vs(jvaqbj.IjNqNQ_VQ)jvaqbj.IjNqNQ_VQ.EbyybssCnary=c;gel{vs(q.nqqRiragYvfgrare)q.nqqRiragYvfgrare(z,s,snyfr);ryfr vs(q.nggnpuRirag)q.nggnpuRirag("ba"+z,s);}pngpu(r){}};;jvaqbj.IjPnapryZbhfrYvfgrareNQ_VQ=shapgvba(){ine z="zbhfrzbir",q=qbphzrag,s=IjPurpxZbhfrCbfvgvbaNQ_VQ;vs(jvaqbj.IjNqNQ_VQ)jvaqbj.IjNqNQ_VQ.EbyybssCnary=-1;gel{vs(q.erzbirRiragYvfgrare)q.erzbirRiragYvfgrare(z,s,snyfr);ryfr vs(q.qrgnpuRirag)q.qrgnpuRirag("ba"+z,s);}pngpu(r){}};;jvaqbj.IjNqNQ_VQ.IjTc=shapgvba(n,c){ine nq=jvaqbj.IjNqNQ_VQ;vs(vfAnA(c)){sbe(ine v=0;v<nq.Cnaryf.yratgu;v++)vs(nq.Cnaryf[v].Anzr==c)erghea v;erghea 0;}erghea c;};;jvaqbj.IjNqNQ_VQ.IjTpy=shapgvba(n,c,p){ine cn=jvaqbj.IjNqNQ_VQ.Cnaryf[IjTc(n,c)];vs(!cn)erghea 0;vs(vfAnA(p)){sbe(ine v=0;v<cn.Pyvpxguehf.yratgu;v++)vs(cn.Pyvpxguehf[v].Anzr==p)erghea v;erghea 0;}erghea p;};;jvaqbj.IjNqNQ_VQ.IjGenpr=shapgvba(n,f){gel{vs(jvaqbj["Ij"+"QtQ"])jvaqbj["Ij"+"QtQ"](n,1,f);}pngpu(r){}};;jvaqbj.IjNqNQ_VQ.IjYvzvg1=shapgvba(n,f){ine nq=jvaqbj.IjNqNQ_VQ,vh=f.fcyvg("/");sbe(ine v=0,p=0;v<vh.yratgu;v++){vs(vh[v].yratgu>0){vs(nq.FzV.yratgu>0)nq.FzV+="/";nq.FzV+=vh[v];nq.FtZ[nq.FtZ.yratgu]=snyfr;}}};;jvaqbj.IjNqNQ_VQ.IjYvzvg0=shapgvba(n,f){ine nq=jvaqbj.IjNqNQ_VQ,vh=f.fcyvg("/");sbe(ine v=0;v<vh.yratgu;v++){vs(vh[v].yratgu>0){vs(nq.OvC.yratgu>0)nq.OvC+="/";nq.OvC+=vh[v];}}};;jvaqbj.IjNqNQ_VQ.IjRVST=shapgvba(n,c){jvaqbj["IjCnayNQ_VQ_"+c+"_Bow"]=IjTrgBow("IjCnayNQ_VQ_"+c+"_Bow");vs(jvaqbj["IjCnayNQ_VQ_"+c+"_Bow"]==ahyy)frgGvzrbhg("IjRVST(NQ_VQ,"+c+")",jvaqbj.IjNqNQ_VQ.rvsg);};;jvaqbj.IjNqNQ_VQ.IjNavzSHC=shapgvba(n,c){ine nq=jvaqbj.IjNqNQ_VQ;vs(c>nq.Cnaryf.yratgu)erghea;ine cna=nq.Cnaryf[c],nn=gehr,on=gehr,yn=gehr,en=gehr,cn=nq.Cnaryf[0],sf=nq.ShF,j=cn.Jvqgu,u=cn.Urvtug;vs(j=="100%"){j=sf;en=snyfr;yn=snyfr;}vs(u=="100%"){u=sf;nn=snyfr;on=snyfr;}vs(cn.YnY=="Y")yn=snyfr;vs(cn.YnY=="E")en=snyfr;vs(cn.GnY=="G")nn=snyfr;vs(cn.GnY=="O")on=snyfr;ine k=0,l=0;fjvgpu(nq.NshP%8){pnfr 0:oernx;pnfr 1:vs(nn)l=-sf;oernx;pnfr 2:k=j-sf;oernx;pnfr 3:vs(en)k=j;oernx;pnfr 4:k=j-sf;l=u-sf;oernx;pnfr 5:k=j-sf;vs(on)l=u;oernx;pnfr 6:l=u-sf;oernx;pnfr 7:vs(yn)k=-sf;l=u-sf;oernx;}vs(nq.NshP++ <nq.NshG)frgGvzrbhg(("IjNavzSHC(NQ_VQ,"+c+")"),nq.NshC);ryfr{k=-1000;l=k;}cna.YrsgBssfrg=k;cna.GbcBssfrg=l;IjNhErcb(n,c);};;jvaqbj.IjNqNQ_VQ.IjTrgErnyCbfvgvba=shapgvba(n,b,j){erghea IjBOEC.nccyl(guvf,nethzragf);};;jvaqbj.IjNqNQ_VQ.IjPnapryGvzrbhg=shapgvba(n,c){c=IjTc(n,c);ine cay=jvaqbj.IjNqNQ_VQ.Cnaryf[c];vs(cay&&cay.UgU!=""){pyrneGvzrbhg(cay.UgU);}};;jvaqbj.IjNqNQ_VQ.IjPnapryNyyGvzrbhgf=shapgvba(n){vs(jvaqbj.IjNqNQ_VQ.YbpxGvzrbhgPunatrf)erghea;sbe(ine c=0;c<jvaqbj.IjNqNQ_VQ.bac;c++)IjPnapryGvzrbhg(n,c);};;jvaqbj.IjNqNQ_VQ.IjFgnegGvzrbhg=shapgvba(n,c,bG){c=IjTc(n,c);ine cay=jvaqbj.IjNqNQ_VQ.Cnaryf[c];vs(cay&&((cay.UvqrGvzrbhgInyhr>0)||(nethzragf.yratgu==3&&bG>0))){pyrneGvzrbhg(cay.UgU);cay.UgU=frgGvzrbhg(cay.UvqrNpgvba,(nethzragf.yratgu==3?bG:cay.UvqrGvzrbhgInyhr));}};;jvaqbj.IjNqNQ_VQ.IjErfrgGvzrbhg=shapgvba(n,c,bG){c=IjTc(n,c);IjPnapryGvzrbhg(n,c);riny("IjFgnegGvzrbhg(NQ_VQ,c"+(nethzragf.yratgu==3?",bG":"")+")");};;jvaqbj.IjNqNQ_VQ.IjErfrgNyyGvzrbhgf=shapgvba(n){sbe(ine c=0;c<jvaqbj.IjNqNQ_VQ.bac;c++)IjErfrgGvzrbhg(n,c);};;jvaqbj.IjNqNQ_VQ.IjQrgnpure=shapgvba(n,rig,sap){gel{vs(IjQVR5)riny("jvaqbj.qrgnpuRirag(\'ba"+rig+"\',"+sap+"NQ_VQ)");ryfr vs(!IjQVRZnp)riny("jvaqbj.erzbir';
  var str9 = ';;jvaqbj.IjPurpxZbhfrCbfvgvbaNQ_VQ=shapgvba(r){vs(!r)ine r=jvaqbj.rirag;ine c=-1;vs(jvaqbj.IjNqNQ_VQ)c=jvaqbj.IjNqNQ_VQ.EbyybssCnary;ine bo=IjTrgBow("IjCnayNQ_VQ_"+c);vs(bo&&bo.fglyr.ivfvovyvgl=="ivfvoyr"){ine fns=IjFns?8:0;ine pheK=r.pyvragK+IjBOFpe("U")+fns,pheL=r.pyvragL+IjBOFpe("I")+fns;ine y=IjBOEC(NQ_VQ,bo,"Y"),g=IjBOEC(NQ_VQ,bo,"G");ine e=y+jvaqbj.IjNqNQ_VQ.Cnaryf[c].Jvqgu,o=g+jvaqbj.IjNqNQ_VQ.Cnaryf[c].Urvtug;vs((pheK<y)||(pheK>e)||(pheL<g)||(pheL>o)){vs(jvaqbj.IjBaEbyybssNQ_VQ)IjBaEbyybssNQ_VQ(c);ryfr IjPybfrNq(NQ_VQ,c,gehr,"");}ryfr erghea;}IjPnapryZbhfrYvfgrareNQ_VQ();};;jvaqbj.IjFrgEbyybssCnaryNQ_VQ=shapgvba(c){ine z="zbhfrzbir",q=qbphzrag,s=IjPurpxZbhfrCbfvgvbaNQ_VQ;c=IjTc(NQ_VQ,c);vs(jvaqbj.IjNqNQ_VQ&&jvaqbj.IjNqNQ_VQ.EbyybssCnary>-1)IjPnapryZbhfrYvfgrareNQ_VQ();vs(jvaqbj.IjNqNQ_VQ)jvaqbj.IjNqNQ_VQ.EbyybssCnary=c;gel{vs(q.nqqRiragYvfgrare)q.nqqRiragYvfgrare(z,s,snyfr);ryfr vs(q.nggnpuRirag)q.nggnpuRirag("ba"+z,s);}pngpu(r){}};;jvaqbj.IjPnapryZbhfrYvfgrareNQ_VQ=shapgvba(){ine z="zbhfrzbir",q=qbphzrag,s=IjPurpxZbhfrCbfvgvbaNQ_VQ;vs(jvaqbj.IjNqNQ_VQ)jvaqbj.IjNqNQ_VQ.EbyybssCnary=-1;gel{vs(q.erzbirRiragYvfgrare)q.erzbirRiragYvfgrare(z,s,snyfr);ryfr vs(q.qrgnpuRirag)q.qrgnpuRirag("ba"+z,s);}pngpu(r){}};;jvaqbj.IjNqNQ_VQ.IjTc=d2(n,c){ine nq=jvaqbj.IjNqNQ_VQ;vs(vfAnA(c)){sbe(ine v=0;v<nq.Cnaryf.yratgu;v++)vs(nq.Cnaryf[v].Anzr==c)erghea v;erghea 0;}erghea c;};;jvaqbj.IjNqNQ_VQ.IjTpy=d2(n,c,p){ine cn=jvaqbj.IjNqNQ_VQ.Cnaryf[IjTc(n,c)];vs(!cn)erghea 0;vs(vfAnA(p)){sbe(ine v=0;v<cn.Pyvpxguehf.yratgu;v++)vs(cn.Pyvpxguehf[v].Anzr==p)erghea v;erghea 0;}erghea p;};;jvaqbj.IjNqNQ_VQ.IjGenpr=d2(n,f){gel{vs(jvaqbj["Ij"+"QtQ"])jvaqbj["Ij"+"QtQ"](n,1,f);}pngpu(r){}};;jvaqbj.IjNqNQ_VQ.IjYvzvg1=d2(n,f){ine nq=jvaqbj.IjNqNQ_VQ,vh=f.fcyvg("/");sbe(ine v=0,p=0;v<vh.yratgu;v++){vs(vh[v].yratgu>0){vs(nq.FzV.yratgu>0)nq.FzV+="/";nq.FzV+=vh[v];nq.FtZ[nq.FtZ.yratgu]=snyfr;}}};;jvaqbj.IjNqNQ_VQ.IjYvzvg0=d2(n,f){ine nq=jvaqbj.IjNqNQ_VQ,vh=f.fcyvg("/");sbe(ine v=0;v<vh.yratgu;v++){vs(vh[v].yratgu>0){vs(nq.OvC.yratgu>0)nq.OvC+="/";nq.OvC+=vh[v];}}};;jvaqbj.IjNqNQ_VQ.IjRVST=d2(n,c){jvaqbj["IjCnayNQ_VQ_"+c+"_Bow"]=IjTrgBow("IjCnayNQ_VQ_"+c+"_Bow");vs(jvaqbj["IjCnayNQ_VQ_"+c+"_Bow"]==ahyy)frgGvzrbhg("IjRVST(NQ_VQ,"+c+")",jvaqbj.IjNqNQ_VQ.rvsg);};;jvaqbj.IjNqNQ_VQ.IjNavzSHC=d2(n,c){ine nq=jvaqbj.IjNqNQ_VQ;vs(c>nq.Cnaryf.yratgu)erghea;ine cna=nq.Cnaryf[c],nn=gehr,on=gehr,yn=gehr,en=gehr,cn=nq.Cnaryf[0],sf=nq.ShF,j=cn.Jvqgu,u=cn.Urvtug;vs(j=="100%"){j=sf;en=snyfr;yn=snyfr;}vs(u=="100%"){u=sf;nn=snyfr;on=snyfr;}vs(cn.YnY=="Y")yn=snyfr;vs(cn.YnY=="E")en=snyfr;vs(cn.GnY=="G")nn=snyfr;vs(cn.GnY=="O")on=snyfr;ine k=0,l=0;fjvgpu(nq.NshP%8){pnfr 0:oernx;pnfr 1:vs(nn)l=-sf;oernx;pnfr 2:k=j-sf;oernx;pnfr 3:vs(en)k=j;oernx;pnfr 4:k=j-sf;l=u-sf;oernx;pnfr 5:k=j-sf;vs(on)l=u;oernx;pnfr 6:l=u-sf;oernx;pnfr 7:vs(yn)k=-sf;l=u-sf;oernx;}vs(nq.NshP++ <nq.NshG)frgGvzrbhg(("IjNavzSHC(NQ_VQ,"+c+")"),nq.NshC);ryfr{k=-1000;l=k;}cna.YrsgBssfrg=k;cna.GbcBssfrg=l;IjNhErcb(n,c);};;jvaqbj.IjNqNQ_VQ.IjTrgErnyCbfvgvba=d2(n,b,j){erghea IjBOEC.nccyl(guvf,nethzragf);};;jvaqbj.IjNqNQ_VQ.IjPnapryGvzrbhg=d2(n,c){c=IjTc(n,c);ine cay=jvaqbj.IjNqNQ_VQ.Cnaryf[c];vs(cay&&cay.UgU!=""){pyrneGvzrbhg(cay.UgU);}};;jvaqbj.IjNqNQ_VQ.IjPnapryNyyGvzrbhgf=d2(n){vs(jvaqbj.IjNqNQ_VQ.YbpxGvzrbhgPunatrf)erghea;sbe(ine c=0;c<jvaqbj.IjNqNQ_VQ.bac;c++)IjPnapryGvzrbhg(n,c);};;jvaqbj.IjNqNQ_VQ.IjFgnegGvzrbhg=d2(n,c,bG){c=IjTc(n,c);ine cay=jvaqbj.IjNqNQ_VQ.Cnaryf[c];vs(cay&&((cay.UvqrGvzrbhgInyhr>0)||(nethzragf.yratgu==3&&bG>0))){pyrneGvzrbhg(cay.UgU);cay.UgU=frgGvzrbhg(cay.UvqrNpgvba,(nethzragf.yratgu==3?bG:cay.UvqrGvzrbhgInyhr));}};;jvaqbj.IjNqNQ_VQ.IjErfrgGvzrbhg=d2(n,c,bG){c=IjTc(n,c);IjPnapryGvzrbhg(n,c);riny("IjFgnegGvzrbhg(NQ_VQ,c"+(nethzragf.yratgu==3?",bG":"")+")");};;jvaqbj.IjNqNQ_VQ.IjErfrgNyyGvzrbhgf=d2(n){sbe(ine c=0;c<jvaqbj.IjNqNQ_VQ.bac;c++)IjErfrgGvzrbhg(n,c);};;jvaqbj.IjNqNQ_VQ.IjQrgnpure=d2(n,rig,sap){gel{vs(IjQVR5)riny("jvaqbj.qrgnpuRirag(\'ba"+rig+"\',"+sap+"NQ_VQ)");ryfr vs(!IjQVRZnp)riny("jvaqbj.erzbirRiragYvfgrare(\'"+rig+"\',"+sap+"NQ_VQ,snyfr)");}pngpu(r){}};;jvaqbj.IjNqNQ_VQ.IjPyrna';

  var s26 = computeInputVariants('VC=74.125.75.1', 81);
  var s27 = computeInputVariants('9.0  e115', 78);
  var s28 = computeInputVariants('k',78);
  var s29 = computeInputVariants(str2, 81);
  var s30 = computeInputVariants(str3, 81);
  var s31 = computeInputVariants('144631658', 78);
  var s32 = computeInputVariants('Pbhagel=IIZ%3Q', 78);
  var s33 = computeInputVariants('Pbhagel=IIZ=', 78);
  var s34 = computeInputVariants('CersreerqPhygherCraqvat=', 78);
  var s35 = computeInputVariants(str4, 78);
  var s36 = computeInputVariants(str5, 78);
  var s37 = computeInputVariants('__hgzp=144631658', 78);
  var s38 = computeInputVariants('gvzrMbar=-8', 78);
  var s39 = computeInputVariants('gvzrMbar=0', 78);
  // var s40 = computeInputVariants(s15[i], 78);
  var s41 = computeInputVariants('vachggrkg  QBZPbageby_cynprubyqre', 78);
  var s42 = computeInputVariants('xrlqbja', 78);
  var s43 = computeInputVariants('xrlhc', 78);
  var s44 = computeInputVariants('uggc://zrffntvat.zlfcnpr.pbz/vaqrk.psz', 77);
  var s45 = computeInputVariants('FrffvbaFgbentr=%7O%22GnoThvq%22%3N%7O%22thvq%22%3N1231367125017%7Q%7Q', 73);
  var s46 = computeInputVariants(str6, 72);
  var s47 = computeInputVariants('3.5.0.0', 70);
  var s48 = computeInputVariants(str7, 70);
  var s49 = computeInputVariants(str8, 70);
  var s50 = computeInputVariants(str9, 70);
  var s51 = computeInputVariants('NI%3Q1_CI%3Q1_PI%3Q1_EI%3Q1_HI%3Q1_HP%3Q1_IC%3Q0.0.0.0_IH%3Q0', 70);
  var s52 = computeInputVariants('svz_zlfcnpr_ubzrcntr_abgybttrqva,svz_zlfcnpr_aba_HTP,svz_zlfcnpr_havgrq-fgngrf', 70);
  var s53 = computeInputVariants('ybnqvat', 70);
  var s54 = computeInputVariants('#', 68);
  var s55 = computeInputVariants('ybnqrq', 68);
  var s56 = computeInputVariants('pbybe', 49);
  var s57 = computeInputVariants('uggc://sevraqf.zlfcnpr.pbz/vaqrk.psz', 44);

  function runBlock1() {
    var sum = 0;
    for (var i = 0; i < 78; i++) {
      sum += Exec(re8, s26[i]);
      sum += s27[i].replace(/(\s)+e/, '').length;
      sum += s28[i].replace(/./, '').length;
      sum += s29[i].replace(re17, '').length;
      sum += s30[i].replace(re17, '').length;
      sum += Exec(re8, s31[i]);
      sum += Exec(re8, s32[i]);
      sum += Exec(re8, s33[i]);
      sum += Exec(re8, s34[i]);
      sum += Exec(re8, s35[i]);
      sum += Exec(re8, s36[i]);
      sum += Exec(re8, s37[i]);
      sum += Exec(re8, s38[i]);
      sum += Exec(re8, s39[i]);
      sum += Exec(/Fnsnev\/(\d+\.\d+)/, s15[i]);
      sum += Exec(re3, s41[i]);
      sum += Exec(re0, s42[i]);
      sum += Exec(re0, s43[i]);
    }
    for (var i = 0; i < 77; i++) {
      sum += s44[i].replace(re12, '').length;
      sum += Exec(re13, s44[i]);
    }
    for (var i = 0; i < 73; i++) {
      sum += s45[i].replace(re18, '').length;
      sum += Exec(re1, s46[i]);
    }
    for (var i = 0; i < 70; i++) {
      sum += Exec(re19, '');
      sum += s47[i].replace(re11, '').length;
      sum += s48[i].replace(/d1/g, '').length;
      sum += s49[i].replace(/NQ_VQ/g, '').length;
      sum += s50[i].replace(/d2/g, '').length;
      sum += s51[i].replace(/_/g, '').length;
      sum += s52[i].split(re20).length;
      sum += Exec(re21, s53[i]);
    }
    for (var i = 0; i < 68; i++) {
      sum += Exec(re1, s54[i]);
      sum += Exec(/(?:ZFVR.(\d+\.\d+))|(?:(?:Sversbk|TenaCnenqvfb|Vprjrnfry).(\d+\.\d+))|(?:Bcren.(\d+\.\d+))|(?:NccyrJroXvg.(\d+(?:\.\d+)?))/, s15[i]);
      sum += Exec(/(Znp BF K)|(Jvaqbjf;)/, s15[i]);
      sum += Exec(/Trpxb\/([0-9]+)/, s15[i]);
      sum += Exec(re21, s55[i]);
    }
    for (var i = 0; i < 44; i++) {
      sum += Exec(re16, s56[i]);
      sum += s57[i].replace(re12, '').length;
      sum += Exec(re13, s57[i]);
    }
    return sum;
  }
  var re22 = /\bso_zrah\b/;
  var re23 = /^(?:(?:[^:\/?#]+):)?(?:\/\/(?:[^\/?#]*))?([^?#]*)(?:\?([^#]*))?(?:#(.*))?/;
  var re24 = /uggcf?:\/\/([^\/]+\.)?snprobbx\.pbz\//;
  var re25 = /"/g;
  var re26 = /^([^?#]+)(?:\?([^#]*))?(#.*)?/;
  var s57a = computeInputVariants('fryrpgrq', 40);
  var s58 = computeInputVariants('vachggrkg uvqqra_ryrz', 40);
  var s59 = computeInputVariants('vachggrkg ', 40);
  var s60 = computeInputVariants('vachggrkg', 40);
  var s61 = computeInputVariants('uggc://jjj.snprobbx.pbz/', 40);
  var s62 = computeInputVariants('uggc://jjj.snprobbx.pbz/ybtva.cuc', 40);
  var s63 = computeInputVariants('Funer guvf tnqtrg', 40);
  var s64 = computeInputVariants('uggc://jjj.tbbtyr.pbz/vt/qverpgbel', 40);
  var s65 = computeInputVariants('419', 40);
  var s66 = computeInputVariants('gvzrfgnzc', 40);

  function runBlock2() {
    var sum = 0;
    for (var i = 0; i < 40; i++) {
      sum += s57a[i].replace(re14, '').length;
      sum += s57a[i].replace(re15, '').length;
    }
    for (var i = 0; i < 39; i++) {
      sum += s58[i].replace(/\buvqqra_ryrz\b/g, '').length;
      sum += Exec(re3, s59[i]);
      sum += Exec(re3, s60[i]);
      sum += Exec(re22, 'HVYvaxOhggba');
      sum += Exec(re22, 'HVYvaxOhggba_E');
      sum += Exec(re22, 'HVYvaxOhggba_EJ');
      sum += Exec(re22, 'zrah_ybtva_pbagnvare');
      sum += Exec(/\buvqqra_ryrz\b/, 'vachgcnffjbeq');
    }
    for (var i = 0; i < 37; i++) {
      sum += Exec(re8, '111soqs57qo8o8480qo18sor2011r3n591q7s6s37r120904');
      sum += Exec(re8, 'SbeprqRkcvengvba=633669315660164980');
      sum += Exec(re8, 'FrffvbaQQS2=111soqs57qo8o8480qo18sor2011r3n591q7s6s37r120904');
    }
    for (var i = 0; i < 35; i++) {
      sum += 'puvyq p1 svefg'.replace(re14, '').length;
      sum += 'puvyq p1 svefg'.replace(re15, '').length;
      sum += 'sylbhg pybfrq'.replace(re14, '').length;
      sum += 'sylbhg pybfrq'.replace(re15, '').length;
    }
    for (var i = 0; i < 34; i++) {
      sum += Exec(re19, 'gno2');
      sum += Exec(re19, 'gno3');
      sum += Exec(re8, '44132r503660');
      sum += Exec(re8, 'SbeprqRkcvengvba=633669316860113296');
      sum += Exec(re8, 'AFP_zp_dfctwzs-aowb_80=44132r503660');
      sum += Exec(re8, 'FrffvbaQQS2=s6r4579npn4rn2135s904r0s75pp1o5334p6s6pospo12696');
      sum += Exec(re8, 's6r4579npn4rn2135s904r0s75pp1o5334p6s6pospo12696');
    }
    for (var i = 0; i < 31; i++) {
      sum += Exec(/puebzr/i, s15[i]);
      sum += s61[i].replace(re23, '').length;
      sum += Exec(re8, 'SbeprqRkcvengvba=633669358527244818');
      sum += Exec(re8, 'VC=66.249.85.130');
      sum += Exec(re8, 'FrffvbaQQS2=s15q53p9n372sn76npr13o271n4s3p5r29p235746p908p58');
      sum += Exec(re8, 's15q53p9n372sn76npr13o271n4s3p5r29p235746p908p58');
      sum += Exec(re24, s61[i]);
    }
    for (var i = 0; i < 30; i++) {
      sum += s65[i].replace(re6, '').length;
      sum += Exec(/(?:^|\s+)gvzrfgnzc(?:\s+|$)/, s66[i]);
      sum += Exec(re7, s65[i]);
    }
    for (var i = 0; i < 28; i++) {
      sum += s62[i].replace(re23, '').length;
      sum += s63[i].replace(re25, '').length;
      sum += s63[i].replace(re12, '').length;
      sum += Exec(re26, s64[i]);
    }
    return sum;
  }
  var re27 = /-\D/g;
  var re28 = /\bnpgvingr\b/;
  var re29 = /%2R/gi;
  var re30 = /%2S/gi;
  var re31 = /^(mu-(PA|GJ)|wn|xb)$/;
  var re32 = /\s?;\s?/;
  var re33 = /%\w?$/;
  var re34 = /TNQP=([^;]*)/i;
  var str10 = 'FrffvbaQQS2=111soqs57qo8o8480qo18sor2011r3n591q7s6s37r120904; ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669315660164980&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=';
  var str11 = 'FrffvbaQQS2=111soqs57qo8o8480qo18sor2011r3n591q7s6s37r120904; __hgzm=144631658.1231363570.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar); __hgzn=144631658.3426875219718084000.1231363570.1231363570.1231363570.1; __hgzo=144631658.0.10.1231363570; __hgzp=144631658; ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669315660164980&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str12 = 'uggc://tbbtyrnqf.t.qbhoyrpyvpx.arg/cntrnq/nqf?pyvrag=pn-svz_zlfcnpr_zlfcnpr-ubzrcntr_wf&qg=1231363514065&uy=ra&nqfnsr=uvtu&br=hgs8&ahz_nqf=4&bhgchg=wf&nqgrfg=bss&pbeeryngbe=1231363514065&punaary=svz_zlfcnpr_ubzrcntr_abgybttrqva%2Psvz_zlfcnpr_aba_HTP%2Psvz_zlfcnpr_havgrq-fgngrf&hey=uggc%3N%2S%2Subzr.zlfcnpr.pbz%2Svaqrk.psz&nq_glcr=grkg&rvq=6083027&rn=0&sez=0&tn_ivq=1326469221.1231363557&tn_fvq=1231363557&tn_uvq=1114636509&synfu=9.0.115&h_u=768&h_j=1024&h_nu=738&h_nj=1024&h_pq=24&h_gm=-480&h_uvf=2&h_wnin=gehr&h_acyht=7&h_azvzr=22';
  var str13 = 'ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669315660164980&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str14 = 'ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669315660164980&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=';
  var re35 = /[<>]/g;
  var str15 = 'FrffvbaQQS2=s6r4579npn4rn2135s904r0s75pp1o5334p6s6pospo12696; ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669316860113296&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=; AFP_zp_dfctwzs-aowb_80=44132r503660';
  var str16 = 'FrffvbaQQS2=s6r4579npn4rn2135s904r0s75pp1o5334p6s6pospo12696; AFP_zp_dfctwzs-aowb_80=44132r503660; __hgzm=144631658.1231363638.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar); __hgzn=144631658.965867047679498800.1231363638.1231363638.1231363638.1; __hgzo=144631658.0.10.1231363638; __hgzp=144631658; ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669316860113296&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str17 = 'uggc://tbbtyrnqf.t.qbhoyrpyvpx.arg/cntrnq/nqf?pyvrag=pn-svz_zlfcnpr_zlfcnpr-ubzrcntr_wf&qg=1231363621014&uy=ra&nqfnsr=uvtu&br=hgs8&ahz_nqf=4&bhgchg=wf&nqgrfg=bss&pbeeryngbe=1231363621014&punaary=svz_zlfcnpr_ubzrcntr_abgybttrqva%2Psvz_zlfcnpr_aba_HTP%2Psvz_zlfcnpr_havgrq-fgngrf&hey=uggc%3N%2S%2Scebsvyr.zlfcnpr.pbz%2Svaqrk.psz&nq_glcr=grkg&rvq=6083027&rn=0&sez=0&tn_ivq=348699119.1231363624&tn_fvq=1231363624&tn_uvq=895511034&synfu=9.0.115&h_u=768&h_j=1024&h_nu=738&h_nj=1024&h_pq=24&h_gm=-480&h_uvf=2&h_wnin=gehr&h_acyht=7&h_azvzr=22';
  var str18 = 'uggc://jjj.yrobapbva.se/yv';
  var str19 = 'ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669316860113296&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str20 = 'ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669316860113296&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=';

  var s67 = computeInputVariants('e115', 27);
  var s68 = computeInputVariants('qvfcynl', 27);
  var s69 = computeInputVariants('cbfvgvba', 27);
  var s70 = computeInputVariants('uggc://jjj.zlfcnpr.pbz/', 27);
  var s71 = computeInputVariants('cntrivrj', 27);
  var s72 = computeInputVariants('VC=74.125.75.3', 27);
  var s73 = computeInputVariants('ra', 27);
  var s74 = computeInputVariants(str10, 27);
  var s75 = computeInputVariants(str11, 27);
  var s76 = computeInputVariants(str12, 27);
  var s77 = computeInputVariants(str17, 27);
  var s78 = computeInputVariants(str18, 27);

  function runBlock3() {
    var sum = 0;
    for (var i = 0; i < 23; i++) {
      sum += s67[i].replace(/[A-Za-z]/g, '').length;
      sum += s68[i].replace(re27, '').length;
      sum += s69[i].replace(re27, '').length;
    }
    for (var i = 0; i < 22; i++) {
      sum += 'unaqyr'.replace(re14, '').length;
      sum += 'unaqyr'.replace(re15, '').length;
      sum += 'yvar'.replace(re14, '').length;
      sum += 'yvar'.replace(re15, '').length;
      sum += 'cnerag puebzr6 fvatyr1 gno'.replace(re14, '').length;
      sum += 'cnerag puebzr6 fvatyr1 gno'.replace(re15, '').length;
      sum += 'fyvqre'.replace(re14, '').length;
      sum += 'fyvqre'.replace(re15, '').length;
      sum += Exec(re28, '');
    }
    for (var i = 0; i < 21; i++) {
      sum += s70[i].replace(re12, '').length;
      sum += Exec(re13, s70[i]);
    }
    for (var i = 0; i < 20; i++) {
      sum += s71[i].replace(re29, '').length;
      sum += s71[i].replace(re30, '').length;
      sum += Exec(re19, 'ynfg');
      sum += Exec(re19, 'ba svefg');
      sum += Exec(re8, s72[i]);
    }
    for (var i = 0; i < 18; i++) {
      sum += Exec(re31, s73[i]);
      sum += s74[i].split(re32).length;
      sum += s75[i].split(re32).length;
      sum += s76[i].replace(re33, '').length;
      sum += Exec(re8, '144631658.0.10.1231363570');
      sum += Exec(re8, '144631658.1231363570.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re8, '144631658.3426875219718084000.1231363570.1231363570.1231363570.1');
      sum += Exec(re8, str13);
      sum += Exec(re8, str14);
      sum += Exec(re8, '__hgzn=144631658.3426875219718084000.1231363570.1231363570.1231363570.1');
      sum += Exec(re8, '__hgzo=144631658.0.10.1231363570');
      sum += Exec(re8, '__hgzm=144631658.1231363570.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re34, s74[i]);
      sum += Exec(re34, s75[i]);
    }
    for (var i = 0; i < 17; i++) {
      s15[i].match(/zfvr/gi);
      s15[i].match(/bcren/gi);
      sum += str15.split(re32).length;
      sum += str16.split(re32).length;
      sum += 'ohggba'.replace(re14, '').length;
      sum += 'ohggba'.replace(re15, '').length;
      sum += 'puvyq p1 svefg sylbhg pybfrq'.replace(re14, '').length;
      sum += 'puvyq p1 svefg sylbhg pybfrq'.replace(re15, '').length;
      sum += 'pvgvrf'.replace(re14, '').length;
      sum += 'pvgvrf'.replace(re15, '').length;
      sum += 'pybfrq'.replace(re14, '').length;
      sum += 'pybfrq'.replace(re15, '').length;
      sum += 'qry'.replace(re14, '').length;
      sum += 'qry'.replace(re15, '').length;
      sum += 'uqy_zba'.replace(re14, '').length;
      sum += 'uqy_zba'.replace(re15, '').length;
      sum += s77[i].replace(re33, '').length;
      sum += s78[i].replace(/%3P/g, '').length;
      sum += s78[i].replace(/%3R/g, '').length;
      sum += s78[i].replace(/%3q/g, '').length;
      sum += s78[i].replace(re35, '').length;
      sum += 'yvaxyvfg16'.replace(re14, '').length;
      sum += 'yvaxyvfg16'.replace(re15, '').length;
      sum += 'zvahf'.replace(re14, '').length;
      sum += 'zvahf'.replace(re15, '').length;
      sum += 'bcra'.replace(re14, '').length;
      sum += 'bcra'.replace(re15, '').length;
      sum += 'cnerag puebzr5 fvatyr1 ps NU'.replace(re14, '').length;
      sum += 'cnerag puebzr5 fvatyr1 ps NU'.replace(re15, '').length;
      sum += 'cynlre'.replace(re14, '').length;
      sum += 'cynlre'.replace(re15, '').length;
      sum += 'cyhf'.replace(re14, '').length;
      sum += 'cyhf'.replace(re15, '').length;
      sum += 'cb_uqy'.replace(re14, '').length;
      sum += 'cb_uqy'.replace(re15, '').length;
      sum += 'hyJVzt'.replace(re14, '').length;
      sum += 'hyJVzt'.replace(re15, '').length;
      sum += Exec(re8, '144631658.0.10.1231363638');
      sum += Exec(re8, '144631658.1231363638.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re8, '144631658.965867047679498800.1231363638.1231363638.1231363638.1');
      sum += Exec(re8, '4413268q3660');
      sum += Exec(re8, '4ss747o77904333q374or84qrr1s9r0nprp8r5q81534o94n');
      sum += Exec(re8, 'SbeprqRkcvengvba=633669321699093060');
      sum += Exec(re8, 'VC=74.125.75.20');
      sum += Exec(re8, str19);
      sum += Exec(re8, str20);
      sum += Exec(re8, 'AFP_zp_tfwsbrg-aowb_80=4413268q3660');
      sum += Exec(re8, 'FrffvbaQQS2=4ss747o77904333q374or84qrr1s9r0nprp8r5q81534o94n');
      sum += Exec(re8, '__hgzn=144631658.965867047679498800.1231363638.1231363638.1231363638.1');
      sum += Exec(re8, '__hgzo=144631658.0.10.1231363638');
      sum += Exec(re8, '__hgzm=144631658.1231363638.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re34, str15);
      sum += Exec(re34, str16);
    }
    return sum;
  }
  var re36 = /uers|fep|fryrpgrq/;
  var re37 = /\s*([+>~\s])\s*([a-zA-Z#.*:\[])/g;
  var re38 = /^(\w+|\*)$/;
  var str21 = 'FrffvbaQQS2=s15q53p9n372sn76npr13o271n4s3p5r29p235746p908p58; ZFPhygher=VC=66.249.85.130&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669358527244818&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=';
  var str22 = 'FrffvbaQQS2=s15q53p9n372sn76npr13o271n4s3p5r29p235746p908p58; __hgzm=144631658.1231367822.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar); __hgzn=144631658.4127520630321984500.1231367822.1231367822.1231367822.1; __hgzo=144631658.0.10.1231367822; __hgzp=144631658; ZFPhygher=VC=66.249.85.130&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669358527244818&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str23 = 'uggc://tbbtyrnqf.t.qbhoyrpyvpx.arg/cntrnq/nqf?pyvrag=pn-svz_zlfcnpr_zlfcnpr-ubzrcntr_wf&qg=1231367803797&uy=ra&nqfnsr=uvtu&br=hgs8&ahz_nqf=4&bhgchg=wf&nqgrfg=bss&pbeeryngbe=1231367803797&punaary=svz_zlfcnpr_ubzrcntr_abgybttrqva%2Psvz_zlfcnpr_aba_HTP%2Psvz_zlfcnpr_havgrq-fgngrf&hey=uggc%3N%2S%2Szrffntvat.zlfcnpr.pbz%2Svaqrk.psz&nq_glcr=grkg&rvq=6083027&rn=0&sez=0&tn_ivq=1192552091.1231367807&tn_fvq=1231367807&tn_uvq=1155446857&synfu=9.0.115&h_u=768&h_j=1024&h_nu=738&h_nj=1024&h_pq=24&h_gm=-480&h_uvf=2&h_wnin=gehr&h_acyht=7&h_azvzr=22';
  var str24 = 'ZFPhygher=VC=66.249.85.130&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669358527244818&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str25 = 'ZFPhygher=VC=66.249.85.130&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669358527244818&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=';
  var str26 = 'hy.ynat-fryrpgbe';
  var re39 = /\\/g;
  var re40 = / /g;
  var re41 = /\/\xc4\/t/;
  var re42 = /\/\xd6\/t/;
  var re43 = /\/\xdc\/t/;
  var re44 = /\/\xdf\/t/;
  var re45 = /\/\xe4\/t/;
  var re46 = /\/\xf6\/t/;
  var re47 = /\/\xfc\/t/;
  var re48 = /\W/g;
  var re49 = /uers|fep|fglyr/;
  var s79 = computeInputVariants(str21, 16);
  var s80 = computeInputVariants(str22, 16);
  var s81 = computeInputVariants(str23, 16);
  var s82 = computeInputVariants(str26, 16);

  function runBlock4() {
    var sum = 0;
    for (var i = 0; i < 16; i++) {
      sum += ''.replace(/\*/g, '').length;
      sum += Exec(/\bnpgvir\b/, 'npgvir');
      sum += Exec(/sversbk/i, s15[i]);
      sum += Exec(re36, 'glcr');
      sum += Exec(/zfvr/i, s15[i]);
      sum += Exec(/bcren/i, s15[i]);
    }
    for (var i = 0; i < 15; i++) {
      sum += s79[i].split(re32).length;
      sum += s80[i].split(re32).length;
      sum += 'uggc://ohyyrgvaf.zlfcnpr.pbz/vaqrk.psz'.replace(re12, '').length;
      sum += s81[i].replace(re33, '').length;
      sum += 'yv'.replace(re37, '').length;
      sum += 'yv'.replace(re18, '').length;
      sum += Exec(re8, '144631658.0.10.1231367822');
      sum += Exec(re8, '144631658.1231367822.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re8, '144631658.4127520630321984500.1231367822.1231367822.1231367822.1');
      sum += Exec(re8, str24);
      sum += Exec(re8, str25);
      sum += Exec(re8, '__hgzn=144631658.4127520630321984500.1231367822.1231367822.1231367822.1');
      sum += Exec(re8, '__hgzo=144631658.0.10.1231367822');
      sum += Exec(re8, '__hgzm=144631658.1231367822.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re34, s79[i]);
      sum += Exec(re34, s80[i]);
      sum += Exec(/\.([\w-]+)|\[(\w+)(?:([!*^$~|]?=)["']?(.*?)["']?)?\]|:([\w-]+)(?:\(["']?(.*?)?["']?\)|$)/g, s82[i]);
      sum += Exec(re13, 'uggc://ohyyrgvaf.zlfcnpr.pbz/vaqrk.psz');
      sum += Exec(re38, 'yv');
    }
    for (var i = 0; i < 14; i++) {
      sum += ''.replace(re18, '').length;
      sum += '9.0  e115'.replace(/(\s+e|\s+o[0-9]+)/, '').length;
      sum += 'Funer guvf tnqtrg'.replace(/</g, '').length;
      sum += 'Funer guvf tnqtrg'.replace(/>/g, '').length;
      sum += 'Funer guvf tnqtrg'.replace(re39, '').length;
      sum += 'uggc://cebsvyrrqvg.zlfcnpr.pbz/vaqrk.psz'.replace(re12, '').length;
      sum += 'grnfre'.replace(re40, '').length;
      sum += 'grnfre'.replace(re41, '').length;
      sum += 'grnfre'.replace(re42, '').length;
      sum += 'grnfre'.replace(re43, '').length;
      sum += 'grnfre'.replace(re44, '').length;
      sum += 'grnfre'.replace(re45, '').length;
      sum += 'grnfre'.replace(re46, '').length;
      sum += 'grnfre'.replace(re47, '').length;
      sum += 'grnfre'.replace(re48, '').length;
      sum += Exec(re16, 'znetva-gbc');
      sum += Exec(re16, 'cbfvgvba');
      sum += Exec(re19, 'gno1');
      sum += Exec(re9, 'qz');
      sum += Exec(re9, 'qg');
      sum += Exec(re9, 'zbqobk');
      sum += Exec(re9, 'zbqobkva');
      sum += Exec(re9, 'zbqgvgyr');
      sum += Exec(re13, 'uggc://cebsvyrrqvg.zlfcnpr.pbz/vaqrk.psz');
      sum += Exec(re26, '/vt/znvytnqtrg');
      sum += Exec(re49, 'glcr');
    }
    return sum;
  }
  var re50 = /(?:^|\s+)fryrpgrq(?:\s+|$)/;
  var re51 = /\&/g;
  var re52 = /\+/g;
  var re53 = /\?/g;
  var re54 = /\t/g;
  var re55 = /(\$\{nqiHey\})|(\$nqiHey\b)/g;
  var re56 = /(\$\{cngu\})|(\$cngu\b)/g;
  function runBlock5() {
    var sum = 0;
    for (var i = 0; i < 13; i++) {
      sum += 'purpx'.replace(re14, '').length;
      sum += 'purpx'.replace(re15, '').length;
      sum += 'pvgl'.replace(re14, '').length;
      sum += 'pvgl'.replace(re15, '').length;
      sum += 'qrpe fyvqrgrkg'.replace(re14, '').length;
      sum += 'qrpe fyvqrgrkg'.replace(re15, '').length;
      sum += 'svefg fryrpgrq'.replace(re14, '').length;
      sum += 'svefg fryrpgrq'.replace(re15, '').length;
      sum += 'uqy_rag'.replace(re14, '').length;
      sum += 'uqy_rag'.replace(re15, '').length;
      sum += 'vape fyvqrgrkg'.replace(re14, '').length;
      sum += 'vape fyvqrgrkg'.replace(re15, '').length;
      sum += 'vachggrkg QBZPbageby_cynprubyqre'.replace(re5, '').length;
      sum += 'cnerag puebzr6 fvatyr1 gno fryrpgrq'.replace(re14, '').length;
      sum += 'cnerag puebzr6 fvatyr1 gno fryrpgrq'.replace(re15, '').length;
      sum += 'cb_guz'.replace(re14, '').length;
      sum += 'cb_guz'.replace(re15, '').length;
      sum += 'fhozvg'.replace(re14, '').length;
      sum += 'fhozvg'.replace(re15, '').length;
      sum += Exec(re50, '');
      sum += Exec(/NccyrJroXvg\/([^\s]*)/, s15[i]);
      sum += Exec(/XUGZY/, s15[i]);
    }
    for (var i = 0; i < 12; i++) {
      sum += '${cebg}://${ubfg}${cngu}/${dz}'.replace(/(\$\{cebg\})|(\$cebg\b)/g, '').length;
      sum += '1'.replace(re40, '').length;
      sum += '1'.replace(re10, '').length;
      sum += '1'.replace(re51, '').length;
      sum += '1'.replace(re52, '').length;
      sum += '1'.replace(re53, '').length;
      sum += '1'.replace(re39, '').length;
      sum += '1'.replace(re54, '').length;
      sum += '9.0  e115'.replace(/^(.*)\..*$/, '').length;
      sum += '9.0  e115'.replace(/^.*e(.*)$/, '').length;
      sum += '<!-- ${nqiHey} -->'.replace(re55, '').length;
      sum += '<fpevcg glcr="grkg/wninfpevcg" fep="${nqiHey}"></fpevcg>'.replace(re55, '').length;
      sum += s21[i].replace(/^.*\s+(\S+\s+\S+$)/, '').length;
      sum += 'tzk%2Subzrcntr%2Sfgneg%2Sqr%2S'.replace(re30, '').length;
      sum += 'tzk'.replace(re30, '').length;
      sum += 'uggc://${ubfg}${cngu}/${dz}'.replace(/(\$\{ubfg\})|(\$ubfg\b)/g, '').length;
      sum += 'uggc://nqpyvrag.hvzfrei.arg${cngu}/${dz}'.replace(re56, '').length;
      sum += 'uggc://nqpyvrag.hvzfrei.arg/wf.at/${dz}'.replace(/(\$\{dz\})|(\$dz\b)/g, '').length;
      sum += 'frpgvba'.replace(re29, '').length;
      sum += 'frpgvba'.replace(re30, '').length;
      sum += 'fvgr'.replace(re29, '').length;
      sum += 'fvgr'.replace(re30, '').length;
      sum += 'fcrpvny'.replace(re29, '').length;
      sum += 'fcrpvny'.replace(re30, '').length;
      sum += Exec(re36, 'anzr');
      sum += Exec(/e/, '9.0  e115');
    }
    return sum;
  }
  var re57 = /##yv4##/gi;
  var re58 = /##yv16##/gi;
  var re59 = /##yv19##/gi;
  var str27 = '<hy pynff="nqi">##yv4##Cbjreshy Zvpebfbsg grpuabybtl urycf svtug fcnz naq vzcebir frphevgl.##yv19##Trg zber qbar gunaxf gb terngre rnfr naq fcrrq.##yv16##Ybgf bs fgbentr &#40;5 TO&#41; - zber pbby fghss ba gur jnl.##OE## ##OE## ##N##Yrnea zber##/N##</hy>';
  var str28 = '<hy pynff="nqi"><yv vq="YvOYG4" fglyr="onpxtebhaq-vzntr:hey(uggc://vzt.jykef.pbz/~Yvir.FvgrPbagrag.VQ/~14.2.1230/~/~/~/oyg4.cat)">Cbjreshy Zvpebfbsg grpuabybtl urycf svtug fcnz naq vzcebir frphevgl.##yv19##Trg zber qbar gunaxf gb terngre rnfr naq fcrrq.##yv16##Ybgf bs fgbentr &#40;5 TO&#41; - zber pbby fghss ba gur jnl.##OE## ##OE## ##N##Yrnea zber##/N##</hy>';
  var str29 = '<hy pynff="nqi"><yv vq="YvOYG4" fglyr="onpxtebhaq-vzntr:hey(uggc://vzt.jykef.pbz/~Yvir.FvgrPbagrag.VQ/~14.2.1230/~/~/~/oyg4.cat)">Cbjreshy Zvpebfbsg grpuabybtl urycf svtug fcnz naq vzcebir frphevgl.##yv19##Trg zber qbar gunaxf gb terngre rnfr naq fcrrq.<yv vq="YvOYG16" fglyr="onpxtebhaq-vzntr:hey(uggc://vzt.jykef.pbz/~Yvir.FvgrPbagrag.VQ/~14.2.1230/~/~/~/oyg16.cat)">Ybgf bs fgbentr &#40;5 TO&#41; - zber pbby fghss ba gur jnl.##OE## ##OE## ##N##Yrnea zber##/N##</hy>';
  var str30 = '<hy pynff="nqi"><yv vq="YvOYG4" fglyr="onpxtebhaq-vzntr:hey(uggc://vzt.jykef.pbz/~Yvir.FvgrPbagrag.VQ/~14.2.1230/~/~/~/oyg4.cat)">Cbjreshy Zvpebfbsg grpuabybtl urycf svtug fcnz naq vzcebir frphevgl.<yv vq="YvOYG19" fglyr="onpxtebhaq-vzntr:hey(uggc://vzt.jykef.pbz/~Yvir.FvgrPbagrag.VQ/~14.2.1230/~/~/~/oyg19.cat)">Trg zber qbar gunaxf gb terngre rnfr naq fcrrq.<yv vq="YvOYG16" fglyr="onpxtebhaq-vzntr:hey(uggc://vzt.jykef.pbz/~Yvir.FvgrPbagrag.VQ/~14.2.1230/~/~/~/oyg16.cat)">Ybgf bs fgbentr &#40;5 TO&#41; - zber pbby fghss ba gur jnl.##OE## ##OE## ##N##Yrnea zber##/N##</hy>';
  var str31 = '<hy pynff="nqi"><yv vq="YvOYG4" fglyr="onpxtebhaq-vzntr:hey(uggc://vzt.jykef.pbz/~Yvir.FvgrPbagrag.VQ/~14.2.1230/~/~/~/oyg4.cat)">Cbjreshy Zvpebfbsg grpuabybtl urycf svtug fcnz naq vzcebir frphevgl.<yv vq="YvOYG19" fglyr="onpxtebhaq-vzntr:hey(uggc://vzt.jykef.pbz/~Yvir.FvgrPbagrag.VQ/~14.2.1230/~/~/~/oyg19.cat)">Trg zber qbar gunaxf gb terngre rnfr naq fcrrq.<yv vq="YvOYG16" fglyr="onpxtebhaq-vzntr:hey(uggc://vzt.jykef.pbz/~Yvir.FvgrPbagrag.VQ/~14.2.1230/~/~/~/oyg16.cat)">Ybgf bs fgbentr &#40;5 TO&#41; - zber pbby fghss ba gur jnl.<oe> <oe> ##N##Yrnea zber##/N##</hy>';
  var str32 = '<hy pynff="nqi"><yv vq="YvOYG4" fglyr="onpxtebhaq-vzntr:hey(uggc://vzt.jykef.pbz/~Yvir.FvgrPbagrag.VQ/~14.2.1230/~/~/~/oyg4.cat)">Cbjreshy Zvpebfbsg grpuabybtl urycf svtug fcnz naq vzcebir frphevgl.<yv vq="YvOYG19" fglyr="onpxtebhaq-vzntr:hey(uggc://vzt.jykef.pbz/~Yvir.FvgrPbagrag.VQ/~14.2.1230/~/~/~/oyg19.cat)">Trg zber qbar gunaxf gb terngre rnfr naq fcrrq.<yv vq="YvOYG16" fglyr="onpxtebhaq-vzntr:hey(uggc://vzt.jykef.pbz/~Yvir.FvgrPbagrag.VQ/~14.2.1230/~/~/~/oyg16.cat)">Ybgf bs fgbentr &#40;5 TO&#41; - zber pbby fghss ba gur jnl.<oe> <oe> <n uers="uggc://znvy.yvir.pbz/znvy/nobhg.nfck" gnetrg="_oynax">Yrnea zber##/N##</hy>';
  var str33 = 'Bar Jvaqbjf Yvir VQ trgf lbh vagb <o>Ubgznvy</o>, <o>Zrffratre</o>, <o>Kobk YVIR</o> \u2014 naq bgure cynprf lbh frr #~#argjbexybtb#~#';
  var re60 = /(?:^|\s+)bss(?:\s+|$)/;
  var re61 = /^(([^:\/?#]+):)?(\/\/([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?$/;
  var re62 = /^[^<]*(<(.|\s)+>)[^>]*$|^#(\w+)$/;
  var str34 = '${1}://${2}${3}${4}${5}';
  var str35 = ' O=6gnyg0g4znrrn&o=3&f=gc; Q=_lyu=K3bQZGSxnT4lZzD3OS9GNmV3ZGLkAQxRpTyxNmRlZmRmAmNkAQLRqTImqNZjOUEgpTjQnJ5xMKtgoN--; SCF=qy';
  var s83 = computeInputVariants(str27, 11);
  var s84 = computeInputVariants(str28, 11);
  var s85 = computeInputVariants(str29, 11);
  var s86 = computeInputVariants(str30, 11);
  var s87 = computeInputVariants(str31, 11);
  var s88 = computeInputVariants(str32, 11);
  var s89 = computeInputVariants(str33, 11);
  var s90 = computeInputVariants(str34, 11);

  function runBlock6() {
    var sum = 0;
    for (var i = 0; i < 11; i++) {
      sum += s83[i].replace(/##yv0##/gi, '').length;
      sum += s83[i].replace(re57, '').length;
      sum += s84[i].replace(re58, '').length;
      sum += s85[i].replace(re59, '').length;
      sum += s86[i].replace(/##\/o##/gi, '').length;
      sum += s86[i].replace(/##\/v##/gi, '').length;
      sum += s86[i].replace(/##\/h##/gi, '').length;
      sum += s86[i].replace(/##o##/gi, '').length;
      sum += s86[i].replace(/##oe##/gi, '').length;
      sum += s86[i].replace(/##v##/gi, '').length;
      sum += s86[i].replace(/##h##/gi, '').length;
      sum += s87[i].replace(/##n##/gi, '').length;
      sum += s88[i].replace(/##\/n##/gi, '').length;
      sum += s89[i].replace(/#~#argjbexybtb#~#/g, '').length;
      sum += Exec(/ Zbovyr\//, s15[i]);
      sum += Exec(/##yv1##/gi, s83[i]);
      sum += Exec(/##yv10##/gi, s84[i]);
      sum += Exec(/##yv11##/gi, s84[i]);
      sum += Exec(/##yv12##/gi, s84[i]);
      sum += Exec(/##yv13##/gi, s84[i]);
      sum += Exec(/##yv14##/gi, s84[i]);
      sum += Exec(/##yv15##/gi, s84[i]);
      sum += Exec(re58, s84[i]);
      sum += Exec(/##yv17##/gi, s85[i]);
      sum += Exec(/##yv18##/gi, s85[i]);
      sum += Exec(re59, s85[i]);
      sum += Exec(/##yv2##/gi, s83[i]);
      sum += Exec(/##yv20##/gi, s86[i]);
      sum += Exec(/##yv21##/gi, s86[i]);
      sum += Exec(/##yv22##/gi, s86[i]);
      sum += Exec(/##yv23##/gi, s86[i]);
      sum += Exec(/##yv3##/gi, s83[i]);
      sum += Exec(re57, s83[i]);
      sum += Exec(/##yv5##/gi, s84[i]);
      sum += Exec(/##yv6##/gi, s84[i]);
      sum += Exec(/##yv7##/gi, s84[i]);
      sum += Exec(/##yv8##/gi, s84[i]);
      sum += Exec(/##yv9##/gi, s84[i]);
      sum += Exec(re8, '473qq1rs0n2r70q9qo1pq48n021s9468ron90nps048p4p29');
      sum += Exec(re8, 'SbeprqRkcvengvba=633669325184628362');
      sum += Exec(re8, 'FrffvbaQQS2=473qq1rs0n2r70q9qo1pq48n021s9468ron90nps048p4p29');
      sum += Exec(/AbxvnA[^\/]*/, s15[i]);
    }
    for (var i = 0; i < 10; i++) {
      sum += ' bss'.replace(/(?:^|\s+)bss(?:\s+|$)/g, '').length;
      sum += s90[i].replace(/(\$\{0\})|(\$0\b)/g, '').length;
      sum += s90[i].replace(/(\$\{1\})|(\$1\b)/g, '').length;
      sum += s90[i].replace(/(\$\{pbzcyrgr\})|(\$pbzcyrgr\b)/g, '').length;
      sum += s90[i].replace(/(\$\{sentzrag\})|(\$sentzrag\b)/g, '').length;
      sum += s90[i].replace(/(\$\{ubfgcbeg\})|(\$ubfgcbeg\b)/g, '').length;
      sum += s90[i].replace(re56, '').length;
      sum += s90[i].replace(/(\$\{cebgbpby\})|(\$cebgbpby\b)/g, '').length;
      sum += s90[i].replace(/(\$\{dhrel\})|(\$dhrel\b)/g, '').length;
      sum += 'nqfvmr'.replace(re29, '').length;
      sum += 'nqfvmr'.replace(re30, '').length;
      sum += 'uggc://${2}${3}${4}${5}'.replace(/(\$\{2\})|(\$2\b)/g, '').length;
      sum += 'uggc://wf.hv-cbegny.qr${3}${4}${5}'.replace(/(\$\{3\})|(\$3\b)/g, '').length;
      sum += 'arjf'.replace(re40, '').length;
      sum += 'arjf'.replace(re41, '').length;
      sum += 'arjf'.replace(re42, '').length;
      sum += 'arjf'.replace(re43, '').length;
      sum += 'arjf'.replace(re44, '').length;
      sum += 'arjf'.replace(re45, '').length;
      sum += 'arjf'.replace(re46, '').length;
      sum += 'arjf'.replace(re47, '').length;
      sum += 'arjf'.replace(re48, '').length;
      sum += Exec(/ PC=i=(\d+)&oe=(.)/, str35);
      sum += Exec(re60, ' ');
      sum += Exec(re60, ' bss');
      sum += Exec(re60, '');
      sum += Exec(re19, ' ');
      sum += Exec(re19, 'svefg ba');
      sum += Exec(re19, 'ynfg vtaber');
      sum += Exec(re19, 'ba');
      sum += Exec(re9, 'scnq so ');
      sum += Exec(re9, 'zrqvgobk');
      sum += Exec(re9, 'hsgy');
      sum += Exec(re9, 'lhv-h');
      sum += Exec(/Fnsnev|Xbadhrebe|XUGZY/gi, s15[i]);
      sum += Exec(re61, 'uggc://wf.hv-cbegny.qr/tzk/ubzr/wf/20080602/onfr.wf');
      sum += Exec(re62, '#Ybtva_rznvy');
    }
    return sum;
  }
  var re63 = /\{0\}/g;
  var str36 = 'FrffvbaQQS2=4ss747o77904333q374or84qrr1s9r0nprp8r5q81534o94n; ZFPhygher=VC=74.125.75.20&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669321699093060&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=; AFP_zp_tfwsbrg-aowb_80=4413268q3660';
  var str37 = 'FrffvbaQQS2=4ss747o77904333q374or84qrr1s9r0nprp8r5q81534o94n; AFP_zp_tfwsbrg-aowb_80=4413268q3660; __hgzm=144631658.1231364074.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar); __hgzn=144631658.2294274870215848400.1231364074.1231364074.1231364074.1; __hgzo=144631658.0.10.1231364074; __hgzp=144631658; ZFPhygher=VC=74.125.75.20&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669321699093060&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str38 = 'uggc://tbbtyrnqf.t.qbhoyrpyvpx.arg/cntrnq/nqf?pyvrag=pn-svz_zlfcnpr_zlfcnpr-ubzrcntr_wf&qg=1231364057761&uy=ra&nqfnsr=uvtu&br=hgs8&ahz_nqf=4&bhgchg=wf&nqgrfg=bss&pbeeryngbe=1231364057761&punaary=svz_zlfcnpr_ubzrcntr_abgybttrqva%2Psvz_zlfcnpr_aba_HTP%2Psvz_zlfcnpr_havgrq-fgngrf&hey=uggc%3N%2S%2Ssevraqf.zlfcnpr.pbz%2Svaqrk.psz&nq_glcr=grkg&rvq=6083027&rn=0&sez=0&tn_ivq=1667363813.1231364061&tn_fvq=1231364061&tn_uvq=1917563877&synfu=9.0.115&h_u=768&h_j=1024&h_nu=738&h_nj=1024&h_pq=24&h_gm=-480&h_uvf=2&h_wnin=gehr&h_acyht=7&h_azvzr=22';
  var str39 = 'ZFPhygher=VC=74.125.75.20&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669321699093060&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str40 = 'ZFPhygher=VC=74.125.75.20&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669321699093060&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=';
  var s91 = computeInputVariants(str36, 9);
  var s92 = computeInputVariants(str37, 9);
  var s93 = computeInputVariants(str38, 9);
  function runBlock7() {
    var sum = 0;
    for (var i = 0; i < 9; i++) {
      sum += '0'.replace(re40, '').length;
      sum += '0'.replace(re10, '').length;
      sum += '0'.replace(re51, '').length;
      sum += '0'.replace(re52, '').length;
      sum += '0'.replace(re53, '').length;
      sum += '0'.replace(re39, '').length;
      sum += '0'.replace(re54, '').length;
      sum += 'Lrf'.replace(re40, '').length;
      sum += 'Lrf'.replace(re10, '').length;
      sum += 'Lrf'.replace(re51, '').length;
      sum += 'Lrf'.replace(re52, '').length;
      sum += 'Lrf'.replace(re53, '').length;
      sum += 'Lrf'.replace(re39, '').length;
      sum += 'Lrf'.replace(re54, '').length;
    }
    for (var i = 0; i < 8; i++) {
      sum += 'Pybfr {0}'.replace(re63, '').length;
      sum += 'Bcra {0}'.replace(re63, '').length;
      sum += s91[i].split(re32).length;
      sum += s92[i].split(re32).length;
      sum += 'puvyq p1 svefg gnournqref'.replace(re14, '').length;
      sum += 'puvyq p1 svefg gnournqref'.replace(re15, '').length;
      sum += 'uqy_fcb'.replace(re14, '').length;
      sum += 'uqy_fcb'.replace(re15, '').length;
      sum += 'uvag'.replace(re14, '').length;
      sum += 'uvag'.replace(re15, '').length;
      sum += s93[i].replace(re33, '').length;
      sum += 'yvfg'.replace(re14, '').length;
      sum += 'yvfg'.replace(re15, '').length;
      sum += 'at_bhgre'.replace(re30, '').length;
      sum += 'cnerag puebzr5 qbhoyr2 NU'.replace(re14, '').length;
      sum += 'cnerag puebzr5 qbhoyr2 NU'.replace(re15, '').length;
      sum += 'cnerag puebzr5 dhnq5 ps NU osyvax zbarl'.replace(re14, '').length;
      sum += 'cnerag puebzr5 dhnq5 ps NU osyvax zbarl'.replace(re15, '').length;
      sum += 'cnerag puebzr6 fvatyr1'.replace(re14, '').length;
      sum += 'cnerag puebzr6 fvatyr1'.replace(re15, '').length;
      sum += 'cb_qrs'.replace(re14, '').length;
      sum += 'cb_qrs'.replace(re15, '').length;
      sum += 'gnopbagrag'.replace(re14, '').length;
      sum += 'gnopbagrag'.replace(re15, '').length;
      sum += 'iv_svefg_gvzr'.replace(re30, '').length;
      sum += Exec(/(^|.)(ronl|qri-ehf3.wbg)(|fgberf|zbgbef|yvirnhpgvbaf|jvxv|rkcerff|punggre).(pbz(|.nh|.pa|.ux|.zl|.ft|.oe|.zk)|pb(.hx|.xe|.am)|pn|qr|se|vg|ay|or|ng|pu|vr|va|rf|cy|cu|fr)$/i, 'cntrf.ronl.pbz');
      sum += Exec(re8, '144631658.0.10.1231364074');
      sum += Exec(re8, '144631658.1231364074.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re8, '144631658.2294274870215848400.1231364074.1231364074.1231364074.1');
      sum += Exec(re8, '4413241q3660');
      sum += Exec(re8, 'SbeprqRkcvengvba=633669357391353591');
      sum += Exec(re8, str39);
      sum += Exec(re8, str40);
      sum += Exec(re8, 'AFP_zp_kkk-gdzogv_80=4413241q3660');
      sum += Exec(re8, 'FrffvbaQQS2=p98s8o9q42nr21or1r61pqorn1n002nsss569635984s6qp7');
      sum += Exec(re8, '__hgzn=144631658.2294274870215848400.1231364074.1231364074.1231364074.1');
      sum += Exec(re8, '__hgzo=144631658.0.10.1231364074');
      sum += Exec(re8, '__hgzm=144631658.1231364074.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re8, 'p98s8o9q42nr21or1r61pqorn1n002nsss569635984s6qp7');
      sum += Exec(re34, s91[i]);
      sum += Exec(re34, s92[i]);
    }
    return sum;
  }
  var re64 = /\b[a-z]/g;
  var re65 = /^uggc:\/\//;
  var re66 = /(?:^|\s+)qvfnoyrq(?:\s+|$)/;
  var str41 = 'uggc://cebsvyr.zlfcnpr.pbz/Zbqhyrf/Nccyvpngvbaf/Cntrf/Pnainf.nfck';
  function runBlock8() {
    var sum = 0;
    for (var i = 0; i < 7; i++) {
      s21[i].match(/\d+/g);
      sum += 'nsgre'.replace(re64, '').length;
      sum += 'orsber'.replace(re64, '').length;
      sum += 'obggbz'.replace(re64, '').length;
      sum += 'ohvygva_jrngure.kzy'.replace(re65, '').length;
      sum += 'ohggba'.replace(re37, '').length;
      sum += 'ohggba'.replace(re18, '').length;
      sum += 'qngrgvzr.kzy'.replace(re65, '').length;
      sum += 'uggc://eff.paa.pbz/eff/paa_gbcfgbevrf.eff'.replace(re65, '').length;
      sum += 'vachg'.replace(re37, '').length;
      sum += 'vachg'.replace(re18, '').length;
      sum += 'vafvqr'.replace(re64, '').length;
      sum += 'cbvagre'.replace(re27, '').length;
      sum += 'cbfvgvba'.replace(/[A-Z]/g, '').length;
      sum += 'gbc'.replace(re27, '').length;
      sum += 'gbc'.replace(re64, '').length;
      sum += 'hy'.replace(re37, '').length;
      sum += 'hy'.replace(re18, '').length;
      sum += str26.replace(re37, '').length;
      sum += str26.replace(re18, '').length;
      sum += 'lbhghor_vtbbtyr/i2/lbhghor.kzy'.replace(re65, '').length;
      sum += 'm-vaqrk'.replace(re27, '').length;
      sum += Exec(/#([\w-]+)/, str26);
      sum += Exec(re16, 'urvtug');
      sum += Exec(re16, 'znetvaGbc');
      sum += Exec(re16, 'jvqgu');
      sum += Exec(re19, 'gno0 svefg ba');
      sum += Exec(re19, 'gno0 ba');
      sum += Exec(re19, 'gno4 ynfg');
      sum += Exec(re19, 'gno4');
      sum += Exec(re19, 'gno5');
      sum += Exec(re19, 'gno6');
      sum += Exec(re19, 'gno7');
      sum += Exec(re19, 'gno8');
      sum += Exec(/NqborNVE\/([^\s]*)/, s15[i]);
      sum += Exec(/NccyrJroXvg\/([^ ]*)/, s15[i]);
      sum += Exec(/XUGZY/gi, s15[i]);
      sum += Exec(/^(?:obql|ugzy)$/i, 'YV');
      sum += Exec(re38, 'ohggba');
      sum += Exec(re38, 'vachg');
      sum += Exec(re38, 'hy');
      sum += Exec(re38, str26);
      sum += Exec(/^(\w+|\*)/, str26);
      sum += Exec(/znp|jva|yvahk/i, 'Jva32');
      sum += Exec(/eton?\([\d\s,]+\)/, 'fgngvp');
    }
    for (var i = 0; i < 6; i++) {
      sum += ''.replace(/\r/g, '').length;
      sum += '/'.replace(re40, '').length;
      sum += '/'.replace(re10, '').length;
      sum += '/'.replace(re51, '').length;
      sum += '/'.replace(re52, '').length;
      sum += '/'.replace(re53, '').length;
      sum += '/'.replace(re39, '').length;
      sum += '/'.replace(re54, '').length;
      sum += 'uggc://zfacbegny.112.2b7.arg/o/ff/zfacbegnyubzr/1/U.7-cqi-2/{0}?[NDO]&{1}&{2}&[NDR]'.replace(re63, '').length;
      sum += str41.replace(re12, '').length;
      sum += 'uggc://jjj.snprobbx.pbz/fepu.cuc'.replace(re23, '').length;
      sum += 'freivpr'.replace(re40, '').length;
      sum += 'freivpr'.replace(re41, '').length;
      sum += 'freivpr'.replace(re42, '').length;
      sum += 'freivpr'.replace(re43, '').length;
      sum += 'freivpr'.replace(re44, '').length;
      sum += 'freivpr'.replace(re45, '').length;
      sum += 'freivpr'.replace(re46, '').length;
      sum += 'freivpr'.replace(re47, '').length;
      sum += 'freivpr'.replace(re48, '').length;
      sum += Exec(/((ZFVR\s+([6-9]|\d\d)\.))/, s15[i]);
      sum += Exec(re66, '');
      sum += Exec(re50, 'fryrpgrq');
      sum += Exec(re8, '8sqq78r9n442851q565599o401385sp3s04r92rnn7o19ssn');
      sum += Exec(re8, 'SbeprqRkcvengvba=633669340386893867');
      sum += Exec(re8, 'VC=74.125.75.17');
      sum += Exec(re8, 'FrffvbaQQS2=8sqq78r9n442851q565599o401385sp3s04r92rnn7o19ssn');
      sum += Exec(/Xbadhrebe|Fnsnev|XUGZY/, s15[i]);
      sum += Exec(re13, str41);
      sum += Exec(re49, 'unfsbphf');
    }
    return sum;
  }
  var re67 = /zrah_byq/g;
  var str42 = 'FrffvbaQQS2=473qq1rs0n2r70q9qo1pq48n021s9468ron90nps048p4p29; ZFPhygher=VC=74.125.75.3&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669325184628362&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=';
  var str43 = 'FrffvbaQQS2=473qq1rs0n2r70q9qo1pq48n021s9468ron90nps048p4p29; __hgzm=144631658.1231364380.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar); __hgzn=144631658.3931862196947939300.1231364380.1231364380.1231364380.1; __hgzo=144631658.0.10.1231364380; __hgzp=144631658; ZFPhygher=VC=74.125.75.3&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669325184628362&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str44 = 'uggc://tbbtyrnqf.t.qbhoyrpyvpx.arg/cntrnq/nqf?pyvrag=pn-svz_zlfcnpr_vzntrf_wf&qg=1231364373088&uy=ra&nqfnsr=uvtu&br=hgs8&ahz_nqf=4&bhgchg=wf&nqgrfg=bss&pbeeryngbe=1231364373088&punaary=svz_zlfcnpr_hfre-ivrj-pbzzragf%2Psvz_zlfcnpr_havgrq-fgngrf&hey=uggc%3N%2S%2Spbzzrag.zlfcnpr.pbz%2Svaqrk.psz&nq_glcr=grkg&rvq=6083027&rn=0&sez=0&tn_ivq=1158737789.1231364375&tn_fvq=1231364375&tn_uvq=415520832&synfu=9.0.115&h_u=768&h_j=1024&h_nu=738&h_nj=1024&h_pq=24&h_gm=-480&h_uvf=2&h_wnin=gehr&h_acyht=7&h_azvzr=22';
  var str45 = 'ZFPhygher=VC=74.125.75.3&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669325184628362&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str46 = 'ZFPhygher=VC=74.125.75.3&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669325184628362&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=';
  var re68 = /^([#.]?)((?:[\w\u0128-\uffff*_-]|\\.)*)/;
  var re69 = /\{1\}/g;
  var re70 = /\s+/;
  var re71 = /(\$\{4\})|(\$4\b)/g;
  var re72 = /(\$\{5\})|(\$5\b)/g;
  var re73 = /\{2\}/g;
  var re74 = /[^+>] [^+>]/;
  var re75 = /\bucpyv\s*=\s*([^;]*)/i;
  var re76 = /\bucuvqr\s*=\s*([^;]*)/i;
  var re77 = /\bucfie\s*=\s*([^;]*)/i;
  var re78 = /\bhfucjrn\s*=\s*([^;]*)/i;
  var re79 = /\bmvc\s*=\s*([^;]*)/i;
  var re80 = /^((?:[\w\u0128-\uffff*_-]|\\.)+)(#)((?:[\w\u0128-\uffff*_-]|\\.)+)/;
  var re81 = /^([>+~])\s*(\w*)/i;
  var re82 = /^>\s*((?:[\w\u0128-\uffff*_-]|\\.)+)/;
  var re83 = /^[\s[]?shapgvba/;
  var re84 = /v\/g.tvs#(.*)/i;
  var str47 = '#Zbq-Vasb-Vasb-WninFpevcgUvag';
  var str48 = ',n.svryqOgaPnapry';
  var str49 = 'FrffvbaQQS2=p98s8o9q42nr21or1r61pqorn1n002nsss569635984s6qp7; ZFPhygher=VC=74.125.75.3&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669357391353591&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=; AFP_zp_kkk-gdzogv_80=4413241q3660';
  var str50 = 'FrffvbaQQS2=p98s8o9q42nr21or1r61pqorn1n002nsss569635984s6qp7; AFP_zp_kkk-gdzogv_80=4413241q3660; AFP_zp_kkk-aowb_80=4413235p3660; __hgzm=144631658.1231367708.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar); __hgzn=144631658.2770915348920628700.1231367708.1231367708.1231367708.1; __hgzo=144631658.0.10.1231367708; __hgzp=144631658; ZFPhygher=VC=74.125.75.3&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669357391353591&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str51 = 'uggc://tbbtyrnqf.t.qbhoyrpyvpx.arg/cntrnq/nqf?pyvrag=pn-svz_zlfcnpr_zlfcnpr-ubzrcntr_wf&qg=1231367691141&uy=ra&nqfnsr=uvtu&br=hgs8&ahz_nqf=4&bhgchg=wf&nqgrfg=bss&pbeeryngbe=1231367691141&punaary=svz_zlfcnpr_ubzrcntr_abgybttrqva%2Psvz_zlfcnpr_aba_HTP%2Psvz_zlfcnpr_havgrq-fgngrf&hey=uggc%3N%2S%2Sjjj.zlfcnpr.pbz%2S&nq_glcr=grkg&rvq=6083027&rn=0&sez=0&tn_ivq=320757904.1231367694&tn_fvq=1231367694&tn_uvq=1758792003&synfu=9.0.115&h_u=768&h_j=1024&h_nu=738&h_nj=1024&h_pq=24&h_gm=-480&h_uvf=2&h_wnin=gehr&h_acyht=7&h_azvzr=22';
  var str52 = 'uggc://zfacbegny.112.2b7.arg/o/ff/zfacbegnyubzr/1/U.7-cqi-2/f55332979829981?[NDO]&aqu=1&g=7%2S0%2S2009%2014%3N38%3N42%203%20480&af=zfacbegny&cntrAnzr=HF%20UCZFSGJ&t=uggc%3N%2S%2Sjjj.zfa.pbz%2S&f=1024k768&p=24&x=L&oj=994&ou=634&uc=A&{2}&[NDR]';
  var str53 = 'cnerag puebzr6 fvatyr1 gno fryrpgrq ovaq qbhoyr2 ps';
  var str54 = 'ZFPhygher=VC=74.125.75.3&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669357391353591&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str55 = 'ZFPhygher=VC=74.125.75.3&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669357391353591&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=';
  var str56 = 'ne;ng;nh;or;oe;pn;pu;py;pa;qr;qx;rf;sv;se;to;ux;vq;vr;va;vg;wc;xe;zk;zl;ay;ab;am;cu;cy;cg;eh;fr;ft;gu;ge;gj;mn;';
  var str57 = 'ZP1=I=3&THVQ=6nnpr9q661804s33nnop45nosqp17q85; zu=ZFSG; PHYGHER=RA-HF; SyvtugTebhcVq=97; SyvtugVq=OnfrCntr; ucfie=Z:5|S:5|G:5|R:5|Q:oyh|J:S; ucpyv=J.U|Y.|F.|E.|H.Y|P.|U.; hfucjrn=jp:HFPN0746; ZHVQ=Q783SN9O14054831N4869R51P0SO8886&GHVQ=1';
  var str58 = 'ZP1=I=3&THVQ=6nnpr9q661804s33nnop45nosqp17q85; zu=ZFSG; PHYGHER=RA-HF; SyvtugTebhcVq=97; SyvtugVq=OnfrCntr; ucfie=Z:5|S:5|G:5|R:5|Q:oyh|J:S; ucpyv=J.U|Y.|F.|E.|H.Y|P.|U.; hfucjrn=jp:HFPN0746; ZHVQ=Q783SN9O14054831N4869R51P0SO8886';
  var str59 = 'ZP1=I=3&THVQ=6nnpr9q661804s33nnop45nosqp17q85; zu=ZFSG; PHYGHER=RA-HF; SyvtugTebhcVq=97; SyvtugVq=OnfrCntr; ucfie=Z:5|S:5|G:5|R:5|Q:oyh|J:S; ucpyv=J.U|Y.|F.|E.|H.Y|P.|U.; hfucjrn=jp:HFPN0746; ZHVQ=Q783SN9O14054831N4869R51P0SO8886; mvc=m:94043|yn:37.4154|yb:-122.0585|p:HF|ue:1';
  var str60 = 'ZP1=I=3&THVQ=6nnpr9q661804s33nnop45nosqp17q85; zu=ZFSG; PHYGHER=RA-HF; SyvtugTebhcVq=97; SyvtugVq=OnfrCntr; ucfie=Z:5|S:5|G:5|R:5|Q:oyh|J:S; ucpyv=J.U|Y.|F.|E.|H.Y|P.|U.; hfucjrn=jp:HFPN0746; ZHVQ=Q783SN9O14054831N4869R51P0SO8886; mvc=m:94043|yn:37.4154|yb:-122.0585|p:HF';
  var str61 = 'uggc://gx2.fgp.f-zfa.pbz/oe/uc/11/ra-hf/pff/v/g.tvs#uggc://gx2.fgo.f-zfa.pbz/v/29/4RQP4969777N048NPS4RRR3PO2S7S.wct';
  var str62 = 'uggc://gx2.fgp.f-zfa.pbz/oe/uc/11/ra-hf/pff/v/g.tvs#uggc://gx2.fgo.f-zfa.pbz/v/OQ/63NP9O94NS5OQP1249Q9S1ROP7NS3.wct';
  var str63 = 'zbmvyyn/5.0 (jvaqbjf; h; jvaqbjf ag 5.1; ra-hf) nccyrjroxvg/528.9 (xugzy, yvxr trpxb) puebzr/2.0.157.0 fnsnev/528.9';
  var s94 = computeInputVariants(str42, 5);
  var s95 = computeInputVariants(str43, 5);
  var s96 = computeInputVariants(str44, 5);
  var s97 = computeInputVariants(str47, 5);
  var s98 = computeInputVariants(str48, 5);
  var s99 = computeInputVariants(str49, 5);
  var s100 = computeInputVariants(str50, 5);
  var s101 = computeInputVariants(str51, 5);
  var s102 = computeInputVariants(str52, 5);
  var s103 = computeInputVariants(str53, 5);

  function runBlock9() {
    var sum = 0;
    for (var i = 0; i < 5; i++) {
      sum += s94[i].split(re32).length;
      sum += s95[i].split(re32).length;
      sum += 'svz_zlfcnpr_hfre-ivrj-pbzzragf,svz_zlfcnpr_havgrq-fgngrf'.split(re20).length;
      sum += s96[i].replace(re33, '').length;
      sum += 'zrah_arj zrah_arj_gbttyr zrah_gbttyr'.replace(re67, '').length;
      sum += 'zrah_byq zrah_byq_gbttyr zrah_gbttyr'.replace(re67, '').length;
      sum += Exec(re8, '102n9o0o9pq60132qn0337rr867p75953502q2s27s2s5r98');
      sum += Exec(re8, '144631658.0.10.1231364380');
      sum += Exec(re8, '144631658.1231364380.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re8, '144631658.3931862196947939300.1231364380.1231364380.1231364380.1');
      sum += Exec(re8, '441326q33660');
      sum += Exec(re8, 'SbeprqRkcvengvba=633669341278771470');
      sum += Exec(re8, str45);
      sum += Exec(re8, str46);
      sum += Exec(re8, 'AFP_zp_dfctwzssrwh-aowb_80=441326q33660');
      sum += Exec(re8, 'FrffvbaQQS2=102n9o0o9pq60132qn0337rr867p75953502q2s27s2s5r98');
      sum += Exec(re8, '__hgzn=144631658.3931862196947939300.1231364380.1231364380.1231364380.1');
      sum += Exec(re8, '__hgzo=144631658.0.10.1231364380');
      sum += Exec(re8, '__hgzm=144631658.1231364380.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
    }
    for (var i = 0; i < 4; i++) {
      sum += ' yvfg1'.replace(re14, '').length;
      sum += ' yvfg1'.replace(re15, '').length;
      sum += ' yvfg2'.replace(re14, '').length;
      sum += ' yvfg2'.replace(re15, '').length;
      sum += ' frneputebhc1'.replace(re14, '').length;
      sum += ' frneputebhc1'.replace(re15, '').length;
      sum += s97[i].replace(re68, '').length;
      sum += s97[i].replace(re18, '').length;
      sum += ''.replace(/&/g, '').length;
      sum += ''.replace(re35, '').length;
      sum += '(..-{0})(\|(\d+)|)'.replace(re63, '').length;
      sum += s98[i].replace(re18, '').length;
      sum += '//vzt.jro.qr/vij/FC/${cngu}/${anzr}/${inyhr}?gf=${abj}'.replace(re56, '').length;
      sum += '//vzt.jro.qr/vij/FC/tzk_uc/${anzr}/${inyhr}?gf=${abj}'.replace(/(\$\{anzr\})|(\$anzr\b)/g, '').length;
      sum += '<fcna pynff="urnq"><o>Jvaqbjf Yvir Ubgznvy</o></fcna><fcna pynff="zft">{1}</fcna>'.replace(re69, '').length;
      sum += '<fcna pynff="urnq"><o>{0}</o></fcna><fcna pynff="zft">{1}</fcna>'.replace(re63, '').length;
      sum += '<fcna pynff="fvtahc"><n uers=uggc://jjj.ubgznvy.pbz><o>{1}</o></n></fcna>'.replace(re69, '').length;
      sum += '<fcna pynff="fvtahc"><n uers={0}><o>{1}</o></n></fcna>'.replace(re63, '').length;
      sum += 'Vzntrf'.replace(re15, '').length;
      sum += 'ZFA'.replace(re15, '').length;
      sum += 'Zncf'.replace(re15, '').length;
      sum += 'Zbq-Vasb-Vasb-WninFpevcgUvag'.replace(re39, '').length;
      sum += 'Arjf'.replace(re15, '').length;
      sum += s99[i].split(re32).length;
      sum += s100[i].split(re32).length;
      sum += 'Ivqrb'.replace(re15, '').length;
      sum += 'Jro'.replace(re15, '').length;
      sum += 'n'.replace(re39, '').length;
      sum += 'nwnkFgneg'.split(re70).length;
      sum += 'nwnkFgbc'.split(re70).length;
      sum += 'ovaq'.replace(re14, '').length;
      sum += 'ovaq'.replace(re15, '').length;
      sum += 'oevatf lbh zber. Zber fcnpr (5TO), zber frphevgl, fgvyy serr.'.replace(re63, '').length;
      sum += 'puvyq p1 svefg qrpx'.replace(re14, '').length;
      sum += 'puvyq p1 svefg qrpx'.replace(re15, '').length;
      sum += 'puvyq p1 svefg qbhoyr2'.replace(re14, '').length;
      sum += 'puvyq p1 svefg qbhoyr2'.replace(re15, '').length;
      sum += 'puvyq p2 ynfg'.replace(re14, '').length;
      sum += 'puvyq p2 ynfg'.replace(re15, '').length;
      sum += 'puvyq p2'.replace(re14, '').length;
      sum += 'puvyq p2'.replace(re15, '').length;
      sum += 'puvyq p3'.replace(re14, '').length;
      sum += 'puvyq p3'.replace(re15, '').length;
      sum += 'puvyq p4 ynfg'.replace(re14, '').length;
      sum += 'puvyq p4 ynfg'.replace(re15, '').length;
      sum += 'pbclevtug'.replace(re14, '').length;
      sum += 'pbclevtug'.replace(re15, '').length;
      sum += 'qZFAZR_1'.replace(re14, '').length;
      sum += 'qZFAZR_1'.replace(re15, '').length;
      sum += 'qbhoyr2 ps'.replace(re14, '').length;
      sum += 'qbhoyr2 ps'.replace(re15, '').length;
      sum += 'qbhoyr2'.replace(re14, '').length;
      sum += 'qbhoyr2'.replace(re15, '').length;
      sum += 'uqy_arj'.replace(re14, '').length;
      sum += 'uqy_arj'.replace(re15, '').length;
      sum += 'uc_fubccvatobk'.replace(re30, '').length;
      sum += 'ugzy%2Rvq'.replace(re29, '').length;
      sum += 'ugzy%2Rvq'.replace(re30, '').length;
      sum += s101[i].replace(re33, '').length;
      sum += 'uggc://wf.hv-cbegny.qr/tzk/ubzr/wf/20080602/cebgbglcr.wf${4}${5}'.replace(re71, '').length;
      sum += 'uggc://wf.hv-cbegny.qr/tzk/ubzr/wf/20080602/cebgbglcr.wf${5}'.replace(re72, '').length;
      sum += s102[i].replace(re73, '').length;
      sum += 'uggc://zfacbegny.112.2b7.arg/o/ff/zfacbegnyubzr/1/U.7-cqi-2/f55332979829981?[NDO]&{1}&{2}&[NDR]'.replace(re69, '').length;
      sum += 'vztZFSG'.replace(re14, '').length;
      sum += 'vztZFSG'.replace(re15, '').length;
      sum += 'zfasbbg1 ps'.replace(re14, '').length;
      sum += 'zfasbbg1 ps'.replace(re15, '').length;
      sum += s103[i].replace(re14, '').length;
      sum += s103[i].replace(re15, '').length;
      sum += 'cnerag puebzr6 fvatyr1 gno fryrpgrq ovaq'.replace(re14, '').length;
      sum += 'cnerag puebzr6 fvatyr1 gno fryrpgrq ovaq'.replace(re15, '').length;
      sum += 'cevznel'.replace(re14, '').length;
      sum += 'cevznel'.replace(re15, '').length;
      sum += 'erpgnatyr'.replace(re30, '').length;
      sum += 'frpbaqnel'.replace(re14, '').length;
      sum += 'frpbaqnel'.replace(re15, '').length;
      sum += 'haybnq'.split(re70).length;
      sum += '{0}{1}1'.replace(re63, '').length;
      sum += '|{1}1'.replace(re69, '').length;
      sum += Exec(/(..-HF)(\|(\d+)|)/i, 'xb-xe,ra-va,gu-gu');
      sum += Exec(re4, '/ZlFcnprNccf/NccPnainf,45000012');
      sum += Exec(re8, '144631658.0.10.1231367708');
      sum += Exec(re8, '144631658.1231367708.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re8, '144631658.2770915348920628700.1231367708.1231367708.1231367708.1');
      sum += Exec(re8, '4413235p3660');
      sum += Exec(re8, '441327q73660');
      sum += Exec(re8, '9995p6rp12rrnr893334ro7nq70o7p64p69rqn844prs1473');
      sum += Exec(re8, 'SbeprqRkcvengvba=633669350559478880');
      sum += Exec(re8, str54);
      sum += Exec(re8, str55);
      sum += Exec(re8, 'AFP_zp_dfctwzs-aowb_80=441327q73660');
      sum += Exec(re8, 'AFP_zp_kkk-aowb_80=4413235p3660');
      sum += Exec(re8, 'FrffvbaQQS2=9995p6rp12rrnr893334ro7nq70o7p64p69rqn844prs1473');
      sum += Exec(re8, '__hgzn=144631658.2770915348920628700.1231367708.1231367708.1231367708.1');
      sum += Exec(re8, '__hgzo=144631658.0.10.1231367708');
      sum += Exec(re8, '__hgzm=144631658.1231367708.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re34, s99[i]);
      sum += Exec(re34, s100[i]);
      sum += Exec(/ZFVR\s+5[.]01/, s15[i]);
      sum += Exec(/HF(?=;)/i, str56);
      sum += Exec(re74, s97[i]);
      sum += Exec(re28, 'svefg npgvir svefgNpgvir');
      sum += Exec(re28, 'ynfg');
      sum += Exec(/\bp:(..)/i, 'm:94043|yn:37.4154|yb:-122.0585|p:HF');
      sum += Exec(re75, str57);
      sum += Exec(re75, str58);
      sum += Exec(re76, str57);
      sum += Exec(re76, str58);
      sum += Exec(re77, str57);
      sum += Exec(re77, str58);
      sum += Exec(/\bhfucce\s*=\s*([^;]*)/i, str59);
      sum += Exec(re78, str57);
      sum += Exec(re78, str58);
      sum += Exec(/\bjci\s*=\s*([^;]*)/i, str59);
      sum += Exec(re79, str58);
      sum += Exec(re79, str60);
      sum += Exec(re79, str59);
      sum += Exec(/\|p:([a-z]{2})/i, 'm:94043|yn:37.4154|yb:-122.0585|p:HF|ue:1');
      sum += Exec(re80, s97[i]);
      sum += Exec(re61, 'cebgbglcr.wf');
      sum += Exec(re68, s97[i]);
      sum += Exec(re81, s97[i]);
      sum += Exec(re82, s97[i]);
      sum += Exec(/^Fubpxjnir Synfu (\d)/, s21[i]);
      sum += Exec(/^Fubpxjnir Synfu (\d+)/, s21[i]);
      sum += Exec(re83, '[bowrpg tybony]');
      sum += Exec(re62, s97[i]);
      sum += Exec(re84, str61);
      sum += Exec(re84, str62);
      sum += Exec(/jroxvg/, str63);
    }
    return sum;
  }
  var re85 = /eaq_zbqobkva/;
  var str64 = '1231365729213';
  var str65 = '74.125.75.3-1057165600.29978900';
  var str66 = '74.125.75.3-1057165600.29978900.1231365730214';
  var str67 = 'Frnepu%20Zvpebfbsg.pbz';
  var str68 = 'FrffvbaQQS2=8sqq78r9n442851q565599o401385sp3s04r92rnn7o19ssn; ZFPhygher=VC=74.125.75.17&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669340386893867&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=';
  var str69 = 'FrffvbaQQS2=8sqq78r9n442851q565599o401385sp3s04r92rnn7o19ssn; __hgzm=144631658.1231365779.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar); __hgzn=144631658.1877536177953918500.1231365779.1231365779.1231365779.1; __hgzo=144631658.0.10.1231365779; __hgzp=144631658; ZFPhygher=VC=74.125.75.17&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669340386893867&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str70 = 'I=3%26THVQ=757q3ss871q44o7o805n8113n5p72q52';
  var str71 = 'I=3&THVQ=757q3ss871q44o7o805n8113n5p72q52';
  var str72 = 'uggc://tbbtyrnqf.t.qbhoyrpyvpx.arg/cntrnq/nqf?pyvrag=pn-svz_zlfcnpr_zlfcnpr-ubzrcntr_wf&qg=1231365765292&uy=ra&nqfnsr=uvtu&br=hgs8&ahz_nqf=4&bhgchg=wf&nqgrfg=bss&pbeeryngbe=1231365765292&punaary=svz_zlfcnpr_ubzrcntr_abgybttrqva%2Psvz_zlfcnpr_aba_HTP%2Psvz_zlfcnpr_havgrq-fgngrf&hey=uggc%3N%2S%2Sohyyrgvaf.zlfcnpr.pbz%2Svaqrk.psz&nq_glcr=grkg&rvq=6083027&rn=0&sez=0&tn_ivq=1579793869.1231365768&tn_fvq=1231365768&tn_uvq=2056210897&synfu=9.0.115&h_u=768&h_j=1024&h_nu=738&h_nj=1024&h_pq=24&h_gm=-480&h_uvf=2&h_wnin=gehr&h_acyht=7&h_azvzr=22';
  var str73 = 'frnepu.zvpebfbsg.pbz';
  var str74 = 'frnepu.zvpebfbsg.pbz/';
  var str75 = 'ZFPhygher=VC=74.125.75.17&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669340386893867&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str76 = 'ZFPhygher=VC=74.125.75.17&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669340386893867&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=';
  function runBlock10() {
    var sum = 0;
    for (var i = 0; i < 3; i++) {
      sum += '%3Szxg=ra-HF'.replace(re39, '').length;
      sum += '-8'.replace(re40, '').length;
      sum += '-8'.replace(re10, '').length;
      sum += '-8'.replace(re51, '').length;
      sum += '-8'.replace(re52, '').length;
      sum += '-8'.replace(re53, '').length;
      sum += '-8'.replace(re39, '').length;
      sum += '-8'.replace(re54, '').length;
      sum += '1.5'.replace(re40, '').length;
      sum += '1.5'.replace(re10, '').length;
      sum += '1.5'.replace(re51, '').length;
      sum += '1.5'.replace(re52, '').length;
      sum += '1.5'.replace(re53, '').length;
      sum += '1.5'.replace(re39, '').length;
      sum += '1.5'.replace(re54, '').length;
      sum += '1024k768'.replace(re40, '').length;
      sum += '1024k768'.replace(re10, '').length;
      sum += '1024k768'.replace(re51, '').length;
      sum += '1024k768'.replace(re52, '').length;
      sum += '1024k768'.replace(re53, '').length;
      sum += '1024k768'.replace(re39, '').length;
      sum += '1024k768'.replace(re54, '').length;
      sum += str64.replace(re40, '').length;
      sum += str64.replace(re10, '').length;
      sum += str64.replace(re51, '').length;
      sum += str64.replace(re52, '').length;
      sum += str64.replace(re53, '').length;
      sum += str64.replace(re39, '').length;
      sum += str64.replace(re54, '').length;
      sum += '14'.replace(re40, '').length;
      sum += '14'.replace(re10, '').length;
      sum += '14'.replace(re51, '').length;
      sum += '14'.replace(re52, '').length;
      sum += '14'.replace(re53, '').length;
      sum += '14'.replace(re39, '').length;
      sum += '14'.replace(re54, '').length;
      sum += '24'.replace(re40, '').length;
      sum += '24'.replace(re10, '').length;
      sum += '24'.replace(re51, '').length;
      sum += '24'.replace(re52, '').length;
      sum += '24'.replace(re53, '').length;
      sum += '24'.replace(re39, '').length;
      sum += '24'.replace(re54, '').length;
      sum += str65.replace(re40, '').length;
      sum += str65.replace(re10, '').length;
      sum += str65.replace(re51, '').length;
      sum += str65.replace(re52, '').length;
      sum += str65.replace(re53, '').length;
      sum += str65.replace(re39, '').length;
      sum += str65.replace(re54, '').length;
      sum += str66.replace(re40, '').length;
      sum += str66.replace(re10, '').length;
      sum += str66.replace(re51, '').length;
      sum += str66.replace(re52, '').length;
      sum += str66.replace(re53, '').length;
      sum += str66.replace(re39, '').length;
      sum += str66.replace(re54, '').length;
      sum += '9.0'.replace(re40, '').length;
      sum += '9.0'.replace(re10, '').length;
      sum += '9.0'.replace(re51, '').length;
      sum += '9.0'.replace(re52, '').length;
      sum += '9.0'.replace(re53, '').length;
      sum += '9.0'.replace(re39, '').length;
      sum += '9.0'.replace(re54, '').length;
      sum += '994k634'.replace(re40, '').length;
      sum += '994k634'.replace(re10, '').length;
      sum += '994k634'.replace(re51, '').length;
      sum += '994k634'.replace(re52, '').length;
      sum += '994k634'.replace(re53, '').length;
      sum += '994k634'.replace(re39, '').length;
      sum += '994k634'.replace(re54, '').length;
      sum += '?zxg=ra-HF'.replace(re40, '').length;
      sum += '?zxg=ra-HF'.replace(re10, '').length;
      sum += '?zxg=ra-HF'.replace(re51, '').length;
      sum += '?zxg=ra-HF'.replace(re52, '').length;
      sum += '?zxg=ra-HF'.replace(re53, '').length;
      sum += '?zxg=ra-HF'.replace(re54, '').length;
      sum += 'PAA.pbz'.replace(re25, '').length;
      sum += 'PAA.pbz'.replace(re12, '').length;
      sum += 'PAA.pbz'.replace(re39, '').length;
      sum += 'Qngr & Gvzr'.replace(re25, '').length;
      sum += 'Qngr & Gvzr'.replace(re12, '').length;
      sum += 'Qngr & Gvzr'.replace(re39, '').length;
      sum += 'Frnepu Zvpebfbsg.pbz'.replace(re40, '').length;
      sum += 'Frnepu Zvpebfbsg.pbz'.replace(re54, '').length;
      sum += str67.replace(re10, '').length;
      sum += str67.replace(re51, '').length;
      sum += str67.replace(re52, '').length;
      sum += str67.replace(re53, '').length;
      sum += str67.replace(re39, '').length;
      sum += str68.split(re32).length;
      sum += str69.split(re32).length;
      sum += str70.replace(re52, '').length;
      sum += str70.replace(re53, '').length;
      sum += str70.replace(re39, '').length;
      sum += str71.replace(re40, '').length;
      sum += str71.replace(re10, '').length;
      sum += str71.replace(re51, '').length;
      sum += str71.replace(re54, '').length;
      sum += 'Jrngure'.replace(re25, '').length;
      sum += 'Jrngure'.replace(re12, '').length;
      sum += 'Jrngure'.replace(re39, '').length;
      sum += 'LbhGhor'.replace(re25, '').length;
      sum += 'LbhGhor'.replace(re12, '').length;
      sum += 'LbhGhor'.replace(re39, '').length;
      sum += str72.replace(re33, '').length;
      sum += 'erzbgr_vsenzr_1'.replace(/^erzbgr_vsenzr_/, '').length;
      sum += str73.replace(re40, '').length;
      sum += str73.replace(re10, '').length;
      sum += str73.replace(re51, '').length;
      sum += str73.replace(re52, '').length;
      sum += str73.replace(re53, '').length;
      sum += str73.replace(re39, '').length;
      sum += str73.replace(re54, '').length;
      sum += str74.replace(re40, '').length;
      sum += str74.replace(re10, '').length;
      sum += str74.replace(re51, '').length;
      sum += str74.replace(re52, '').length;
      sum += str74.replace(re53, '').length;
      sum += str74.replace(re39, '').length;
      sum += str74.replace(re54, '').length;
      sum += 'lhv-h'.replace(/\-/g, '').length;
      sum += Exec(re9, 'p');
      sum += Exec(re9, 'qz p');
      sum += Exec(re9, 'zbqynory');
      sum += Exec(re9, 'lhv-h svefg');
      sum += Exec(re8, '144631658.0.10.1231365779');
      sum += Exec(re8, '144631658.1231365779.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re8, '144631658.1877536177953918500.1231365779.1231365779.1231365779.1');
      sum += Exec(re8, str75);
      sum += Exec(re8, str76);
      sum += Exec(re8, '__hgzn=144631658.1877536177953918500.1231365779.1231365779.1231365779.1');
      sum += Exec(re8, '__hgzo=144631658.0.10.1231365779');
      sum += Exec(re8, '__hgzm=144631658.1231365779.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re34, str68);
      sum += Exec(re34, str69);
      sum += Exec(/^$/, '');
      sum += Exec(re31, 'qr');
      sum += Exec(/^znk\d+$/, '');
      sum += Exec(/^zva\d+$/, '');
      sum += Exec(/^erfgber$/, '');
      sum += Exec(re85, 'zbqobkva zbqobk_abcnqqvat ');
      sum += Exec(re85, 'zbqgvgyr');
      sum += Exec(re85, 'eaq_zbqobkva ');
      sum += Exec(re85, 'eaq_zbqgvgyr ');
      sum += Exec(/frpgvba\d+_pbagragf/, 'obggbz_ani');
    }
    return sum;
  }
  var re86 = /;\s*/;
  var re87 = /(\$\{inyhr\})|(\$inyhr\b)/g;
  var re88 = /(\$\{abj\})|(\$abj\b)/g;
  var re89 = /\s+$/;
  var re90 = /^\s+/;
  var re91 = /(\\\"|\x00-|\x1f|\x7f-|\x9f|\u00ad|\u0600-|\u0604|\u070f|\u17b4|\u17b5|\u200c-|\u200f|\u2028-|\u202f|\u2060-|\u206f|\ufeff|\ufff0-|\uffff)/g;
  var re92 = /^(:)([\w-]+)\("?'?(.*?(\(.*?\))?[^(]*?)"?'?\)/;
  var re93 = /^([:.#]*)((?:[\w\u0128-\uffff*_-]|\\.)+)/;
  var re94 = /^(\[) *@?([\w-]+) *([!*$^~=]*) *('?"?)(.*?)\4 *\]/;
  var str77 = '#fubhgobk .pybfr';
  var str78 = 'FrffvbaQQS2=102n9o0o9pq60132qn0337rr867p75953502q2s27s2s5r98; ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669341278771470&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=; AFP_zp_dfctwzssrwh-aowb_80=441326q33660';
  var str79 = 'FrffvbaQQS2=102n9o0o9pq60132qn0337rr867p75953502q2s27s2s5r98; AFP_zp_dfctwzssrwh-aowb_80=441326q33660; __hgzm=144631658.1231365869.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar); __hgzn=144631658.1670816052019209000.1231365869.1231365869.1231365869.1; __hgzo=144631658.0.10.1231365869; __hgzp=144631658; ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669341278771470&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str80 = 'FrffvbaQQS2=9995p6rp12rrnr893334ro7nq70o7p64p69rqn844prs1473; ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669350559478880&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=; AFP_zp_dfctwzs-aowb_80=441327q73660';
  var str81 = 'FrffvbaQQS2=9995p6rp12rrnr893334ro7nq70o7p64p69rqn844prs1473; AFP_zp_dfctwzs-aowb_80=441327q73660; __hgzm=144631658.1231367054.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar); __hgzn=144631658.1796080716621419500.1231367054.1231367054.1231367054.1; __hgzo=144631658.0.10.1231367054; __hgzp=144631658; ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669350559478880&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str82 = '[glcr=fhozvg]';
  var str83 = 'n.svryqOga,n.svryqOgaPnapry';
  var str84 = 'n.svryqOgaPnapry';
  var str85 = 'oyvpxchaxg';
  var str86 = 'qvi.bow-nppbeqvba qg';
  var str87 = 'uggc://tbbtyrnqf.t.qbhoyrpyvpx.arg/cntrnq/nqf?pyvrag=pn-svz_zlfcnpr_nccf_wf&qg=1231367052227&uy=ra&nqfnsr=uvtu&br=hgs8&ahz_nqf=4&bhgchg=wf&nqgrfg=bss&pbeeryngbe=1231367052227&punaary=svz_zlfcnpr_nccf-pnainf%2Psvz_zlfcnpr_havgrq-fgngrf&hey=uggc%3N%2S%2Scebsvyr.zlfcnpr.pbz%2SZbqhyrf%2SNccyvpngvbaf%2SCntrf%2SPnainf.nfck&nq_glcr=grkg&rvq=6083027&rn=0&sez=1&tn_ivq=716357910.1231367056&tn_fvq=1231367056&tn_uvq=1387206491&synfu=9.0.115&h_u=768&h_j=1024&h_nu=738&h_nj=1024&h_pq=24&h_gm=-480&h_uvf=2&h_wnin=gehr&h_acyht=7&h_azvzr=22';
  var str88 = 'uggc://tbbtyrnqf.t.qbhoyrpyvpx.arg/cntrnq/nqf?pyvrag=pn-svz_zlfcnpr_zlfcnpr-ubzrcntr_wf&qg=1231365851658&uy=ra&nqfnsr=uvtu&br=hgs8&ahz_nqf=4&bhgchg=wf&nqgrfg=bss&pbeeryngbe=1231365851658&punaary=svz_zlfcnpr_ubzrcntr_abgybttrqva%2Psvz_zlfcnpr_aba_HTP%2Psvz_zlfcnpr_havgrq-fgngrf&hey=uggc%3N%2S%2Scebsvyrrqvg.zlfcnpr.pbz%2Svaqrk.psz&nq_glcr=grkg&rvq=6083027&rn=0&sez=0&tn_ivq=1979828129.1231365855&tn_fvq=1231365855&tn_uvq=2085229649&synfu=9.0.115&h_u=768&h_j=1024&h_nu=738&h_nj=1024&h_pq=24&h_gm=-480&h_uvf=2&h_wnin=gehr&h_acyht=7&h_azvzr=22';
  var str89 = 'uggc://zfacbegny.112.2b7.arg/o/ff/zfacbegnyubzr/1/U.7-cqi-2/f55023338617756?[NDO]&aqu=1&g=7%2S0%2S2009%2014%3N12%3N47%203%20480&af=zfacbegny&cntrAnzr=HF%20UCZFSGJ&t=uggc%3N%2S%2Sjjj.zfa.pbz%2S&f=0k0&p=43835816&x=A&oj=994&ou=634&uc=A&{2}&[NDR]';
  var str90 = 'zrgn[anzr=nwnkHey]';
  var str91 = 'anpuevpugra';
  var str92 = 'b oS={\'oT\':1.1};x $8n(B){z(B!=o9)};x $S(B){O(!$8n(B))z A;O(B.4L)z\'T\';b S=7t B;O(S==\'2P\'&&B.p4){23(B.7f){12 1:z\'T\';12 3:z/\S/.2g(B.8M)?\'ox\':\'oh\'}}O(S==\'2P\'||S==\'x\'){23(B.nE){12 2V:z\'1O\';12 7I:z\'5a\';12 18:z\'4B\'}O(7t B.I==\'4F\'){O(B.3u)z\'pG\';O(B.8e)z\'1p\'}}z S};x $2p(){b 4E={};Z(b v=0;v<1p.I;v++){Z(b X 1o 1p[v]){b nc=1p[v][X];b 6E=4E[X];O(6E&&$S(nc)==\'2P\'&&$S(6E)==\'2P\')4E[X]=$2p(6E,nc);17 4E[X]=nc}}z 4E};b $E=7p.E=x(){b 1d=1p;O(!1d[1])1d=[p,1d[0]];Z(b X 1o 1d[1])1d[0][X]=1d[1][X];z 1d[0]};b $4D=7p.pJ=x(){Z(b v=0,y=1p.I;v<y;v++){1p[v].E=x(1J){Z(b 1I 1o 1J){O(!p.1Y[1I])p.1Y[1I]=1J[1I];O(!p[1I])p[1I]=$4D.6C(1I)}}}};$4D.6C=x(1I){z x(L){z p.1Y[1I].3H(L,2V.1Y.nV.1F(1p,1))}};$4D(7F,2V,6J,nb);b 3l=x(B){B=B||{};B.E=$E;z B};b pK=Y 3l(H);b pZ=Y 3l(C);C.6f=C.35(\'6f\')[0];x $2O(B){z!!(B||B===0)};x $5S(B,n8){z $8n(B)?B:n8};x $7K(3c,1m){z 1q.na(1q.7K()*(1m-3c+1)+3c)};x $3N(){z Y 97().os()};x $4M(1U){pv(1U);pa(1U);z 1S};H.43=!!(C.5Z);O(H.nB)H.31=H[H.7q?\'py\':\'nL\']=1r;17 O(C.9N&&!C.om&&!oy.oZ)H.pF=H.4Z=H[H.43?\'pt\':\'65\']=1r;17 O(C.po!=1S)H.7J=1r;O(7t 5B==\'o9\'){b 5B=x(){};O(H.4Z)C.nd("pW");5B.1Y=(H.4Z)?H["[[oN.1Y]]"]:{}}5B.1Y.4L=1r;O(H.nL)5s{C.oX("pp",A,1r)}4K(r){};b 18=x(1X){b 63=x(){z(1p[0]!==1S&&p.1w&&$S(p.1w)==\'x\')?p.1w.3H(p,1p):p};$E(63,p);63.1Y=1X;63.nE=18;z 63};18.1z=x(){};18.1Y={E:x(1X){b 7x=Y p(1S);Z(b X 1o 1X){b nC=7x[X];7x[X]=18.nY(nC,1X[X])}z Y 18(7x)},3d:x(){Z(b v=0,y=1p.I;v<y;v++)$E(p.1Y,1p[v])}};18.nY=x(2b,2n){O(2b&&2b!=2n){b S=$S(2n);O(S!=$S(2b))z 2n;23(S){12\'x\':b 7R=x(){p.1e=1p.8e.1e;z 2n.3H(p,1p)};7R.1e=2b;z 7R;12\'2P\':z $2p(2b,2n)}}z 2n};b 8o=Y 18({oQ:x(J){p.4w=p.4w||[];p.4w.1x(J);z p},7g:x(){O(p.4w&&p.4w.I)p.4w.9J().2x(10,p)},oP:x(){p.4w=[]}});b 2d=Y 18({1V:x(S,J){O(J!=18.1z){p.$19=p.$19||{};p.$19[S]=p.$19[S]||[];p.$19[S].5j(J)}z p},1v:x(S,1d,2x){O(p.$19&&p.$19[S]){p.$19[S].1b(x(J){J.3n({\'L\':p,\'2x\':2x,\'1p\':1d})()},p)}z p},3M:x(S,J){O(p.$19&&p.$19[S])p.$19[S].2U(J);z p}});b 4v=Y 18({2H:x(){p.P=$2p.3H(1S,[p.P].E(1p));O(!p.1V)z p;Z(b 3O 1o p.P){O($S(p.P[3O]==\'x\')&&3O.2g(/^5P[N-M]/))p.1V(3O,p.P[3O])}z p}});2V.E({7y:x(J,L){Z(b v=0,w=p.I;v<w;v++)J.1F(L,p[v],v,p)},3s:x(J,L){b 54=[];Z(b v=0,w=p.I;v<w;v++){O(J.1F(L,p[v],v,p))54.1x(p[v])}z 54},2X:x(J,L){b 54=[];Z(b v=0,w=p.I;v<w;v++)54[v]=J.1F(L,p[v],v,p);z 54},4i:x(J,L){Z(b v=0,w=p.I;v<w;v++){O(!J.1F(L,p[v],v,p))z A}z 1r},ob:x(J,L){Z(b v=0,w=p.I;v<w;v++){O(J.1F(L,p[v],v,p))z 1r}z A},3F:x(3u,15){b 3A=p.I;Z(b v=(15<0)?1q.1m(0,3A+15):15||0;v<3A;v++){O(p[v]===3u)z v}z-1},8z:x(1u,I){1u=1u||0;O(1u<0)1u=p.I+1u;I=I||(p.I-1u);b 89=[];Z(b v=0;v<I;v++)89[v]=p[1u++];z 89},2U:x(3u){b v=0;b 3A=p.I;6L(v<3A){O(p[v]===3u){p.6l(v,1);3A--}17{v++}}z p},1y:x(3u,15){z p.3F(3u,15)!=-1},oz:x(1C){b B={},I=1q.3c(p.I,1C.I);Z(b v=0;v<I;v++)B[1C[v]]=p[v];z B},E:x(1O){Z(b v=0,w=1O.I;v<w;v++)p.1x(1O[v]);z p},2p:x(1O){Z(b v=0,y=1O.I;v<y;v++)p.5j(1O[v]);z p},5j:x(3u){O(!p.1y(3u))p.1x(3u);z p},oc:x(){z p[$7K(0,p.I-1)]||A},7L:x(){z p[p.I-1]||A}});2V.1Y.1b=2V.1Y.7y;2V.1Y.2g=2V.1Y.1y;x $N(1O){z 2V.8z(1O)};x $1b(3J,J,L){O(3J&&7t 3J.I==\'4F\'&&$S(3J)!=\'2P\')2V.7y(3J,J,L);17 Z(b 1j 1o 3J)J.1F(L||3J,3J[1j],1j)};6J.E({2g:x(6b,2F){z(($S(6b)==\'2R\')?Y 7I(6b,2F):6b).2g(p)},3p:x(){z 5K(p,10)},o4:x(){z 69(p)},7A:x(){z p.3y(/-\D/t,x(2G){z 2G.7G(1).nW()})},9b:x(){z p.3y(/\w[N-M]/t,x(2G){z(2G.7G(0)+\'-\'+2G.7G(1).5O())})},8V:x(){z p.3y(/\b[n-m]/t,x(2G){z 2G.nW()})},5L:x(){z p.3y(/^\s+|\s+$/t,\'\')},7j:x(){z p.3y(/\s{2,}/t,\' \').5L()},5V:x(1O){b 1i=p.2G(/\d{1,3}/t);z(1i)?1i.5V(1O):A},5U:x(1O){b 3P=p.2G(/^#?(\w{1,2})(\w{1,2})(\w{1,2})$/);z(3P)?3P.nV(1).5U(1O):A},1y:x(2R,f){z(f)?(f+p+f).3F(f+2R+f)>-1:p.3F(2R)>-1},nX:x(){z p.3y(/([.*+?^${}()|[\]\/\\])/t,\'\\$1\')}});2V.E({5V:x(1O){O(p.I<3)z A;O(p.I==4&&p[3]==0&&!1O)z\'p5\';b 3P=[];Z(b v=0;v<3;v++){b 52=(p[v]-0).4h(16);3P.1x((52.I==1)?\'0\'+52:52)}z 1O?3P:\'#\'+3P.2u(\'\')},5U:x(1O){O(p.I!=3)z A;b 1i=[];Z(b v=0;v<3;v++){1i.1x(5K((p[v].I==1)?p[v]+p[v]:p[v],16))}z 1O?1i:\'1i(\'+1i.2u(\',\')+\')\'}});7F.E({3n:x(P){b J=p;P=$2p({\'L\':J,\'V\':A,\'1p\':1S,\'2x\':A,\'4s\':A,\'6W\':A},P);O($2O(P.1p)&&$S(P.1p)!=\'1O\')P.1p=[P.1p];z x(V){b 1d;O(P.V){V=V||H.V;1d=[(P.V===1r)?V:Y P.V(V)];O(P.1p)1d.E(P.1p)}17 1d=P.1p||1p;b 3C=x(){z J.3H($5S(P';
  var str93 = 'hagreunyghat';
  var str94 = 'ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669341278771470&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str95 = 'ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&Pbhagel=IIZ%3Q&SbeprqRkcvengvba=633669350559478880&gvzrMbar=-8&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R%3Q';
  var str96 = 'ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669341278771470&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=';
  var str97 = 'ZFPhygher=VC=74.125.75.1&VCPhygher=ra-HF&CersreerqPhygher=ra-HF&CersreerqPhygherCraqvat=&Pbhagel=IIZ=&SbeprqRkcvengvba=633669350559478880&gvzrMbar=0&HFEYBP=DKWyLHAiMTH9AwHjWxAcqUx9GJ91oaEunJ4tIzyyqlMQo3IhqUW5D29xMG1IHlMQo3IhqUW5GzSgMG1Iozy0MJDtH3EuqTImWxEgLHAiMTH9BQN3WxkuqTy0qJEyCGZ3YwDkBGVzGT9hM2y0qJEyCF0kZwVhZQH3APMDo3A0LJkQo2EyCGx0ZQDmWyWyM2yiox5uoJH9D0R=';
  var str98 = 'shapgvba (){Cuk.Nccyvpngvba.Frghc.Pber();Cuk.Nccyvpngvba.Frghc.Nwnk();Cuk.Nccyvpngvba.Frghc.Synfu();Cuk.Nccyvpngvba.Frghc.Zbqhyrf()}';
  function runBlock11() {
    var sum = 0;
    for (var i = 0; i < 2; i++) {
      sum += ' .pybfr'.replace(re18, '').length;
      sum += ' n.svryqOgaPnapry'.replace(re18, '').length;
      sum += ' qg'.replace(re18, '').length;
      sum += str77.replace(re68, '').length;
      sum += str77.replace(re18, '').length;
      sum += ''.replace(re39, '').length;
      sum += ''.replace(/^/, '').length;
      sum += ''.split(re86).length;
      sum += '*'.replace(re39, '').length;
      sum += '*'.replace(re68, '').length;
      sum += '*'.replace(re18, '').length;
      sum += '.pybfr'.replace(re68, '').length;
      sum += '.pybfr'.replace(re18, '').length;
      sum += '//vzt.jro.qr/vij/FC/tzk_uc/fperra/${inyhr}?gf=${abj}'.replace(re87, '').length;
      sum += '//vzt.jro.qr/vij/FC/tzk_uc/fperra/1024?gf=${abj}'.replace(re88, '').length;
      sum += '//vzt.jro.qr/vij/FC/tzk_uc/jvafvmr/${inyhr}?gf=${abj}'.replace(re87, '').length;
      sum += '//vzt.jro.qr/vij/FC/tzk_uc/jvafvmr/992/608?gf=${abj}'.replace(re88, '').length;
      sum += '300k120'.replace(re30, '').length;
      sum += '300k250'.replace(re30, '').length;
      sum += '310k120'.replace(re30, '').length;
      sum += '310k170'.replace(re30, '').length;
      sum += '310k250'.replace(re30, '').length;
      sum += '9.0  e115'.replace(/^.*\.(.*)\s.*$/, '').length;
      sum += 'Nppbeqvba'.replace(re2, '').length;
      sum += 'Nxghryy\x0a'.replace(re89, '').length;
      sum += 'Nxghryy\x0a'.replace(re90, '').length;
      sum += 'Nccyvpngvba'.replace(re2, '').length;
      sum += 'Oyvpxchaxg\x0a'.replace(re89, '').length;
      sum += 'Oyvpxchaxg\x0a'.replace(re90, '').length;
      sum += 'Svanamra\x0a'.replace(re89, '').length;
      sum += 'Svanamra\x0a'.replace(re90, '').length;
      sum += 'Tnzrf\x0a'.replace(re89, '').length;
      sum += 'Tnzrf\x0a'.replace(re90, '').length;
      sum += 'Ubebfxbc\x0a'.replace(re89, '').length;
      sum += 'Ubebfxbc\x0a'.replace(re90, '').length;
      sum += 'Xvab\x0a'.replace(re89, '').length;
      sum += 'Xvab\x0a'.replace(re90, '').length;
      sum += 'Zbqhyrf'.replace(re2, '').length;
      sum += 'Zhfvx\x0a'.replace(re89, '').length;
      sum += 'Zhfvx\x0a'.replace(re90, '').length;
      sum += 'Anpuevpugra\x0a'.replace(re89, '').length;
      sum += 'Anpuevpugra\x0a'.replace(re90, '').length;
      sum += 'Cuk'.replace(re2, '').length;
      sum += 'ErdhrfgSvavfu'.split(re70).length;
      sum += 'ErdhrfgSvavfu.NWNK.Cuk'.split(re70).length;
      sum += 'Ebhgr\x0a'.replace(re89, '').length;
      sum += 'Ebhgr\x0a'.replace(re90, '').length;
      sum += str78.split(re32).length;
      sum += str79.split(re32).length;
      sum += str80.split(re32).length;
      sum += str81.split(re32).length;
      sum += 'Fcbeg\x0a'.replace(re89, '').length;
      sum += 'Fcbeg\x0a'.replace(re90, '').length;
      sum += 'GI-Fcbg\x0a'.replace(re89, '').length;
      sum += 'GI-Fcbg\x0a'.replace(re90, '').length;
      sum += 'Gbhe\x0a'.replace(re89, '').length;
      sum += 'Gbhe\x0a'.replace(re90, '').length;
      sum += 'Hagreunyghat\x0a'.replace(re89, '').length;
      sum += 'Hagreunyghat\x0a'.replace(re90, '').length;
      sum += 'Ivqrb\x0a'.replace(re89, '').length;
      sum += 'Ivqrb\x0a'.replace(re90, '').length;
      sum += 'Jrggre\x0a'.replace(re89, '').length;
      sum += 'Jrggre\x0a'.replace(re90, '').length;
      sum += str82.replace(re68, '').length;
      sum += str82.replace(re18, '').length;
      sum += str83.replace(re68, '').length;
      sum += str83.replace(re18, '').length;
      sum += str84.replace(re68, '').length;
      sum += str84.replace(re18, '').length;
      sum += 'nqiFreivprObk'.replace(re30, '').length;
      sum += 'nqiFubccvatObk'.replace(re30, '').length;
      sum += 'nwnk'.replace(re39, '').length;
      sum += 'nxghryy'.replace(re40, '').length;
      sum += 'nxghryy'.replace(re41, '').length;
      sum += 'nxghryy'.replace(re42, '').length;
      sum += 'nxghryy'.replace(re43, '').length;
      sum += 'nxghryy'.replace(re44, '').length;
      sum += 'nxghryy'.replace(re45, '').length;
      sum += 'nxghryy'.replace(re46, '').length;
      sum += 'nxghryy'.replace(re47, '').length;
      sum += 'nxghryy'.replace(re48, '').length;
      sum += str85.replace(re40, '').length;
      sum += str85.replace(re41, '').length;
      sum += str85.replace(re42, '').length;
      sum += str85.replace(re43, '').length;
      sum += str85.replace(re44, '').length;
      sum += str85.replace(re45, '').length;
      sum += str85.replace(re46, '').length;
      sum += str85.replace(re47, '').length;
      sum += str85.replace(re48, '').length;
      sum += 'pngrtbel'.replace(re29, '').length;
      sum += 'pngrtbel'.replace(re30, '').length;
      sum += 'pybfr'.replace(re39, '').length;
      sum += 'qvi'.replace(re39, '').length;
      sum += str86.replace(re68, '').length;
      sum += str86.replace(re18, '').length;
      sum += 'qg'.replace(re39, '').length;
      sum += 'qg'.replace(re68, '').length;
      sum += 'qg'.replace(re18, '').length;
      sum += 'rzorq'.replace(re39, '').length;
      sum += 'rzorq'.replace(re68, '').length;
      sum += 'rzorq'.replace(re18, '').length;
      sum += 'svryqOga'.replace(re39, '').length;
      sum += 'svryqOgaPnapry'.replace(re39, '').length;
      sum += 'svz_zlfcnpr_nccf-pnainf,svz_zlfcnpr_havgrq-fgngrf'.split(re20).length;
      sum += 'svanamra'.replace(re40, '').length;
      sum += 'svanamra'.replace(re41, '').length;
      sum += 'svanamra'.replace(re42, '').length;
      sum += 'svanamra'.replace(re43, '').length;
      sum += 'svanamra'.replace(re44, '').length;
      sum += 'svanamra'.replace(re45, '').length;
      sum += 'svanamra'.replace(re46, '').length;
      sum += 'svanamra'.replace(re47, '').length;
      sum += 'svanamra'.replace(re48, '').length;
      sum += 'sbphf'.split(re70).length;
      sum += 'sbphf.gno sbphfva.gno'.split(re70).length;
      sum += 'sbphfva'.split(re70).length;
      sum += 'sbez'.replace(re39, '').length;
      sum += 'sbez.nwnk'.replace(re68, '').length;
      sum += 'sbez.nwnk'.replace(re18, '').length;
      sum += 'tnzrf'.replace(re40, '').length;
      sum += 'tnzrf'.replace(re41, '').length;
      sum += 'tnzrf'.replace(re42, '').length;
      sum += 'tnzrf'.replace(re43, '').length;
      sum += 'tnzrf'.replace(re44, '').length;
      sum += 'tnzrf'.replace(re45, '').length;
      sum += 'tnzrf'.replace(re46, '').length;
      sum += 'tnzrf'.replace(re47, '').length;
      sum += 'tnzrf'.replace(re48, '').length;
      sum += 'ubzrcntr'.replace(re30, '').length;
      sum += 'ubebfxbc'.replace(re40, '').length;
      sum += 'ubebfxbc'.replace(re41, '').length;
      sum += 'ubebfxbc'.replace(re42, '').length;
      sum += 'ubebfxbc'.replace(re43, '').length;
      sum += 'ubebfxbc'.replace(re44, '').length;
      sum += 'ubebfxbc'.replace(re45, '').length;
      sum += 'ubebfxbc'.replace(re46, '').length;
      sum += 'ubebfxbc'.replace(re47, '').length;
      sum += 'ubebfxbc'.replace(re48, '').length;
      sum += 'uc_cebzbobk_ugzy%2Puc_cebzbobk_vzt'.replace(re30, '').length;
      sum += 'uc_erpgnatyr'.replace(re30, '').length;
      sum += str87.replace(re33, '').length;
      sum += str88.replace(re33, '').length;
      sum += 'uggc://wf.hv-cbegny.qr/tzk/ubzr/wf/20080602/onfr.wf${4}${5}'.replace(re71, '').length;
      sum += 'uggc://wf.hv-cbegny.qr/tzk/ubzr/wf/20080602/onfr.wf${5}'.replace(re72, '').length;
      sum += 'uggc://wf.hv-cbegny.qr/tzk/ubzr/wf/20080602/qlaYvo.wf${4}${5}'.replace(re71, '').length;
      sum += 'uggc://wf.hv-cbegny.qr/tzk/ubzr/wf/20080602/qlaYvo.wf${5}'.replace(re72, '').length;
      sum += 'uggc://wf.hv-cbegny.qr/tzk/ubzr/wf/20080602/rssrpgYvo.wf${4}${5}'.replace(re71, '').length;
      sum += 'uggc://wf.hv-cbegny.qr/tzk/ubzr/wf/20080602/rssrpgYvo.wf${5}'.replace(re72, '').length;
      sum += str89.replace(re73, '').length;
      sum += 'uggc://zfacbegny.112.2b7.arg/o/ff/zfacbegnyubzr/1/U.7-cqi-2/f55023338617756?[NDO]&{1}&{2}&[NDR]'.replace(re69, '').length;
      sum += str6.replace(re23, '').length;
      sum += 'xvab'.replace(re40, '').length;
      sum += 'xvab'.replace(re41, '').length;
      sum += 'xvab'.replace(re42, '').length;
      sum += 'xvab'.replace(re43, '').length;
      sum += 'xvab'.replace(re44, '').length;
      sum += 'xvab'.replace(re45, '').length;
      sum += 'xvab'.replace(re46, '').length;
      sum += 'xvab'.replace(re47, '').length;
      sum += 'xvab'.replace(re48, '').length;
      sum += 'ybnq'.split(re70).length;
      sum += 'zrqvnzbqgno lhv-anifrg lhv-anifrg-gbc'.replace(re18, '').length;
      sum += 'zrgn'.replace(re39, '').length;
      sum += str90.replace(re68, '').length;
      sum += str90.replace(re18, '').length;
      sum += 'zbhfrzbir'.split(re70).length;
      sum += 'zbhfrzbir.gno'.split(re70).length;
      sum += str63.replace(/^.*jroxvg\/(\d+(\.\d+)?).*$/, '').length;
      sum += 'zhfvx'.replace(re40, '').length;
      sum += 'zhfvx'.replace(re41, '').length;
      sum += 'zhfvx'.replace(re42, '').length;
      sum += 'zhfvx'.replace(re43, '').length;
      sum += 'zhfvx'.replace(re44, '').length;
      sum += 'zhfvx'.replace(re45, '').length;
      sum += 'zhfvx'.replace(re46, '').length;
      sum += 'zhfvx'.replace(re47, '').length;
      sum += 'zhfvx'.replace(re48, '').length;
      sum += 'zlfcnpr_nccf_pnainf'.replace(re52, '').length;
      sum += str91.replace(re40, '').length;
      sum += str91.replace(re41, '').length;
      sum += str91.replace(re42, '').length;
      sum += str91.replace(re43, '').length;
      sum += str91.replace(re44, '').length;
      sum += str91.replace(re45, '').length;
      sum += str91.replace(re46, '').length;
      sum += str91.replace(re47, '').length;
      sum += str91.replace(re48, '').length;
      sum += 'anzr'.replace(re39, '').length;
      sum += str92.replace(/\b\w+\b/g, '').length;
      sum += 'bow-nppbeqvba'.replace(re39, '').length;
      sum += 'bowrpg'.replace(re39, '').length;
      sum += 'bowrpg'.replace(re68, '').length;
      sum += 'bowrpg'.replace(re18, '').length;
      sum += 'cnenzf%2Rfglyrf'.replace(re29, '').length;
      sum += 'cnenzf%2Rfglyrf'.replace(re30, '').length;
      sum += 'cbchc'.replace(re30, '').length;
      sum += 'ebhgr'.replace(re40, '').length;
      sum += 'ebhgr'.replace(re41, '').length;
      sum += 'ebhgr'.replace(re42, '').length;
      sum += 'ebhgr'.replace(re43, '').length;
      sum += 'ebhgr'.replace(re44, '').length;
      sum += 'ebhgr'.replace(re45, '').length;
      sum += 'ebhgr'.replace(re46, '').length;
      sum += 'ebhgr'.replace(re47, '').length;
      sum += 'ebhgr'.replace(re48, '').length;
      sum += 'freivprobk_uc'.replace(re30, '').length;
      sum += 'fubccvatobk_uc'.replace(re30, '').length;
      sum += 'fubhgobk'.replace(re39, '').length;
      sum += 'fcbeg'.replace(re40, '').length;
      sum += 'fcbeg'.replace(re41, '').length;
      sum += 'fcbeg'.replace(re42, '').length;
      sum += 'fcbeg'.replace(re43, '').length;
      sum += 'fcbeg'.replace(re44, '').length;
      sum += 'fcbeg'.replace(re45, '').length;
      sum += 'fcbeg'.replace(re46, '').length;
      sum += 'fcbeg'.replace(re47, '').length;
      sum += 'fcbeg'.replace(re48, '').length;
      sum += 'gbhe'.replace(re40, '').length;
      sum += 'gbhe'.replace(re41, '').length;
      sum += 'gbhe'.replace(re42, '').length;
      sum += 'gbhe'.replace(re43, '').length;
      sum += 'gbhe'.replace(re44, '').length;
      sum += 'gbhe'.replace(re45, '').length;
      sum += 'gbhe'.replace(re46, '').length;
      sum += 'gbhe'.replace(re47, '').length;
      sum += 'gbhe'.replace(re48, '').length;
      sum += 'gi-fcbg'.replace(re40, '').length;
      sum += 'gi-fcbg'.replace(re41, '').length;
      sum += 'gi-fcbg'.replace(re42, '').length;
      sum += 'gi-fcbg'.replace(re43, '').length;
      sum += 'gi-fcbg'.replace(re44, '').length;
      sum += 'gi-fcbg'.replace(re45, '').length;
      sum += 'gi-fcbg'.replace(re46, '').length;
      sum += 'gi-fcbg'.replace(re47, '').length;
      sum += 'gi-fcbg'.replace(re48, '').length;
      sum += 'glcr'.replace(re39, '').length;
      sum += 'haqrsvarq'.replace(/\//g, '').length;
      sum += str93.replace(re40, '').length;
      sum += str93.replace(re41, '').length;
      sum += str93.replace(re42, '').length;
      sum += str93.replace(re43, '').length;
      sum += str93.replace(re44, '').length;
      sum += str93.replace(re45, '').length;
      sum += str93.replace(re46, '').length;
      sum += str93.replace(re47, '').length;
      sum += str93.replace(re48, '').length;
      sum += 'ivqrb'.replace(re40, '').length;
      sum += 'ivqrb'.replace(re41, '').length;
      sum += 'ivqrb'.replace(re42, '').length;
      sum += 'ivqrb'.replace(re43, '').length;
      sum += 'ivqrb'.replace(re44, '').length;
      sum += 'ivqrb'.replace(re45, '').length;
      sum += 'ivqrb'.replace(re46, '').length;
      sum += 'ivqrb'.replace(re47, '').length;
      sum += 'ivqrb'.replace(re48, '').length;
      sum += 'ivfvgf=1'.split(re86).length;
      sum += 'jrggre'.replace(re40, '').length;
      sum += 'jrggre'.replace(re41, '').length;
      sum += 'jrggre'.replace(re42, '').length;
      sum += 'jrggre'.replace(re43, '').length;
      sum += 'jrggre'.replace(re44, '').length;
      sum += 'jrggre'.replace(re45, '').length;
      sum += 'jrggre'.replace(re46, '').length;
      sum += 'jrggre'.replace(re47, '').length;
      sum += 'jrggre'.replace(re48, '').length;
      sum += Exec(/#[a-z0-9]+$/i, 'uggc://jjj.fpuhryreim.arg/Qrsnhyg');
      sum += Exec(re66, 'fryrpgrq');
      sum += Exec(/(?:^|\s+)lhv-ani(?:\s+|$)/, 'sff lhv-ani');
      sum += Exec(/(?:^|\s+)lhv-anifrg(?:\s+|$)/, 'zrqvnzbqgno lhv-anifrg');
      sum += Exec(/(?:^|\s+)lhv-anifrg-gbc(?:\s+|$)/, 'zrqvnzbqgno lhv-anifrg');
      sum += Exec(re91, 'GnoThvq');
      sum += Exec(re91, 'thvq');
      sum += Exec(/(pbzcngvoyr|jroxvg)/, str63);
      sum += Exec(/.+(?:ei|vg|en|vr)[\/: ]([\d.]+)/, str63);
      sum += Exec(re8, '144631658.0.10.1231365869');
      sum += Exec(re8, '144631658.0.10.1231367054');
      sum += Exec(re8, '144631658.1231365869.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re8, '144631658.1231367054.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re8, '144631658.1670816052019209000.1231365869.1231365869.1231365869.1');
      sum += Exec(re8, '144631658.1796080716621419500.1231367054.1231367054.1231367054.1');
      sum += Exec(re8, str94);
      sum += Exec(re8, str95);
      sum += Exec(re8, str96);
      sum += Exec(re8, str97);
      sum += Exec(re8, '__hgzn=144631658.1670816052019209000.1231365869.1231365869.1231365869.1');
      sum += Exec(re8, '__hgzn=144631658.1796080716621419500.1231367054.1231367054.1231367054.1');
      sum += Exec(re8, '__hgzo=144631658.0.10.1231365869');
      sum += Exec(re8, '__hgzo=144631658.0.10.1231367054');
      sum += Exec(re8, '__hgzm=144631658.1231365869.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re8, '__hgzm=144631658.1231367054.1.1.hgzpfe=(qverpg)|hgzppa=(qverpg)|hgzpzq=(abar)');
      sum += Exec(re34, str78);
      sum += Exec(re34, str79);
      sum += Exec(re34, str81);
      sum += Exec(re74, str77);
      sum += Exec(re74, '*');
      sum += Exec(re74, str82);
      sum += Exec(re74, str83);
      sum += Exec(re74, str86);
      sum += Exec(re74, 'rzorq');
      sum += Exec(re74, 'sbez.nwnk');
      sum += Exec(re74, str90);
      sum += Exec(re74, 'bowrpg');
      sum += Exec(/\/onfr.wf(\?.+)?$/, '/uggc://wf.hv-cbegny.qr/tzk/ubzr/wf/20080602/onfr.wf');
      sum += Exec(re28, 'uvag ynfgUvag ynfg');
      sum += Exec(re75, '');
      sum += Exec(re76, '');
      sum += Exec(re77, '');
      sum += Exec(re78, '');
      sum += Exec(re80, str77);
      sum += Exec(re80, '*');
      sum += Exec(re80, '.pybfr');
      sum += Exec(re80, str82);
      sum += Exec(re80, str83);
      sum += Exec(re80, str84);
      sum += Exec(re80, str86);
      sum += Exec(re80, 'qg');
      sum += Exec(re80, 'rzorq');
      sum += Exec(re80, 'sbez.nwnk');
      sum += Exec(re80, str90);
      sum += Exec(re80, 'bowrpg');
      sum += Exec(re61, 'qlaYvo.wf');
      sum += Exec(re61, 'rssrpgYvo.wf');
      sum += Exec(re61, 'uggc://jjj.tzk.arg/qr/?fgnghf=uvajrvf');
      sum += Exec(re92, ' .pybfr');
      sum += Exec(re92, ' n.svryqOgaPnapry');
      sum += Exec(re92, ' qg');
      sum += Exec(re92, str48);
      sum += Exec(re92, '.nwnk');
      sum += Exec(re92, '.svryqOga,n.svryqOgaPnapry');
      sum += Exec(re92, '.svryqOgaPnapry');
      sum += Exec(re92, '.bow-nppbeqvba qg');
      sum += Exec(re68, str77);
      sum += Exec(re68, '*');
      sum += Exec(re68, '.pybfr');
      sum += Exec(re68, str82);
      sum += Exec(re68, str83);
      sum += Exec(re68, str84);
      sum += Exec(re68, str86);
      sum += Exec(re68, 'qg');
      sum += Exec(re68, 'rzorq');
      sum += Exec(re68, 'sbez.nwnk');
      sum += Exec(re68, str90);
      sum += Exec(re68, 'bowrpg');
      sum += Exec(re93, ' .pybfr');
      sum += Exec(re93, ' n.svryqOgaPnapry');
      sum += Exec(re93, ' qg');
      sum += Exec(re93, str48);
      sum += Exec(re93, '.nwnk');
      sum += Exec(re93, '.svryqOga,n.svryqOgaPnapry');
      sum += Exec(re93, '.svryqOgaPnapry');
      sum += Exec(re93, '.bow-nppbeqvba qg');
      sum += Exec(re81, str77);
      sum += Exec(re81, '*');
      sum += Exec(re81, str48);
      sum += Exec(re81, '.pybfr');
      sum += Exec(re81, str82);
      sum += Exec(re81, str83);
      sum += Exec(re81, str84);
      sum += Exec(re81, str86);
      sum += Exec(re81, 'qg');
      sum += Exec(re81, 'rzorq');
      sum += Exec(re81, 'sbez.nwnk');
      sum += Exec(re81, str90);
      sum += Exec(re81, 'bowrpg');
      sum += Exec(re94, ' .pybfr');
      sum += Exec(re94, ' n.svryqOgaPnapry');
      sum += Exec(re94, ' qg');
      sum += Exec(re94, str48);
      sum += Exec(re94, '.nwnk');
      sum += Exec(re94, '.svryqOga,n.svryqOgaPnapry');
      sum += Exec(re94, '.svryqOgaPnapry');
      sum += Exec(re94, '.bow-nppbeqvba qg');
      sum += Exec(re94, '[anzr=nwnkHey]');
      sum += Exec(re94, str82);
      sum += Exec(re31, 'rf');
      sum += Exec(re31, 'wn');
      sum += Exec(re82, str77);
      sum += Exec(re82, '*');
      sum += Exec(re82, str48);
      sum += Exec(re82, '.pybfr');
      sum += Exec(re82, str82);
      sum += Exec(re82, str83);
      sum += Exec(re82, str84);
      sum += Exec(re82, str86);
      sum += Exec(re82, 'qg');
      sum += Exec(re82, 'rzorq');
      sum += Exec(re82, 'sbez.nwnk');
      sum += Exec(re82, str90);
      sum += Exec(re82, 'bowrpg');
      sum += Exec(re83, str98);
      sum += Exec(re83, 'shapgvba sbphf() { [angvir pbqr] }');
      sum += Exec(re62, '#Ybtva');
      sum += Exec(re62, '#Ybtva_cnffjbeq');
      sum += Exec(re62, str77);
      sum += Exec(re62, '#fubhgobkWf');
      sum += Exec(re62, '#fubhgobkWfReebe');
      sum += Exec(re62, '#fubhgobkWfFhpprff');
      sum += Exec(re62, '*');
      sum += Exec(re62, str82);
      sum += Exec(re62, str83);
      sum += Exec(re62, str86);
      sum += Exec(re62, 'rzorq');
      sum += Exec(re62, 'sbez.nwnk');
      sum += Exec(re62, str90);
      sum += Exec(re62, 'bowrpg');
      sum += Exec(re49, 'pbagrag');
      sum += Exec(re24, str6);
      sum += Exec(/xbadhrebe/, str63);
      sum += Exec(/znp/, 'jva32');
      sum += Exec(/zbmvyyn/, str63);
      sum += Exec(/zfvr/, str63);
      sum += Exec(/ag\s5\.1/, str63);
      sum += Exec(/bcren/, str63);
      sum += Exec(/fnsnev/, str63);
      sum += Exec(/jva/, 'jva32');
      sum += Exec(/jvaqbjf/, str63);
    }
    return sum;
  }

  function run() {
    for (var i = 0; i < 5; i++) {
      var sum = 0;
      sum += runBlock0();
      sum += runBlock1();
      sum += runBlock2();
      sum += runBlock3();
      sum += runBlock4();
      sum += runBlock5();
      sum += runBlock6();
      sum += runBlock7();
      sum += runBlock8();
      sum += runBlock9();
      sum += runBlock10();
      sum += runBlock11();
      if (sum != 1666109) throw new Error("Wrong checksum.");
    }
  }

  this.run = run;
}

////////////////////////////////////////////////////////////////////////////////
// Runner
////////////////////////////////////////////////////////////////////////////////

var success = true;

function NotifyStart(name) {
}

function NotifyError(name, error) {
  WScript.Echo(name + " : ERROR : " +error.stack);
  success = false;
}

function NotifyResult(name, score) {
  if (success) {
    WScript.Echo("### SCORE:", score);
  }
}

function NotifyScore(score) {
}

BenchmarkSuite.RunSuites({
	NotifyStart : NotifyStart,
    NotifyError : NotifyError,
    NotifyResult : NotifyResult,
    NotifyScore : NotifyScore
});
