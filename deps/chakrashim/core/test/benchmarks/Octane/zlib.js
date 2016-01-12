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

/////////////////////////////////////////////////////////////////////////////
//	zlib.js
/////////////////////////////////////////////////////////////////////////////
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

new BenchmarkSuite('zlib', [152815148], [
  new Benchmark('zlib', false, true, 
    runZlib, undefined, tearDownZlib, null, 3)]);

// Generate 100kB pseudo-random bytes (compressed 25906 bytes) and
// compress/decompress them 60 times.
var zlibEval = eval;
function runZlib() {
  if (typeof Ya != "function") {
    InitializeZlibBenchmark();
  }
  Ya(["1"]);
}

function tearDownZlib() {
  delete $;
  delete $a;
  delete Aa;
  delete Ab;
  delete Ba;
  delete Bb;
  delete C;
  delete Ca;
  delete Cb;
  delete D;
  delete Da;
  delete Db;
  delete Ea;
  delete Eb;
  delete F;
  delete Fa;
  delete Fb;
  delete G;
  delete Ga;
  delete Gb;
  delete Ha;
  delete Hb;
  delete I;
  delete Ia;
  delete Ib;
  delete J;
  delete Ja;
  delete Jb;
  delete Ka;
  delete Kb;
  delete L;
  delete La;
  delete Lb;
  delete Ma;
  delete Mb;
  delete Module;
  delete N;
  delete Na;
  delete Nb;
  delete O;
  delete Oa;
  delete Ob;
  delete P;
  delete Pa;
  delete Pb;
  delete Q;
  delete Qa;
  delete Qb;
  delete R;
  delete Ra;
  delete Rb;
  delete S;
  delete Sa;
  delete Sb;
  delete T;
  delete Ta;
  delete Tb;
  delete U;
  delete Ua;
  delete Ub;
  delete V;
  delete Va;
  delete Vb;
  delete W;
  delete Wa;
  delete Wb;
  delete X;
  delete Xa;
  delete Y;
  delete Ya;
  delete Z;
  delete Za;
  delete ab;
  delete ba;
  delete bb;
  delete ca;
  delete cb;
  delete da;
  delete db;
  delete ea;
  delete eb;
  delete fa;
  delete fb;
  delete ga;
  delete gb;
  delete ha;
  delete hb;
  delete ia;
  delete ib;
  delete j;
  delete ja;
  delete jb;
  delete k;
  delete ka;
  delete kb;
  delete la;
  delete lb;
  delete ma;
  delete mb;
  delete n;
  delete na;
  delete nb;
  delete oa;
  delete ob;
  delete pa;
  delete pb;
  delete qa;
  delete qb;
  delete r;
  delete ra;
  delete rb;
  delete sa;
  delete sb;
  delete t;
  delete ta;
  delete tb;
  delete u;
  delete ua;
  delete ub;
  delete v;
  delete va;
  delete vb;
  delete w;
  delete wa;
  delete wb;
  delete x;
  delete xa;
  delete xb;
  delete ya;
  delete yb;
  delete z;
  delete za;
  delete zb;
}

/////////////////////////////////////////////////////////////////////////////////
//	zlib-data.js
/////////////////////////////////////////////////////////////////////////////////

// This file is generated by the Python script below. It contains code from
//    https://github.com/dvander/arewefastyet/blob/master/benchmarks/asmjs-apps/zlib.js
// which is basically a trivial driver in C from
//    https://github.com/kripken/emscripten/blob/master/tests/zlib/benchmark.c
// plus the zlib library from
//    http://www.zlib.net/
// (which is Copyright (C) 1995-2013 Jean-loup Gailly and Mark Adler) compiled
// via Emscripten.
// The script applies a few changes to avoid any output and to not start
// the benchmark directly.

/*
# ---------------------- BEGIN GENERATOR SCRIPT --------------------
#!/usr/bin/env python

import urllib2

url = "https://github.com/dvander/arewefastyet/raw/master/benchmarks/asmjs-apps/zlib.js"

def do_replacements(data):
  # Update the *_original patterns if generated identifier names change:
  printf_original = "_printf:function(a,b){return Cb(L[W>> 2],a,b)}"
  printf_neutered = "_printf:function(a,b){}"
  puts_original = ("_puts:function(a){var b=L[W>>2],a=Gb(a,b);" +
                  "return 0>a?a:0>Hb(10,b)?-1:a+1}")
  puts_neutered = "_puts:function(a){}"
  runner_original = "Ya([].concat(Module.arguments));"
  runner_neutered = ""
  assert printf_original in data
  assert puts_original in data
  assert runner_original in data

  data = data.replace("\\", "\\\\").replace("\'", "\\\'")
  data = data.replace(printf_original, printf_neutered)
  data = data.replace(puts_original, puts_neutered)
  data = data.replace(runner_original, runner_neutered)

def format_80col(data):
  while len(data) > 0:
    cutoff = min(79, len(data))
    while data[cutoff-1] == '\\':
      cutoff -= 1
    line = data[0:cutoff]
    data = data[cutoff:]
    if len(data) > 0:
      line += '\\'
    print line

download = urllib2.urlopen(url)
data = ""
for line in download:
  line = line.strip()
  if line.startswith("//"): continue
  data += line + " "

data = do_replacements(data)
format_80col("function InitializeZlibBenchmark() {" +
             "zlibEval('" +
             data +
             "');" +
             "}")
# ----------------------- END GENERATOR SCRIPT ---------------------
*/

var window = this;

function InitializeZlibBenchmark() {zlibEval('function j(a){throw a;}var k=\
void 0,n=!0,r=null,t=!1;function u(){return function(){}}try{this.Module=Module\
,Module.test}catch(aa){this.Module=Module={}}var ba="object"===typeof process&&\
"function"===typeof require,ca="object"===typeof window,v="function"===typeof i\
mportScripts,da=!ca&&!ba&&!v;"object"===typeof module&&(module.T=Module); if(ba\
){Module.print=function(a){process.stdout.write(a+"\\n")};Module.printErr=funct\
ion(a){process.stderr.write(a+"\\n")};var ea=require("fs"),fa=require("path");M\
odule.read=function(a,b){var a=fa.normalize(a),c=ea.readFileSync(a);!c&&a!=fa.r\
esolve(a)&&(a=path.join(__dirname,"..","src",a),c=ea.readFileSync(a));c&&!b&&(c\
=c.toString());return c};Module.readBinary=function(a){return Module.read(a,n)}\
;Module.load=function(a){ga(read(a))};Module.arguments||(Module.arguments=proce\
ss.argv.slice(2))} da&&(Module.print=print,"undefined"!=typeof printErr&&(Modul\
e.printErr=printErr),Module.read=read,Module.readBinary=function(a){return read\
(a,"binary")},Module.arguments||("undefined"!=typeof scriptArgs?Module.argument\
s=scriptArgs:"undefined"!=typeof arguments&&(Module.arguments=arguments)));ca&&\
!v&&(Module.print||(Module.print=function(a){console.log(a)}),Module.printErr||\
(Module.printErr=function(a){console.log(a)})); if(ca||v)Module.read=function(a\
){var b=new XMLHttpRequest;b.open("GET",a,t);b.send(r);return b.responseText},M\
odule.arguments||"undefined"!=typeof arguments&&(Module.arguments=arguments);v&\
&(Module.print||(Module.print=u()),Module.load=importScripts);!v&&(!ca&&!ba&&!d\
a)&&j("Unknown runtime environment. Where are we?");function ga(a){eval.call(r,\
a)}"undefined"==!Module.load&&Module.read&&(Module.load=function(a){ga(Module.r\
ead(a))});Module.print||(Module.print=u()); Module.printErr||(Module.printErr=M\
odule.print);Module.arguments||(Module.arguments=[]);Module.print=Module.print;\
Module.g=Module.printErr;Module.preRun||(Module.preRun=[]);Module.postRun||(Mod\
ule.postRun=[]);function ha(){return w}function ia(a){w=a}function ja(a){if(1==\
x)return 1;var b={"%i1":1,"%i8":1,"%i16":2,"%i32":4,"%i64":8,"%float":4,"%doubl\
e":8}["%"+a];b||("*"==a.charAt(a.length-1)?b=x:"i"==a[0]&&(a=parseInt(a.substr(\
1)),z(0==a%8),b=a/8));return b} function ka(a,b,c){c&&c.length?(c.splice||(c=Ar\
ray.prototype.slice.call(c)),c.splice(0,0,b),Module["dynCall_"+a].apply(r,c)):M\
odule["dynCall_"+a].call(r,b)}var la; function ma(){var a=[],b=0;this.B=functio\
n(c){c&=255;b&&(a.push(c),b--);if(0==a.length){if(128>c)return String.fromCharC\
ode(c);a.push(c);b=191<c&&224>c?1:2;return""}if(0<b)return"";var c=a[0],d=a[1],\
e=a[2],c=191<c&&224>c?String.fromCharCode((c&31)<<6|d&63):String.fromCharCode((\
c&15)<<12|(d&63)<<6|e&63);a.length=0;return c};this.O=function(a){for(var a=une\
scape(encodeURIComponent(a)),b=[],e=0;e<a.length;e++)b.push(a.charCodeAt(e));re\
turn b}}function na(a){var b=w;w=w+a|0;w=w+7>>3<<3;return b} function oa(a){var\
 b=C;C=C+a|0;C=C+7>>3<<3;return b}function pa(a){var b=D;D=D+a|0;D=D+7>>3<<3;D>\
=qa&&F("Cannot enlarge memory arrays in asm.js. Either (1) compile with -s TOTA\
L_MEMORY=X with X higher than the current value, or (2) set Module.TOTAL_MEMORY\
 before the program runs.");return b}function ra(a,b){return Math.ceil(a/(b?b:8\
))*(b?b:8)}var x=4,sa={},G=t,ta;function F(a){Module.print(a+":\\n"+Error().sta\
ck);G=n;j("Assertion: "+a)}function z(a,b){a||F("Assertion failed: "+b)}var ua=\
this; Module.ccall=function(a,b,c,d){return va(wa(a),b,c,d)};function wa(a){try\
{var b=ua.Module["_"+a];b||(b=eval("_"+a))}catch(c){}z(b,"Cannot call unknown f\
unction "+a+" (perhaps LLVM optimizations or closure removed it?)");return b} f\
unction va(a,b,c,d){function e(a,b){if("string"==b){if(a===r||a===k||0===a)retu\
rn 0;f||(f=ha());var c=na(a.length+1);xa(a,c);return c}return"array"==b?(f||(f=\
ha()),c=na(a.length),ya(a,c),c):a}var f=0,g=0,d=d?d.map(function(a){return e(a,\
c[g++])}):[];a=a.apply(r,d);"string"==b?b=I(a):(z("array"!=b),b=a);f&&ia(f);ret\
urn b}Module.cwrap=function(a,b,c){var d=wa(a);return function(){return va(d,b,\
c,Array.prototype.slice.call(arguments))}}; function za(a,b,c){c=c||"i8";"*"===\
c.charAt(c.length-1)&&(c="i32");switch(c){case "i1":J[a]=b;break;case "i8":J[a]\
=b;break;case "i16":Aa[a>>1]=b;break;case "i32":L[a>>2]=b;break;case "i64":ta=[\
b>>>0,(Math.min(+Math.floor(b/4294967296),4294967295)|0)>>>0];L[a>>2]=ta[0];L[a\
+4>>2]=ta[1];break;case "float":Ba[a>>2]=b;break;case "double":N[a>>3]=b;break;\
default:F("invalid type for setValue: "+c)}}Module.setValue=za; Module.getValue\
=function(a,b){b=b||"i8";"*"===b.charAt(b.length-1)&&(b="i32");switch(b){case "\
i1":return J[a];case "i8":return J[a];case "i16":return Aa[a>>1];case "i32":ret\
urn L[a>>2];case "i64":return L[a>>2];case "float":return Ba[a>>2];case "double\
":return N[a>>3];default:F("invalid type for setValue: "+b)}return r};var Ca=1,\
O=2,Da=4;Module.ALLOC_NORMAL=0;Module.ALLOC_STACK=Ca;Module.ALLOC_STATIC=O;Modu\
le.ALLOC_DYNAMIC=3;Module.ALLOC_NONE=Da; function P(a,b,c,d){var e,f;"number"==\
=typeof a?(e=n,f=a):(e=t,f=a.length);var g="string"===typeof b?b:r,c=c==Da?d:[E\
a,na,oa,pa][c===k?O:c](Math.max(f,g?1:b.length));if(e){d=c;z(0==(c&3));for(a=c+\
(f&-4);d<a;d+=4)L[d>>2]=0;for(a=c+f;d<a;)J[d++|0]=0;return c}if("i8"===g)return\
 a.subarray||a.slice?Q.set(a,c):Q.set(new Uint8Array(a),c),c;for(var d=0,i,l;d<\
f;){var y=a[d];"function"===typeof y&&(y=sa.U(y));e=g||b[d];0===e?d++:("i64"==e\
&&(e="i32"),za(c+d,y,e),l!==e&&(i=ja(e),l=e),d+=i)}return c} Module.allocate=P;\
function I(a,b){for(var c=t,d,e=0;;){d=Q[a+e|0];if(128<=d)c=n;else if(0==d&&!b)\
break;e++;if(b&&e==b)break}b||(b=e);var f="";if(!c){for(;0<b;)d=String.fromChar\
Code.apply(String,Q.subarray(a,a+Math.min(b,1024))),f=f?f+d:d,a+=1024,b-=1024;r\
eturn f}c=new ma;for(e=0;e<b;e++)d=Q[a+e|0],f+=c.B(d);return f}Module.Pointer_s\
tringify=I;var J,Q,Aa,Fa,L,Ga,Ba,N,Ha=0,C=0,Ia=0,w=0,Ja=0,Ka=0,D=0,qa=Module.TO\
TAL_MEMORY||134217728; z(!!Int32Array&&!!Float64Array&&!!(new Int32Array(1)).su\
barray&&!!(new Int32Array(1)).set,"Cannot fallback to non-typed array case: Cod\
e is too specialized");var R=new ArrayBuffer(qa);J=new Int8Array(R);Aa=new Int1\
6Array(R);L=new Int32Array(R);Q=new Uint8Array(R);Fa=new Uint16Array(R);Ga=new \
Uint32Array(R);Ba=new Float32Array(R);N=new Float64Array(R);L[0]=255;z(255===Q[\
0]&&0===Q[3],"Typed arrays 2 must be run on a little-endian system");Module.HEA\
P=k;Module.HEAP8=J;Module.HEAP16=Aa; Module.HEAP32=L;Module.HEAPU8=Q;Module.HEA\
PU16=Fa;Module.HEAPU32=Ga;Module.HEAPF32=Ba;Module.HEAPF64=N;function La(a){for\
(;0<a.length;){var b=a.shift();if("function"==typeof b)b();else{var c=b.l;"numb\
er"===typeof c?b.i===k?ka("v",c):ka("vi",c,[b.i]):c(b.i===k?r:b.i)}}}var Ma=[],\
Na=[],Oa=[],Pa=t;function S(a,b,c){a=(new ma).O(a);c&&(a.length=c);b||a.push(0)\
;return a}Module.intArrayFromString=S;Module.intArrayToString=function(a){for(v\
ar b=[],c=0;c<a.length;c++){var d=a[c];255<d&&(d&=255);b.push(String.fromCharCo\
de(d))}return b.join("")}; function xa(a,b,c){a=S(a,c);for(c=0;c<a.length;)J[b+\
c|0]=a[c],c+=1}Module.writeStringToMemory=xa;function ya(a,b){for(var c=0;c<a.l\
ength;c++)J[b+c|0]=a[c]}Module.writeArrayToMemory=ya;function Qa(a,b){return 0<\
=a?a:32>=b?2*Math.abs(1<<b-1)+a:Math.pow(2,b)+a}function Ra(a,b){if(0>=a)return\
 a;var c=32>=b?Math.abs(1<<b-1):Math.pow(2,b-1);if(a>=c&&(32>=b||a>c))a=-2*c+a;\
return a}Math.imul||(Math.imul=function(a,b){var c=a&65535,d=b&65535;return c*d\
+((a>>>16)*d+c*(b>>>16)<<16)|0}); var T=0,Sa={},Ta=t,Ua=r;function Va(a){T++;Mo\
dule.monitorRunDependencies&&Module.monitorRunDependencies(T);a?(z(!Sa[a]),Sa[a\
]=1):Module.g("warning: run dependency added without ID")}Module.addRunDependen\
cy=Va;function Wa(a){T--;Module.monitorRunDependencies&&Module.monitorRunDepend\
encies(T);a?(z(Sa[a]),delete Sa[a]):Module.g("warning: run dependency removed w\
ithout ID");0==T&&(Ua!==r&&(clearInterval(Ua),Ua=r),!Ta&&Xa&&Ya([].concat(Modul\
e.arguments)))}Module.removeRunDependency=Wa; Module.preloadedImages={};Module.\
preloadedAudios={};Ha=8;C=Ha+14192; P([111,107,46,0,0,0,0,0,12,0,8,0,140,0,8,0,\
76,0,8,0,204,0,8,0,44,0,8,0,172,0,8,0,108,0,8,0,236,0,8,0,28,0,8,0,156,0,8,0,92\
,0,8,0,220,0,8,0,60,0,8,0,188,0,8,0,124,0,8,0,252,0,8,0,2,0,8,0,130,0,8,0,66,0,\
8,0,194,0,8,0,34,0,8,0,162,0,8,0,98,0,8,0,226,0,8,0,18,0,8,0,146,0,8,0,82,0,8,0\
,210,0,8,0,50,0,8,0,178,0,8,0,114,0,8,0,242,0,8,0,10,0,8,0,138,0,8,0,74,0,8,0,2\
02,0,8,0,42,0,8,0,170,0,8,0,106,0,8,0,234,0,8,0,26,0,8,0,154,0,8,0,90,0,8,0,218\
,0,8,0,58,0,8,0,186,0,8,0,122,0,8,0,250,0,8,0,6,0,8,0,134,0,8,0, 70,0,8,0,198,0\
,8,0,38,0,8,0,166,0,8,0,102,0,8,0,230,0,8,0,22,0,8,0,150,0,8,0,86,0,8,0,214,0,8\
,0,54,0,8,0,182,0,8,0,118,0,8,0,246,0,8,0,14,0,8,0,142,0,8,0,78,0,8,0,206,0,8,0\
,46,0,8,0,174,0,8,0,110,0,8,0,238,0,8,0,30,0,8,0,158,0,8,0,94,0,8,0,222,0,8,0,6\
2,0,8,0,190,0,8,0,126,0,8,0,254,0,8,0,1,0,8,0,129,0,8,0,65,0,8,0,193,0,8,0,33,0\
,8,0,161,0,8,0,97,0,8,0,225,0,8,0,17,0,8,0,145,0,8,0,81,0,8,0,209,0,8,0,49,0,8,\
0,177,0,8,0,113,0,8,0,241,0,8,0,9,0,8,0,137,0,8,0,73,0,8,0,201,0,8,0,41,0,8,0,1\
69,0,8,0,105, 0,8,0,233,0,8,0,25,0,8,0,153,0,8,0,89,0,8,0,217,0,8,0,57,0,8,0,18\
5,0,8,0,121,0,8,0,249,0,8,0,5,0,8,0,133,0,8,0,69,0,8,0,197,0,8,0,37,0,8,0,165,0\
,8,0,101,0,8,0,229,0,8,0,21,0,8,0,149,0,8,0,85,0,8,0,213,0,8,0,53,0,8,0,181,0,8\
,0,117,0,8,0,245,0,8,0,13,0,8,0,141,0,8,0,77,0,8,0,205,0,8,0,45,0,8,0,173,0,8,0\
,109,0,8,0,237,0,8,0,29,0,8,0,157,0,8,0,93,0,8,0,221,0,8,0,61,0,8,0,189,0,8,0,1\
25,0,8,0,253,0,8,0,19,0,9,0,19,1,9,0,147,0,9,0,147,1,9,0,83,0,9,0,83,1,9,0,211,\
0,9,0,211,1,9,0,51,0,9,0,51,1,9,0,179,0,9, 0,179,1,9,0,115,0,9,0,115,1,9,0,243,\
0,9,0,243,1,9,0,11,0,9,0,11,1,9,0,139,0,9,0,139,1,9,0,75,0,9,0,75,1,9,0,203,0,9\
,0,203,1,9,0,43,0,9,0,43,1,9,0,171,0,9,0,171,1,9,0,107,0,9,0,107,1,9,0,235,0,9,\
0,235,1,9,0,27,0,9,0,27,1,9,0,155,0,9,0,155,1,9,0,91,0,9,0,91,1,9,0,219,0,9,0,2\
19,1,9,0,59,0,9,0,59,1,9,0,187,0,9,0,187,1,9,0,123,0,9,0,123,1,9,0,251,0,9,0,25\
1,1,9,0,7,0,9,0,7,1,9,0,135,0,9,0,135,1,9,0,71,0,9,0,71,1,9,0,199,0,9,0,199,1,9\
,0,39,0,9,0,39,1,9,0,167,0,9,0,167,1,9,0,103,0,9,0,103,1,9,0,231,0,9,0, 231,1,9\
,0,23,0,9,0,23,1,9,0,151,0,9,0,151,1,9,0,87,0,9,0,87,1,9,0,215,0,9,0,215,1,9,0,\
55,0,9,0,55,1,9,0,183,0,9,0,183,1,9,0,119,0,9,0,119,1,9,0,247,0,9,0,247,1,9,0,1\
5,0,9,0,15,1,9,0,143,0,9,0,143,1,9,0,79,0,9,0,79,1,9,0,207,0,9,0,207,1,9,0,47,0\
,9,0,47,1,9,0,175,0,9,0,175,1,9,0,111,0,9,0,111,1,9,0,239,0,9,0,239,1,9,0,31,0,\
9,0,31,1,9,0,159,0,9,0,159,1,9,0,95,0,9,0,95,1,9,0,223,0,9,0,223,1,9,0,63,0,9,0\
,63,1,9,0,191,0,9,0,191,1,9,0,127,0,9,0,127,1,9,0,255,0,9,0,255,1,9,0,0,0,7,0,6\
4,0,7,0,32,0,7,0,96, 0,7,0,16,0,7,0,80,0,7,0,48,0,7,0,112,0,7,0,8,0,7,0,72,0,7,\
0,40,0,7,0,104,0,7,0,24,0,7,0,88,0,7,0,56,0,7,0,120,0,7,0,4,0,7,0,68,0,7,0,36,0\
,7,0,100,0,7,0,20,0,7,0,84,0,7,0,52,0,7,0,116,0,7,0,3,0,8,0,131,0,8,0,67,0,8,0,\
195,0,8,0,35,0,8,0,163,0,8,0,99,0,8,0,227,0,8,0,16,0,0,0,16,15,0,0,1,1,0,0,30,1\
,0,0,15,0,0,0,0,0,0,0,0,0,5,0,16,0,5,0,8,0,5,0,24,0,5,0,4,0,5,0,20,0,5,0,12,0,5\
,0,28,0,5,0,2,0,5,0,18,0,5,0,10,0,5,0,26,0,5,0,6,0,5,0,22,0,5,0,14,0,5,0,30,0,5\
,0,1,0,5,0,17,0,5,0,9,0,5,0,25,0,5,0,5,0,5,0,21, 0,5,0,13,0,5,0,29,0,5,0,3,0,5,\
0,19,0,5,0,11,0,5,0,27,0,5,0,7,0,5,0,23,0,5,0,168,4,0,0,136,15,0,0,0,0,0,0,30,0\
,0,0,15,0,0,0,0,0,0,0,0,0,0,0,0,16,0,0,0,0,0,0,19,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0\
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,\
0,17,0,17,0,17,0,17,0,18,0,18,0,18,0,18,0,19,0,19,0,19,0,19,0,20,0,20,0,20,0,20\
,0,21,0,21,0,21,0,21,0,16,0,73,0,195,0,0,0,3,0,4,0,5,0,6,0,7,0,8,0,9,0,10,0,11,\
0,13,0,15,0,17,0,19,0,23,0,27,0,31,0,35,0,43,0,51,0,59,0,67,0,83,0,99,0,115, 0,\
131,0,163,0,195,0,227,0,2,1,0,0,0,0,0,0,16,0,16,0,16,0,16,0,17,0,17,0,18,0,18,0\
,19,0,19,0,20,0,20,0,21,0,21,0,22,0,22,0,23,0,23,0,24,0,24,0,25,0,25,0,26,0,26,\
0,27,0,27,0,28,0,28,0,29,0,29,0,64,0,64,0,1,0,2,0,3,0,4,0,5,0,7,0,9,0,13,0,17,0\
,25,0,33,0,49,0,65,0,97,0,129,0,193,0,1,1,129,1,1,2,1,3,1,4,1,6,1,8,1,12,1,16,1\
,24,1,32,1,48,1,64,1,96,0,0,0,0,16,0,17,0,18,0,0,0,8,0,7,0,9,0,6,0,10,0,5,0,11,\
0,4,0,12,0,3,0,13,0,2,0,14,0,1,0,15,0,0,0,96,7,0,0,0,8,80,0,0,8,16,0,20,8,115,0\
,18,7,31,0,0,8,112,0,0,8, 48,0,0,9,192,0,16,7,10,0,0,8,96,0,0,8,32,0,0,9,160,0,\
0,8,0,0,0,8,128,0,0,8,64,0,0,9,224,0,16,7,6,0,0,8,88,0,0,8,24,0,0,9,144,0,19,7,\
59,0,0,8,120,0,0,8,56,0,0,9,208,0,17,7,17,0,0,8,104,0,0,8,40,0,0,9,176,0,0,8,8,\
0,0,8,136,0,0,8,72,0,0,9,240,0,16,7,4,0,0,8,84,0,0,8,20,0,21,8,227,0,19,7,43,0,\
0,8,116,0,0,8,52,0,0,9,200,0,17,7,13,0,0,8,100,0,0,8,36,0,0,9,168,0,0,8,4,0,0,8\
,132,0,0,8,68,0,0,9,232,0,16,7,8,0,0,8,92,0,0,8,28,0,0,9,152,0,20,7,83,0,0,8,12\
4,0,0,8,60,0,0,9,216,0,18,7,23,0,0,8,108,0,0,8,44,0,0, 9,184,0,0,8,12,0,0,8,140\
,0,0,8,76,0,0,9,248,0,16,7,3,0,0,8,82,0,0,8,18,0,21,8,163,0,19,7,35,0,0,8,114,0\
,0,8,50,0,0,9,196,0,17,7,11,0,0,8,98,0,0,8,34,0,0,9,164,0,0,8,2,0,0,8,130,0,0,8\
,66,0,0,9,228,0,16,7,7,0,0,8,90,0,0,8,26,0,0,9,148,0,20,7,67,0,0,8,122,0,0,8,58\
,0,0,9,212,0,18,7,19,0,0,8,106,0,0,8,42,0,0,9,180,0,0,8,10,0,0,8,138,0,0,8,74,0\
,0,9,244,0,16,7,5,0,0,8,86,0,0,8,22,0,64,8,0,0,19,7,51,0,0,8,118,0,0,8,54,0,0,9\
,204,0,17,7,15,0,0,8,102,0,0,8,38,0,0,9,172,0,0,8,6,0,0,8,134,0,0,8,70,0,0,9,23\
6,0, 16,7,9,0,0,8,94,0,0,8,30,0,0,9,156,0,20,7,99,0,0,8,126,0,0,8,62,0,0,9,220,\
0,18,7,27,0,0,8,110,0,0,8,46,0,0,9,188,0,0,8,14,0,0,8,142,0,0,8,78,0,0,9,252,0,\
96,7,0,0,0,8,81,0,0,8,17,0,21,8,131,0,18,7,31,0,0,8,113,0,0,8,49,0,0,9,194,0,16\
,7,10,0,0,8,97,0,0,8,33,0,0,9,162,0,0,8,1,0,0,8,129,0,0,8,65,0,0,9,226,0,16,7,6\
,0,0,8,89,0,0,8,25,0,0,9,146,0,19,7,59,0,0,8,121,0,0,8,57,0,0,9,210,0,17,7,17,0\
,0,8,105,0,0,8,41,0,0,9,178,0,0,8,9,0,0,8,137,0,0,8,73,0,0,9,242,0,16,7,4,0,0,8\
,85,0,0,8,21,0,16,8,2,1,19,7,43, 0,0,8,117,0,0,8,53,0,0,9,202,0,17,7,13,0,0,8,1\
01,0,0,8,37,0,0,9,170,0,0,8,5,0,0,8,133,0,0,8,69,0,0,9,234,0,16,7,8,0,0,8,93,0,\
0,8,29,0,0,9,154,0,20,7,83,0,0,8,125,0,0,8,61,0,0,9,218,0,18,7,23,0,0,8,109,0,0\
,8,45,0,0,9,186,0,0,8,13,0,0,8,141,0,0,8,77,0,0,9,250,0,16,7,3,0,0,8,83,0,0,8,1\
9,0,21,8,195,0,19,7,35,0,0,8,115,0,0,8,51,0,0,9,198,0,17,7,11,0,0,8,99,0,0,8,35\
,0,0,9,166,0,0,8,3,0,0,8,131,0,0,8,67,0,0,9,230,0,16,7,7,0,0,8,91,0,0,8,27,0,0,\
9,150,0,20,7,67,0,0,8,123,0,0,8,59,0,0,9,214,0,18,7,19,0,0,8, 107,0,0,8,43,0,0,\
9,182,0,0,8,11,0,0,8,139,0,0,8,75,0,0,9,246,0,16,7,5,0,0,8,87,0,0,8,23,0,64,8,0\
,0,19,7,51,0,0,8,119,0,0,8,55,0,0,9,206,0,17,7,15,0,0,8,103,0,0,8,39,0,0,9,174,\
0,0,8,7,0,0,8,135,0,0,8,71,0,0,9,238,0,16,7,9,0,0,8,95,0,0,8,31,0,0,9,158,0,20,\
7,99,0,0,8,127,0,0,8,63,0,0,9,222,0,18,7,27,0,0,8,111,0,0,8,47,0,0,9,190,0,0,8,\
15,0,0,8,143,0,0,8,79,0,0,9,254,0,96,7,0,0,0,8,80,0,0,8,16,0,20,8,115,0,18,7,31\
,0,0,8,112,0,0,8,48,0,0,9,193,0,16,7,10,0,0,8,96,0,0,8,32,0,0,9,161,0,0,8,0,0,0\
,8,128,0,0, 8,64,0,0,9,225,0,16,7,6,0,0,8,88,0,0,8,24,0,0,9,145,0,19,7,59,0,0,8\
,120,0,0,8,56,0,0,9,209,0,17,7,17,0,0,8,104,0,0,8,40,0,0,9,177,0,0,8,8,0,0,8,13\
6,0,0,8,72,0,0,9,241,0,16,7,4,0,0,8,84,0,0,8,20,0,21,8,227,0,19,7,43,0,0,8,116,\
0,0,8,52,0,0,9,201,0,17,7,13,0,0,8,100,0,0,8,36,0,0,9,169,0,0,8,4,0,0,8,132,0,0\
,8,68,0,0,9,233,0,16,7,8,0,0,8,92,0,0,8,28,0,0,9,153,0,20,7,83,0,0,8,124,0,0,8,\
60,0,0,9,217,0,18,7,23,0,0,8,108,0,0,8,44,0,0,9,185,0,0,8,12,0,0,8,140,0,0,8,76\
,0,0,9,249,0,16,7,3,0,0,8,82,0,0,8,18,0, 21,8,163,0,19,7,35,0,0,8,114,0,0,8,50,\
0,0,9,197,0,17,7,11,0,0,8,98,0,0,8,34,0,0,9,165,0,0,8,2,0,0,8,130,0,0,8,66,0,0,\
9,229,0,16,7,7,0,0,8,90,0,0,8,26,0,0,9,149,0,20,7,67,0,0,8,122,0,0,8,58,0,0,9,2\
13,0,18,7,19,0,0,8,106,0,0,8,42,0,0,9,181,0,0,8,10,0,0,8,138,0,0,8,74,0,0,9,245\
,0,16,7,5,0,0,8,86,0,0,8,22,0,64,8,0,0,19,7,51,0,0,8,118,0,0,8,54,0,0,9,205,0,1\
7,7,15,0,0,8,102,0,0,8,38,0,0,9,173,0,0,8,6,0,0,8,134,0,0,8,70,0,0,9,237,0,16,7\
,9,0,0,8,94,0,0,8,30,0,0,9,157,0,20,7,99,0,0,8,126,0,0,8,62,0,0,9,221, 0,18,7,2\
7,0,0,8,110,0,0,8,46,0,0,9,189,0,0,8,14,0,0,8,142,0,0,8,78,0,0,9,253,0,96,7,0,0\
,0,8,81,0,0,8,17,0,21,8,131,0,18,7,31,0,0,8,113,0,0,8,49,0,0,9,195,0,16,7,10,0,\
0,8,97,0,0,8,33,0,0,9,163,0,0,8,1,0,0,8,129,0,0,8,65,0,0,9,227,0,16,7,6,0,0,8,8\
9,0,0,8,25,0,0,9,147,0,19,7,59,0,0,8,121,0,0,8,57,0,0,9,211,0,17,7,17,0,0,8,105\
,0,0,8,41,0,0,9,179,0,0,8,9,0,0,8,137,0,0,8,73,0,0,9,243,0,16,7,4,0,0,8,85,0,0,\
8,21,0,16,8,2,1,19,7,43,0,0,8,117,0,0,8,53,0,0,9,203,0,17,7,13,0,0,8,101,0,0,8,\
37,0,0,9,171,0,0,8, 5,0,0,8,133,0,0,8,69,0,0,9,235,0,16,7,8,0,0,8,93,0,0,8,29,0\
,0,9,155,0,20,7,83,0,0,8,125,0,0,8,61,0,0,9,219,0,18,7,23,0,0,8,109,0,0,8,45,0,\
0,9,187,0,0,8,13,0,0,8,141,0,0,8,77,0,0,9,251,0,16,7,3,0,0,8,83,0,0,8,19,0,21,8\
,195,0,19,7,35,0,0,8,115,0,0,8,51,0,0,9,199,0,17,7,11,0,0,8,99,0,0,8,35,0,0,9,1\
67,0,0,8,3,0,0,8,131,0,0,8,67,0,0,9,231,0,16,7,7,0,0,8,91,0,0,8,27,0,0,9,151,0,\
20,7,67,0,0,8,123,0,0,8,59,0,0,9,215,0,18,7,19,0,0,8,107,0,0,8,43,0,0,9,183,0,0\
,8,11,0,0,8,139,0,0,8,75,0,0,9,247,0,16,7,5,0,0, 8,87,0,0,8,23,0,64,8,0,0,19,7,\
51,0,0,8,119,0,0,8,55,0,0,9,207,0,17,7,15,0,0,8,103,0,0,8,39,0,0,9,175,0,0,8,7,\
0,0,8,135,0,0,8,71,0,0,9,239,0,16,7,9,0,0,8,95,0,0,8,31,0,0,9,159,0,20,7,99,0,0\
,8,127,0,0,8,63,0,0,9,223,0,18,7,27,0,0,8,111,0,0,8,47,0,0,9,191,0,0,8,15,0,0,8\
,143,0,0,8,79,0,0,9,255,0,16,5,1,0,23,5,1,1,19,5,17,0,27,5,1,16,17,5,5,0,25,5,1\
,4,21,5,65,0,29,5,1,64,16,5,3,0,24,5,1,2,20,5,33,0,28,5,1,32,18,5,9,0,26,5,1,8,\
22,5,129,0,64,5,0,0,16,5,2,0,23,5,129,1,19,5,25,0,27,5,1,24,17,5,7,0,25,5,1, 6,\
21,5,97,0,29,5,1,96,16,5,4,0,24,5,1,3,20,5,49,0,28,5,1,48,18,5,13,0,26,5,1,12,2\
2,5,193,0,64,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,\
0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,2,0,0,0,2,0,0,0,2,0,0,0,2,0,0,0,3,0,0,0,3,0\
,0,0,3,0,0,0,3,0,0,0,4,0,0,0,4,0,0,0,4,0,0,0,4,0,0,0,5,0,0,0,5,0,0,0,5,0,0,0,5,\
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,2,0,0,0,2\
,0,0,0,3,0,0,0,3,0,0,0,4,0,0,0,4,0,0,0,5,0,0,0,5,0,0,0,6,0,0,0,6,0,0,0,7,0,0,0,\
7,0,0,0,8,0,0,0,8,0,0,0,9, 0,0,0,9,0,0,0,10,0,0,0,10,0,0,0,11,0,0,0,11,0,0,0,12\
,0,0,0,12,0,0,0,13,0,0,0,13,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,\
0,0,2,0,0,0,3,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,150\
,48,7,119,44,97,14,238,186,81,9,153,25,196,109,7,143,244,106,112,53,165,99,233,\
163,149,100,158,50,136,219,14,164,184,220,121,30,233,213,224,136,217,210,151,43\
,76,182,9,189,124,177,126,7,45,184,231,145,29,191,144, 100,16,183,29,242,32,176\
,106,72,113,185,243,222,65,190,132,125,212,218,26,235,228,221,109,81,181,212,24\
4,199,133,211,131,86,152,108,19,192,168,107,100,122,249,98,253,236,201,101,138,\
79,92,1,20,217,108,6,99,99,61,15,250,245,13,8,141,200,32,110,59,94,16,105,76,22\
8,65,96,213,114,113,103,162,209,228,3,60,71,212,4,75,253,133,13,210,107,181,10,\
165,250,168,181,53,108,152,178,66,214,201,187,219,64,249,188,172,227,108,216,50\
,117,92,223,69,207,13,214,220,89,61,209,171,172,48,217,38,58,0,222,81,128,81,21\
5,200, 22,97,208,191,181,244,180,33,35,196,179,86,153,149,186,207,15,165,189,18\
4,158,184,2,40,8,136,5,95,178,217,12,198,36,233,11,177,135,124,111,47,17,76,104\
,88,171,29,97,193,61,45,102,182,144,65,220,118,6,113,219,1,188,32,210,152,42,16\
,213,239,137,133,177,113,31,181,182,6,165,228,191,159,51,212,184,232,162,201,7,\
120,52,249,0,15,142,168,9,150,24,152,14,225,187,13,106,127,45,61,109,8,151,108,\
100,145,1,92,99,230,244,81,107,107,98,97,108,28,216,48,101,133,78,0,98,242,237,\
149,6,108,123,165,1,27,193,244,8,130, 87,196,15,245,198,217,176,101,80,233,183,\
18,234,184,190,139,124,136,185,252,223,29,221,98,73,45,218,21,243,124,211,140,1\
01,76,212,251,88,97,178,77,206,81,181,58,116,0,188,163,226,48,187,212,65,165,22\
3,74,215,149,216,61,109,196,209,164,251,244,214,211,106,233,105,67,252,217,110,\
52,70,136,103,173,208,184,96,218,115,45,4,68,229,29,3,51,95,76,10,170,201,124,1\
3,221,60,113,5,80,170,65,2,39,16,16,11,190,134,32,12,201,37,181,104,87,179,133,\
111,32,9,212,102,185,159,228,97,206,14,249,222,94,152,201,217,41, 34,152,208,17\
6,180,168,215,199,23,61,179,89,129,13,180,46,59,92,189,183,173,108,186,192,32,1\
31,184,237,182,179,191,154,12,226,182,3,154,210,177,116,57,71,213,234,175,119,2\
10,157,21,38,219,4,131,22,220,115,18,11,99,227,132,59,100,148,62,106,109,13,168\
,90,106,122,11,207,14,228,157,255,9,147,39,174,0,10,177,158,7,125,68,147,15,240\
,210,163,8,135,104,242,1,30,254,194,6,105,93,87,98,247,203,103,101,128,113,54,1\
08,25,231,6,107,110,118,27,212,254,224,43,211,137,90,122,218,16,204,74,221,103,\
111,223,185,249, 249,239,190,142,67,190,183,23,213,142,176,96,232,163,214,214,1\
26,147,209,161,196,194,216,56,82,242,223,79,241,103,187,209,103,87,188,166,221,\
6,181,63,75,54,178,72,218,43,13,216,76,27,10,175,246,74,3,54,96,122,4,65,195,23\
9,96,223,85,223,103,168,239,142,110,49,121,190,105,70,140,179,97,203,26,131,102\
,188,160,210,111,37,54,226,104,82,149,119,12,204,3,71,11,187,185,22,2,34,47,38,\
5,85,190,59,186,197,40,11,189,178,146,90,180,43,4,106,179,92,167,255,215,194,49\
,207,208,181,139,158,217,44,29,174,222,91,176, 194,100,155,38,242,99,236,156,16\
3,106,117,10,147,109,2,169,6,9,156,63,54,14,235,133,103,7,114,19,87,0,5,130,74,\
191,149,20,122,184,226,174,43,177,123,56,27,182,12,155,142,210,146,13,190,213,2\
29,183,239,220,124,33,223,219,11,212,210,211,134,66,226,212,241,248,179,221,104\
,110,131,218,31,205,22,190,129,91,38,185,246,225,119,176,111,119,71,183,24,230,\
90,8,136,112,106,15,255,202,59,6,102,92,11,1,17,255,158,101,143,105,174,98,248,\
211,255,107,97,69,207,108,22,120,226,10,160,238,210,13,215,84,131,4,78,194, 179\
,3,57,97,38,103,167,247,22,96,208,77,71,105,73,219,119,110,62,74,106,209,174,22\
0,90,214,217,102,11,223,64,240,59,216,55,83,174,188,169,197,158,187,222,127,207\
,178,71,233,255,181,48,28,242,189,189,138,194,186,202,48,147,179,83,166,163,180\
,36,5,54,208,186,147,6,215,205,41,87,222,84,191,103,217,35,46,122,102,179,184,7\
4,97,196,2,27,104,93,148,43,111,42,55,190,11,180,161,142,12,195,27,223,5,90,141\
,239,2,45,0,0,0,0,65,49,27,25,130,98,54,50,195,83,45,43,4,197,108,100,69,244,11\
9,125,134,167,90,86,199, 150,65,79,8,138,217,200,73,187,194,209,138,232,239,250\
,203,217,244,227,12,79,181,172,77,126,174,181,142,45,131,158,207,28,152,135,81,\
18,194,74,16,35,217,83,211,112,244,120,146,65,239,97,85,215,174,46,20,230,181,5\
5,215,181,152,28,150,132,131,5,89,152,27,130,24,169,0,155,219,250,45,176,154,20\
3,54,169,93,93,119,230,28,108,108,255,223,63,65,212,158,14,90,205,162,36,132,14\
9,227,21,159,140,32,70,178,167,97,119,169,190,166,225,232,241,231,208,243,232,3\
6,131,222,195,101,178,197,218,170,174,93,93,235,159, 70,68,40,204,107,111,105,2\
53,112,118,174,107,49,57,239,90,42,32,44,9,7,11,109,56,28,18,243,54,70,223,178,\
7,93,198,113,84,112,237,48,101,107,244,247,243,42,187,182,194,49,162,117,145,28\
,137,52,160,7,144,251,188,159,23,186,141,132,14,121,222,169,37,56,239,178,60,25\
5,121,243,115,190,72,232,106,125,27,197,65,60,42,222,88,5,79,121,240,68,126,98,\
233,135,45,79,194,198,28,84,219,1,138,21,148,64,187,14,141,131,232,35,166,194,2\
17,56,191,13,197,160,56,76,244,187,33,143,167,150,10,206,150,141,19,9,0,204,92,\
72, 49,215,69,139,98,250,110,202,83,225,119,84,93,187,186,21,108,160,163,214,63\
,141,136,151,14,150,145,80,152,215,222,17,169,204,199,210,250,225,236,147,203,2\
50,245,92,215,98,114,29,230,121,107,222,181,84,64,159,132,79,89,88,18,14,22,25,\
35,21,15,218,112,56,36,155,65,35,61,167,107,253,101,230,90,230,124,37,9,203,87,\
100,56,208,78,163,174,145,1,226,159,138,24,33,204,167,51,96,253,188,42,175,225,\
36,173,238,208,63,180,45,131,18,159,108,178,9,134,171,36,72,201,234,21,83,208,4\
1,70,126,251,104,119,101,226,246, 121,63,47,183,72,36,54,116,27,9,29,53,42,18,4\
,242,188,83,75,179,141,72,82,112,222,101,121,49,239,126,96,254,243,230,231,191,\
194,253,254,124,145,208,213,61,160,203,204,250,54,138,131,187,7,145,154,120,84,\
188,177,57,101,167,168,75,152,131,59,10,169,152,34,201,250,181,9,136,203,174,16\
,79,93,239,95,14,108,244,70,205,63,217,109,140,14,194,116,67,18,90,243,2,35,65,\
234,193,112,108,193,128,65,119,216,71,215,54,151,6,230,45,142,197,181,0,165,132\
,132,27,188,26,138,65,113,91,187,90,104,152,232,119,67,217,217, 108,90,30,79,45\
,21,95,126,54,12,156,45,27,39,221,28,0,62,18,0,152,185,83,49,131,160,144,98,174\
,139,209,83,181,146,22,197,244,221,87,244,239,196,148,167,194,239,213,150,217,2\
46,233,188,7,174,168,141,28,183,107,222,49,156,42,239,42,133,237,121,107,202,17\
2,72,112,211,111,27,93,248,46,42,70,225,225,54,222,102,160,7,197,127,99,84,232,\
84,34,101,243,77,229,243,178,2,164,194,169,27,103,145,132,48,38,160,159,41,184,\
174,197,228,249,159,222,253,58,204,243,214,123,253,232,207,188,107,169,128,253,\
90,178,153,62, 9,159,178,127,56,132,171,176,36,28,44,241,21,7,53,50,70,42,30,11\
5,119,49,7,180,225,112,72,245,208,107,81,54,131,70,122,119,178,93,99,78,215,250\
,203,15,230,225,210,204,181,204,249,141,132,215,224,74,18,150,175,11,35,141,182\
,200,112,160,157,137,65,187,132,70,93,35,3,7,108,56,26,196,63,21,49,133,14,14,4\
0,66,152,79,103,3,169,84,126,192,250,121,85,129,203,98,76,31,197,56,129,94,244,\
35,152,157,167,14,179,220,150,21,170,27,0,84,229,90,49,79,252,153,98,98,215,216\
,83,121,206,23,79,225,73,86,126,250,80,149, 45,215,123,212,28,204,98,19,138,141\
,45,82,187,150,52,145,232,187,31,208,217,160,6,236,243,126,94,173,194,101,71,11\
0,145,72,108,47,160,83,117,232,54,18,58,169,7,9,35,106,84,36,8,43,101,63,17,228\
,121,167,150,165,72,188,143,102,27,145,164,39,42,138,189,224,188,203,242,161,14\
1,208,235,98,222,253,192,35,239,230,217,189,225,188,20,252,208,167,13,63,131,13\
8,38,126,178,145,63,185,36,208,112,248,21,203,105,59,70,230,66,122,119,253,91,1\
81,107,101,220,244,90,126,197,55,9,83,238,118,56,72,247,177,174,9,184,240, 159,\
18,161,51,204,63,138,114,253,36,147,0,0,0,0,55,106,194,1,110,212,132,3,89,190,7\
0,2,220,168,9,7,235,194,203,6,178,124,141,4,133,22,79,5,184,81,19,14,143,59,209\
,15,214,133,151,13,225,239,85,12,100,249,26,9,83,147,216,8,10,45,158,10,61,71,9\
2,11,112,163,38,28,71,201,228,29,30,119,162,31,41,29,96,30,172,11,47,27,155,97,\
237,26,194,223,171,24,245,181,105,25,200,242,53,18,255,152,247,19,166,38,177,17\
,145,76,115,16,20,90,60,21,35,48,254,20,122,142,184,22,77,228,122,23,224,70,77,\
56,215,44,143,57,142,146, 201,59,185,248,11,58,60,238,68,63,11,132,134,62,82,58\
,192,60,101,80,2,61,88,23,94,54,111,125,156,55,54,195,218,53,1,169,24,52,132,19\
1,87,49,179,213,149,48,234,107,211,50,221,1,17,51,144,229,107,36,167,143,169,37\
,254,49,239,39,201,91,45,38,76,77,98,35,123,39,160,34,34,153,230,32,21,243,36,3\
3,40,180,120,42,31,222,186,43,70,96,252,41,113,10,62,40,244,28,113,45,195,118,1\
79,44,154,200,245,46,173,162,55,47,192,141,154,112,247,231,88,113,174,89,30,115\
,153,51,220,114,28,37,147,119,43,79,81,118,114,241,23, 116,69,155,213,117,120,2\
20,137,126,79,182,75,127,22,8,13,125,33,98,207,124,164,116,128,121,147,30,66,12\
0,202,160,4,122,253,202,198,123,176,46,188,108,135,68,126,109,222,250,56,111,23\
3,144,250,110,108,134,181,107,91,236,119,106,2,82,49,104,53,56,243,105,8,127,17\
5,98,63,21,109,99,102,171,43,97,81,193,233,96,212,215,166,101,227,189,100,100,1\
86,3,34,102,141,105,224,103,32,203,215,72,23,161,21,73,78,31,83,75,121,117,145,\
74,252,99,222,79,203,9,28,78,146,183,90,76,165,221,152,77,152,154,196,70,175,24\
0,6, 71,246,78,64,69,193,36,130,68,68,50,205,65,115,88,15,64,42,230,73,66,29,14\
0,139,67,80,104,241,84,103,2,51,85,62,188,117,87,9,214,183,86,140,192,248,83,18\
7,170,58,82,226,20,124,80,213,126,190,81,232,57,226,90,223,83,32,91,134,237,102\
,89,177,135,164,88,52,145,235,93,3,251,41,92,90,69,111,94,109,47,173,95,128,27,\
53,225,183,113,247,224,238,207,177,226,217,165,115,227,92,179,60,230,107,217,25\
4,231,50,103,184,229,5,13,122,228,56,74,38,239,15,32,228,238,86,158,162,236,97,\
244,96,237,228,226,47,232,211,136, 237,233,138,54,171,235,189,92,105,234,240,18\
4,19,253,199,210,209,252,158,108,151,254,169,6,85,255,44,16,26,250,27,122,216,2\
51,66,196,158,249,117,174,92,248,72,233,0,243,127,131,194,242,38,61,132,240,17,\
87,70,241,148,65,9,244,163,43,203,245,250,149,141,247,205,255,79,246,96,93,120,\
217,87,55,186,216,14,137,252,218,57,227,62,219,188,245,113,222,139,159,179,223,\
210,33,245,221,229,75,55,220,216,12,107,215,239,102,169,214,182,216,239,212,129\
,178,45,213,4,164,98,208,51,206,160,209,106,112,230,211,93,26, 36,210,16,254,94\
,197,39,148,156,196,126,42,218,198,73,64,24,199,204,86,87,194,251,60,149,195,16\
2,130,211,193,149,232,17,192,168,175,77,203,159,197,143,202,198,123,201,200,241\
,17,11,201,116,7,68,204,67,109,134,205,26,211,192,207,45,185,2,206,64,150,175,1\
45,119,252,109,144,46,66,43,146,25,40,233,147,156,62,166,150,171,84,100,151,242\
,234,34,149,197,128,224,148,248,199,188,159,207,173,126,158,150,19,56,156,161,1\
21,250,157,36,111,181,152,19,5,119,153,74,187,49,155,125,209,243,154,48,53,137,\
141,7,95,75, 140,94,225,13,142,105,139,207,143,236,157,128,138,219,247,66,139,1\
30,73,4,137,181,35,198,136,136,100,154,131,191,14,88,130,230,176,30,128,209,218\
,220,129,84,204,147,132,99,166,81,133,58,24,23,135,13,114,213,134,160,208,226,1\
69,151,186,32,168,206,4,102,170,249,110,164,171,124,120,235,174,75,18,41,175,18\
,172,111,173,37,198,173,172,24,129,241,167,47,235,51,166,118,85,117,164,65,63,1\
83,165,196,41,248,160,243,67,58,161,170,253,124,163,157,151,190,162,208,115,196\
,181,231,25,6,180,190,167,64,182,137,205, 130,183,12,219,205,178,59,177,15,179,\
98,15,73,177,85,101,139,176,104,34,215,187,95,72,21,186,6,246,83,184,49,156,145\
,185,180,138,222,188,131,224,28,189,218,94,90,191,237,52,152,190,0,0,0,0,101,10\
3,188,184,139,200,9,170,238,175,181,18,87,151,98,143,50,240,222,55,220,95,107,3\
7,185,56,215,157,239,40,180,197,138,79,8,125,100,224,189,111,1,135,1,215,184,19\
1,214,74,221,216,106,242,51,119,223,224,86,16,99,88,159,87,25,80,250,48,165,232\
,20,159,16,250,113,248,172,66,200,192,123,223,173,167,199,103,67,8,114, 117,38,\
111,206,205,112,127,173,149,21,24,17,45,251,183,164,63,158,208,24,135,39,232,20\
7,26,66,143,115,162,172,32,198,176,201,71,122,8,62,175,50,160,91,200,142,24,181\
,103,59,10,208,0,135,178,105,56,80,47,12,95,236,151,226,240,89,133,135,151,229,\
61,209,135,134,101,180,224,58,221,90,79,143,207,63,40,51,119,134,16,228,234,227\
,119,88,82,13,216,237,64,104,191,81,248,161,248,43,240,196,159,151,72,42,48,34,\
90,79,87,158,226,246,111,73,127,147,8,245,199,125,167,64,213,24,192,252,109,78,\
208,159,53,43,183,35, 141,197,24,150,159,160,127,42,39,25,71,253,186,124,32,65,\
2,146,143,244,16,247,232,72,168,61,88,20,155,88,63,168,35,182,144,29,49,211,247\
,161,137,106,207,118,20,15,168,202,172,225,7,127,190,132,96,195,6,210,112,160,9\
4,183,23,28,230,89,184,169,244,60,223,21,76,133,231,194,209,224,128,126,105,14,\
47,203,123,107,72,119,195,162,15,13,203,199,104,177,115,41,199,4,97,76,160,184,\
217,245,152,111,68,144,255,211,252,126,80,102,238,27,55,218,86,77,39,185,14,40,\
64,5,182,198,239,176,164,163,136,12,28,26,176,219, 129,127,215,103,57,145,120,2\
10,43,244,31,110,147,3,247,38,59,102,144,154,131,136,63,47,145,237,88,147,41,84\
,96,68,180,49,7,248,12,223,168,77,30,186,207,241,166,236,223,146,254,137,184,46\
,70,103,23,155,84,2,112,39,236,187,72,240,113,222,47,76,201,48,128,249,219,85,2\
31,69,99,156,160,63,107,249,199,131,211,23,104,54,193,114,15,138,121,203,55,93,\
228,174,80,225,92,64,255,84,78,37,152,232,246,115,136,139,174,22,239,55,22,248,\
64,130,4,157,39,62,188,36,31,233,33,65,120,85,153,175,215,224,139,202,176,92,51\
, 59,182,89,237,94,209,229,85,176,126,80,71,213,25,236,255,108,33,59,98,9,70,13\
5,218,231,233,50,200,130,142,142,112,212,158,237,40,177,249,81,144,95,86,228,13\
0,58,49,88,58,131,9,143,167,230,110,51,31,8,193,134,13,109,166,58,181,164,225,6\
4,189,193,134,252,5,47,41,73,23,74,78,245,175,243,118,34,50,150,17,158,138,120,\
190,43,152,29,217,151,32,75,201,244,120,46,174,72,192,192,1,253,210,165,102,65,\
106,28,94,150,247,121,57,42,79,151,150,159,93,242,241,35,229,5,25,107,77,96,126\
,215,245,142,209,98,231,235,182, 222,95,82,142,9,194,55,233,181,122,217,70,0,10\
4,188,33,188,208,234,49,223,136,143,86,99,48,97,249,214,34,4,158,106,154,189,16\
6,189,7,216,193,1,191,54,110,180,173,83,9,8,21,154,78,114,29,255,41,206,165,17,\
134,123,183,116,225,199,15,205,217,16,146,168,190,172,42,70,17,25,56,35,118,165\
,128,117,102,198,216,16,1,122,96,254,174,207,114,155,201,115,202,34,241,164,87,\
71,150,24,239,169,57,173,253,204,94,17,69,6,238,77,118,99,137,241,206,141,38,68\
,220,232,65,248,100,81,121,47,249,52,30,147,65,218,177,38,83, 191,214,154,235,2\
33,198,249,179,140,161,69,11,98,14,240,25,7,105,76,161,190,81,155,60,219,54,39,\
132,53,153,146,150,80,254,46,46,153,185,84,38,252,222,232,158,18,113,93,140,119\
,22,225,52,206,46,54,169,171,73,138,17,69,230,63,3,32,129,131,187,118,145,224,2\
27,19,246,92,91,253,89,233,73,152,62,85,241,33,6,130,108,68,97,62,212,170,206,1\
39,198,207,169,55,126,56,65,127,214,93,38,195,110,179,137,118,124,214,238,202,1\
96,111,214,29,89,10,177,161,225,228,30,20,243,129,121,168,75,215,105,203,19,178\
,14,119,171, 92,161,194,185,57,198,126,1,128,254,169,156,229,153,21,36,11,54,16\
0,54,110,81,28,142,167,22,102,134,194,113,218,62,44,222,111,44,73,185,211,148,2\
40,129,4,9,149,230,184,177,123,73,13,163,30,46,177,27,72,62,210,67,45,89,110,25\
1,195,246,219,233,166,145,103,81,31,169,176,204,122,206,12,116,148,97,185,102,2\
41,6,5,222,0,0,0,0,119,7,48,150,238,14,97,44,153,9,81,186,7,109,196,25,112,106,\
244,143,233,99,165,53,158,100,149,163,14,219,136,50,121,220,184,164,224,213,233\
,30,151,210,217,136,9,182,76,43,126,177, 124,189,231,184,45,7,144,191,29,145,29\
,183,16,100,106,176,32,242,243,185,113,72,132,190,65,222,26,218,212,125,109,221\
,228,235,244,212,181,81,131,211,133,199,19,108,152,86,100,107,168,192,253,98,24\
9,122,138,101,201,236,20,1,92,79,99,6,108,217,250,15,61,99,141,8,13,245,59,110,\
32,200,76,105,16,94,213,96,65,228,162,103,113,114,60,3,228,209,75,4,212,71,210,\
13,133,253,165,10,181,107,53,181,168,250,66,178,152,108,219,187,201,214,172,188\
,249,64,50,216,108,227,69,223,92,117,220,214,13,207,171,209,61,89,38, 217,48,17\
2,81,222,0,58,200,215,81,128,191,208,97,22,33,180,244,181,86,179,196,35,207,186\
,149,153,184,189,165,15,40,2,184,158,95,5,136,8,198,12,217,178,177,11,233,36,47\
,111,124,135,88,104,76,17,193,97,29,171,182,102,45,61,118,220,65,144,1,219,113,\
6,152,210,32,188,239,213,16,42,113,177,133,137,6,182,181,31,159,191,228,165,232\
,184,212,51,120,7,201,162,15,0,249,52,150,9,168,142,225,14,152,24,127,106,13,18\
7,8,109,61,45,145,100,108,151,230,99,92,1,107,107,81,244,28,108,97,98,133,101,4\
8,216,242,98,0,78, 108,6,149,237,27,1,165,123,130,8,244,193,245,15,196,87,101,1\
76,217,198,18,183,233,80,139,190,184,234,252,185,136,124,98,221,29,223,21,218,4\
5,73,140,211,124,243,251,212,76,101,77,178,97,88,58,181,81,206,163,188,0,116,21\
2,187,48,226,74,223,165,65,61,216,149,215,164,209,196,109,211,214,244,251,67,10\
5,233,106,52,110,217,252,173,103,136,70,218,96,184,208,68,4,45,115,51,3,29,229,\
170,10,76,95,221,13,124,201,80,5,113,60,39,2,65,170,190,11,16,16,201,12,32,134,\
87,104,181,37,32,111,133,179,185,102,212,9,206, 97,228,159,94,222,249,14,41,217\
,201,152,176,208,152,34,199,215,168,180,89,179,61,23,46,180,13,129,183,189,92,5\
9,192,186,108,173,237,184,131,32,154,191,179,182,3,182,226,12,116,177,210,154,2\
34,213,71,57,157,210,119,175,4,219,38,21,115,220,22,131,227,99,11,18,148,100,59\
,132,13,109,106,62,122,106,90,168,228,14,207,11,147,9,255,157,10,0,174,39,125,7\
,158,177,240,15,147,68,135,8,163,210,30,1,242,104,105,6,194,254,247,98,87,93,12\
8,101,103,203,25,108,54,113,110,107,6,231,254,212,27,118,137,211,43,224,16, 218\
,122,90,103,221,74,204,249,185,223,111,142,190,239,249,23,183,190,67,96,176,142\
,213,214,214,163,232,161,209,147,126,56,216,194,196,79,223,242,82,209,187,103,2\
41,166,188,87,103,63,181,6,221,72,178,54,75,216,13,43,218,175,10,27,76,54,3,74,\
246,65,4,122,96,223,96,239,195,168,103,223,85,49,110,142,239,70,105,190,121,203\
,97,179,140,188,102,131,26,37,111,210,160,82,104,226,54,204,12,119,149,187,11,7\
1,3,34,2,22,185,85,5,38,47,197,186,59,190,178,189,11,40,43,180,90,146,92,179,10\
6,4,194,215,255,167,181, 208,207,49,44,217,158,139,91,222,174,29,155,100,194,17\
6,236,99,242,38,117,106,163,156,2,109,147,10,156,9,6,169,235,14,54,63,114,7,103\
,133,5,0,87,19,149,191,74,130,226,184,122,20,123,177,43,174,12,182,27,56,146,21\
0,142,155,229,213,190,13,124,220,239,183,11,219,223,33,134,211,210,212,241,212,\
226,66,104,221,179,248,31,218,131,110,129,190,22,205,246,185,38,91,111,176,119,\
225,24,183,71,119,136,8,90,230,255,15,106,112,102,6,59,202,17,1,11,92,143,101,1\
58,255,248,98,174,105,97,107,255,211,22,108,207,69,160, 10,226,120,215,13,210,2\
38,78,4,131,84,57,3,179,194,167,103,38,97,208,96,22,247,73,105,71,77,62,110,119\
,219,174,209,106,74,217,214,90,220,64,223,11,102,55,216,59,240,169,188,174,83,2\
22,187,158,197,71,178,207,127,48,181,255,233,189,189,242,28,202,186,194,138,83,\
179,147,48,36,180,163,166,186,208,54,5,205,215,6,147,84,222,87,41,35,217,103,19\
1,179,102,122,46,196,97,74,184,93,104,27,2,42,111,43,148,180,11,190,55,195,12,1\
42,161,90,5,223,27,45,2,239,141,0,0,0,0,25,27,49,65,50,54,98,130,43,45,83,195,1\
00,108, 197,4,125,119,244,69,86,90,167,134,79,65,150,199,200,217,138,8,209,194,\
187,73,250,239,232,138,227,244,217,203,172,181,79,12,181,174,126,77,158,131,45,\
142,135,152,28,207,74,194,18,81,83,217,35,16,120,244,112,211,97,239,65,146,46,1\
74,215,85,55,181,230,20,28,152,181,215,5,131,132,150,130,27,152,89,155,0,169,24\
,176,45,250,219,169,54,203,154,230,119,93,93,255,108,108,28,212,65,63,223,205,9\
0,14,158,149,132,36,162,140,159,21,227,167,178,70,32,190,169,119,97,241,232,225\
,166,232,243,208,231,195,222,131,36, 218,197,178,101,93,93,174,170,68,70,159,23\
5,111,107,204,40,118,112,253,105,57,49,107,174,32,42,90,239,11,7,9,44,18,28,56,\
109,223,70,54,243,198,93,7,178,237,112,84,113,244,107,101,48,187,42,243,247,162\
,49,194,182,137,28,145,117,144,7,160,52,23,159,188,251,14,132,141,186,37,169,22\
2,121,60,178,239,56,115,243,121,255,106,232,72,190,65,197,27,125,88,222,42,60,2\
40,121,79,5,233,98,126,68,194,79,45,135,219,84,28,198,148,21,138,1,141,14,187,6\
4,166,35,232,131,191,56,217,194,56,160,197,13,33,187,244,76,10,150, 167,143,19,\
141,150,206,92,204,0,9,69,215,49,72,110,250,98,139,119,225,83,202,186,187,93,84\
,163,160,108,21,136,141,63,214,145,150,14,151,222,215,152,80,199,204,169,17,236\
,225,250,210,245,250,203,147,114,98,215,92,107,121,230,29,64,84,181,222,89,79,1\
32,159,22,14,18,88,15,21,35,25,36,56,112,218,61,35,65,155,101,253,107,167,124,2\
30,90,230,87,203,9,37,78,208,56,100,1,145,174,163,24,138,159,226,51,167,204,33,\
42,188,253,96,173,36,225,175,180,63,208,238,159,18,131,45,134,9,178,108,201,72,\
36,171,208,83,21, 234,251,126,70,41,226,101,119,104,47,63,121,246,54,36,72,183,\
29,9,27,116,4,18,42,53,75,83,188,242,82,72,141,179,121,101,222,112,96,126,239,4\
9,231,230,243,254,254,253,194,191,213,208,145,124,204,203,160,61,131,138,54,250\
,154,145,7,187,177,188,84,120,168,167,101,57,59,131,152,75,34,152,169,10,9,181,\
250,201,16,174,203,136,95,239,93,79,70,244,108,14,109,217,63,205,116,194,14,140\
,243,90,18,67,234,65,35,2,193,108,112,193,216,119,65,128,151,54,215,71,142,45,2\
30,6,165,0,181,197,188,27,132,132,113,65,138, 26,104,90,187,91,67,119,232,152,9\
0,108,217,217,21,45,79,30,12,54,126,95,39,27,45,156,62,0,28,221,185,152,0,18,16\
0,131,49,83,139,174,98,144,146,181,83,209,221,244,197,22,196,239,244,87,239,194\
,167,148,246,217,150,213,174,7,188,233,183,28,141,168,156,49,222,107,133,42,239\
,42,202,107,121,237,211,112,72,172,248,93,27,111,225,70,42,46,102,222,54,225,12\
7,197,7,160,84,232,84,99,77,243,101,34,2,178,243,229,27,169,194,164,48,132,145,\
103,41,159,160,38,228,197,174,184,253,222,159,249,214,243,204,58,207,232, 253,1\
23,128,169,107,188,153,178,90,253,178,159,9,62,171,132,56,127,44,28,36,176,53,7\
,21,241,30,42,70,50,7,49,119,115,72,112,225,180,81,107,208,245,122,70,131,54,99\
,93,178,119,203,250,215,78,210,225,230,15,249,204,181,204,224,215,132,141,175,1\
50,18,74,182,141,35,11,157,160,112,200,132,187,65,137,3,35,93,70,26,56,108,7,49\
,21,63,196,40,14,14,133,103,79,152,66,126,84,169,3,85,121,250,192,76,98,203,129\
,129,56,197,31,152,35,244,94,179,14,167,157,170,21,150,220,229,84,0,27,252,79,4\
9,90,215,98,98,153,206, 121,83,216,73,225,79,23,80,250,126,86,123,215,45,149,98\
,204,28,212,45,141,138,19,52,150,187,82,31,187,232,145,6,160,217,208,94,126,243\
,236,71,101,194,173,108,72,145,110,117,83,160,47,58,18,54,232,35,9,7,169,8,36,8\
4,106,17,63,101,43,150,167,121,228,143,188,72,165].concat([164,145,27,102,189,1\
38,42,39,242,203,188,224,235,208,141,161,192,253,222,98,217,230,239,35,20,188,2\
25,189,13,167,208,252,38,138,131,63,63,145,178,126,112,208,36,185,105,203,21,24\
8,66,230,70,59,91,253,119,122,220,101,107,181,197,126, 90,244,238,83,9,55,247,7\
2,56,118,184,9,174,177,161,18,159,240,138,63,204,51,147,36,253,114,0,0,0,0,1,19\
4,106,55,3,132,212,110,2,70,190,89,7,9,168,220,6,203,194,235,4,141,124,178,5,79\
,22,133,14,19,81,184,15,209,59,143,13,151,133,214,12,85,239,225,9,26,249,100,8,\
216,147,83,10,158,45,10,11,92,71,61,28,38,163,112,29,228,201,71,31,162,119,30,3\
0,96,29,41,27,47,11,172,26,237,97,155,24,171,223,194,25,105,181,245,18,53,242,2\
00,19,247,152,255,17,177,38,166,16,115,76,145,21,60,90,20,20,254,48,35,22,184,1\
42, 122,23,122,228,77,56,77,70,224,57,143,44,215,59,201,146,142,58,11,248,185,6\
3,68,238,60,62,134,132,11,60,192,58,82,61,2,80,101,54,94,23,88,55,156,125,111,5\
3,218,195,54,52,24,169,1,49,87,191,132,48,149,213,179,50,211,107,234,51,17,1,22\
1,36,107,229,144,37,169,143,167,39,239,49,254,38,45,91,201,35,98,77,76,34,160,3\
9,123,32,230,153,34,33,36,243,21,42,120,180,40,43,186,222,31,41,252,96,70,40,62\
,10,113,45,113,28,244,44,179,118,195,46,245,200,154,47,55,162,173,112,154,141,1\
92,113,88,231,247,115,30,89,174, 114,220,51,153,119,147,37,28,118,81,79,43,116,\
23,241,114,117,213,155,69,126,137,220,120,127,75,182,79,125,13,8,22,124,207,98,\
33,121,128,116,164,120,66,30,147,122,4,160,202,123,198,202,253,108,188,46,176,1\
09,126,68,135,111,56,250,222,110,250,144,233,107,181,134,108,106,119,236,91,104\
,49,82,2,105,243,56,53,98,175,127,8,99,109,21,63,97,43,171,102,96,233,193,81,10\
1,166,215,212,100,100,189,227,102,34,3,186,103,224,105,141,72,215,203,32,73,21,\
161,23,75,83,31,78,74,145,117,121,79,222,99,252,78,28,9,203, 76,90,183,146,77,1\
52,221,165,70,196,154,152,71,6,240,175,69,64,78,246,68,130,36,193,65,205,50,68,\
64,15,88,115,66,73,230,42,67,139,140,29,84,241,104,80,85,51,2,103,87,117,188,62\
,86,183,214,9,83,248,192,140,82,58,170,187,80,124,20,226,81,190,126,213,90,226,\
57,232,91,32,83,223,89,102,237,134,88,164,135,177,93,235,145,52,92,41,251,3,94,\
111,69,90,95,173,47,109,225,53,27,128,224,247,113,183,226,177,207,238,227,115,1\
65,217,230,60,179,92,231,254,217,107,229,184,103,50,228,122,13,5,239,38,74,56,2\
38,228,32, 15,236,162,158,86,237,96,244,97,232,47,226,228,233,237,136,211,235,1\
71,54,138,234,105,92,189,253,19,184,240,252,209,210,199,254,151,108,158,255,85,\
6,169,250,26,16,44,251,216,122,27,249,158,196,66,248,92,174,117,243,0,233,72,24\
2,194,131,127,240,132,61,38,241,70,87,17,244,9,65,148,245,203,43,163,247,141,14\
9,250,246,79,255,205,217,120,93,96,216,186,55,87,218,252,137,14,219,62,227,57,2\
22,113,245,188,223,179,159,139,221,245,33,210,220,55,75,229,215,107,12,216,214,\
169,102,239,212,239,216,182,213,45,178, 129,208,98,164,4,209,160,206,51,211,230\
,112,106,210,36,26,93,197,94,254,16,196,156,148,39,198,218,42,126,199,24,64,73,\
194,87,86,204,195,149,60,251,193,211,130,162,192,17,232,149,203,77,175,168,202,\
143,197,159,200,201,123,198,201,11,17,241,204,68,7,116,205,134,109,67,207,192,2\
11,26,206,2,185,45,145,175,150,64,144,109,252,119,146,43,66,46,147,233,40,25,15\
0,166,62,156,151,100,84,171,149,34,234,242,148,224,128,197,159,188,199,248,158,\
126,173,207,156,56,19,150,157,250,121,161,152,181,111,36,153,119,5, 19,155,49,1\
87,74,154,243,209,125,141,137,53,48,140,75,95,7,142,13,225,94,143,207,139,105,1\
38,128,157,236,139,66,247,219,137,4,73,130,136,198,35,181,131,154,100,136,130,8\
8,14,191,128,30,176,230,129,220,218,209,132,147,204,84,133,81,166,99,135,23,24,\
58,134,213,114,13,169,226,208,160,168,32,186,151,170,102,4,206,171,164,110,249,\
174,235,120,124,175,41,18,75,173,111,172,18,172,173,198,37,167,241,129,24,166,5\
1,235,47,164,117,85,118,165,183,63,65,160,248,41,196,161,58,67,243,163,124,253,\
170,162,190,151, 157,181,196,115,208,180,6,25,231,182,64,167,190,183,130,205,13\
7,178,205,219,12,179,15,177,59,177,73,15,98,176,139,101,85,187,215,34,104,186,2\
1,72,95,184,83,246,6,185,145,156,49,188,222,138,180,189,28,224,131,191,90,94,21\
8,190,152,52,237,0,0,0,0,184,188,103,101,170,9,200,139,18,181,175,238,143,98,15\
1,87,55,222,240,50,37,107,95,220,157,215,56,185,197,180,40,239,125,8,79,138,111\
,189,224,100,215,1,135,1,74,214,191,184,242,106,216,221,224,223,119,51,88,99,16\
,86,80,25,87,159,232,165,48,250,250,16,159,20, 66,172,248,113,223,123,192,200,1\
03,199,167,173,117,114,8,67,205,206,111,38,149,173,127,112,45,17,24,21,63,164,1\
83,251,135,24,208,158,26,207,232,39,162,115,143,66,176,198,32,172,8,122,71,201,\
160,50,175,62,24,142,200,91,10,59,103,181,178,135,0,208,47,80,56,105,151,236,95\
,12,133,89,240,226,61,229,151,135,101,134,135,209,221,58,224,180,207,143,79,90,\
119,51,40,63,234,228,16,134,82,88,119,227,64,237,216,13,248,81,191,104,240,43,2\
48,161,72,151,159,196,90,34,48,42,226,158,87,79,127,73,111,246,199,245,8,147, 2\
13,64,167,125,109,252,192,24,53,159,208,78,141,35,183,43,159,150,24,197,39,42,1\
27,160,186,253,71,25,2,65,32,124,16,244,143,146,168,72,232,247,155,20,88,61,35,\
168,63,88,49,29,144,182,137,161,247,211,20,118,207,106,172,202,168,15,190,127,7\
,225,6,195,96,132,94,160,112,210,230,28,23,183,244,169,184,89,76,21,223,60,209,\
194,231,133,105,126,128,224,123,203,47,14,195,119,72,107,203,13,15,162,115,177,\
104,199,97,4,199,41,217,184,160,76,68,111,152,245,252,211,255,144,238,102,80,12\
6,86,218,55,27,14,185,39,77, 182,5,64,40,164,176,239,198,28,12,136,163,129,219,\
176,26,57,103,215,127,43,210,120,145,147,110,31,244,59,38,247,3,131,154,144,102\
,145,47,63,136,41,147,88,237,180,68,96,84,12,248,7,49,30,77,168,223,166,241,207\
,186,254,146,223,236,70,46,184,137,84,155,23,103,236,39,112,2,113,240,72,187,20\
1,76,47,222,219,249,128,48,99,69,231,85,107,63,160,156,211,131,199,249,193,54,1\
04,23,121,138,15,114,228,93,55,203,92,225,80,174,78,84,255,64,246,232,152,37,17\
4,139,136,115,22,55,239,22,4,130,64,248,188,62,39,157,33, 233,31,36,153,85,120,\
65,139,224,215,175,51,92,176,202,237,89,182,59,85,229,209,94,71,80,126,176,255,\
236,25,213,98,59,33,108,218,135,70,9,200,50,233,231,112,142,142,130,40,237,158,\
212,144,81,249,177,130,228,86,95,58,88,49,58,167,143,9,131,31,51,110,230,13,134\
,193,8,181,58,166,109,189,64,225,164,5,252,134,193,23,73,41,47,175,245,78,74,50\
,34,118,243,138,158,17,150,152,43,190,120,32,151,217,29,120,244,201,75,192,72,1\
74,46,210,253,1,192,106,65,102,165,247,150,94,28,79,42,57,121,93,159,150,151,22\
9,35,241, 242,77,107,25,5,245,215,126,96,231,98,209,142,95,222,182,235,194,9,14\
2,82,122,181,233,55,104,0,70,217,208,188,33,188,136,223,49,234,48,99,86,143,34,\
214,249,97,154,106,158,4,7,189,166,189,191,1,193,216,173,180,110,54,21,8,9,83,2\
9,114,78,154,165,206,41,255,183,123,134,17,15,199,225,116,146,16,217,205,42,172\
,190,168,56,25,17,70,128,165,118,35,216,198,102,117,96,122,1,16,114,207,174,254\
,202,115,201,155,87,164,241,34,239,24,150,71,253,173,57,169,69,17,94,204,118,77\
,238,6,206,241,137,99,220,68,38,141,100, 248,65,232,249,47,121,81,65,147,30,52,\
83,38,177,218,235,154,214,191,179,249,198,233,11,69,161,140,25,240,14,98,161,76\
,105,7,60,155,81,190,132,39,54,219,150,146,153,53,46,46,254,80,38,84,185,153,15\
8,232,222,252,140,93,113,18,52,225,22,119,169,54,46,206,17,138,73,171,3,63,230,\
69,187,131,129,32,227,224,145,118,91,92,246,19,73,233,89,253,241,85,62,152,108,\
130,6,33,212,62,97,68,198,139,206,170,126,55,169,207,214,127,65,56,110,195,38,9\
3,124,118,137,179,196,202,238,214,89,29,214,111,225,161,177,10,243,20, 30,228,7\
5,168,121,129,19,203,105,215,171,119,14,178,185,194,161,92,1,126,198,57,156,169\
,254,128,36,21,153,229,54,160,54,11,142,28,81,110,134,102,22,167,62,218,113,194\
,44,111,222,44,148,211,185,73,9,4,129,240,177,184,230,149,163,13,73,123,27,177,\
46,30,67,210,62,72,251,110,89,45,233,219,246,195,81,103,145,166,204,176,169,31,\
116,12,206,122,102,185,97,148,222,5,6,241,0,0,0,0,0,0,0,0,6,0,0,0,4,0,4,0,8,0,4\
,0,2,0,0,0,4,0,5,0,16,0,8,0,2,0,0,0,4,0,6,0,32,0,32,0,2,0,0,0,4,0,4,0,16,0,16,0\
,4,0,0,0,8,0,16,0,32, 0,32,0,4,0,0,0,8,0,16,0,128,0,128,0,4,0,0,0,8,0,32,0,128,\
0,0,1,4,0,0,0,32,0,128,0,2,1,0,4,4,0,0,0,32,0,2,1,2,1,0,16,4,0,0,0,16,17,18,0,8\
,7,9,6,10,5,11,4,12,3,13,2,14,1,15,0,0,0,0,0,0,0,0,0,1,0,0,0,2,0,0,0,3,0,0,0,4,\
0,0,0,5,0,0,0,6,0,0,0,7,0,0,0,8,0,0,0,10,0,0,0,12,0,0,0,14,0,0,0,16,0,0,0,20,0,\
0,0,24,0,0,0,28,0,0,0,32,0,0,0,40,0,0,0,48,0,0,0,56,0,0,0,64,0,0,0,80,0,0,0,96,\
0,0,0,112,0,0,0,128,0,0,0,160,0,0,0,192,0,0,0,224,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\
,1,0,0,0,2,0,0,0,3,0,0,0,4,0,0,0,6,0,0,0,8,0,0,0, 12,0,0,0,16,0,0,0,24,0,0,0,32\
,0,0,0,48,0,0,0,64,0,0,0,96,0,0,0,128,0,0,0,192,0,0,0,0,1,0,0,128,1,0,0,0,2,0,0\
,0,3,0,0,0,4,0,0,0,6,0,0,0,8,0,0,0,12,0,0,0,16,0,0,0,24,0,0,0,32,0,0,0,48,0,0,0\
,64,0,0,0,96,0,0,98,117,102,102,101,114,32,101,114,114,111,114,0,0,0,0,105,110,\
115,117,102,102,105,99,105,101,110,116,32,109,101,109,111,114,121,0,0,0,0,0,115\
,116,114,101,97,109,32,101,114,114,111,114,0,0,0,0,101,114,114,111,114,58,32,37\
,100,92,110,0,0,0,0,0,115,116,114,99,109,112,40,98,117,102,102,101,114,44,32, 9\
8,117,102,102,101,114,51,41,32,61,61,32,48,0,0,0,0,100,101,99,111,109,112,114,1\
01,115,115,101,100,83,105,122,101,32,61,61,32,115,105,122,101,0,0,0,0,0,0,0,0,4\
7,116,109,112,47,101,109,115,99,114,105,112,116,101,110,95,116,101,109,112,47,1\
22,108,105,98,46,99,0,0,0,0,0,115,105,122,101,115,58,32,37,100,44,37,100,10,0,0\
,0,0,1,2,3,4,5,6,7,8,8,9,9,10,10,11,11,12,12,12,12,13,13,13,13,14,14,14,14,15,1\
5,15,15,16,16,16,16,16,16,16,16,17,17,17,17,17,17,17,17,18,18,18,18,18,18,18,18\
,19,19,19,19,19,19,19,19,20, 20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,21,21\
,21,21,21,21,21,21,21,21,21,21,21,21,21,21,22,22,22,22,22,22,22,22,22,22,22,22,\
22,22,22,22,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,24,24,24,24,24,24,2\
4,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,25\
,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,\
25,25,25,25,25,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,2\
6,26,26,26,26,26,26,26,26,26,26,27,27,27,27,27,27,27,27, 27,27,27,27,27,27,27,2\
7,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,28,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,\
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,\
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,\
0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,\
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,\
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,\
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1,2,3,4,4,5,5,6,6,6,6,7,7,7,7\
,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,1\
0,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12\
,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,13,13,13,13,\
13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,1\
3,13,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14\
,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14, 14,14,14,14,14\
,14,14,14,14,14,14,14,14,14,14,14,14,14,15,15,15,15,15,15,15,15,15,15,15,15,15,\
15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,1\
5,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,0,0,1\
6,17,18,18,19,19,20,20,20,20,21,21,21,21,22,22,22,22,22,22,22,22,23,23,23,23,23\
,23,23,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,\
25,25,25,25,25,25,25,25,25,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,2\
6,26,26,26,26, 26,26,26,26,26,26,26,26,26,26,27,27,27,27,27,27,27,27,27,27,27,2\
7,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,28,28,28,28,28,28\
,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,\
28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,2\
8,28,28,28,28,28,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29\
,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,\
29,29,29,29,29,29,29,29,29,29,29,29,29,29, 29,29,29,100,111,105,116,0,0,0,0]),"\
i8",Da,8);var Za=ra(P(12,"i8",O),8);z(0==Za%8);var $a=5,ab=6,bb=9,U=13,cb=21,db\
=22,eb=0;function V(a){return L[eb>>2]=a}var fb=P(1,"i32*",O),W=P(1,"i32*",O),g\
b=P(1,"i32*",O),hb=P(1,"i32*",O),ib=2,X=[r],jb=n;function kb(a,b){if("string"!=\
=typeof a)return r;b===k&&(b="/");a&&"/"==a[0]&&(b="");for(var c=(b+"/"+a).spli\
t("/").reverse(),d=[""];c.length;){var e=c.pop();""==e||"."==e||(".."==e?1<d.le\
ngth&&d.pop():d.push(e))}return 1==d.length?"/":d.join("/")} function lb(a,b,c)\
{var d={N:t,k:t,error:0,name:r,path:r,object:r,w:t,A:r,z:r},a=kb(a);if("/"==a)d\
.N=n,d.k=d.w=n,d.name="/",d.path=d.A="/",d.object=d.z=mb;else if(a!==r)for(var \
c=c||0,a=a.slice(1).split("/"),e=mb,f=[""];a.length;){1==a.length&&e.c&&(d.w=n,\
d.A=1==f.length?"/":f.join("/"),d.z=e,d.name=a[0]);var g=a.shift();if(e.c)if(e.\
C){if(!e.a.hasOwnProperty(g)){d.error=2;break}}else{d.error=U;break}else{d.erro\
r=20;break}e=e.a[g];if(e.link&&!(b&&0==a.length)){if(40<c){d.error=92;break}d=k\
b(e.link, f.join("/"));d=lb([d].concat(a).join("/"),b,c+1);break}f.push(g);0==a\
.length&&(d.k=n,d.path=f.join("/"),d.object=e)}return d}function nb(a){ob();a=l\
b(a,k);if(a.k)return a.object;V(a.error);return r} function pb(a,b,c,d,e){a||(a\
="/");"string"===typeof a&&(a=nb(a));a||(V(U),j(Error("Parent path must exist."\
)));a.c||(V(20),j(Error("Parent must be a folder.")));!a.write&&!jb&&(V(U),j(Er\
ror("Parent folder must be writeable.")));if(!b||"."==b||".."==b)V(2),j(Error("\
Name must not be empty."));a.a.hasOwnProperty(b)&&(V(17),j(Error("Can\'t overwr\
ite object.")));a.a[b]={C:d===k?n:d,write:e===k?t:e,timestamp:Date.now(),M:ib++\
};for(var f in c)c.hasOwnProperty(f)&&(a.a[b][f]=c[f]);return a.a[b]} function \
qb(a,b,c,d){return pb(a,b,{c:n,b:t,a:{}},c,d)}function rb(a,b,c,d){a=nb(a);a===\
r&&j(Error("Invalid parent."));for(b=b.split("/").reverse();b.length;){var e=b.\
pop();e&&(a.a.hasOwnProperty(e)||qb(a,e,c,d),a=a.a[e])}return a}function sb(a,b\
,c,d,e){c.c=t;return pb(a,b,c,d,e)}function tb(a,b,c,d,e){if("string"===typeof \
c){for(var f=Array(c.length),g=0,i=c.length;g<i;++g)f[g]=c.charCodeAt(g);c=f}c=\
{b:t,a:c.subarray?c.subarray(0):c};return sb(a,b,c,d,e)} function Y(a,b,c,d){!c\
&&!d&&j(Error("A device must have at least one callback defined."));return sb(a\
,b,{b:n,input:c,d:d},Boolean(c),Boolean(d))}function ob(){mb||(mb={C:n,write:n,\
c:n,b:t,timestamp:Date.now(),M:1,a:{}})}var ub,mb;function vb(a,b,c){a=X[a];if(\
!a)return-1;a.sender(Q.subarray(b,b+c));return c} function wb(a,b,c,d){a=X[a];i\
f(!a||a.object.b)return V(bb),-1;if(a.f){if(a.object.c)return V(cb),-1;if(0>c||\
0>d)return V(db),-1;for(var e=a.object.a;e.length<d;)e.push(0);for(var f=0;f<c;\
f++)e[d+f]=Q[b+f|0];a.object.timestamp=Date.now();return f}V(U);return-1} funct\
ion xb(a,b,c){var d=X[a];if(d&&"socket"in d)return vb(a,b,c);if(d){if(d.f){if(0\
>c)return V(db),-1;if(d.object.b){if(d.object.d){for(a=0;a<c;a++)try{d.object.d\
(J[b+a|0])}catch(e){return V($a),-1}d.object.timestamp=Date.now();return a}V(ab\
);return-1}b=wb(a,b,c,d.position);-1!=b&&(d.position+=b);return b}V(U);return-1\
}V(bb);return-1}function yb(a,b,c,d){c*=b;if(0==c)return 0;a=xb(d,a,c);return-1\
==a?(X[d]&&(X[d].error=n),0):Math.floor(a/b)}Module._strlen=zb; function Ab(a){\
return 0>a||0===a&&-Infinity===1/a} function Bb(a,b){function c(a){var c;"doubl\
e"===a?c=N[b+e>>3]:"i64"==a?(c=[L[b+e>>2],L[b+(e+8)>>2]],e+=8):(a="i32",c=L[b+e\
>>2]);e+=Math.max(Math.max(ja(a),x),8);return c}for(var d=a,e=0,f=[],g,i;;){var\
 l=d;g=J[d];if(0===g)break;i=J[d+1|0];if(37==g){var y=t,E=t,A=t,q=t;a:for(;;){s\
witch(i){case 43:y=n;break;case 45:E=n;break;case 35:A=n;break;case 48:if(q)bre\
ak a;else{q=n;break}default:break a}d++;i=J[d+1|0]}var B=0;if(42==i)B=c("i32"),\
d++,i=J[d+1|0];else for(;48<=i&&57>=i;)B=10*B+(i-48),d++,i=J[d+ 1|0];var K=t;if\
(46==i){var m=0,K=n;d++;i=J[d+1|0];if(42==i)m=c("i32"),d++;else for(;;){i=J[d+1\
|0];if(48>i||57<i)break;m=10*m+(i-48);d++}i=J[d+1|0]}else m=6;var p;switch(Stri\
ng.fromCharCode(i)){case "h":i=J[d+2|0];104==i?(d++,p=1):p=2;break;case "l":i=J\
[d+2|0];108==i?(d++,p=8):p=4;break;case "L":case "q":case "j":p=8;break;case "z\
":case "t":case "I":p=4;break;default:p=r}p&&d++;i=J[d+1|0];switch(String.fromC\
harCode(i)){case "d":case "i":case "u":case "o":case "x":case "X":case "p":l=10\
0==i||105==i; p=p||4;g=c("i"+8*p);var h;8==p&&(g=117==i?+(g[0]>>>0)+4294967296*\
+(g[1]>>>0):+(g[0]>>>0)+4294967296*+(g[1]|0));4>=p&&(g=(l?Ra:Qa)(g&Math.pow(256\
,p)-1,8*p));var H=Math.abs(g),l="";if(100==i||105==i)h=Ra(g,8*p).toString(10);e\
lse if(117==i)h=Qa(g,8*p).toString(10),g=Math.abs(g);else if(111==i)h=(A?"0":""\
)+H.toString(8);else if(120==i||88==i){l=A&&0!=g?"0x":"";if(0>g){g=-g;h=(H-1).t\
oString(16);H=[];for(A=0;A<h.length;A++)H.push((15-parseInt(h[A],16)).toString(\
16));for(h=H.join("");h.length<2*p;)h="f"+ h}else h=H.toString(16);88==i&&(l=l.\
toUpperCase(),h=h.toUpperCase())}else 112==i&&(0===H?h="(nil)":(l="0x",h=H.toSt\
ring(16)));if(K)for(;h.length<m;)h="0"+h;for(y&&(l=0>g?"-"+l:"+"+l);l.length+h.\
length<B;)E?h+=" ":q?h="0"+h:l=" "+l;h=l+h;h.split("").forEach(function(a){f.pu\
sh(a.charCodeAt(0))});break;case "f":case "F":case "e":case "E":case "g":case "\
G":g=c("double");if(isNaN(g))h="nan",q=t;else if(isFinite(g)){K=t;p=Math.min(m,\
20);if(103==i||71==i)K=n,m=m||1,p=parseInt(g.toExponential(p).split("e")[1], 10\
),m>p&&-4<=p?(i=(103==i?"f":"F").charCodeAt(0),m-=p+1):(i=(103==i?"e":"E").char\
CodeAt(0),m--),p=Math.min(m,20);if(101==i||69==i)h=g.toExponential(p),/[eE][-+]\
\\d$/.test(h)&&(h=h.slice(0,-1)+"0"+h.slice(-1));else if(102==i||70==i)h=g.toFi\
xed(p),0===g&&Ab(g)&&(h="-"+h);l=h.split("e");if(K&&!A)for(;1<l[0].length&&-1!=\
l[0].indexOf(".")&&("0"==l[0].slice(-1)||"."==l[0].slice(-1));)l[0]=l[0].slice(\
0,-1);else for(A&&-1==h.indexOf(".")&&(l[0]+=".");m>p++;)l[0]+="0";h=l[0]+(1<l.\
length?"e"+l[1]:"");69==i&& (h=h.toUpperCase());y&&0<=g&&(h="+"+h)}else h=(0>g?\
"-":"")+"inf",q=t;for(;h.length<B;)h=E?h+" ":q&&("-"==h[0]||"+"==h[0])?h[0]+"0"\
+h.slice(1):(q?"0":" ")+h;97>i&&(h=h.toUpperCase());h.split("").forEach(functio\
n(a){f.push(a.charCodeAt(0))});break;case "s":q=(y=c("i8*"))?zb(y):6;K&&(q=Math\
.min(q,m));if(!E)for(;q<B--;)f.push(32);if(y)for(A=0;A<q;A++)f.push(Q[y++|0]);e\
lse f=f.concat(S("(null)".substr(0,q),n));if(E)for(;q<B--;)f.push(32);break;cas\
e "c":for(E&&f.push(c("i8"));0<--B;)f.push(32);E||f.push(c("i8")); break;case "\
n":E=c("i32*");L[E>>2]=f.length;break;case "%":f.push(g);break;default:for(A=l;\
A<d+2;A++)f.push(J[A])}d+=2}else f.push(g),d+=1}return f}function Cb(a,b,c){c=B\
b(b,c);b=ha();a=yb(P(c,"i8",Ca),1,c.length,a);ia(b);return a}function Db(a,b,c)\
{for(var d=0;d<c;){var e=Q[a+d|0],f=Q[b+d|0];if(e==f&&0==e)break;if(0==e)return\
-1;if(0==f)return 1;if(e==f)d++;else return e>f?1:-1}return 0}Module._memset=Eb\
;Module._memcpy=Fb; function Z(a){Z.J||(D=D+4095>>12<<12,Z.J=n,z(pa),Z.I=pa,pa=\
function(){F("cannot dynamically allocate, sbrk now has control")});var b=D;0!=\
a&&Z.I(a);return b}function Gb(a,b){return xb(b,a,zb(a))}function Hb(a,b){var c\
=Qa(a&255);J[Hb.D|0]=c;return-1==xb(b,Hb.D,1)?(X[b]&&(X[b].error=n),-1):c}var I\
b=t,Jb=t,Kb=t,Lb=t,Mb=k,Nb=k,Ob=[];function Pb(){var a=Module.canvas;Ob.forEach\
(function(b){b(a.width,a.height)})} function Qb(){var a=Module.canvas;this.S=a.\
width;this.R=a.height;a.width=screen.width;a.height=screen.height;a=Ga[SDL.scre\
en+0*x>>2];L[SDL.screen+0*x>>2]=a|8388608;Pb()}function Rb(){var a=Module.canva\
s;a.width=this.S;a.height=this.R;a=Ga[SDL.screen+0*x>>2];L[SDL.screen+0*x>>2]=a\
&-8388609;Pb()}var Sb,Tb,Ub,Vb; Ma.unshift({l:function(){if(!Module.noFSInit&&!\
ub){var a,b,c,d=function(a){a===r||10===a?(b.h(b.buffer.join("")),b.buffer=[]):\
b.buffer.push(i.B(a))};z(!ub,"FS.init was previously called. If you want to ini\
tialize later with custom parameters, remove any earlier calls (note that one i\
s automatically added to the generated code)");ub=n;ob();a=a||Module.stdin;b=b|\
|Module.stdout;c=c||Module.stderr;var e=n,f=n,g=n;a||(e=t,a=function(){if(!a.j|\
|!a.j.length){var b;"undefined"!=typeof window&&"function"== typeof window.prom\
pt?(b=window.prompt("Input: "),b===r&&(b=String.fromCharCode(0))):"function"==t\
ypeof readline&&(b=readline());b||(b="");a.j=S(b+"\\n",n)}return a.j.shift()});\
var i=new ma;b||(f=t,b=d);b.h||(b.h=Module.print);b.buffer||(b.buffer=[]);c||(g\
=t,c=d);c.h||(c.h=Module.print);c.buffer||(c.buffer=[]);try{qb("/","tmp",n,n)}c\
atch(l){}var d=qb("/","dev",n,n),y=Y(d,"stdin",a),E=Y(d,"stdout",r,b);c=Y(d,"st\
derr",r,c);Y(d,"tty",a,b);Y(d,"null",u(),u());X[1]={path:"/dev/stdin",object:y,\
position:0, u:n,f:t,t:t,v:!e,error:t,q:t,F:[]};X[2]={path:"/dev/stdout",object:\
E,position:0,u:t,f:n,t:t,v:!f,error:t,q:t,F:[]};X[3]={path:"/dev/stderr",object\
:c,position:0,u:t,f:n,t:t,v:!g,error:t,q:t,F:[]};L[fb>>2]=1;L[W>>2]=2;L[gb>>2]=\
3;rb("/","dev/shm/tmp",n,n);for(e=X.length;e<Math.max(fb,W,gb)+4;e++)X[e]=r;X[f\
b]=X[1];X[W]=X[2];X[gb]=X[3];P([P([0,0,0,0,fb,0,0,0,W,0,0,0,gb,0,0,0],"void*",0\
)],"void*",Da,hb)}}});Na.push({l:function(){jb=t}}); Oa.push({l:function(){ub&&\
(X[2]&&0<X[2].object.d.buffer.length&&X[2].object.d(10),X[3]&&0<X[3].object.d.b\
uffer.length&&X[3].object.d(10))}});Module.FS_createFolder=qb;Module.FS_createP\
ath=rb;Module.FS_createDataFile=tb; Module.FS_createPreloadedFile=function(a,b,\
c,d,e,f,g,i){function l(){Kb=document.pointerLockElement===q||document.mozPoint\
erLockElement===q||document.webkitPointerLockElement===q}function y(a){return{j\
pg:"image/jpeg",jpeg:"image/jpeg",png:"image/png",bmp:"image/bmp",ogg:"audio/og\
g",wav:"audio/wav",mp3:"audio/mpeg"}[a.substr(a.lastIndexOf(".")+1)]}function E\
(c){function h(c){i||tb(a,b,c,d,e);f&&f();Wa("cp "+B)}var l=t;Module.preloadPlu\
gins.forEach(function(a){!l&&a.canHandle(B)&&(a.handle(c,B,h,function(){g&& g()\
;Wa("cp "+B)}),l=n)});l||h(c)}Module.preloadPlugins||(Module.preloadPlugins=[])\
;if(!Sb&&!v){Sb=n;try{new Blob,Tb=n}catch(A){Tb=t,console.log("warning: no blob\
 constructor, cannot create blobs with mimetypes")}Ub="undefined"!=typeof MozBl\
obBuilder?MozBlobBuilder:"undefined"!=typeof WebKitBlobBuilder?WebKitBlobBuilde\
r:!Tb?console.log("warning: no BlobBuilder"):r;Vb="undefined"!=typeof window?wi\
ndow.URL?window.URL:window.webkitURL:console.log("warning: cannot create object\
 URLs");Module.preloadPlugins.push({canHandle:function(a){return!Module.W&& /\
\\.(jpg|jpeg|png|bmp)$/.exec(a)},handle:function(a,b,c,d){var e=r;if(Tb)try{e=n\
ew Blob([a],{type:y(b)})}catch(f){var g="Blob constructor present but fails: "+\
f+"; falling back to blob builder";la||(la={});la[g]||(la[g]=1,Module.g(g))}e||\
(e=new Ub,e.append((new Uint8Array(a)).buffer),e=e.getBlob());var i=Vb.createOb\
jectURL(e),h=new Image;h.onload=function(){z(h.complete,"Image "+b+" could not \
be decoded");var d=document.createElement("canvas");d.width=h.width;d.height=h.\
height;d.getContext("2d").drawImage(h, 0,0);Module.preloadedImages[b]=d;Vb.revo\
keObjectURL(i);c&&c(a)};h.onerror=function(){console.log("Image "+i+" could not\
 be decoded");d&&d()};h.src=i}});Module.preloadPlugins.push({canHandle:function\
(a){return!Module.V&&a.substr(-4)in{".ogg":1,".wav":1,".mp3":1}},handle:functio\
n(a,b,c,d){function e(d){g||(g=n,Module.preloadedAudios[b]=d,c&&c(a))}function \
f(){g||(g=n,Module.preloadedAudios[b]=new Audio,d&&d())}var g=t;if(Tb){try{var \
i=new Blob([a],{type:y(b)})}catch(h){return f()}var i=Vb.createObjectURL(i), l=\
new Audio;l.addEventListener("canplaythrough",function(){e(l)},t);l.onerror=fun\
ction(){if(!g){console.log("warning: browser could not fully decode audio "+b+"\
, trying slower base64 approach");for(var c="",d=0,f=0,i=0;i<a.length;i++){d=d<\
<8|a[i];for(f+=8;6<=f;)var h=d>>f-6&63,f=f-6,c=c+"ABCDEFGHIJKLMNOPQRSTUVWXYZabc\
defghijklmnopqrstuvwxyz0123456789+/"[h]}2==f?(c+="ABCDEFGHIJKLMNOPQRSTUVWXYZabc\
defghijklmnopqrstuvwxyz0123456789+/"[(d&3)<<4],c+="=="):4==f&&(c+="ABCDEFGHIJKL\
MNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(d& 15)<<2],c+="=");l.src\
="data:audio/x-"+b.substr(-3)+";base64,"+c;e(l)}};l.src=i;setTimeout(function()\
{G||e(l)},1E4)}else return f()}});var q=Module.canvas;q.n=q.requestPointerLock|\
|q.mozRequestPointerLock||q.webkitRequestPointerLock;q.r=document.exitPointerLo\
ck||document.mozExitPointerLock||document.webkitExitPointerLock||u();q.r=q.r.bi\
nd(document);document.addEventListener("pointerlockchange",l,t);document.addEve\
ntListener("mozpointerlockchange",l,t);document.addEventListener("webkitpointer\
lockchange", l,t);Module.elementPointerLock&&q.addEventListener("click",functio\
n(a){!Kb&&q.n&&(q.n(),a.preventDefault())},t)}for(var B,K=[a,b],m=K[0],p=1;p<K.\
length;p++)"/"!=m[m.length-1]&&(m+="/"),m+=K[p];"/"==m[0]&&(m=m.substr(1));B=m;\
Va("cp "+B);if("string"==typeof c){var h=g,H=function(){h?h():j(\'Loading data \
file "\'+c+\'" failed.\')},M=new XMLHttpRequest;M.open("GET",c,n);M.responseTyp\
e="arraybuffer";M.onload=function(){if(200==M.status||0==M.status&&M.response){\
var a=M.response;z(a,\'Loading data file "\'+c+ \'" failed (no arrayBuffer).\')\
;a=new Uint8Array(a);E(a);Wa("al "+c)}else H()};M.onerror=H;M.send(r);Va("al "+\
c)}else E(c)}; Module.FS_createLazyFile=function(a,b,c,d,e){if("undefined"!==ty\
peof XMLHttpRequest){v||j("Cannot do synchronous binary XHRs outside webworkers\
 in modern browsers. Use --embed-file or --preload-file in emcc");var f=functio\
n(){this.m=t;this.e=[]};f.prototype.get=function(a){if(!(a>this.length-1||0>a))\
{var b=a%this.K;return this.L(Math.floor(a/this.K))[b]}};f.prototype.Q=function\
(a){this.L=a};f.prototype.o=function(){var a=new XMLHttpRequest;a.open("HEAD",c\
,t);a.send(r);200<=a.status&&300>a.status|| 304===a.status||j(Error("Couldn\'t \
load "+c+". Status: "+a.status));var b=Number(a.getResponseHeader("Content-leng\
th")),d,e=1048576;if(!((d=a.getResponseHeader("Accept-Ranges"))&&"bytes"===d))e\
=b;var f=this;f.Q(function(a){var d=a*e,g=(a+1)*e-1,g=Math.min(g,b-1);if("undef\
ined"===typeof f.e[a]){var l=f.e;d>g&&j(Error("invalid range ("+d+", "+g+") or \
no bytes requested!"));g>b-1&&j(Error("only "+b+" bytes available! programmer e\
rror!"));var m=new XMLHttpRequest;m.open("GET",c,t);b!==e&&m.setRequestHeader("\
Range", "bytes="+d+"-"+g);"undefined"!=typeof Uint8Array&&(m.responseType="arra\
ybuffer");m.overrideMimeType&&m.overrideMimeType("text/plain; charset=x-user-de\
fined");m.send(r);200<=m.status&&300>m.status||304===m.status||j(Error("Couldn\
\'t load "+c+". Status: "+m.status));d=m.response!==k?new Uint8Array(m.response\
||[]):S(m.responseText||"",n);l[a]=d}"undefined"===typeof f.e[a]&&j(Error("doXH\
R failed!"));return f.e[a]});this.H=b;this.G=e;this.m=n};f=new f;Object.defineP\
roperty(f,"length",{get:function(){this.m|| this.o();return this.H}});Object.de\
fineProperty(f,"chunkSize",{get:function(){this.m||this.o();return this.G}});f=\
{b:t,a:f}}else f={b:t,url:c};return sb(a,b,f,d,e)};Module.FS_createLink=functio\
n(a,b,c,d,e){return sb(a,b,{b:t,link:c},d,e)};Module.FS_createDevice=Y;eb=oa(4)\
;L[eb>>2]=0;Hb.D=P([0],"i8",O); Module.requestFullScreen=function(a,b){function\
 c(){Jb=t;(document.webkitFullScreenElement||document.webkitFullscreenElement||\
document.mozFullScreenElement||document.mozFullscreenElement||document.fullScre\
enElement||document.fullscreenElement)===d?(d.p=document.cancelFullScreen||docu\
ment.mozCancelFullScreen||document.webkitCancelFullScreen,d.p=d.p.bind(document\
),Mb&&d.n(),Jb=n,Nb&&Qb()):Nb&&Rb();if(Module.onFullScreen)Module.onFullScreen(\
Jb)}Mb=a;Nb=b;"undefined"===typeof Mb&&(Mb=n);"undefined"=== typeof Nb&&(Nb=t);\
var d=Module.canvas;Lb||(Lb=n,document.addEventListener("fullscreenchange",c,t)\
,document.addEventListener("mozfullscreenchange",c,t),document.addEventListener\
("webkitfullscreenchange",c,t));d.P=d.requestFullScreen||d.mozRequestFullScreen\
||(d.webkitRequestFullScreen?function(){d.webkitRequestFullScreen(Element.ALLOW\
_KEYBOARD_INPUT)}:r);d.P()}; Module.requestAnimationFrame=function(a){window.re\
questAnimationFrame||(window.requestAnimationFrame=window.requestAnimationFrame\
||window.mozRequestAnimationFrame||window.webkitRequestAnimationFrame||window.m\
sRequestAnimationFrame||window.oRequestAnimationFrame||window.setTimeout);windo\
w.requestAnimationFrame(a)};Module.pauseMainLoop=u();Module.resumeMainLoop=func\
tion(){Ib&&(Ib=t,r())};Module.getUserMedia=function(){window.s||(window.s=navig\
ator.getUserMedia||navigator.mozGetUserMedia);window.s(k)}; Ia=w=ra(C);Ja=Ia+52\
42880;Ka=D=ra(Ja);z(Ka<qa);var Wb=Math.min; var $=(function(global,env,buffer) \
{ "use asm";var a=new global.Int8Array(buffer);var b=new global.Int16Array(buff\
er);var c=new global.Int32Array(buffer);var d=new global.Uint8Array(buffer);var\
 e=new global.Uint16Array(buffer);var f=new global.Uint32Array(buffer);var g=ne\
w global.Float32Array(buffer);var h=new global.Float64Array(buffer);var i=env.S\
TACKTOP|0;var j=env.STACK_MAX|0;var k=env.tempDoublePtr|0;var l=env.ABORT|0;var\
 m=+env.NaN;var n=+env.Infinity;var o=0;var p=0;var q=0;var r=0;var s=0,t=0,u=0\
,v=0,w=0.0,x=0,y=0,z=0,A=0.0;var B=0;var C=0;var D=0;var E=0;var F=0;var G=0;va\
r H=0;var I=0;var J=0;var K=0;var L=global.Math.floor;var M=global.Math.abs;var\
 N=global.Math.sqrt;var O=global.Math.pow;var P=global.Math.cos;var Q=global.Ma\
th.sin;var R=global.Math.tan;var S=global.Math.acos;var T=global.Math.asin;var \
U=global.Math.atan;var V=global.Math.atan2;var W=global.Math.exp;var X=global.M\
ath.log;var Y=global.Math.ceil;var Z=global.Math.imul;var _=env.abort;var $=env\
.assert;var aa=env.asmPrintInt;var ab=env.asmPrintFloat;var ac=env.min;var ad=e\
nv.invoke_ii;var ae=env.invoke_vi;var af=env.invoke_vii;var ag=env.invoke_iiii;\
var ah=env.invoke_v;var ai=env.invoke_iii;var aj=env._strncmp;var ak=env._llvm_\
lifetime_end;var al=env._sysconf;var am=env._abort;var an=env._fprintf;var ao=e\
nv._printf;var ap=env.__reallyNegative;var aq=env._fputc;var ar=env._puts;var a\
s=env.___setErrNo;var at=env._fwrite;var au=env._send;var av=env._write;var aw=\
env._fputs;var ax=env.__formatString;var ay=env._free;var az=env.___assert_func\
;var aA=env._pwrite;var aB=env._sbrk;var aC=env.___errno_location;var aD=env._l\
lvm_lifetime_start;var aE=env._llvm_bswap_i32;var aF=env._time;var aG=env._strc\
mp; function aN(a){a=a|0;var b=0;b=i;i=i+a|0;i=i+7>>3<<3;return b|0}function aO\
(){return i|0}function aP(a){a=a|0;i=a}function aQ(a,b){a=a|0;b=b|0;if((o|0)==0\
){o=a;p=b}}function aR(b){b=b|0;a[k]=a[b];a[k+1|0]=a[b+1|0];a[k+2|0]=a[b+2|0];a\
[k+3|0]=a[b+3|0]}function aS(b){b=b|0;a[k]=a[b];a[k+1|0]=a[b+1|0];a[k+2|0]=a[b+\
2|0];a[k+3|0]=a[b+3|0];a[k+4|0]=a[b+4|0];a[k+5|0]=a[b+5|0];a[k+6|0]=a[b+6|0];a[\
k+7|0]=a[b+7|0]}function aT(a){a=a|0;B=a}function aU(a){a=a|0;C=a}function aV(a\
){a=a|0;D=a}function aW(a){a=a|0;E=a}function aX(a){a=a|0;F=a}function aY(a){a=\
a|0;G=a}function aZ(a){a=a|0;H=a}function a_(a){a=a|0;I=a}function a$(a){a=a|0;\
J=a}function a0(a){a=a|0;K=a}function a1(f,g){f=f|0;g=g|0;var h=0,j=0,k=0,l=0,m\
=0,n=0,o=0,p=0,q=0,r=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,\
H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0\
,$=0,aa=0,ab=0,ac=0,ad=0,ae=0,af=0,ag=0,ah=0,ai=0,aj=0,ak=0,al=0,am=0,an=0,ap=0\
,aq=0,ar=0,as=0,at=0,au=0,av=0,aw=0,ax=0,ay=0,aA=0,aB=0,aC=0,aD=0,aF=0,aH=0,aI=\
0,aL=0,aN=0,aO=0,aP=0,aQ=0,aR=0,aS=0,aT=0,aU=0,aV=0,aW=0,aX=0,aY=0,aZ=0,a_=0,a$\
=0,a0=0,a1=0,a2=0,a4=0,a5=0,a6=0,a8=0,bb=0,bc=0,bd=0,be=0,bl=0,bo=0,bp=0,bq=0,b\
r=0,bs=0,bt=0,bu=0,bv=0,bw=0,bx=0,by=0,bz=0,bA=0,bB=0,bC=0,bD=0,bE=0,bF=0,bG=0,\
bH=0,bI=0,bJ=0,bK=0,bL=0,bM=0,bN=0,bO=0,bP=0,bQ=0,bR=0,bS=0,bT=0,bU=0,bV=0,bW=0\
,bX=0,bY=0,bZ=0,b_=0,b$=0,b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0,b7=0,b8=0,b9=0,ca=\
0,cb=0,cc=0,cd=0,ce=0,cf=0,cg=0,ch=0,ci=0,cj=0,ck=0,cl=0,cm=0,cn=0,co=0,cp=0,cq\
=0,cr=0,cs=0,ct=0,cu=0,cv=0,cw=0,cx=0,cy=0,cz=0,cA=0,cB=0,cC=0,cD=0,cE=0,cF=0,c\
G=0,cH=0,cI=0,cJ=0,cK=0,cL=0,cM=0,cN=0,cO=0,cP=0,cQ=0,cR=0,cS=0,cT=0,cU=0,cV=0,\
cW=0,cX=0,cY=0,cZ=0,c_=0,c$=0,c0=0,c1=0,c2=0,c3=0,c4=0,c5=0,c6=0,c7=0,c8=0,c9=0\
,da=0,db=0,dc=0,dd=0,de=0,df=0,dg=0,dh=0,di=0,dj=0,dk=0,dl=0,dm=0,dn=0,dp=0,dq=\
0,dr=0,ds=0,dt=0,du=0,dv=0,dw=0,dx=0,dy=0,dz=0,dA=0,dB=0,dC=0,dD=0,dE=0,dF=0,dG\
=0,dH=0,dI=0,dJ=0,dK=0,dL=0,dM=0,dN=0,dO=0,dP=0,dQ=0,dR=0,dS=0,dT=0,dU=0,dV=0,d\
W=0,dX=0,dY=0,dZ=0,d_=0,d$=0,d0=0,d1=0,d2=0,d3=0,d4=0,d5=0,d6=0,d7=0,d8=0,d9=0,\
ea=0,eb=0,ec=0,ed=0,ee=0,ef=0,eg=0,eh=0,ei=0,ej=0,ek=0,el=0,em=0,en=0,eo=0,ep=0\
,eq=0,er=0,es=0,et=0,eu=0,ev=0,ew=0,ex=0,ey=0,ez=0,eA=0,eB=0,eC=0,eD=0,eE=0,eF=\
0,eG=0,eH=0,eI=0,eJ=0,eK=0,eL=0,eM=0,eN=0,eO=0,eP=0,eQ=0,eR=0,eS=0,eT=0,eU=0,eV\
=0,eW=0,eX=0,eY=0,eZ=0,e_=0,e$=0,e0=0,e1=0,e2=0,e3=0,e4=0,e5=0,e6=0,e7=0,e8=0,e\
9=0,fa=0,fb=0,fc=0,fd=0,fe=0,ff=0,fg=0,fh=0,fi=0,fj=0,fk=0,fl=0,fm=0,fn=0,fo=0,\
fp=0,fq=0,fr=0,fs=0,ft=0,fu=0,fv=0,fw=0,fx=0,fy=0,fz=0,fA=0,fB=0,fC=0,fD=0,fE=0\
,fF=0,fG=0,fH=0,fI=0,fJ=0,fK=0,fL=0,fM=0,fN=0,fO=0,fP=0,fQ=0,fR=0,fS=0,fT=0,fU=\
0,fV=0,fW=0,fX=0,fY=0,fZ=0,f_=0,f$=0,f0=0,f1=0,f2=0,f3=0,f4=0,f5=0,f6=0,f7=0,f8\
=0,f9=0,ga=0,gb=0,gc=0,gd=0,ge=0,gf=0,gg=0,gh=0,gi=0,gj=0,gk=0,gl=0,gm=0,gn=0,g\
o=0,gp=0,gq=0,gr=0,gs=0,gt=0,gu=0,gv=0,gw=0,gx=0,gy=0,gz=0,gA=0,gB=0,gC=0,gD=0,\
gE=0,gF=0,gG=0,gH=0,gI=0,gJ=0,gK=0,gL=0,gM=0,gN=0,gO=0,gP=0,gQ=0,gR=0,gS=0,gT=0\
,gU=0,gV=0,gW=0,gX=0,gY=0,gZ=0,g_=0,g$=0,g0=0,g1=0,g2=0,g3=0,g4=0,g5=0,g6=0,g7=\
0,g8=0,g9=0,ha=0;h=i;i=i+64|0;j=h|0;k=h+8|0;l=c[1046]|0;if((l|0)==0){m=bk(10004\
3)|0;c[1046]=m;n=m}else{n=l}if((c[1044]|0)==0){c[1044]=bk(1e5)|0;o=c[1046]|0}el\
se{o=n}n=k|0;c[n>>2]=f;l=k+4|0;c[l>>2]=1e5;m=k+12|0;c[m>>2]=o;o=k+16|0;c[o>>2]=\
100043;p=k+32|0;c[p>>2]=0;q=k+36|0;c[q>>2]=0;r=k+40|0;c[r>>2]=0;t=k+24|0;c[t>>2\
]=0;c[p>>2]=2;c[r>>2]=0;c[q>>2]=2;u=bf(0,1,5828)|0;L7:do{if((u|0)==0){v=100043}\
else{w=k+28|0;c[w>>2]=u;c[u>>2]=k;c[u+24>>2]=1;c[u+28>>2]=0;c[u+48>>2]=15;x=u+4\
4|0;c[x>>2]=32768;c[u+52>>2]=32767;c[u+80>>2]=15;y=u+76|0;c[y>>2]=32768;c[u+84>\
>2]=32767;c[u+88>>2]=5;z=u+56|0;c[z>>2]=aK[c[p>>2]&3](c[r>>2]|0,32768,2)|0;A=aK\
[c[p>>2]&3](c[r>>2]|0,c[x>>2]|0,2)|0;B=u+64|0;c[B>>2]=A;bm(A|0,0,c[x>>2]<<1|0);\
x=u+68|0;c[x>>2]=aK[c[p>>2]&3](c[r>>2]|0,c[y>>2]|0,2)|0;c[u+5824>>2]=0;y=u+5788\
|0;c[y>>2]=16384;A=aK[c[p>>2]&3](c[r>>2]|0,16384,4)|0;C=A;c[u+8>>2]=A;D=c[y>>2]\
|0;c[u+12>>2]=D<<2;do{if((c[z>>2]|0)!=0){if((c[B>>2]|0)==0){break}if((c[x>>2]|0\
)==0|(A|0)==0){break}c[u+5796>>2]=C+(D>>>1<<1);c[u+5784>>2]=A+(D*3&-1);c[u+132>\
>2]=6;c[u+136>>2]=0;a[u+36|0]=8;y=c[w>>2]|0;if((y|0)==0){v=100043;break L7}if((\
c[p>>2]|0)==0){v=100043;break L7}if((c[q>>2]|0)==0){v=100043;break L7}E=k+20|0;\
c[E>>2]=0;F=k+8|0;c[F>>2]=0;c[t>>2]=0;c[k+44>>2]=2;c[y+20>>2]=0;c[y+16>>2]=c[y+\
8>>2];G=y+24|0;H=c[G>>2]|0;if((H|0)<0){I=-H|0;c[G>>2]=I;J=I}else{J=H}c[y+4>>2]=\
(J|0)!=0?42:113;H=k+48|0;c[H>>2]=(J|0)!=2&1;c[y+40>>2]=0;c[y+2840>>2]=y+148;c[y\
+2848>>2]=1168;c[y+2852>>2]=y+2440;c[y+2860>>2]=1312;c[y+2864>>2]=y+2684;c[y+28\
72>>2]=1336;b[y+5816>>1]=0;c[y+5820>>2]=0;c[y+5812>>2]=8;a7(y);c[y+60>>2]=c[y+4\
4>>2]<<1;I=y+76|0;G=y+68|0;b[(c[G>>2]|0)+((c[I>>2]|0)-1<<1)>>1]=0;bm(c[G>>2]|0,\
0,(c[I>>2]<<1)-2|0);I=c[y+132>>2]|0;c[y+128>>2]=e[12386+(I*12&-1)>>1]|0;c[y+140\
>>2]=e[12384+(I*12&-1)>>1]|0;c[y+144>>2]=e[12388+(I*12&-1)>>1]|0;c[y+124>>2]=e[\
12390+(I*12&-1)>>1]|0;c[y+108>>2]=0;c[y+92>>2]=0;c[y+116>>2]=0;c[y+120>>2]=2;c[\
y+96>>2]=2;c[y+112>>2]=0;c[y+104>>2]=0;c[y+72>>2]=0;y=c[w>>2]|0;I=y;if((y|0)==0\
){v=100043;break L7}L20:do{if((c[m>>2]|0)==0){K=30}else{if((c[n>>2]|0)==0){if((\
c[l>>2]|0)!=0){K=30;break}}G=y+4|0;L=c[G>>2]|0;if((c[o>>2]|0)==0){c[t>>2]=12768\
;break}M=y;c[y>>2]=k;N=y+40|0;c[N>>2]=4;do{if((L|0)==42){if((c[y+24>>2]|0)!=2){\
O=(c[y+48>>2]<<12)-30720|0;do{if((c[y+136>>2]|0)>1){P=0}else{Q=c[y+132>>2]|0;if\
((Q|0)<2){P=0;break}if((Q|0)<6){P=64;break}P=(Q|0)==6?128:192}}while(0);Q=P|O;R\
=y+108|0;S=(c[R>>2]|0)==0?Q:Q|32;Q=(S|(S>>>0)%31>>>0)^31;c[G>>2]=113;S=y+20|0;T\
=c[S>>2]|0;c[S>>2]=T+1;U=y+8|0;a[(c[U>>2]|0)+T|0]=Q>>>8&255;T=c[S>>2]|0;c[S>>2]\
=T+1;a[(c[U>>2]|0)+T|0]=Q&255;if((c[R>>2]|0)!=0){R=c[H>>2]|0;Q=c[S>>2]|0;c[S>>2\
]=Q+1;a[(c[U>>2]|0)+Q|0]=R>>>24&255;Q=c[S>>2]|0;c[S>>2]=Q+1;a[(c[U>>2]|0)+Q|0]=\
R>>>16&255;R=c[H>>2]|0;Q=c[S>>2]|0;c[S>>2]=Q+1;a[(c[U>>2]|0)+Q|0]=R>>>8&255;Q=c\
[S>>2]|0;c[S>>2]=Q+1;a[(c[U>>2]|0)+Q|0]=R&255}c[H>>2]=1;V=c[G>>2]|0;K=54;break}\
c[H>>2]=0;R=y+20|0;Q=c[R>>2]|0;c[R>>2]=Q+1;U=y+8|0;a[(c[U>>2]|0)+Q|0]=31;Q=c[R>\
>2]|0;c[R>>2]=Q+1;a[(c[U>>2]|0)+Q|0]=-117;Q=c[R>>2]|0;c[R>>2]=Q+1;a[(c[U>>2]|0)\
+Q|0]=8;Q=y+28|0;S=c[Q>>2]|0;if((S|0)==0){T=c[R>>2]|0;c[R>>2]=T+1;a[(c[U>>2]|0)\
+T|0]=0;T=c[R>>2]|0;c[R>>2]=T+1;a[(c[U>>2]|0)+T|0]=0;T=c[R>>2]|0;c[R>>2]=T+1;a[\
(c[U>>2]|0)+T|0]=0;T=c[R>>2]|0;c[R>>2]=T+1;a[(c[U>>2]|0)+T|0]=0;T=c[R>>2]|0;c[R\
>>2]=T+1;a[(c[U>>2]|0)+T|0]=0;T=c[y+132>>2]|0;do{if((T|0)==9){W=2}else{if((c[y+\
136>>2]|0)>1){W=4;break}W=(T|0)<2?4:0}}while(0);T=c[R>>2]|0;c[R>>2]=T+1;a[(c[U>\
>2]|0)+T|0]=W;T=c[R>>2]|0;c[R>>2]=T+1;a[(c[U>>2]|0)+T|0]=3;c[G>>2]=113;break}T=\
((c[S+44>>2]|0)!=0?2:0)|(c[S>>2]|0)!=0&1|((c[S+16>>2]|0)==0?0:4)|((c[S+28>>2]|0\
)==0?0:8)|((c[S+36>>2]|0)==0?0:16);O=c[R>>2]|0;c[R>>2]=O+1;a[(c[U>>2]|0)+O|0]=T\
;T=c[(c[Q>>2]|0)+4>>2]&255;O=c[R>>2]|0;c[R>>2]=O+1;a[(c[U>>2]|0)+O|0]=T;T=(c[(c\
[Q>>2]|0)+4>>2]|0)>>>8&255;O=c[R>>2]|0;c[R>>2]=O+1;a[(c[U>>2]|0)+O|0]=T;T=(c[(c\
[Q>>2]|0)+4>>2]|0)>>>16&255;O=c[R>>2]|0;c[R>>2]=O+1;a[(c[U>>2]|0)+O|0]=T;T=(c[(\
c[Q>>2]|0)+4>>2]|0)>>>24&255;O=c[R>>2]|0;c[R>>2]=O+1;a[(c[U>>2]|0)+O|0]=T;T=c[y\
+132>>2]|0;do{if((T|0)==9){X=2}else{if((c[y+136>>2]|0)>1){X=4;break}X=(T|0)<2?4\
:0}}while(0);T=c[R>>2]|0;c[R>>2]=T+1;a[(c[U>>2]|0)+T|0]=X;T=c[(c[Q>>2]|0)+12>>2\
]&255;S=c[R>>2]|0;c[R>>2]=S+1;a[(c[U>>2]|0)+S|0]=T;T=c[Q>>2]|0;if((c[T+16>>2]|0\
)==0){Y=T}else{S=c[T+20>>2]&255;T=c[R>>2]|0;c[R>>2]=T+1;a[(c[U>>2]|0)+T|0]=S;S=\
(c[(c[Q>>2]|0)+20>>2]|0)>>>8&255;T=c[R>>2]|0;c[R>>2]=T+1;a[(c[U>>2]|0)+T|0]=S;Y\
=c[Q>>2]|0}if((c[Y+44>>2]|0)!=0){c[H>>2]=bi(c[H>>2]|0,c[U>>2]|0,c[R>>2]|0)|0}c[\
y+32>>2]=0;c[G>>2]=69;Z=Q;K=56}else{V=L;K=54}}while(0);do{if((K|0)==54){if((V|0\
)!=69){_=V;K=77;break}Z=y+28|0;K=56}}while(0);do{if((K|0)==56){L=c[Z>>2]|0;if((\
c[L+16>>2]|0)==0){c[G>>2]=73;$=L;K=79;break}S=y+20|0;T=c[S>>2]|0;O=y+32|0;aa=c[\
O>>2]|0;L66:do{if(aa>>>0<(c[L+20>>2]&65535)>>>0){ab=y+12|0;ac=y+8|0;ad=T;ae=L;a\
f=T;ag=aa;while(1){if((af|0)==(c[ab>>2]|0)){if((c[ae+44>>2]|0)!=0&af>>>0>ad>>>0\
){c[H>>2]=bi(c[H>>2]|0,(c[ac>>2]|0)+ad|0,af-ad|0)|0}ah=c[w>>2]|0;ai=c[ah+20>>2]\
|0;aj=c[o>>2]|0;ak=ai>>>0>aj>>>0?aj:ai;do{if((ak|0)!=0){ai=c[m>>2]|0;aj=c[ah+16\
>>2]|0;bn(ai|0,aj|0,ak)|0;c[m>>2]=(c[m>>2]|0)+ak;aj=(c[w>>2]|0)+16|0;c[aj>>2]=(\
c[aj>>2]|0)+ak;c[E>>2]=(c[E>>2]|0)+ak;c[o>>2]=(c[o>>2]|0)-ak;aj=(c[w>>2]|0)+20|\
0;c[aj>>2]=(c[aj>>2]|0)-ak;aj=c[w>>2]|0;if((c[aj+20>>2]|0)!=0){break}c[aj+16>>2\
]=c[aj+8>>2]}}while(0);al=c[S>>2]|0;if((al|0)==(c[ab>>2]|0)){break}am=al;an=al;\
ap=c[O>>2]|0;aq=c[Z>>2]|0}else{am=ad;an=af;ap=ag;aq=ae}ak=a[(c[aq+16>>2]|0)+ap|\
0]|0;c[S>>2]=an+1;a[(c[ac>>2]|0)+an|0]=ak;ak=(c[O>>2]|0)+1|0;c[O>>2]=ak;ah=c[Z>\
>2]|0;if(ak>>>0>=(c[ah+20>>2]&65535)>>>0){ar=am;as=ah;break L66}ad=am;ae=ah;af=\
c[S>>2]|0;ag=ak}ar=al;as=c[Z>>2]|0}else{ar=T;as=L}}while(0);do{if((c[as+44>>2]|\
0)==0){at=as}else{L=c[S>>2]|0;if(L>>>0<=ar>>>0){at=as;break}c[H>>2]=bi(c[H>>2]|\
0,(c[y+8>>2]|0)+ar|0,L-ar|0)|0;at=c[Z>>2]|0}}while(0);if((c[O>>2]|0)==(c[at+20>\
>2]|0)){c[O>>2]=0;c[G>>2]=73;$=at;K=79;break}else{_=c[G>>2]|0;K=77;break}}}whil\
e(0);do{if((K|0)==77){if((_|0)!=73){au=_;K=97;break}$=c[y+28>>2]|0;K=79}}while(\
0);do{if((K|0)==79){S=y+28|0;if((c[$+28>>2]|0)==0){c[G>>2]=91;av=S;K=99;break}L\
=y+20|0;T=c[L>>2]|0;aa=y+12|0;Q=y+8|0;R=y+32|0;U=T;ag=T;while(1){if((ag|0)==(c[\
aa>>2]|0)){if((c[(c[S>>2]|0)+44>>2]|0)!=0&ag>>>0>U>>>0){c[H>>2]=bi(c[H>>2]|0,(c\
[Q>>2]|0)+U|0,ag-U|0)|0}T=c[w>>2]|0;af=c[T+20>>2]|0;ae=c[o>>2]|0;ad=af>>>0>ae>>\
>0?ae:af;do{if((ad|0)!=0){af=c[m>>2]|0;ae=c[T+16>>2]|0;bn(af|0,ae|0,ad)|0;c[m>>\
2]=(c[m>>2]|0)+ad;ae=(c[w>>2]|0)+16|0;c[ae>>2]=(c[ae>>2]|0)+ad;c[E>>2]=(c[E>>2]\
|0)+ad;c[o>>2]=(c[o>>2]|0)-ad;ae=(c[w>>2]|0)+20|0;c[ae>>2]=(c[ae>>2]|0)-ad;ae=c\
[w>>2]|0;if((c[ae+20>>2]|0)!=0){break}c[ae+16>>2]=c[ae+8>>2]}}while(0);ad=c[L>>\
2]|0;if((ad|0)==(c[aa>>2]|0)){aw=1;ax=ad;break}else{ay=ad;aA=ad}}else{ay=U;aA=a\
g}ad=c[R>>2]|0;c[R>>2]=ad+1;T=a[(c[(c[S>>2]|0)+28>>2]|0)+ad|0]|0;c[L>>2]=aA+1;a\
[(c[Q>>2]|0)+aA|0]=T;if(T<<24>>24==0){aw=0;ax=ay;break}U=ay;ag=c[L>>2]|0}do{if(\
(c[(c[S>>2]|0)+44>>2]|0)!=0){ag=c[L>>2]|0;if(ag>>>0<=ax>>>0){break}c[H>>2]=bi(c\
[H>>2]|0,(c[Q>>2]|0)+ax|0,ag-ax|0)|0}}while(0);if((aw|0)==0){c[R>>2]=0;c[G>>2]=\
91;av=S;K=99;break}else{au=c[G>>2]|0;K=97;break}}}while(0);do{if((K|0)==97){if(\
(au|0)!=91){aB=au;K=117;break}av=y+28|0;K=99}}while(0);do{if((K|0)==99){if((c[(\
c[av>>2]|0)+36>>2]|0)==0){c[G>>2]=103;aC=av;K=119;break}Q=y+20|0;L=c[Q>>2]|0;ag\
=y+12|0;U=y+8|0;aa=y+32|0;O=L;T=L;while(1){if((T|0)==(c[ag>>2]|0)){if((c[(c[av>\
>2]|0)+44>>2]|0)!=0&T>>>0>O>>>0){c[H>>2]=bi(c[H>>2]|0,(c[U>>2]|0)+O|0,T-O|0)|0}\
L=c[w>>2]|0;ad=c[L+20>>2]|0;ae=c[o>>2]|0;af=ad>>>0>ae>>>0?ae:ad;do{if((af|0)!=0\
){ad=c[m>>2]|0;ae=c[L+16>>2]|0;bn(ad|0,ae|0,af)|0;c[m>>2]=(c[m>>2]|0)+af;ae=(c[\
w>>2]|0)+16|0;c[ae>>2]=(c[ae>>2]|0)+af;c[E>>2]=(c[E>>2]|0)+af;c[o>>2]=(c[o>>2]|\
0)-af;ae=(c[w>>2]|0)+20|0;c[ae>>2]=(c[ae>>2]|0)-af;ae=c[w>>2]|0;if((c[ae+20>>2]\
|0)!=0){break}c[ae+16>>2]=c[ae+8>>2]}}while(0);af=c[Q>>2]|0;if((af|0)==(c[ag>>2\
]|0)){aD=1;aF=af;break}else{aH=af;aI=af}}else{aH=O;aI=T}af=c[aa>>2]|0;c[aa>>2]=\
af+1;L=a[(c[(c[av>>2]|0)+36>>2]|0)+af|0]|0;c[Q>>2]=aI+1;a[(c[U>>2]|0)+aI|0]=L;i\
f(L<<24>>24==0){aD=0;aF=aH;break}O=aH;T=c[Q>>2]|0}do{if((c[(c[av>>2]|0)+44>>2]|\
0)!=0){T=c[Q>>2]|0;if(T>>>0<=aF>>>0){break}c[H>>2]=bi(c[H>>2]|0,(c[U>>2]|0)+aF|\
0,T-aF|0)|0}}while(0);if((aD|0)==0){c[G>>2]=103;aC=av;K=119;break}else{aB=c[G>>\
2]|0;K=117;break}}}while(0);do{if((K|0)==117){if((aB|0)!=103){break}aC=y+28|0;K\
=119}}while(0);do{if((K|0)==119){if((c[(c[aC>>2]|0)+44>>2]|0)==0){c[G>>2]=113;b\
reak}U=y+20|0;Q=y+12|0;do{if(((c[U>>2]|0)+2|0)>>>0>(c[Q>>2]|0)>>>0){T=c[w>>2]|0\
;O=c[T+20>>2]|0;aa=c[o>>2]|0;ag=O>>>0>aa>>>0?aa:O;if((ag|0)==0){break}O=c[m>>2]\
|0;aa=c[T+16>>2]|0;bn(O|0,aa|0,ag)|0;c[m>>2]=(c[m>>2]|0)+ag;aa=(c[w>>2]|0)+16|0\
;c[aa>>2]=(c[aa>>2]|0)+ag;c[E>>2]=(c[E>>2]|0)+ag;c[o>>2]=(c[o>>2]|0)-ag;aa=(c[w\
>>2]|0)+20|0;c[aa>>2]=(c[aa>>2]|0)-ag;ag=c[w>>2]|0;if((c[ag+20>>2]|0)!=0){break\
}c[ag+16>>2]=c[ag+8>>2]}}while(0);ag=c[U>>2]|0;if((ag+2|0)>>>0>(c[Q>>2]|0)>>>0)\
{break}aa=c[H>>2]&255;c[U>>2]=ag+1;O=y+8|0;a[(c[O>>2]|0)+ag|0]=aa;aa=(c[H>>2]|0\
)>>>8&255;ag=c[U>>2]|0;c[U>>2]=ag+1;a[(c[O>>2]|0)+ag|0]=aa;c[H>>2]=0;c[G>>2]=11\
3}}while(0);aa=y+20|0;do{if((c[aa>>2]|0)==0){ag=c[l>>2]|0;aL=(ag|0)==0?0:ag}els\
e{ag=c[w>>2]|0;O=c[ag+20>>2]|0;T=c[o>>2]|0;S=O>>>0>T>>>0?T:O;if((S|0)==0){aN=T}\
else{T=c[m>>2]|0;O=c[ag+16>>2]|0;bn(T|0,O|0,S)|0;c[m>>2]=(c[m>>2]|0)+S;O=(c[w>>\
2]|0)+16|0;c[O>>2]=(c[O>>2]|0)+S;c[E>>2]=(c[E>>2]|0)+S;c[o>>2]=(c[o>>2]|0)-S;O=\
(c[w>>2]|0)+20|0;c[O>>2]=(c[O>>2]|0)-S;S=c[w>>2]|0;if((c[S+20>>2]|0)==0){c[S+16\
>>2]=c[S+8>>2]}aN=c[o>>2]|0}if((aN|0)==0){c[N>>2]=-1;break L20}else{aL=c[l>>2]|\
0;break}}}while(0);S=(c[G>>2]|0)==666;O=(aL|0)==0;do{if(S){if(O){K=140;break}c[\
t>>2]=12768;break L20}else{if(O){K=140}else{K=141}}}while(0);if((K|0)==140){if(\
(c[y+116>>2]|0)==0^1|S^1){K=141}}L183:do{if((K|0)==141){O=c[y+136>>2]|0;L185:do\
{if((O|0)==2){T=y+116|0;ag=y+96|0;R=y+108|0;L=y+56|0;af=y+5792|0;ae=y+5796|0;ad\
=y+5784|0;ac=y+5788|0;ab=y+92|0;ak=y;while(1){if((c[T>>2]|0)==0){a3(I);if((c[T>\
>2]|0)==0){break}}c[ag>>2]=0;ah=a[(c[L>>2]|0)+(c[R>>2]|0)|0]|0;b[(c[ae>>2]|0)+(\
c[af>>2]<<1)>>1]=0;aj=c[af>>2]|0;c[af>>2]=aj+1;a[(c[ad>>2]|0)+aj|0]=ah;aj=I+148\
+((ah&255)<<2)|0;b[aj>>1]=(b[aj>>1]|0)+1&65535;aj=(c[af>>2]|0)==((c[ac>>2]|0)-1\
|0);c[T>>2]=(c[T>>2]|0)-1;ah=(c[R>>2]|0)+1|0;c[R>>2]=ah;if(!aj){continue}aj=c[a\
b>>2]|0;if((aj|0)>-1){aO=(c[L>>2]|0)+aj|0}else{aO=0}ba(ak,aO,ah-aj|0,0);c[ab>>2\
]=c[R>>2];aj=c[M>>2]|0;ah=aj+28|0;ai=c[ah>>2]|0;aP=c[ai+20>>2]|0;aQ=aj+16|0;aR=\
c[aQ>>2]|0;aS=aP>>>0>aR>>>0?aR:aP;do{if((aS|0)!=0){aP=aj+12|0;aR=c[aP>>2]|0;aT=\
c[ai+16>>2]|0;bn(aR|0,aT|0,aS)|0;c[aP>>2]=(c[aP>>2]|0)+aS;aP=(c[ah>>2]|0)+16|0;\
c[aP>>2]=(c[aP>>2]|0)+aS;aP=aj+20|0;c[aP>>2]=(c[aP>>2]|0)+aS;c[aQ>>2]=(c[aQ>>2]\
|0)-aS;aP=(c[ah>>2]|0)+20|0;c[aP>>2]=(c[aP>>2]|0)-aS;aP=c[ah>>2]|0;if((c[aP+20>\
>2]|0)!=0){break}c[aP+16>>2]=c[aP+8>>2]}}while(0);if((c[(c[M>>2]|0)+16>>2]|0)==\
0){break L185}}T=c[ab>>2]|0;if((T|0)>-1){aU=(c[L>>2]|0)+T|0}else{aU=0}ba(ak,aU,\
(c[R>>2]|0)-T|0,1);c[ab>>2]=c[R>>2];T=c[M>>2]|0;ac=T+28|0;af=c[ac>>2]|0;ad=c[af\
+20>>2]|0;ae=T+16|0;ag=c[ae>>2]|0;ah=ad>>>0>ag>>>0?ag:ad;do{if((ah|0)!=0){ad=T+\
12|0;ag=c[ad>>2]|0;aS=c[af+16>>2]|0;bn(ag|0,aS|0,ah)|0;c[ad>>2]=(c[ad>>2]|0)+ah\
;ad=(c[ac>>2]|0)+16|0;c[ad>>2]=(c[ad>>2]|0)+ah;ad=T+20|0;c[ad>>2]=(c[ad>>2]|0)+\
ah;c[ae>>2]=(c[ae>>2]|0)-ah;ad=(c[ac>>2]|0)+20|0;c[ad>>2]=(c[ad>>2]|0)-ah;ad=c[\
ac>>2]|0;if((c[ad+20>>2]|0)!=0){break}c[ad+16>>2]=c[ad+8>>2]}}while(0);aV=(c[(c\
[M>>2]|0)+16>>2]|0)==0?2:3;K=193}else if((O|0)==3){ac=y+116|0;ah=y+96|0;ae=y+10\
8|0;T=y+5792|0;af=y+5796|0;R=y+5784|0;ab=y+2440|0;ak=y+5788|0;L=y+56|0;ad=y+92|\
0;aS=y;L209:while(1){ag=c[ac>>2]|0;do{if(ag>>>0<258){a3(I);aQ=c[ac>>2]|0;if((aQ\
|0)==0){break L209}c[ah>>2]=0;if(aQ>>>0>2){aW=aQ;K=164;break}aX=c[ae>>2]|0;K=17\
9}else{c[ah>>2]=0;aW=ag;K=164}}while(0);do{if((K|0)==164){K=0;ag=c[ae>>2]|0;if(\
(ag|0)==0){aX=0;K=179;break}aQ=c[L>>2]|0;aj=a[aQ+(ag-1)|0]|0;if(aj<<24>>24!=(a[\
aQ+ag|0]|0)){aX=ag;K=179;break}if(aj<<24>>24!=(a[aQ+(ag+1)|0]|0)){aX=ag;K=179;b\
reak}ai=aQ+(ag+2)|0;if(aj<<24>>24!=(a[ai]|0)){aX=ag;K=179;break}aP=aQ+(ag+258)|\
0;aQ=ai;while(1){ai=aQ+1|0;if(aj<<24>>24!=(a[ai]|0)){aY=ai;break}ai=aQ+2|0;if(a\
j<<24>>24!=(a[ai]|0)){aY=ai;break}ai=aQ+3|0;if(aj<<24>>24!=(a[ai]|0)){aY=ai;bre\
ak}ai=aQ+4|0;if(aj<<24>>24!=(a[ai]|0)){aY=ai;break}ai=aQ+5|0;if(aj<<24>>24!=(a[\
ai]|0)){aY=ai;break}ai=aQ+6|0;if(aj<<24>>24!=(a[ai]|0)){aY=ai;break}ai=aQ+7|0;i\
f(aj<<24>>24!=(a[ai]|0)){aY=ai;break}ai=aQ+8|0;if(aj<<24>>24==(a[ai]|0)&ai>>>0<\
aP>>>0){aQ=ai}else{aY=ai;break}}aQ=aY-aP+258|0;aj=aQ>>>0>aW>>>0?aW:aQ;c[ah>>2]=\
aj;if(aj>>>0<=2){aX=ag;K=179;break}aQ=aj+253|0;b[(c[af>>2]|0)+(c[T>>2]<<1)>>1]=\
1;aj=c[T>>2]|0;c[T>>2]=aj+1;a[(c[R>>2]|0)+aj|0]=aQ&255;aj=I+148+((d[12952+(aQ&2\
55)|0]|256)+1<<2)|0;b[aj>>1]=(b[aj>>1]|0)+1&65535;b[ab>>1]=(b[ab>>1]|0)+1&65535\
;aj=(c[T>>2]|0)==((c[ak>>2]|0)-1|0);aQ=c[ah>>2]|0;c[ac>>2]=(c[ac>>2]|0)-aQ;ai=(\
c[ae>>2]|0)+aQ|0;c[ae>>2]=ai;c[ah>>2]=0;if(aj){aZ=ai}else{continue L209}}}while\
(0);if((K|0)==179){K=0;ai=a[(c[L>>2]|0)+aX|0]|0;b[(c[af>>2]|0)+(c[T>>2]<<1)>>1]\
=0;aj=c[T>>2]|0;c[T>>2]=aj+1;a[(c[R>>2]|0)+aj|0]=ai;aj=I+148+((ai&255)<<2)|0;b[\
aj>>1]=(b[aj>>1]|0)+1&65535;aj=(c[T>>2]|0)==((c[ak>>2]|0)-1|0);c[ac>>2]=(c[ac>>\
2]|0)-1;ai=(c[ae>>2]|0)+1|0;c[ae>>2]=ai;if(aj){aZ=ai}else{continue}}ai=c[ad>>2]\
|0;if((ai|0)>-1){a_=(c[L>>2]|0)+ai|0}else{a_=0}ba(aS,a_,aZ-ai|0,0);c[ad>>2]=c[a\
e>>2];ai=c[M>>2]|0;aj=ai+28|0;aQ=c[aj>>2]|0;aT=c[aQ+20>>2]|0;aR=ai+16|0;a$=c[aR\
>>2]|0;a0=aT>>>0>a$>>>0?a$:aT;do{if((a0|0)!=0){aT=ai+12|0;a$=c[aT>>2]|0;a1=c[aQ\
+16>>2]|0;bn(a$|0,a1|0,a0)|0;c[aT>>2]=(c[aT>>2]|0)+a0;aT=(c[aj>>2]|0)+16|0;c[aT\
>>2]=(c[aT>>2]|0)+a0;aT=ai+20|0;c[aT>>2]=(c[aT>>2]|0)+a0;c[aR>>2]=(c[aR>>2]|0)-\
a0;aT=(c[aj>>2]|0)+20|0;c[aT>>2]=(c[aT>>2]|0)-a0;aT=c[aj>>2]|0;if((c[aT+20>>2]|\
0)!=0){break}c[aT+16>>2]=c[aT+8>>2]}}while(0);if((c[(c[M>>2]|0)+16>>2]|0)==0){b\
reak L185}}ac=c[ad>>2]|0;if((ac|0)>-1){a2=(c[L>>2]|0)+ac|0}else{a2=0}ba(aS,a2,(\
c[ae>>2]|0)-ac|0,1);c[ad>>2]=c[ae>>2];ac=c[M>>2]|0;ak=ac+28|0;T=c[ak>>2]|0;R=c[\
T+20>>2]|0;af=ac+16|0;ah=c[af>>2]|0;ab=R>>>0>ah>>>0?ah:R;do{if((ab|0)!=0){R=ac+\
12|0;ah=c[R>>2]|0;aj=c[T+16>>2]|0;bn(ah|0,aj|0,ab)|0;c[R>>2]=(c[R>>2]|0)+ab;R=(\
c[ak>>2]|0)+16|0;c[R>>2]=(c[R>>2]|0)+ab;R=ac+20|0;c[R>>2]=(c[R>>2]|0)+ab;c[af>>\
2]=(c[af>>2]|0)-ab;R=(c[ak>>2]|0)+20|0;c[R>>2]=(c[R>>2]|0)-ab;R=c[ak>>2]|0;if((\
c[R+20>>2]|0)!=0){break}c[R+16>>2]=c[R+8>>2]}}while(0);aV=(c[(c[M>>2]|0)+16>>2]\
|0)==0?2:3;K=193}else{ak=aM[c[12392+((c[y+132>>2]|0)*12&-1)>>2]&7](I,4)|0;if((a\
k-2|0)>>>0<2){aV=ak;K=193}else{a4=ak;K=194}}}while(0);if((K|0)==193){c[G>>2]=66\
6;a4=aV;K=194}do{if((K|0)==194){if((a4|0)==2|(a4|0)==0){break}else if((a4|0)!=1\
){break L183}a9(y,0,0,0);O=c[w>>2]|0;U=c[O+20>>2]|0;Q=c[o>>2]|0;ak=U>>>0>Q>>>0?\
Q:U;if((ak|0)==0){a5=Q}else{Q=c[m>>2]|0;U=c[O+16>>2]|0;bn(Q|0,U|0,ak)|0;c[m>>2]\
=(c[m>>2]|0)+ak;U=(c[w>>2]|0)+16|0;c[U>>2]=(c[U>>2]|0)+ak;c[E>>2]=(c[E>>2]|0)+a\
k;c[o>>2]=(c[o>>2]|0)-ak;U=(c[w>>2]|0)+20|0;c[U>>2]=(c[U>>2]|0)-ak;ak=c[w>>2]|0\
;if((c[ak+20>>2]|0)==0){c[ak+16>>2]=c[ak+8>>2]}a5=c[o>>2]|0}if((a5|0)!=0){break\
 L183}c[N>>2]=-1;break L20}}while(0);if((c[o>>2]|0)!=0){break L20}c[N>>2]=-1;br\
eak L20}}while(0);N=y+24|0;G=c[N>>2]|0;if((G|0)>=1){M=c[H>>2]|0;if((G|0)==2){G=\
c[aa>>2]|0;c[aa>>2]=G+1;S=y+8|0;a[(c[S>>2]|0)+G|0]=M&255;G=(c[H>>2]|0)>>>8&255;\
ak=c[aa>>2]|0;c[aa>>2]=ak+1;a[(c[S>>2]|0)+ak|0]=G;G=(c[H>>2]|0)>>>16&255;ak=c[a\
a>>2]|0;c[aa>>2]=ak+1;a[(c[S>>2]|0)+ak|0]=G;G=(c[H>>2]|0)>>>24&255;ak=c[aa>>2]|\
0;c[aa>>2]=ak+1;a[(c[S>>2]|0)+ak|0]=G;G=c[F>>2]&255;ak=c[aa>>2]|0;c[aa>>2]=ak+1\
;a[(c[S>>2]|0)+ak|0]=G;G=(c[F>>2]|0)>>>8&255;ak=c[aa>>2]|0;c[aa>>2]=ak+1;a[(c[S\
>>2]|0)+ak|0]=G;G=(c[F>>2]|0)>>>16&255;ak=c[aa>>2]|0;c[aa>>2]=ak+1;a[(c[S>>2]|0\
)+ak|0]=G;G=(c[F>>2]|0)>>>24&255;ak=c[aa>>2]|0;c[aa>>2]=ak+1;a[(c[S>>2]|0)+ak|0\
]=G}else{G=c[aa>>2]|0;c[aa>>2]=G+1;ak=y+8|0;a[(c[ak>>2]|0)+G|0]=M>>>24&255;G=c[\
aa>>2]|0;c[aa>>2]=G+1;a[(c[ak>>2]|0)+G|0]=M>>>16&255;M=c[H>>2]|0;G=c[aa>>2]|0;c\
[aa>>2]=G+1;a[(c[ak>>2]|0)+G|0]=M>>>8&255;G=c[aa>>2]|0;c[aa>>2]=G+1;a[(c[ak>>2]\
|0)+G|0]=M&255}M=c[w>>2]|0;G=c[M+20>>2]|0;ak=c[o>>2]|0;S=G>>>0>ak>>>0?ak:G;do{i\
f((S|0)!=0){G=c[m>>2]|0;ak=c[M+16>>2]|0;bn(G|0,ak|0,S)|0;c[m>>2]=(c[m>>2]|0)+S;\
ak=(c[w>>2]|0)+16|0;c[ak>>2]=(c[ak>>2]|0)+S;c[E>>2]=(c[E>>2]|0)+S;c[o>>2]=(c[o>\
>2]|0)-S;ak=(c[w>>2]|0)+20|0;c[ak>>2]=(c[ak>>2]|0)-S;ak=c[w>>2]|0;if((c[ak+20>>\
2]|0)!=0){break}c[ak+16>>2]=c[ak+8>>2]}}while(0);S=c[N>>2]|0;if((S|0)>0){c[N>>2\
]=-S}if((c[aa>>2]|0)!=0){break}}S=c[E>>2]|0;M=c[w>>2]|0;if((M|0)==0){v=S;break \
L7}ak=c[M+4>>2]|0;if(!((ak|0)==666|(ak|0)==113|(ak|0)==103|(ak|0)==91|(ak|0)==7\
3|(ak|0)==69|(ak|0)==42)){v=S;break L7}ak=c[M+8>>2]|0;if((ak|0)==0){a6=M}else{a\
J[c[q>>2]&3](c[r>>2]|0,ak);a6=c[w>>2]|0}ak=c[a6+68>>2]|0;if((ak|0)==0){a8=a6}el\
se{aJ[c[q>>2]&3](c[r>>2]|0,ak);a8=c[w>>2]|0}ak=c[a8+64>>2]|0;if((ak|0)==0){bb=a\
8}else{aJ[c[q>>2]&3](c[r>>2]|0,ak);bb=c[w>>2]|0}ak=c[bb+56>>2]|0;if((ak|0)==0){\
bc=bb}else{aJ[c[q>>2]&3](c[r>>2]|0,ak);bc=c[w>>2]|0}aJ[c[q>>2]&3](c[r>>2]|0,bc)\
;c[w>>2]=0;v=S;break L7}}while(0);if((K|0)==30){c[t>>2]=12808}E=c[w>>2]|0;if((E\
|0)==0){v=100043;break L7}H=c[E+4>>2]|0;if(!((H|0)==666|(H|0)==113|(H|0)==103|(\
H|0)==91|(H|0)==73|(H|0)==69|(H|0)==42)){v=100043;break L7}H=c[E+8>>2]|0;if((H|\
0)==0){bd=E}else{aJ[c[q>>2]&3](c[r>>2]|0,H);bd=c[w>>2]|0}H=c[bd+68>>2]|0;if((H|\
0)==0){be=bd}else{aJ[c[q>>2]&3](c[r>>2]|0,H);be=c[w>>2]|0}H=c[be+64>>2]|0;if((H\
|0)==0){bl=be}else{aJ[c[q>>2]&3](c[r>>2]|0,H);bl=c[w>>2]|0}H=c[bl+56>>2]|0;if((\
H|0)==0){bo=bl}else{aJ[c[q>>2]&3](c[r>>2]|0,H);bo=c[w>>2]|0}aJ[c[q>>2]&3](c[r>>\
2]|0,bo);c[w>>2]=0;v=100043;break L7}}while(0);c[u+4>>2]=666;c[t>>2]=12784;D=c[\
w>>2]|0;if((D|0)==0){v=100043;break}A=c[D+4>>2]|0;if(!((A|0)==666|(A|0)==113|(A\
|0)==103|(A|0)==91|(A|0)==73|(A|0)==69|(A|0)==42)){v=100043;break}A=c[D+8>>2]|0\
;if((A|0)==0){bp=D}else{aJ[c[q>>2]&3](c[r>>2]|0,A);bp=c[w>>2]|0}A=c[bp+68>>2]|0\
;if((A|0)==0){bq=bp}else{aJ[c[q>>2]&3](c[r>>2]|0,A);bq=c[w>>2]|0}A=c[bq+64>>2]|\
0;if((A|0)==0){br=bq}else{aJ[c[q>>2]&3](c[r>>2]|0,A);br=c[w>>2]|0}A=c[br+56>>2]\
|0;if((A|0)==0){bs=br}else{aJ[c[q>>2]&3](c[r>>2]|0,A);bs=c[w>>2]|0}aJ[c[q>>2]&3\
](c[r>>2]|0,bs);c[w>>2]=0;v=100043}}while(0);bs=(g|0)==0;if(bs){ao(12936,(s=i,i\
=i+16|0,c[s>>2]=1e5,c[s+8>>2]=v,s)|0)|0}g=c[1044]|0;r=c[1046]|0;q=bf(0,1,7116)|\
0;L331:do{if((q|0)!=0){c[q+52>>2]=0;br=q+52|0;bq=br;bp=q+36|0;t=q+8|0;c[t>>2]=1\
;c[bp>>2]=15;u=q+28|0;c[u>>2]=0;bo=q;c[bo>>2]=0;bl=q+4|0;c[bl>>2]=0;be=q+12|0;c\
[be>>2]=0;bd=q+20|0;c[bd>>2]=32768;c[q+32>>2]=0;bc=q+40|0;c[bc>>2]=0;bb=q+44|0;\
c[bb>>2]=0;a8=q+48|0;c[a8>>2]=0;a6=q+56|0;c[a6>>2]=0;o=q+60|0;c[o>>2]=0;m=q+132\
8|0;c[q+108>>2]=m;a5=q+80|0;c[a5>>2]=m;a4=q+76|0;c[a4>>2]=m;aV=q+7104|0;c[aV>>2\
]=1;a2=q+7108|0;c[a2>>2]=-1;aZ=j|0;L333:do{if((g|0)!=0){if(!((r|0)!=0|(v|0)==0)\
){break}a_=q+24|0;aX=j+1|0;aW=q+16|0;aY=q+32|0;aU=q+64|0;aO=q+84|0;aL=q+88|0;l=\
q+76|0;aN=q+72|0;aC=q+7112|0;aB=q+68|0;av=q+96|0;aD=q+100|0;aF=q+92|0;aH=q+104|\
0;aI=q+108|0;au=aI;aw=aI;aI=q+112|0;ax=aI;ay=q+752|0;aA=aI;aI=q+624|0;$=q+80|0;\
_=j+2|0;at=j+3|0;Z=0;ar=1e5;as=0;al=0;am=1e5;an=v;ap=g;aq=r;V=0;Y=v;X=0;L336:wh\
ile(1){L338:do{if((V|0)==27){bt=ar;bu=as;bv=al;bw=an;bx=aq;by=X;bz=c[t>>2]|0;K=\
571}else if((V|0)==6){bA=as;bB=al;bC=an;bD=aq;bE=c[aW>>2]|0;K=317}else if((V|0)\
==21){bF=Z;bG=as;bH=al;bI=an;bJ=aq;bK=c[aN>>2]|0;K=515}else if((V|0)==23){bL=Z;\
bM=as;bN=al;bO=an;bP=aq;bQ=c[aN>>2]|0;K=534}else if((V|0)==18){bR=Z;bS=as;bT=al\
;bU=an;bV=aq;bW=c[aH>>2]|0;K=395}else if((V|0)==1){if(as>>>0<16){W=aq;P=an;k=al\
;n=as;while(1){if((P|0)==0){bX=Z;bY=ar;bZ=n;b_=k;b$=am;b0=X;break L336}J=P-1|0;\
p=W+1|0;A=(d[W]<<n)+k|0;D=n+8|0;if(D>>>0<16){W=p;P=J;k=A;n=D}else{b1=p;b2=J;b3=\
A;b4=D;break}}}else{b1=aq;b2=an;b3=al;b4=as}c[aW>>2]=b3;if((b3&255|0)!=8){c[bo>\
>2]=29;b5=Z;b6=ar;b7=b4;b8=b3;b9=am;ca=b2;cb=ap;cc=b1;cd=Y;ce=X;break}if((b3&57\
344|0)!=0){c[bo>>2]=29;b5=Z;b6=ar;b7=b4;b8=b3;b9=am;ca=b2;cb=ap;cc=b1;cd=Y;ce=X\
;break}n=c[aY>>2]|0;if((n|0)==0){cf=b3}else{c[n>>2]=b3>>>8&1;cf=c[aW>>2]|0}if((\
cf&512|0)!=0){a[aZ]=b3&255;a[aX]=b3>>>8&255;c[a_>>2]=bi(c[a_>>2]|0,aZ,2)|0}c[bo\
>>2]=2;cg=b1;ch=b2;ci=0;cj=0;K=281}else if((V|0)==9){if(as>>>0<32){n=aq;k=an;P=\
al;W=as;while(1){if((k|0)==0){bX=Z;bY=ar;bZ=W;b_=P;b$=am;b0=X;break L336}D=k-1|\
0;A=n+1|0;J=(d[n]<<W)+P|0;p=W+8|0;if(p>>>0<32){n=A;k=D;P=J;W=p}else{ck=A;cl=D;c\
m=J;break}}}else{ck=aq;cl=an;cm=al}c[a_>>2]=aE(cm|0)|0;c[bo>>2]=10;cn=0;co=0;cp\
=cl;cq=ck;K=355}else if((V|0)==16){if(as>>>0<14){W=aq;P=an;k=al;n=as;while(1){i\
f((P|0)==0){bX=Z;bY=ar;bZ=n;b_=k;b$=am;b0=X;break L336}J=P-1|0;D=W+1|0;A=(d[W]<\
<n)+k|0;p=n+8|0;if(p>>>0<14){W=D;P=J;k=A;n=p}else{cr=D;cs=J;ct=A;cu=p;break}}}e\
lse{cr=aq;cs=an;ct=al;cu=as}n=(ct&31)+257|0;c[av>>2]=n;k=(ct>>>5&31)+1|0;c[aD>>\
2]=k;c[aF>>2]=(ct>>>10&15)+4;P=ct>>>14;W=cu-14|0;if(n>>>0>286|k>>>0>30){c[bo>>2\
]=29;b5=Z;b6=ar;b7=W;b8=P;b9=am;ca=cs;cb=ap;cc=cr;cd=Y;ce=X;break}else{c[aH>>2]\
=0;c[bo>>2]=17;cv=cr;cw=cs;cx=P;cy=W;cz=0;K=386;break}}else if((V|0)==0){W=c[t>\
>2]|0;if((W|0)==0){c[bo>>2]=12;b5=Z;b6=ar;b7=as;b8=al;b9=am;ca=an;cb=ap;cc=aq;c\
d=Y;ce=X;break}if(as>>>0<16){P=aq;k=an;n=al;p=as;while(1){if((k|0)==0){bX=Z;bY=\
ar;bZ=p;b_=n;b$=am;b0=X;break L336}A=k-1|0;J=P+1|0;D=(d[P]<<p)+n|0;C=p+8|0;if(C\
>>>0<16){P=J;k=A;n=D;p=C}else{cA=J;cB=A;cC=D;cD=C;break}}}else{cA=aq;cB=an;cC=a\
l;cD=as}if((W&2|0)!=0&(cC|0)==35615){c[a_>>2]=0;a[aZ]=31;a[aX]=-117;c[a_>>2]=bi\
(c[a_>>2]|0,aZ,2)|0;c[bo>>2]=1;b5=Z;b6=ar;b7=0;b8=0;b9=am;ca=cB;cb=ap;cc=cA;cd=\
Y;ce=X;break}c[aW>>2]=0;p=c[aY>>2]|0;if((p|0)==0){cE=W}else{c[p+48>>2]=-1;cE=c[\
t>>2]|0}do{if((cE&1|0)!=0){if(((((cC<<8&65280)+(cC>>>8)|0)>>>0)%31>>>0|0)!=0){b\
reak}if((cC&15|0)!=8){c[bo>>2]=29;b5=Z;b6=ar;b7=cD;b8=cC;b9=am;ca=cB;cb=ap;cc=c\
A;cd=Y;ce=X;break L338}p=cC>>>4;n=cD-4|0;k=(p&15)+8|0;P=c[bp>>2]|0;do{if((P|0)=\
=0){c[bp>>2]=k}else{if(k>>>0<=P>>>0){break}c[bo>>2]=29;b5=Z;b6=ar;b7=n;b8=p;b9=\
am;ca=cB;cb=ap;cc=cA;cd=Y;ce=X;break L338}}while(0);c[bd>>2]=1<<k;c[a_>>2]=1;c[\
bo>>2]=cC>>>12&2^11;b5=Z;b6=ar;b7=0;b8=0;b9=am;ca=cB;cb=ap;cc=cA;cd=Y;ce=X;brea\
k L338}}while(0);c[bo>>2]=29;b5=Z;b6=ar;b7=cD;b8=cC;b9=am;ca=cB;cb=ap;cc=cA;cd=\
Y;ce=X}else if((V|0)==2){if(as>>>0<32){cg=aq;ch=an;ci=al;cj=as;K=281}else{cF=aq\
;cG=an;cH=al;K=283}}else if((V|0)==3){if(as>>>0<16){cI=aq;cJ=an;cK=al;cL=as;K=2\
89}else{cM=aq;cN=an;cO=al;K=291}}else if((V|0)==4){cP=as;cQ=al;cR=an;cS=aq;K=29\
6}else if((V|0)==5){cT=as;cU=al;cV=an;cW=aq;K=307}else if((V|0)==7){cX=as;cY=al\
;cZ=an;c_=aq;K=330}else if((V|0)==8){c$=as;c0=al;c1=an;c2=aq;K=343}else if((V|0\
)==10){cn=as;co=al;cp=an;cq=aq;K=355}else if((V|0)==11|(V|0)==12){c3=as;c4=al;c\
5=an;c6=aq;K=358}else if((V|0)==13){W=as&7;p=al>>>(W>>>0);n=as-W|0;if(n>>>0<32)\
{W=aq;P=an;C=p;D=n;while(1){if((P|0)==0){bX=Z;bY=ar;bZ=D;b_=C;b$=am;b0=X;break \
L336}A=P-1|0;J=W+1|0;x=(d[W]<<D)+C|0;B=D+8|0;if(B>>>0<32){W=J;P=A;C=x;D=B}else{\
c7=J;c8=A;c9=x;da=B;break}}}else{c7=aq;c8=an;c9=p;da=n}D=c9&65535;if((D|0)==(c9\
>>>16^65535|0)){c[aU>>2]=D;c[bo>>2]=14;db=0;dc=0;dd=c8;de=c7;K=375;break}else{c\
[bo>>2]=29;b5=Z;b6=ar;b7=da;b8=c9;b9=am;ca=c8;cb=ap;cc=c7;cd=Y;ce=X;break}}else\
 if((V|0)==14){db=as;dc=al;dd=an;de=aq;K=375}else if((V|0)==15){df=as;dg=al;dh=\
an;di=aq;K=376}else if((V|0)==17){D=c[aH>>2]|0;if(D>>>0<(c[aF>>2]|0)>>>0){cv=aq\
;cw=an;cx=al;cy=as;cz=D;K=386}else{dj=aq;dk=an;dl=al;dm=as;dn=D;K=390}}else if(\
(V|0)==19){dp=Z;dq=as;dr=al;ds=an;dt=aq;K=432}else if((V|0)==20){du=Z;dv=as;dw=\
al;dx=an;dy=aq;K=433}else if((V|0)==22){dz=Z;dA=as;dB=al;dC=an;dD=aq;K=522}else\
 if((V|0)==24){dE=Z;dF=as;dG=al;dH=an;dI=aq;K=540}else if((V|0)==25){if((am|0)=\
=0){bX=Z;bY=ar;bZ=as;b_=al;b$=0;b0=X;break L336}a[ap]=c[aU>>2]&255;c[bo>>2]=20;\
b5=Z;b6=ar;b7=as;b8=al;b9=am-1|0;ca=an;cb=ap+1|0;cc=aq;cd=Y;ce=X}else if((V|0)=\
=26){D=c[t>>2]|0;do{if((D|0)==0){dJ=ar;dK=as;dL=al;dM=an;dN=aq;dO=X}else{if(as>\
>>0<32){C=aq;P=an;W=al;B=as;while(1){if((P|0)==0){bX=Z;bY=ar;bZ=B;b_=W;b$=am;b0\
=X;break L336}x=P-1|0;A=C+1|0;J=(d[C]<<B)+W|0;z=B+8|0;if(z>>>0<32){C=A;P=x;W=J;\
B=z}else{dP=A;dQ=x;dR=J;dS=z;break}}}else{dP=aq;dQ=an;dR=al;dS=as}B=ar-am|0;W=X\
+B|0;c[u>>2]=(c[u>>2]|0)+B;P=c[aW>>2]|0;if((ar|0)==(am|0)){dT=P}else{C=c[a_>>2]\
|0;k=ap+(-B|0)|0;if((P|0)==0){dU=bh(C,k,B)|0}else{dU=bi(C,k,B)|0}c[a_>>2]=dU;dT\
=P}if((dT|0)==0){dV=aE(dR|0)|0}else{dV=dR}if((dV|0)==(c[a_>>2]|0)){dJ=am;dK=0;d\
L=0;dM=dQ;dN=dP;dO=W;break}c[bo>>2]=29;b5=Z;b6=am;b7=dS;b8=dR;b9=am;ca=dQ;cb=ap\
;cc=dP;cd=Y;ce=W;break L338}}while(0);c[bo>>2]=27;bt=dJ;bu=dK;bv=dL;bw=dM;bx=dN\
;by=dO;bz=D;K=571}else if((V|0)==29){K=579;break L336}else if((V|0)==28){bX=1;b\
Y=ar;bZ=as;b_=al;b$=am;b0=X;break L336}else{break L333}}while(0);if((K|0)==281)\
{while(1){K=0;if((ch|0)==0){bX=Z;bY=ar;bZ=cj;b_=ci;b$=am;b0=X;break L336}aa=ch-\
1|0;N=cg+1|0;n=(d[cg]<<cj)+ci|0;p=cj+8|0;if(p>>>0<32){cg=N;ch=aa;ci=n;cj=p;K=28\
1}else{cF=N;cG=aa;cH=n;K=283;break}}}else if((K|0)==355){K=0;if((c[be>>2]|0)==0\
){K=356;break}c[a_>>2]=1;c[bo>>2]=11;c3=cn;c4=co;c5=cp;c6=cq;K=358}else if((K|0\
)==375){K=0;c[bo>>2]=15;df=db;dg=dc;dh=dd;di=de;K=376}else if((K|0)==386){while\
(1){K=0;if(cy>>>0<3){n=cv;aa=cw;N=cx;p=cy;while(1){if((aa|0)==0){bX=Z;bY=ar;bZ=\
p;b_=N;b$=am;b0=X;break L336}W=aa-1|0;P=n+1|0;B=(d[n]<<p)+N|0;k=p+8|0;if(k>>>0<\
3){n=P;aa=W;N=B;p=k}else{dW=P;dX=W;dY=B;dZ=k;break}}}else{dW=cv;dX=cw;dY=cx;dZ=\
cy}c[aH>>2]=cz+1;b[aA+(e[1640+(cz<<1)>>1]<<1)>>1]=dY&7;p=dY>>>3;N=dZ-3|0;aa=c[a\
H>>2]|0;if(aa>>>0<(c[aF>>2]|0)>>>0){cv=dW;cw=dX;cx=p;cy=N;cz=aa;K=386}else{dj=d\
W;dk=dX;dl=p;dm=N;dn=aa;K=390;break}}}else if((K|0)==571){K=0;if((bz|0)==0){d_=\
bu;d$=bv;K=578;break}if((c[aW>>2]|0)==0){d_=bu;d$=bv;K=578;break}if(bu>>>0<32){\
aa=bx;N=bw;p=bv;n=bu;while(1){if((N|0)==0){bX=Z;bY=bt;bZ=n;b_=p;b$=am;b0=by;bre\
ak L336}D=N-1|0;k=aa+1|0;B=(d[aa]<<n)+p|0;W=n+8|0;if(W>>>0<32){aa=k;N=D;p=B;n=W\
}else{d0=k;d1=D;d2=B;d3=W;break}}}else{d0=bx;d1=bw;d2=bv;d3=bu}if((d2|0)==(c[u>\
>2]|0)){d_=0;d$=0;K=578;break}c[bo>>2]=29;b5=Z;b6=bt;b7=d3;b8=d2;b9=am;ca=d1;cb\
=ap;cc=d0;cd=Y;ce=by}do{if((K|0)==283){K=0;n=c[aY>>2]|0;if((n|0)!=0){c[n+4>>2]=\
cH}if((c[aW>>2]&512|0)!=0){a[aZ]=cH&255;a[aX]=cH>>>8&255;a[_]=cH>>>16&255;a[at]\
=cH>>>24&255;c[a_>>2]=bi(c[a_>>2]|0,aZ,4)|0}c[bo>>2]=3;cI=cF;cJ=cG;cK=0;cL=0;K=\
289}else if((K|0)==358){K=0;if((c[bl>>2]|0)!=0){n=c3&7;c[bo>>2]=26;b5=Z;b6=ar;b\
7=c3-n|0;b8=c4>>>(n>>>0);b9=am;ca=c5;cb=ap;cc=c6;cd=Y;ce=X;break}if(c3>>>0<3){n\
=c6;p=c5;N=c4;aa=c3;while(1){if((p|0)==0){bX=Z;bY=ar;bZ=aa;b_=N;b$=am;b0=X;brea\
k L336}W=p-1|0;B=n+1|0;D=(d[n]<<aa)+N|0;k=aa+8|0;if(k>>>0<3){n=B;p=W;N=D;aa=k}e\
lse{d4=B;d5=W;d6=D;d7=k;break}}}else{d4=c6;d5=c5;d6=c4;d7=c3}c[bl>>2]=d6&1;aa=d\
6>>>1&3;if((aa|0)==0){c[bo>>2]=13}else if((aa|0)==1){c[a4>>2]=1680;c[aO>>2]=9;c\
[a5>>2]=3728;c[aL>>2]=5;c[bo>>2]=19}else if((aa|0)==2){c[bo>>2]=16}else if((aa|\
0)==3){c[bo>>2]=29}b5=Z;b6=ar;b7=d7-3|0;b8=d6>>>3;b9=am;ca=d5;cb=ap;cc=d4;cd=Y;\
ce=X}else if((K|0)==376){K=0;aa=c[aU>>2]|0;if((aa|0)==0){c[bo>>2]=11;b5=Z;b6=ar\
;b7=df;b8=dg;b9=am;ca=dh;cb=ap;cc=di;cd=Y;ce=X;break}N=aa>>>0>dh>>>0?dh:aa;aa=N\
>>>0>am>>>0?am:N;if((aa|0)==0){bX=Z;bY=ar;bZ=df;b_=dg;b$=am;b0=X;break L336}bn(\
ap|0,di|0,aa)|0;c[aU>>2]=(c[aU>>2]|0)-aa;b5=Z;b6=ar;b7=df;b8=dg;b9=am-aa|0;ca=d\
h-aa|0;cb=ap+aa|0;cc=di+aa|0;cd=Y;ce=X}else if((K|0)==390){K=0;if(dn>>>0<19){aa\
=dn;do{c[aH>>2]=aa+1;b[aA+(e[1640+(aa<<1)>>1]<<1)>>1]=0;aa=c[aH>>2]|0;}while(aa\
>>>0<19)}c[aw>>2]=m;c[a4>>2]=m;c[aO>>2]=7;aa=bj(0,ax,19,au,aO,ay)|0;if((aa|0)==\
0){c[aH>>2]=0;c[bo>>2]=18;bR=0;bS=dm;bT=dl;bU=dk;bV=dj;bW=0;K=395;break}else{c[\
bo>>2]=29;b5=aa;b6=ar;b7=dm;b8=dl;b9=am;ca=dk;cb=ap;cc=dj;cd=Y;ce=X;break}}}whi\
le(0);L497:do{if((K|0)==289){while(1){K=0;if((cJ|0)==0){bX=Z;bY=ar;bZ=cL;b_=cK;\
b$=am;b0=X;break L336}aa=cJ-1|0;N=cI+1|0;p=(d[cI]<<cL)+cK|0;n=cL+8|0;if(n>>>0<1\
6){cI=N;cJ=aa;cK=p;cL=n;K=289}else{cM=N;cN=aa;cO=p;K=291;break}}}else if((K|0)=\
=395){K=0;p=c[av>>2]|0;aa=c[aD>>2]|0;do{if(bW>>>0<(aa+p|0)>>>0){N=bV;n=bU;k=bT;\
D=bS;W=bW;B=p;P=aa;L503:while(1){C=(1<<c[aO>>2])-1|0;z=C&k;J=c[l>>2]|0;x=d[J+(z\
<<2)+1|0]|0;if(x>>>0>D>>>0){A=N;H=n;E=k;y=D;while(1){if((H|0)==0){bX=bR;bY=ar;b\
Z=y;b_=E;b$=am;b0=X;break L336}F=H-1|0;I=A+1|0;S=(d[A]<<y)+E|0;ak=y+8|0;M=C&S;G\
=d[J+(M<<2)+1|0]|0;if(G>>>0>ak>>>0){A=I;H=F;E=S;y=ak}else{d8=I;d9=F;ea=S;eb=ak;\
ec=M;ed=G;break}}}else{d8=N;d9=n;ea=k;eb=D;ec=z;ed=x}y=b[J+(ec<<2)+2>>1]|0;L510\
:do{if((y&65535)<16){if(eb>>>0<ed>>>0){E=d8;H=d9;A=ea;C=eb;while(1){if((H|0)==0\
){bX=bR;bY=ar;bZ=C;b_=A;b$=am;b0=X;break L336}G=H-1|0;M=E+1|0;ak=(d[E]<<C)+A|0;\
S=C+8|0;if(S>>>0<ed>>>0){E=M;H=G;A=ak;C=S}else{ee=M;ef=G;eg=ak;eh=S;break}}}els\
e{ee=d8;ef=d9;eg=ea;eh=eb}c[aH>>2]=W+1;b[aA+(W<<1)>>1]=y;ei=eh-ed|0;ej=eg>>>(ed\
>>>0);ek=ef;el=ee}else{if((y<<16>>16|0)==16){C=ed+2|0;if(eb>>>0<C>>>0){A=d8;H=d\
9;E=ea;ag=eb;while(1){if((H|0)==0){bX=bR;bY=ar;bZ=ag;b_=E;b$=am;b0=X;break L336\
}aP=H-1|0;S=A+1|0;ak=(d[A]<<ag)+E|0;G=ag+8|0;if(G>>>0<C>>>0){A=S;H=aP;E=ak;ag=G\
}else{em=S;en=aP;eo=ak;ep=G;break}}}else{em=d8;en=d9;eo=ea;ep=eb}eq=eo>>>(ed>>>\
0);er=ep-ed|0;if((W|0)==0){K=412;break L503}es=b[aA+(W-1<<1)>>1]|0;et=(eq&3)+3|\
0;eu=er-2|0;ev=eq>>>2;ew=en;ex=em}else if((y<<16>>16|0)==17){ag=ed+3|0;if(eb>>>\
0<ag>>>0){E=d8;H=d9;A=ea;C=eb;while(1){if((H|0)==0){bX=bR;bY=ar;bZ=C;b_=A;b$=am\
;b0=X;break L336}G=H-1|0;ak=E+1|0;aP=(d[E]<<C)+A|0;S=C+8|0;if(S>>>0<ag>>>0){E=a\
k;H=G;A=aP;C=S}else{ey=ak;ez=G;eA=aP;eB=S;break}}}else{ey=d8;ez=d9;eA=ea;eB=eb}\
C=eA>>>(ed>>>0);es=0;et=(C&7)+3|0;eu=-3-ed+eB|0;ev=C>>>3;ew=ez;ex=ey}else{C=ed+\
7|0;if(eb>>>0<C>>>0){A=d8;H=d9;E=ea;ag=eb;while(1){if((H|0)==0){bX=bR;bY=ar;bZ=\
ag;b_=E;b$=am;b0=X;break L336}S=H-1|0;aP=A+1|0;G=(d[A]<<ag)+E|0;ak=ag+8|0;if(ak\
>>>0<C>>>0){A=aP;H=S;E=G;ag=ak}else{eC=aP;eD=S;eE=G;eF=ak;break}}}else{eC=d8;eD\
=d9;eE=ea;eF=eb}ag=eE>>>(ed>>>0);es=0;et=(ag&127)+11|0;eu=-7-ed+eF|0;ev=ag>>>7;\
ew=eD;ex=eC}if((W+et|0)>>>0>(P+B|0)>>>0){K=421;break L503}else{eG=et;eH=W}while\
(1){ag=eG-1|0;c[aH>>2]=eH+1;b[aA+(eH<<1)>>1]=es;if((ag|0)==0){ei=eu;ej=ev;ek=ew\
;el=ex;break L510}eG=ag;eH=c[aH>>2]|0}}}while(0);y=c[aH>>2]|0;eI=c[av>>2]|0;J=c\
[aD>>2]|0;if(y>>>0<(J+eI|0)>>>0){N=el;n=ek;k=ej;D=ei;W=y;B=eI;P=J}else{K=424;br\
eak}}if((K|0)==412){K=0;c[bo>>2]=29;b5=bR;b6=ar;b7=er;b8=eq;b9=am;ca=en;cb=ap;c\
c=em;cd=Y;ce=X;break L497}else if((K|0)==421){K=0;c[bo>>2]=29;b5=bR;b6=ar;b7=eu\
;b8=ev;b9=am;ca=ew;cb=ap;cc=ex;cd=Y;ce=X;break L497}else if((K|0)==424){K=0;if(\
(c[bo>>2]|0)==29){b5=bR;b6=ar;b7=ei;b8=ej;b9=am;ca=ek;cb=ap;cc=el;cd=Y;ce=X;bre\
ak L497}else{eJ=eI;eK=ei;eL=ej;eM=ek;eN=el;break}}}else{eJ=p;eK=bS;eL=bT;eM=bU;\
eN=bV}}while(0);if((b[aI>>1]|0)==0){c[bo>>2]=29;b5=bR;b6=ar;b7=eK;b8=eL;b9=am;c\
a=eM;cb=ap;cc=eN;cd=Y;ce=X;break}c[aw>>2]=m;c[a4>>2]=m;c[aO>>2]=9;p=bj(1,ax,eJ,\
au,aO,ay)|0;if((p|0)!=0){c[bo>>2]=29;b5=p;b6=ar;b7=eK;b8=eL;b9=am;ca=eM;cb=ap;c\
c=eN;cd=Y;ce=X;break}c[a5>>2]=c[au>>2];c[aL>>2]=6;p=bj(2,ax+(c[av>>2]<<1)|0,c[a\
D>>2]|0,au,aL,ay)|0;if((p|0)==0){c[bo>>2]=19;dp=0;dq=eK;dr=eL;ds=eM;dt=eN;K=432\
;break}else{c[bo>>2]=29;b5=p;b6=ar;b7=eK;b8=eL;b9=am;ca=eM;cb=ap;cc=eN;cd=Y;ce=\
X;break}}}while(0);if((K|0)==291){K=0;p=c[aY>>2]|0;if((p|0)!=0){c[p+8>>2]=cO&25\
5;c[(c[aY>>2]|0)+12>>2]=cO>>>8}if((c[aW>>2]&512|0)!=0){a[aZ]=cO&255;a[aX]=cO>>>\
8&255;c[a_>>2]=bi(c[a_>>2]|0,aZ,2)|0}c[bo>>2]=4;cP=0;cQ=0;cR=cN;cS=cM;K=296}els\
e if((K|0)==432){K=0;c[bo>>2]=20;du=dp;dv=dq;dw=dr;dx=ds;dy=dt;K=433}do{if((K|0\
)==296){K=0;p=c[aW>>2]|0;do{if((p&1024|0)==0){aa=c[aY>>2]|0;if((aa|0)==0){eO=cP\
;eP=cQ;eQ=cR;eR=cS;break}c[aa+16>>2]=0;eO=cP;eP=cQ;eQ=cR;eR=cS}else{if(cP>>>0<1\
6){aa=cS;P=cR;B=cQ;W=cP;while(1){if((P|0)==0){bX=Z;bY=ar;bZ=W;b_=B;b$=am;b0=X;b\
reak L336}D=P-1|0;k=aa+1|0;n=(d[aa]<<W)+B|0;N=W+8|0;if(N>>>0<16){aa=k;P=D;B=n;W\
=N}else{eS=k;eT=D;eU=n;break}}}else{eS=cS;eT=cR;eU=cQ}c[aU>>2]=eU;W=c[aY>>2]|0;\
if((W|0)==0){eV=p}else{c[W+20>>2]=eU;eV=c[aW>>2]|0}if((eV&512|0)==0){eO=0;eP=0;\
eQ=eT;eR=eS;break}a[aZ]=eU&255;a[aX]=eU>>>8&255;c[a_>>2]=bi(c[a_>>2]|0,aZ,2)|0;\
eO=0;eP=0;eQ=eT;eR=eS}}while(0);c[bo>>2]=5;cT=eO;cU=eP;cV=eQ;cW=eR;K=307}else i\
f((K|0)==433){K=0;if(!(dx>>>0>5&am>>>0>257)){c[a2>>2]=0;p=(1<<c[aO>>2])-1|0;W=p\
&dw;B=c[l>>2]|0;P=a[B+(W<<2)+1|0]|0;aa=P&255;if(aa>>>0>dv>>>0){n=dy;D=dx;k=dw;N\
=dv;while(1){if((D|0)==0){bX=du;bY=ar;bZ=N;b_=k;b$=am;b0=X;break L336}J=D-1|0;y\
=n+1|0;x=(d[n]<<N)+k|0;z=N+8|0;ag=p&x;E=a[B+(ag<<2)+1|0]|0;H=E&255;if(H>>>0>z>>\
>0){n=y;D=J;k=x;N=z}else{eW=y;eX=J;eY=x;eZ=z;e_=E;e$=ag;e0=H;break}}}else{eW=dy\
;eX=dx;eY=dw;eZ=dv;e_=P;e$=W;e0=aa}N=a[B+(e$<<2)|0]|0;k=b[B+(e$<<2)+2>>1]|0;D=N\
&255;do{if(N<<24>>24==0){e1=0;e2=e_;e3=k;e4=eZ;e5=eY;e6=eX;e7=eW;e8=0}else{if((\
D&240|0)!=0){e1=N;e2=e_;e3=k;e4=eZ;e5=eY;e6=eX;e7=eW;e8=0;break}n=k&65535;p=(1<\
<e0+D)-1|0;H=((eY&p)>>>(e0>>>0))+n|0;ag=a[B+(H<<2)+1|0]|0;if(((ag&255)+e0|0)>>>\
0>eZ>>>0){E=eW;z=eX;x=eY;J=eZ;while(1){if((z|0)==0){bX=du;bY=ar;bZ=J;b_=x;b$=am\
;b0=X;break L336}y=z-1|0;A=E+1|0;C=(d[E]<<J)+x|0;ak=J+8|0;G=((C&p)>>>(e0>>>0))+\
n|0;S=a[B+(G<<2)+1|0]|0;if(((S&255)+e0|0)>>>0>ak>>>0){E=A;z=y;x=C;J=ak}else{e9=\
A;fa=y;fb=C;fc=ak;fd=G;fe=S;break}}}else{e9=eW;fa=eX;fb=eY;fc=eZ;fd=H;fe=ag}J=b\
[B+(fd<<2)+2>>1]|0;x=a[B+(fd<<2)|0]|0;c[a2>>2]=e0;e1=x;e2=fe;e3=J;e4=fc-e0|0;e5\
=fb>>>(e0>>>0);e6=fa;e7=e9;e8=e0}}while(0);B=e2&255;D=e5>>>(B>>>0);k=e4-B|0;c[a\
2>>2]=e8+B;c[aU>>2]=e3&65535;B=e1&255;if(e1<<24>>24==0){c[bo>>2]=25;b5=du;b6=ar\
;b7=k;b8=D;b9=am;ca=e6;cb=ap;cc=e7;cd=Y;ce=X;break}if((B&32|0)!=0){c[a2>>2]=-1;\
c[bo>>2]=11;b5=du;b6=ar;b7=k;b8=D;b9=am;ca=e6;cb=ap;cc=e7;cd=Y;ce=X;break}if((B\
&64|0)==0){N=B&15;c[aN>>2]=N;c[bo>>2]=21;bF=du;bG=k;bH=D;bI=e6;bJ=e7;bK=N;K=515\
;break}else{c[bo>>2]=29;b5=du;b6=ar;b7=k;b8=D;b9=am;ca=e6;cb=ap;cc=e7;cd=Y;ce=X\
;break}}c[a6>>2]=dw;c[o>>2]=dv;D=dy+(dx-6)|0;k=ap+(am-258)|0;N=c[bb>>2]|0;B=c[a\
8>>2]|0;aa=c[bq>>2]|0;W=c[l>>2]|0;P=c[$>>2]|0;J=(1<<c[aO>>2])-1|0;x=(1<<c[aL>>2\
])-1|0;z=ap+(am+(ar^-1))|0;E=aa-1|0;n=(B|0)==0;p=(c[bc>>2]|0)-1|0;S=p+B|0;G=B-1\
|0;ak=z-1|0;C=z-B|0;y=dy-1|0;A=ap-1|0;aP=dw;M=dv;L609:while(1){if(M>>>0<15){F=y\
+2|0;ff=F;fg=(d[y+1|0]<<M)+aP+(d[F]<<M+8)|0;fh=M+16|0}else{ff=y;fg=aP;fh=M}F=fg\
&J;I=a[W+(F<<2)|0]|0;U=b[W+(F<<2)+2>>1]|0;Q=d[W+(F<<2)+1|0]|0;F=fg>>>(Q>>>0);O=\
fh-Q|0;do{if(I<<24>>24==0){fi=U&255;fj=F;fk=O;K=439}else{Q=U;fl=F;fm=O;ab=I;whi\
le(1){fn=ab&255;if((fn&16|0)!=0){break}if((fn&64|0)!=0){K=487;break L609}af=(fl\
&(1<<fn)-1)+(Q&65535)|0;ac=a[W+(af<<2)|0]|0;fo=b[W+(af<<2)+2>>1]|0;T=d[W+(af<<2\
)+1|0]|0;fp=fl>>>(T>>>0);fq=fm-T|0;if(ac<<24>>24==0){K=438;break}else{Q=fo;fl=f\
p;fm=fq;ab=ac}}if((K|0)==438){K=0;fi=fo&255;fj=fp;fk=fq;K=439;break}ab=Q&65535;\
ac=fn&15;if((ac|0)==0){fr=ab;fs=ff;ft=fl;fu=fm}else{if(fm>>>0<ac>>>0){T=ff+1|0;\
fv=T;fw=(d[T]<<fm)+fl|0;fx=fm+8|0}else{fv=ff;fw=fl;fx=fm}fr=(fw&(1<<ac)-1)+ab|0\
;fs=fv;ft=fw>>>(ac>>>0);fu=fx-ac|0}if(fu>>>0<15){ac=fs+2|0;fy=ac;fz=(d[fs+1|0]<\
<fu)+ft+(d[ac]<<fu+8)|0;fA=fu+16|0}else{fy=fs;fz=ft;fA=fu}ac=fz&x;ab=b[P+(ac<<2\
)+2>>1]|0;T=d[P+(ac<<2)+1|0]|0;af=fz>>>(T>>>0);ae=fA-T|0;T=d[P+(ac<<2)|0]|0;if(\
(T&16|0)==0){ac=ab;fB=af;fC=ae;ad=T;while(1){if((ad&64|0)!=0){K=484;break L609}\
aS=(fB&(1<<ad)-1)+(ac&65535)|0;L=b[P+(aS<<2)+2>>1]|0;R=d[P+(aS<<2)+1|0]|0;aj=fB\
>>>(R>>>0);ah=fC-R|0;R=d[P+(aS<<2)|0]|0;if((R&16|0)==0){ac=L;fB=aj;fC=ah;ad=R}e\
lse{fD=L;fE=aj;fF=ah;fG=R;break}}}else{fD=ab;fE=af;fF=ae;fG=T}ad=fD&65535;ac=fG\
&15;do{if(fF>>>0<ac>>>0){Q=fy+1|0;R=(d[Q]<<fF)+fE|0;ah=fF+8|0;if(ah>>>0>=ac>>>0\
){fH=Q;fI=R;fJ=ah;break}Q=fy+2|0;fH=Q;fI=(d[Q]<<ah)+R|0;fJ=fF+16|0}else{fH=fy;f\
I=fE;fJ=fF}}while(0);T=(fI&(1<<ac)-1)+ad|0;fK=fI>>>(ac>>>0);fL=fJ-ac|0;ae=A;af=\
ae-z|0;if(T>>>0<=af>>>0){ab=A+(-T|0)|0;R=fr;ah=A;while(1){a[ah+1|0]=a[ab+1|0]|0\
;a[ah+2|0]=a[ab+2|0]|0;Q=ab+3|0;fM=ah+3|0;a[fM]=a[Q]|0;fN=R-3|0;if(fN>>>0>2){ab\
=Q;R=fN;ah=fM}else{break}}if((fN|0)==0){fO=fH;fP=fM;fQ=fK;fR=fL;break}R=ah+4|0;\
a[R]=a[ab+4|0]|0;if(fN>>>0<=1){fO=fH;fP=R;fQ=fK;fR=fL;break}R=ah+5|0;a[R]=a[ab+\
5|0]|0;fO=fH;fP=R;fQ=fK;fR=fL;break}R=T-af|0;if(R>>>0>N>>>0){if((c[aV>>2]|0)!=0\
){K=454;break L609}}do{if(n){ac=aa+(p-R)|0;if(R>>>0>=fr>>>0){fS=ac;fT=fr;fU=A;b\
reak}ad=fr-R|0;Q=T-ae|0;aj=ac;ac=R;L=A;do{aj=aj+1|0;L=L+1|0;a[L]=a[aj]|0;ac=ac-\
1|0;}while((ac|0)!=0);fS=A+(ak+Q+(1-T))|0;fT=ad;fU=A+(z+Q)|0}else{if(B>>>0>=R>>\
>0){ac=aa+(G-R)|0;if(R>>>0>=fr>>>0){fS=ac;fT=fr;fU=A;break}aj=fr-R|0;L=T-ae|0;a\
S=ac;ac=R;a0=A;do{aS=aS+1|0;a0=a0+1|0;a[a0]=a[aS]|0;ac=ac-1|0;}while((ac|0)!=0)\
;fS=A+(ak+L+(1-T))|0;fT=aj;fU=A+(z+L)|0;break}ac=aa+(S-R)|0;aS=R-B|0;if(aS>>>0>\
=fr>>>0){fS=ac;fT=fr;fU=A;break}a0=fr-aS|0;Q=T-ae|0;ad=ac;ac=aS;aS=A;do{ad=ad+1\
|0;aS=aS+1|0;a[aS]=a[ad]|0;ac=ac-1|0;}while((ac|0)!=0);ac=A+(C+Q)|0;if(B>>>0>=a\
0>>>0){fS=E;fT=a0;fU=ac;break}ad=a0-B|0;aS=E;L=B;aj=ac;do{aS=aS+1|0;aj=aj+1|0;a\
[aj]=a[aS]|0;L=L-1|0;}while((L|0)!=0);fS=A+(ak+Q+(1-T))|0;fT=ad;fU=A+(z+Q)|0}}w\
hile(0);if(fT>>>0>2){T=fU;ae=fT;R=fS;while(1){a[T+1|0]=a[R+1|0]|0;a[T+2|0]=a[R+\
2|0]|0;af=R+3|0;ab=T+3|0;a[ab]=a[af]|0;ah=ae-3|0;if(ah>>>0>2){T=ab;ae=ah;R=af}e\
lse{fV=ab;fW=ah;fX=af;break}}}else{fV=fU;fW=fT;fX=fS}if((fW|0)==0){fO=fH;fP=fV;\
fQ=fK;fR=fL;break}R=fV+1|0;a[R]=a[fX+1|0]|0;if(fW>>>0<=1){fO=fH;fP=R;fQ=fK;fR=f\
L;break}R=fV+2|0;a[R]=a[fX+2|0]|0;fO=fH;fP=R;fQ=fK;fR=fL}}while(0);if((K|0)==43\
9){K=0;I=A+1|0;a[I]=fi;fO=ff;fP=I;fQ=fj;fR=fk}if(fO>>>0<D>>>0&fP>>>0<k>>>0){y=f\
O;A=fP;aP=fQ;M=fR}else{fY=fO;fZ=fP;f_=fQ;f$=fR;break}}do{if((K|0)==454){K=0;c[b\
o>>2]=29;fY=fH;fZ=A;f_=fK;f$=fL}else if((K|0)==484){K=0;c[bo>>2]=29;fY=fy;fZ=A;\
f_=fB;f$=fC}else if((K|0)==487){K=0;if((fn&32|0)==0){c[bo>>2]=29;fY=ff;fZ=A;f_=\
fl;f$=fm;break}else{c[bo>>2]=11;fY=ff;fZ=A;f_=fl;f$=fm;break}}}while(0);A=f$>>>\
3;M=fY+(-A|0)|0;aP=f$-(A<<3)|0;y=(1<<aP)-1&f_;z=fY+(1-A)|0;A=fZ+1|0;if(M>>>0<D>\
>>0){f0=D-M|0}else{f0=D-M|0}M=f0+5|0;if(fZ>>>0<k>>>0){f1=k-fZ|0}else{f1=k-fZ|0}\
ak=f1+257|0;c[a6>>2]=y;c[o>>2]=aP;if((c[bo>>2]|0)!=11){b5=du;b6=ar;b7=aP;b8=y;b\
9=ak;ca=M;cb=A;cc=z;cd=M;ce=X;break}c[a2>>2]=-1;b5=du;b6=ar;b7=aP;b8=y;b9=ak;ca\
=M;cb=A;cc=z;cd=M;ce=X}}while(0);if((K|0)==307){K=0;M=c[aW>>2]|0;if((M&1024|0)=\
=0){f2=cV;f3=cW;f4=M}else{z=c[aU>>2]|0;A=z>>>0>cV>>>0?cV:z;if((A|0)==0){f5=cV;f\
6=cW;f7=z;f8=M}else{ak=c[aY>>2]|0;do{if((ak|0)==0){f9=M}else{y=c[ak+16>>2]|0;if\
((y|0)==0){f9=M;break}aP=(c[ak+20>>2]|0)-z|0;B=y+aP|0;y=c[ak+24>>2]|0;E=(aP+A|0\
)>>>0>y>>>0?y-aP|0:A;bn(B|0,cW|0,E)|0;f9=c[aW>>2]|0}}while(0);if((f9&512|0)!=0)\
{c[a_>>2]=bi(c[a_>>2]|0,cW,A)|0}ak=(c[aU>>2]|0)-A|0;c[aU>>2]=ak;f5=cV-A|0;f6=cW\
+A|0;f7=ak;f8=f9}if((f7|0)==0){f2=f5;f3=f6;f4=f8}else{bX=Z;bY=ar;bZ=cT;b_=cU;b$\
=am;b0=X;break}}c[aU>>2]=0;c[bo>>2]=6;bA=cT;bB=cU;bC=f2;bD=f3;bE=f4;K=317}else \
if((K|0)==515){K=0;if((bK|0)==0){ga=bG;gb=bH;gc=bI;gd=bJ;ge=c[aU>>2]|0}else{if(\
bG>>>0<bK>>>0){ak=bJ;z=bI;M=bH;E=bG;while(1){if((z|0)==0){bX=bF;bY=ar;bZ=E;b_=M\
;b$=am;b0=X;break L336}B=z-1|0;aP=ak+1|0;y=(d[ak]<<E)+M|0;C=E+8|0;if(C>>>0<bK>>\
>0){ak=aP;z=B;M=y;E=C}else{gf=aP;gg=B;gh=y;gi=C;break}}}else{gf=bJ;gg=bI;gh=bH;\
gi=bG}E=(c[aU>>2]|0)+((1<<bK)-1&gh)|0;c[aU>>2]=E;c[a2>>2]=(c[a2>>2]|0)+bK;ga=gi\
-bK|0;gb=gh>>>(bK>>>0);gc=gg;gd=gf;ge=E}c[aC>>2]=ge;c[bo>>2]=22;dz=bF;dA=ga;dB=\
gb;dC=gc;dD=gd;K=522}do{if((K|0)==317){K=0;do{if((bE&2048|0)==0){E=c[aY>>2]|0;i\
f((E|0)==0){gj=bC;gk=bD;break}c[E+28>>2]=0;gj=bC;gk=bD}else{if((bC|0)==0){bX=Z;\
bY=ar;bZ=bA;b_=bB;b$=am;b0=X;break L336}else{gl=0}while(1){gm=gl+1|0;E=a[bD+gl|\
0]|0;M=c[aY>>2]|0;do{if((M|0)!=0){z=M+28|0;if((c[z>>2]|0)==0){break}ak=c[aU>>2]\
|0;if(ak>>>0>=(c[M+32>>2]|0)>>>0){break}c[aU>>2]=ak+1;a[(c[z>>2]|0)+ak|0]=E}}wh\
ile(0);gn=E<<24>>24!=0;if(gn&gm>>>0<bC>>>0){gl=gm}else{break}}if((c[aW>>2]&512|\
0)!=0){c[a_>>2]=bi(c[a_>>2]|0,bD,gm)|0}if(gn){bX=Z;bY=ar;bZ=bA;b_=bB;b$=am;b0=X\
;break L336}else{gj=bC-gm|0;gk=bD+gm|0}}}while(0);c[aU>>2]=0;c[bo>>2]=7;cX=bA;c\
Y=bB;cZ=gj;c_=gk;K=330}else if((K|0)==522){K=0;k=(1<<c[aL>>2])-1|0;D=k&dB;M=c[$\
>>2]|0;ak=a[M+(D<<2)+1|0]|0;z=ak&255;if(z>>>0>dA>>>0){A=dD;C=dC;y=dB;B=dA;while\
(1){if((C|0)==0){bX=dz;bY=ar;bZ=B;b_=y;b$=am;b0=X;break L336}aP=C-1|0;S=A+1|0;a\
a=(d[A]<<B)+y|0;G=B+8|0;p=k&aa;n=a[M+(p<<2)+1|0]|0;N=n&255;if(N>>>0>G>>>0){A=S;\
C=aP;y=aa;B=G}else{go=S;gp=aP;gq=aa;gr=G;gs=n;gt=p;gu=N;break}}}else{go=dD;gp=d\
C;gq=dB;gr=dA;gs=ak;gt=D;gu=z}B=a[M+(gt<<2)|0]|0;y=b[M+(gt<<2)+2>>1]|0;C=B&255;\
if((C&240|0)==0){A=y&65535;k=(1<<gu+C)-1|0;C=((gq&k)>>>(gu>>>0))+A|0;N=a[M+(C<<\
2)+1|0]|0;if(((N&255)+gu|0)>>>0>gr>>>0){p=go;n=gp;G=gq;aa=gr;while(1){if((n|0)=\
=0){bX=dz;bY=ar;bZ=aa;b_=G;b$=am;b0=X;break L336}aP=n-1|0;S=p+1|0;P=(d[p]<<aa)+\
G|0;x=aa+8|0;W=((P&k)>>>(gu>>>0))+A|0;J=a[M+(W<<2)+1|0]|0;if(((J&255)+gu|0)>>>0\
>x>>>0){p=S;n=aP;G=P;aa=x}else{gv=S;gw=aP;gx=P;gy=x;gz=W;gA=J;break}}}else{gv=g\
o;gw=gp;gx=gq;gy=gr;gz=C;gA=N}aa=b[M+(gz<<2)+2>>1]|0;G=a[M+(gz<<2)|0]|0;n=(c[a2\
>>2]|0)+gu|0;c[a2>>2]=n;gB=G;gC=gA;gD=aa;gE=gy-gu|0;gF=gx>>>(gu>>>0);gG=gw;gH=g\
v;gI=n}else{gB=B;gC=gs;gD=y;gE=gr;gF=gq;gG=gp;gH=go;gI=c[a2>>2]|0}n=gC&255;aa=g\
F>>>(n>>>0);G=gE-n|0;c[a2>>2]=gI+n;n=gB&255;if((n&64|0)==0){c[aB>>2]=gD&65535;p\
=n&15;c[aN>>2]=p;c[bo>>2]=23;bL=dz;bM=G;bN=aa;bO=gG;bP=gH;bQ=p;K=534;break}else\
{c[bo>>2]=29;b5=dz;b6=ar;b7=G;b8=aa;b9=am;ca=gG;cb=ap;cc=gH;cd=Y;ce=X;break}}}w\
hile(0);if((K|0)==330){K=0;do{if((c[aW>>2]&4096|0)==0){aa=c[aY>>2]|0;if((aa|0)=\
=0){gJ=cZ;gK=c_;break}c[aa+36>>2]=0;gJ=cZ;gK=c_}else{if((cZ|0)==0){bX=Z;bY=ar;b\
Z=cX;b_=cY;b$=am;b0=X;break L336}else{gL=0}while(1){gM=gL+1|0;aa=a[c_+gL|0]|0;G\
=c[aY>>2]|0;do{if((G|0)!=0){p=G+36|0;if((c[p>>2]|0)==0){break}n=c[aU>>2]|0;if(n\
>>>0>=(c[G+40>>2]|0)>>>0){break}c[aU>>2]=n+1;a[(c[p>>2]|0)+n|0]=aa}}while(0);gN\
=aa<<24>>24!=0;if(gN&gM>>>0<cZ>>>0){gL=gM}else{break}}if((c[aW>>2]&512|0)!=0){c\
[a_>>2]=bi(c[a_>>2]|0,c_,gM)|0}if(gN){bX=Z;bY=ar;bZ=cX;b_=cY;b$=am;b0=X;break L\
336}else{gJ=cZ-gM|0;gK=c_+gM|0}}}while(0);c[bo>>2]=8;c$=cX;c0=cY;c1=gJ;c2=gK;K=\
343}else if((K|0)==534){K=0;if((bQ|0)==0){gO=bM;gP=bN;gQ=bO;gR=bP}else{if(bM>>>\
0<bQ>>>0){y=bP;B=bO;M=bN;N=bM;while(1){if((B|0)==0){bX=bL;bY=ar;bZ=N;b_=M;b$=am\
;b0=X;break L336}C=B-1|0;G=y+1|0;n=(d[y]<<N)+M|0;p=N+8|0;if(p>>>0<bQ>>>0){y=G;B\
=C;M=n;N=p}else{gS=G;gT=C;gU=n;gV=p;break}}}else{gS=bP;gT=bO;gU=bN;gV=bM}c[aB>>\
2]=(c[aB>>2]|0)+((1<<bQ)-1&gU);c[a2>>2]=(c[a2>>2]|0)+bQ;gO=gV-bQ|0;gP=gU>>>(bQ>\
>>0);gQ=gT;gR=gS}c[bo>>2]=24;dE=bL;dF=gO;dG=gP;dH=gQ;dI=gR;K=540}L788:do{if((K|\
0)==343){K=0;N=c[aW>>2]|0;do{if((N&512|0)==0){gW=c$;gX=c0;gY=c1;gZ=c2}else{if(c\
$>>>0<16){M=c2;B=c1;y=c0;p=c$;while(1){if((B|0)==0){bX=Z;bY=ar;bZ=p;b_=y;b$=am;\
b0=X;break L336}n=B-1|0;C=M+1|0;G=(d[M]<<p)+y|0;A=p+8|0;if(A>>>0<16){M=C;B=n;y=\
G;p=A}else{g_=C;g$=n;g0=G;g1=A;break}}}else{g_=c2;g$=c1;g0=c0;g1=c$}if((g0|0)==\
(c[a_>>2]&65535|0)){gW=0;gX=0;gY=g$;gZ=g_;break}c[bo>>2]=29;b5=Z;b6=ar;b7=g1;b8\
=g0;b9=am;ca=g$;cb=ap;cc=g_;cd=Y;ce=X;break L788}}while(0);p=c[aY>>2]|0;if((p|0\
)!=0){c[p+44>>2]=N>>>9&1;c[(c[aY>>2]|0)+48>>2]=1}c[a_>>2]=0;c[bo>>2]=11;b5=Z;b6\
=ar;b7=gW;b8=gX;b9=am;ca=gY;cb=ap;cc=gZ;cd=Y;ce=X}else if((K|0)==540){K=0;if((a\
m|0)==0){bX=dE;bY=ar;bZ=dF;b_=dG;b$=0;b0=X;break L336}p=ar-am|0;y=c[aB>>2]|0;if\
(y>>>0>p>>>0){B=y-p|0;do{if(B>>>0>(c[bb>>2]|0)>>>0){if((c[aV>>2]|0)==0){break}c\
[bo>>2]=29;b5=dE;b6=ar;b7=dF;b8=dG;b9=am;ca=dH;cb=ap;cc=dI;cd=Y;ce=X;break L788\
}}while(0);N=c[a8>>2]|0;if(B>>>0>N>>>0){p=B-N|0;g2=(c[bq>>2]|0)+((c[bc>>2]|0)-p\
)|0;g3=p}else{g2=(c[bq>>2]|0)+(N-B)|0;g3=B}N=c[aU>>2]|0;g4=g2;g5=g3>>>0>N>>>0?N\
:g3;g6=N}else{N=c[aU>>2]|0;g4=ap+(-y|0)|0;g5=N;g6=N}N=g5>>>0>am>>>0?am:g5;c[aU>\
>2]=g6-N;p=am^-1;M=g5^-1;aa=p>>>0>M>>>0?p:M;M=g4;p=N;A=ap;while(1){a[A]=a[M]|0;\
G=p-1|0;if((G|0)==0){break}else{M=M+1|0;p=G;A=A+1|0}}A=am-N|0;p=ap+(aa^-1)|0;if\
((c[aU>>2]|0)!=0){b5=dE;b6=ar;b7=dF;b8=dG;b9=A;ca=dH;cb=p;cc=dI;cd=Y;ce=X;break\
}c[bo>>2]=20;b5=dE;b6=ar;b7=dF;b8=dG;b9=A;ca=dH;cb=p;cc=dI;cd=Y;ce=X}}while(0);\
Z=b5;ar=b6;as=b7;al=b8;am=b9;an=ca;ap=cb;aq=cc;V=c[bo>>2]|0;Y=cd;X=ce}if((K|0)=\
=356){c[a6>>2]=co;c[o>>2]=cn;break}else if((K|0)==578){c[bo>>2]=28;bX=1;bY=bt;b\
Z=d_;b_=d$;b$=am;b0=by}else if((K|0)==579){bX=-3;bY=ar;bZ=as;b_=al;b$=am;b0=X}c\
[a6>>2]=b_;c[o>>2]=bZ;Y=c[bc>>2]|0;if((Y|0)==0){if(!((c[bo>>2]|0)>>>0>=26|(bY|0\
)==(b$|0))){K=582}}else{K=582}do{if((K|0)==582){V=c[bq>>2]|0;do{if((V|0)==0){aq\
=bf(0,1<<c[bp>>2],1)|0;c[br>>2]=aq;if((aq|0)==0){c[bo>>2]=30;break L333}else{g7\
=aq;g8=c[bc>>2]|0;break}}else{g7=V;g8=Y}}while(0);if((g8|0)==0){V=1<<c[bp>>2];c\
[bc>>2]=V;c[a8>>2]=0;c[bb>>2]=0;g9=V}else{g9=g8}V=bY-b$|0;if(V>>>0>=g9>>>0){aq=\
ap+(-g9|0)|0;bn(g7|0,aq|0,g9)|0;c[a8>>2]=0;c[bb>>2]=c[bc>>2];break}aq=c[a8>>2]|\
0;an=g9-aq|0;Z=an>>>0>V>>>0?V:an;an=g7+aq|0;aq=ap+(-V|0)|0;bn(an|0,aq|0,Z)|0;aq\
=V-Z|0;if((V|0)!=(Z|0)){Z=c[bq>>2]|0;an=ap+(-aq|0)|0;bn(Z|0,an|0,aq)|0;c[a8>>2]\
=aq;c[bb>>2]=c[bc>>2];break}aq=(c[a8>>2]|0)+V|0;c[a8>>2]=aq;an=c[bc>>2]|0;if((a\
q|0)==(an|0)){c[a8>>2]=0}aq=c[bb>>2]|0;if(aq>>>0>=an>>>0){break}c[bb>>2]=aq+V}}\
while(0);Y=bY-b$|0;X=b0+Y|0;c[u>>2]=(c[u>>2]|0)+Y;if(!((c[t>>2]|0)==0|(bY|0)==(\
b$|0))){am=c[a_>>2]|0;al=ap+(-Y|0)|0;if((c[aW>>2]|0)==0){ha=bh(am,al,Y)|0}else{\
ha=bi(am,al,Y)|0}c[a_>>2]=ha}if((((bX|0)==0?-5:bX)|0)!=1){break}Y=c[bq>>2]|0;if\
((Y|0)!=0){bg(0,Y)}bg(0,q);if((X|0)==1e5){break L331}az(12904,24,14192,12872)}}\
while(0);t=c[bq>>2]|0;if((t|0)!=0){bg(0,t)}bg(0,q)}}while(0);if(!bs){i=h;return\
}if((aG(f|0,c[1044]|0)|0)==0){i=h;return}else{az(12904,25,14192,12840)}}functio\
n a2(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0;e=i;do{if((b|0)>1\
){f=a[c[d+4>>2]|0]|0;if((f|0)==50){g=250;break}else if((f|0)==51){h=618;break}e\
lse if((f|0)==52){g=2500;break}else if((f|0)==53){g=5e3;break}else if((f|0)==49\
){g=60;break}else if((f|0)==48){j=0;i=e;return j|0}else{ao(12824,(s=i,i=i+8|0,c\
[s>>2]=f-48,s)|0)|0;j=-1;i=e;return j|0}}else{h=618}}while(0);if((h|0)==618){g=\
500}h=bk(1e5)|0;d=0;b=0;f=17;while(1){do{if((b|0)>0){k=f;l=b-1|0}else{if((d&7|0\
)==0){k=0;l=d&31;break}else{k=((Z(d,d)|0)>>>0)%6714>>>0&255;l=b;break}}}while(0\
);a[h+d|0]=k;m=d+1|0;if((m|0)<1e5){d=m;b=l;f=k}else{n=0;break}}do{a1(h,n);n=n+1\
|0;}while((n|0)<(g|0));ar(8)|0;j=0;i=e;return j|0}function a3(a){a=a|0;var f=0,\
g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0\
,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0;f=a+44|0;g=c[f>>2]|0;h=a+60|0;\
i=a+116|0;j=a+108|0;k=g-262|0;l=a|0;m=a+56|0;n=a+72|0;o=a+88|0;p=a+84|0;q=a+112\
|0;r=a+92|0;s=a+76|0;t=a+68|0;u=a+64|0;v=c[i>>2]|0;w=g;while(1){x=c[j>>2]|0;y=(\
c[h>>2]|0)-v-x|0;if(x>>>0<(k+w|0)>>>0){z=y}else{x=c[m>>2]|0;A=x+g|0;bn(x|0,A|0,\
g)|0;c[q>>2]=(c[q>>2]|0)-g;c[j>>2]=(c[j>>2]|0)-g;c[r>>2]=(c[r>>2]|0)-g;A=c[s>>2\
]|0;x=A;B=(c[t>>2]|0)+(A<<1)|0;do{B=B-2|0;A=e[B>>1]|0;if(A>>>0<g>>>0){C=0}else{\
C=A-g&65535}b[B>>1]=C;x=x-1|0;}while((x|0)!=0);x=g;B=(c[u>>2]|0)+(g<<1)|0;do{B=\
B-2|0;A=e[B>>1]|0;if(A>>>0<g>>>0){D=0}else{D=A-g&65535}b[B>>1]=D;x=x-1|0;}while\
((x|0)!=0);z=y+g|0}x=c[l>>2]|0;B=x+4|0;A=c[B>>2]|0;if((A|0)==0){E=663;break}F=c\
[i>>2]|0;G=(c[m>>2]|0)+(F+(c[j>>2]|0))|0;H=A>>>0>z>>>0?z:A;if((H|0)==0){I=0;J=F\
}else{c[B>>2]=A-H;A=c[(c[x+28>>2]|0)+24>>2]|0;if((A|0)==1){B=x+48|0;F=c[x>>2]|0\
;c[B>>2]=bh(c[B>>2]|0,F,H)|0;K=F}else if((A|0)==2){A=x+48|0;F=c[x>>2]|0;c[A>>2]\
=bi(c[A>>2]|0,F,H)|0;K=F}else{K=c[x>>2]|0}F=x|0;bn(G|0,K|0,H)|0;c[F>>2]=(c[F>>2\
]|0)+H;F=x+8|0;c[F>>2]=(c[F>>2]|0)+H;I=H;J=c[i>>2]|0}L=J+I|0;c[i>>2]=L;if(L>>>0\
>2){H=c[j>>2]|0;F=c[m>>2]|0;x=d[F+H|0]|0;c[n>>2]=x;c[n>>2]=((d[F+(H+1)|0]|0)^x<\
<c[o>>2])&c[p>>2];if(L>>>0>=262){break}}if((c[(c[l>>2]|0)+4>>2]|0)==0){break}v=\
L;w=c[f>>2]|0}if((E|0)==663){return}E=a+5824|0;a=c[E>>2]|0;f=c[h>>2]|0;if(a>>>0\
>=f>>>0){return}h=L+(c[j>>2]|0)|0;if(a>>>0<h>>>0){j=f-h|0;L=j>>>0>258?258:j;bm(\
(c[m>>2]|0)+h|0,0,L|0);c[E>>2]=L+h;return}L=h+258|0;if(a>>>0>=L>>>0){return}h=L\
-a|0;L=f-a|0;f=h>>>0>L>>>0?L:h;bm((c[m>>2]|0)+a|0,0,f|0);c[E>>2]=(c[E>>2]|0)+f;\
return}function a4(a,b){a=a|0;b=b|0;var d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0\
,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;d=(c[a+12>>2]|0)-5|0;e\
=d>>>0<65535?d:65535;d=a+116|0;f=a+108|0;g=a+92|0;h=a+44|0;i=a+56|0;j=a;k=a|0;w\
hile(1){l=c[d>>2]|0;if(l>>>0<2){a3(a);m=c[d>>2]|0;if((m|b|0)==0){n=0;o=696;brea\
k}if((m|0)==0){o=687;break}else{p=m}}else{p=l}l=(c[f>>2]|0)+p|0;c[f>>2]=l;c[d>>\
2]=0;m=c[g>>2]|0;q=m+e|0;if((l|0)!=0&l>>>0<q>>>0){r=l;s=m}else{c[d>>2]=l-q;c[f>\
>2]=q;if((m|0)>-1){t=(c[i>>2]|0)+m|0}else{t=0}ba(j,t,e,0);c[g>>2]=c[f>>2];m=c[k\
>>2]|0;q=m+28|0;l=c[q>>2]|0;u=c[l+20>>2]|0;v=m+16|0;w=c[v>>2]|0;x=u>>>0>w>>>0?w\
:u;do{if((x|0)!=0){u=m+12|0;w=c[u>>2]|0;y=c[l+16>>2]|0;bn(w|0,y|0,x)|0;c[u>>2]=\
(c[u>>2]|0)+x;u=(c[q>>2]|0)+16|0;c[u>>2]=(c[u>>2]|0)+x;u=m+20|0;c[u>>2]=(c[u>>2\
]|0)+x;c[v>>2]=(c[v>>2]|0)-x;u=(c[q>>2]|0)+20|0;c[u>>2]=(c[u>>2]|0)-x;u=c[q>>2]\
|0;if((c[u+20>>2]|0)!=0){break}c[u+16>>2]=c[u+8>>2]}}while(0);if((c[(c[k>>2]|0)\
+16>>2]|0)==0){n=0;o=697;break}r=c[f>>2]|0;s=c[g>>2]|0}q=r-s|0;if(q>>>0<((c[h>>\
2]|0)-262|0)>>>0){continue}if((s|0)>-1){z=(c[i>>2]|0)+s|0}else{z=0}ba(j,z,q,0);\
c[g>>2]=c[f>>2];q=c[k>>2]|0;x=q+28|0;v=c[x>>2]|0;m=c[v+20>>2]|0;l=q+16|0;u=c[l>\
>2]|0;y=m>>>0>u>>>0?u:m;do{if((y|0)!=0){m=q+12|0;u=c[m>>2]|0;w=c[v+16>>2]|0;bn(\
u|0,w|0,y)|0;c[m>>2]=(c[m>>2]|0)+y;m=(c[x>>2]|0)+16|0;c[m>>2]=(c[m>>2]|0)+y;m=q\
+20|0;c[m>>2]=(c[m>>2]|0)+y;c[l>>2]=(c[l>>2]|0)-y;m=(c[x>>2]|0)+20|0;c[m>>2]=(c\
[m>>2]|0)-y;m=c[x>>2]|0;if((c[m+20>>2]|0)!=0){break}c[m+16>>2]=c[m+8>>2]}}while\
(0);if((c[(c[k>>2]|0)+16>>2]|0)==0){n=0;o=698;break}}if((o|0)==687){z=c[g>>2]|0\
;if((z|0)>-1){A=(c[i>>2]|0)+z|0}else{A=0}i=(b|0)==4;ba(j,A,(c[f>>2]|0)-z|0,i&1)\
;c[g>>2]=c[f>>2];f=c[k>>2]|0;g=f+28|0;z=c[g>>2]|0;A=c[z+20>>2]|0;j=f+16|0;b=c[j\
>>2]|0;s=A>>>0>b>>>0?b:A;do{if((s|0)!=0){A=f+12|0;b=c[A>>2]|0;h=c[z+16>>2]|0;bn\
(b|0,h|0,s)|0;c[A>>2]=(c[A>>2]|0)+s;A=(c[g>>2]|0)+16|0;c[A>>2]=(c[A>>2]|0)+s;A=\
f+20|0;c[A>>2]=(c[A>>2]|0)+s;c[j>>2]=(c[j>>2]|0)-s;A=(c[g>>2]|0)+20|0;c[A>>2]=(\
c[A>>2]|0)-s;A=c[g>>2]|0;if((c[A+20>>2]|0)!=0){break}c[A+16>>2]=c[A+8>>2]}}whil\
e(0);if((c[(c[k>>2]|0)+16>>2]|0)==0){n=i?2:0;return n|0}else{n=i?3:1;return n|0\
}}else if((o|0)==696){return n|0}else if((o|0)==697){return n|0}else if((o|0)==\
698){return n|0}return 0}function a5(e,f){e=e|0;f=f|0;var g=0,h=0,i=0,j=0,k=0,l\
=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,\
F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0;g=e+116|0;h=(f|0)==0;i=e+72\
|0;j=e+88|0;k=e+108|0;l=e+56|0;m=e+84|0;n=e+68|0;o=e+52|0;p=e+64|0;q=e+44|0;r=e\
+96|0;s=e+112|0;t=e+5792|0;u=e+5796|0;v=e+5784|0;w=e+5788|0;x=e+128|0;y=e+92|0;\
z=e;A=e|0;while(1){if((c[g>>2]|0)>>>0<262){a3(e);B=c[g>>2]|0;if(B>>>0<262&h){C=\
0;D=735;break}if((B|0)==0){D=726;break}if(B>>>0>2){D=706}else{D=709}}else{D=706\
}do{if((D|0)==706){D=0;B=c[k>>2]|0;E=((d[(c[l>>2]|0)+(B+2)|0]|0)^c[i>>2]<<c[j>>\
2])&c[m>>2];c[i>>2]=E;F=b[(c[n>>2]|0)+(E<<1)>>1]|0;b[(c[p>>2]|0)+((c[o>>2]&B)<<\
1)>>1]=F;B=F&65535;b[(c[n>>2]|0)+(c[i>>2]<<1)>>1]=c[k>>2]&65535;if(F<<16>>16==0\
){D=709;break}if(((c[k>>2]|0)-B|0)>>>0>((c[q>>2]|0)-262|0)>>>0){D=709;break}F=a\
6(e,B)|0;c[r>>2]=F;G=F}}while(0);if((D|0)==709){D=0;G=c[r>>2]|0}do{if(G>>>0>2){\
F=G+253|0;B=(c[k>>2]|0)-(c[s>>2]|0)&65535;b[(c[u>>2]|0)+(c[t>>2]<<1)>>1]=B;E=c[\
t>>2]|0;c[t>>2]=E+1;a[(c[v>>2]|0)+E|0]=F&255;E=B-1&65535;B=e+148+((d[12952+(F&2\
55)|0]|0|256)+1<<2)|0;b[B>>1]=(b[B>>1]|0)+1&65535;B=E&65535;if((E&65535)<256){H\
=B}else{H=(B>>>7)+256|0}B=e+2440+((d[H+13680|0]|0)<<2)|0;b[B>>1]=(b[B>>1]|0)+1&\
65535;B=(c[t>>2]|0)==((c[w>>2]|0)-1|0)&1;E=c[r>>2]|0;F=(c[g>>2]|0)-E|0;c[g>>2]=\
F;if(!(E>>>0<=(c[x>>2]|0)>>>0&F>>>0>2)){F=(c[k>>2]|0)+E|0;c[k>>2]=F;c[r>>2]=0;I\
=c[l>>2]|0;J=d[I+F|0]|0;c[i>>2]=J;c[i>>2]=((d[I+(F+1)|0]|0)^J<<c[j>>2])&c[m>>2]\
;K=B;L=F;break}c[r>>2]=E-1;do{E=c[k>>2]|0;F=E+1|0;c[k>>2]=F;J=((d[(c[l>>2]|0)+(\
E+3)|0]|0)^c[i>>2]<<c[j>>2])&c[m>>2];c[i>>2]=J;b[(c[p>>2]|0)+((c[o>>2]&F)<<1)>>\
1]=b[(c[n>>2]|0)+(J<<1)>>1]|0;b[(c[n>>2]|0)+(c[i>>2]<<1)>>1]=c[k>>2]&65535;J=(c\
[r>>2]|0)-1|0;c[r>>2]=J;}while((J|0)!=0);J=(c[k>>2]|0)+1|0;c[k>>2]=J;K=B;L=J}el\
se{J=a[(c[l>>2]|0)+(c[k>>2]|0)|0]|0;b[(c[u>>2]|0)+(c[t>>2]<<1)>>1]=0;F=c[t>>2]|\
0;c[t>>2]=F+1;a[(c[v>>2]|0)+F|0]=J;F=e+148+((J&255)<<2)|0;b[F>>1]=(b[F>>1]|0)+1\
&65535;F=(c[t>>2]|0)==((c[w>>2]|0)-1|0)&1;c[g>>2]=(c[g>>2]|0)-1;J=(c[k>>2]|0)+1\
|0;c[k>>2]=J;K=F;L=J}}while(0);if((K|0)==0){continue}J=c[y>>2]|0;if((J|0)>-1){M\
=(c[l>>2]|0)+J|0}else{M=0}ba(z,M,L-J|0,0);c[y>>2]=c[k>>2];J=c[A>>2]|0;F=J+28|0;\
E=c[F>>2]|0;I=c[E+20>>2]|0;N=J+16|0;O=c[N>>2]|0;P=I>>>0>O>>>0?O:I;do{if((P|0)!=\
0){I=J+12|0;O=c[I>>2]|0;Q=c[E+16>>2]|0;bn(O|0,Q|0,P)|0;c[I>>2]=(c[I>>2]|0)+P;I=\
(c[F>>2]|0)+16|0;c[I>>2]=(c[I>>2]|0)+P;I=J+20|0;c[I>>2]=(c[I>>2]|0)+P;c[N>>2]=(\
c[N>>2]|0)-P;I=(c[F>>2]|0)+20|0;c[I>>2]=(c[I>>2]|0)-P;I=c[F>>2]|0;if((c[I+20>>2\
]|0)!=0){break}c[I+16>>2]=c[I+8>>2]}}while(0);if((c[(c[A>>2]|0)+16>>2]|0)==0){C\
=0;D=736;break}}if((D|0)==726){L=c[y>>2]|0;if((L|0)>-1){R=(c[l>>2]|0)+L|0}else{\
R=0}l=(f|0)==4;ba(z,R,(c[k>>2]|0)-L|0,l&1);c[y>>2]=c[k>>2];k=c[A>>2]|0;y=k+28|0\
;L=c[y>>2]|0;R=c[L+20>>2]|0;z=k+16|0;f=c[z>>2]|0;M=R>>>0>f>>>0?f:R;do{if((M|0)!\
=0){R=k+12|0;f=c[R>>2]|0;K=c[L+16>>2]|0;bn(f|0,K|0,M)|0;c[R>>2]=(c[R>>2]|0)+M;R\
=(c[y>>2]|0)+16|0;c[R>>2]=(c[R>>2]|0)+M;R=k+20|0;c[R>>2]=(c[R>>2]|0)+M;c[z>>2]=\
(c[z>>2]|0)-M;R=(c[y>>2]|0)+20|0;c[R>>2]=(c[R>>2]|0)-M;R=c[y>>2]|0;if((c[R+20>>\
2]|0)!=0){break}c[R+16>>2]=c[R+8>>2]}}while(0);if((c[(c[A>>2]|0)+16>>2]|0)==0){\
C=l?2:0;return C|0}else{C=l?3:1;return C|0}}else if((D|0)==735){return C|0}else\
 if((D|0)==736){return C|0}return 0}function a6(b,d){b=b|0;d=d|0;var f=0,g=0,h=\
0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B\
=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0;f=c[b+124>>2]|0;g=c[b+56>>2]|0;h=c[b+108>>2]|0;i\
=g+h|0;j=c[b+120>>2]|0;k=c[b+144>>2]|0;l=(c[b+44>>2]|0)-262|0;m=h>>>0>l>>>0?h-l\
|0:0;l=c[b+64>>2]|0;n=c[b+52>>2]|0;o=g+(h+258)|0;p=c[b+116>>2]|0;q=k>>>0>p>>>0?\
p:k;k=b+112|0;r=g+(h+1)|0;s=g+(h+2)|0;t=o;u=h+257|0;v=a[g+(j+h)|0]|0;w=a[g+(h-1\
+j)|0]|0;x=d;d=j>>>0<(c[b+140>>2]|0)>>>0?f:f>>>2;f=j;L1039:while(1){j=g+x|0;do{\
if((a[g+(x+f)|0]|0)==v<<24>>24){if((a[g+(f-1+x)|0]|0)!=w<<24>>24){y=v;z=w;A=f;b\
reak}if((a[j]|0)!=(a[i]|0)){y=v;z=w;A=f;break}if((a[g+(x+1)|0]|0)!=(a[r]|0)){y=\
v;z=w;A=f;break}b=s;B=g+(x+2)|0;while(1){C=b+1|0;if((a[C]|0)!=(a[B+1|0]|0)){D=C\
;break}C=b+2|0;if((a[C]|0)!=(a[B+2|0]|0)){D=C;break}C=b+3|0;if((a[C]|0)!=(a[B+3\
|0]|0)){D=C;break}C=b+4|0;if((a[C]|0)!=(a[B+4|0]|0)){D=C;break}C=b+5|0;if((a[C]\
|0)!=(a[B+5|0]|0)){D=C;break}C=b+6|0;if((a[C]|0)!=(a[B+6|0]|0)){D=C;break}C=b+7\
|0;if((a[C]|0)!=(a[B+7|0]|0)){D=C;break}C=b+8|0;E=B+8|0;if((a[C]|0)==(a[E]|0)&C\
>>>0<o>>>0){b=C;B=E}else{D=C;break}}B=D-t|0;b=B+258|0;if((b|0)<=(f|0)){y=v;z=w;\
A=f;break}c[k>>2]=x;if((b|0)>=(q|0)){F=b;G=759;break L1039}y=a[g+(b+h)|0]|0;z=a\
[g+(u+B)|0]|0;A=b}else{y=v;z=w;A=f}}while(0);j=e[l+((x&n)<<1)>>1]|0;if(j>>>0<=m\
>>>0){F=A;G=760;break}b=d-1|0;if((b|0)==0){F=A;G=761;break}else{v=y;w=z;x=j;d=b\
;f=A}}if((G|0)==759){H=F>>>0>p>>>0;I=H?p:F;return I|0}else if((G|0)==760){H=F>>\
>0>p>>>0;I=H?p:F;return I|0}else if((G|0)==761){H=F>>>0>p>>>0;I=H?p:F;return I|\
0}return 0}function a7(a){a=a|0;var d=0;d=0;do{b[a+148+(d<<2)>>1]=0;d=d+1|0;}wh\
ile((d|0)<286);b[a+2440>>1]=0;b[a+2444>>1]=0;b[a+2448>>1]=0;b[a+2452>>1]=0;b[a+\
2456>>1]=0;b[a+2460>>1]=0;b[a+2464>>1]=0;b[a+2468>>1]=0;b[a+2472>>1]=0;b[a+2476\
>>1]=0;b[a+2480>>1]=0;b[a+2484>>1]=0;b[a+2488>>1]=0;b[a+2492>>1]=0;b[a+2496>>1]\
=0;b[a+2500>>1]=0;b[a+2504>>1]=0;b[a+2508>>1]=0;b[a+2512>>1]=0;b[a+2516>>1]=0;b\
[a+2520>>1]=0;b[a+2524>>1]=0;b[a+2528>>1]=0;b[a+2532>>1]=0;b[a+2536>>1]=0;b[a+2\
540>>1]=0;b[a+2544>>1]=0;b[a+2548>>1]=0;b[a+2552>>1]=0;b[a+2556>>1]=0;b[a+2684>\
>1]=0;b[a+2688>>1]=0;b[a+2692>>1]=0;b[a+2696>>1]=0;b[a+2700>>1]=0;b[a+2704>>1]=\
0;b[a+2708>>1]=0;b[a+2712>>1]=0;b[a+2716>>1]=0;b[a+2720>>1]=0;b[a+2724>>1]=0;b[\
a+2728>>1]=0;b[a+2732>>1]=0;b[a+2736>>1]=0;b[a+2740>>1]=0;b[a+2744>>1]=0;b[a+27\
48>>1]=0;b[a+2752>>1]=0;b[a+2756>>1]=0;b[a+1172>>1]=1;c[a+5804>>2]=0;c[a+5800>>\
2]=0;c[a+5808>>2]=0;c[a+5792>>2]=0;return}function a8(e,f){e=e|0;f=f|0;var g=0,\
h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0\
,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=\
0,V=0,W=0,X=0,Y=0;g=e+116|0;h=(f|0)==0;i=e+72|0;j=e+88|0;k=e+108|0;l=e+56|0;m=e\
+84|0;n=e+68|0;o=e+52|0;p=e+64|0;q=e+96|0;r=e+120|0;s=e+112|0;t=e+100|0;u=e+579\
2|0;v=e+5796|0;w=e+5784|0;x=e+5788|0;y=e+104|0;z=e+92|0;A=e;B=e|0;C=e+128|0;D=e\
+44|0;E=e+136|0;L1069:while(1){F=c[g>>2]|0;while(1){do{if(F>>>0<262){a3(e);G=c[\
g>>2]|0;if(G>>>0<262&h){H=0;I=815;break L1069}if((G|0)==0){I=804;break L1069}if\
(G>>>0>2){I=772;break}c[r>>2]=c[q>>2];c[t>>2]=c[s>>2];c[q>>2]=2;J=2;I=780}else{\
I=772}}while(0);do{if((I|0)==772){I=0;G=c[k>>2]|0;K=((d[(c[l>>2]|0)+(G+2)|0]|0)\
^c[i>>2]<<c[j>>2])&c[m>>2];c[i>>2]=K;L=b[(c[n>>2]|0)+(K<<1)>>1]|0;b[(c[p>>2]|0)\
+((c[o>>2]&G)<<1)>>1]=L;G=L&65535;b[(c[n>>2]|0)+(c[i>>2]<<1)>>1]=c[k>>2]&65535;\
K=c[q>>2]|0;c[r>>2]=K;c[t>>2]=c[s>>2];c[q>>2]=2;if(L<<16>>16==0){J=2;I=780;brea\
k}if(K>>>0>=(c[C>>2]|0)>>>0){M=K;N=2;break}if(((c[k>>2]|0)-G|0)>>>0>((c[D>>2]|0\
)-262|0)>>>0){J=2;I=780;break}K=a6(e,G)|0;c[q>>2]=K;if(K>>>0>=6){J=K;I=780;brea\
k}if((c[E>>2]|0)!=1){if((K|0)!=3){J=K;I=780;break}if(((c[k>>2]|0)-(c[s>>2]|0)|0\
)>>>0<=4096){J=3;I=780;break}}c[q>>2]=2;J=2;I=780}}while(0);if((I|0)==780){I=0;\
M=c[r>>2]|0;N=J}if(!(M>>>0<3|N>>>0>M>>>0)){break}if((c[y>>2]|0)==0){c[y>>2]=1;c\
[k>>2]=(c[k>>2]|0)+1;K=(c[g>>2]|0)-1|0;c[g>>2]=K;F=K;continue}K=a[(c[l>>2]|0)+(\
(c[k>>2]|0)-1)|0]|0;b[(c[v>>2]|0)+(c[u>>2]<<1)>>1]=0;G=c[u>>2]|0;c[u>>2]=G+1;a[\
(c[w>>2]|0)+G|0]=K;G=e+148+((K&255)<<2)|0;b[G>>1]=(b[G>>1]|0)+1&65535;do{if((c[\
u>>2]|0)==((c[x>>2]|0)-1|0)){G=c[z>>2]|0;if((G|0)>-1){O=(c[l>>2]|0)+G|0}else{O=\
0}ba(A,O,(c[k>>2]|0)-G|0,0);c[z>>2]=c[k>>2];G=c[B>>2]|0;K=G+28|0;L=c[K>>2]|0;P=\
c[L+20>>2]|0;Q=G+16|0;R=c[Q>>2]|0;S=P>>>0>R>>>0?R:P;if((S|0)==0){break}P=G+12|0\
;R=c[P>>2]|0;T=c[L+16>>2]|0;bn(R|0,T|0,S)|0;c[P>>2]=(c[P>>2]|0)+S;P=(c[K>>2]|0)\
+16|0;c[P>>2]=(c[P>>2]|0)+S;P=G+20|0;c[P>>2]=(c[P>>2]|0)+S;c[Q>>2]=(c[Q>>2]|0)-\
S;Q=(c[K>>2]|0)+20|0;c[Q>>2]=(c[Q>>2]|0)-S;S=c[K>>2]|0;if((c[S+20>>2]|0)!=0){br\
eak}c[S+16>>2]=c[S+8>>2]}}while(0);c[k>>2]=(c[k>>2]|0)+1;S=(c[g>>2]|0)-1|0;c[g>\
>2]=S;if((c[(c[B>>2]|0)+16>>2]|0)==0){H=0;I=817;break L1069}else{F=S}}F=c[k>>2]\
|0;S=F-3+(c[g>>2]|0)|0;K=M+253|0;Q=F+65535-(c[t>>2]|0)&65535;b[(c[v>>2]|0)+(c[u\
>>2]<<1)>>1]=Q;F=c[u>>2]|0;c[u>>2]=F+1;a[(c[w>>2]|0)+F|0]=K&255;F=Q-1&65535;Q=e\
+148+((d[12952+(K&255)|0]|0|256)+1<<2)|0;b[Q>>1]=(b[Q>>1]|0)+1&65535;Q=F&65535;\
if((F&65535)<256){U=Q}else{U=(Q>>>7)+256|0}Q=e+2440+((d[U+13680|0]|0)<<2)|0;b[Q\
>>1]=(b[Q>>1]|0)+1&65535;Q=c[u>>2]|0;F=(c[x>>2]|0)-1|0;K=c[r>>2]|0;c[g>>2]=1-K+\
(c[g>>2]|0);P=K-2|0;c[r>>2]=P;K=P;do{P=c[k>>2]|0;G=P+1|0;c[k>>2]=G;if(G>>>0>S>>\
>0){V=K}else{T=((d[(c[l>>2]|0)+(P+3)|0]|0)^c[i>>2]<<c[j>>2])&c[m>>2];c[i>>2]=T;\
b[(c[p>>2]|0)+((c[o>>2]&G)<<1)>>1]=b[(c[n>>2]|0)+(T<<1)>>1]|0;b[(c[n>>2]|0)+(c[\
i>>2]<<1)>>1]=c[k>>2]&65535;V=c[r>>2]|0}K=V-1|0;c[r>>2]=K;}while((K|0)!=0);c[y>\
>2]=0;c[q>>2]=2;K=(c[k>>2]|0)+1|0;c[k>>2]=K;if((Q|0)!=(F|0)){continue}S=c[z>>2]\
|0;if((S|0)>-1){W=(c[l>>2]|0)+S|0}else{W=0}ba(A,W,K-S|0,0);c[z>>2]=c[k>>2];S=c[\
B>>2]|0;K=S+28|0;T=c[K>>2]|0;G=c[T+20>>2]|0;P=S+16|0;R=c[P>>2]|0;L=G>>>0>R>>>0?\
R:G;do{if((L|0)!=0){G=S+12|0;R=c[G>>2]|0;X=c[T+16>>2]|0;bn(R|0,X|0,L)|0;c[G>>2]\
=(c[G>>2]|0)+L;G=(c[K>>2]|0)+16|0;c[G>>2]=(c[G>>2]|0)+L;G=S+20|0;c[G>>2]=(c[G>>\
2]|0)+L;c[P>>2]=(c[P>>2]|0)-L;G=(c[K>>2]|0)+20|0;c[G>>2]=(c[G>>2]|0)-L;G=c[K>>2\
]|0;if((c[G+20>>2]|0)!=0){break}c[G+16>>2]=c[G+8>>2]}}while(0);if((c[(c[B>>2]|0\
)+16>>2]|0)==0){H=0;I=816;break}}if((I|0)==804){if((c[y>>2]|0)!=0){W=a[(c[l>>2]\
|0)+((c[k>>2]|0)-1)|0]|0;b[(c[v>>2]|0)+(c[u>>2]<<1)>>1]=0;v=c[u>>2]|0;c[u>>2]=v\
+1;a[(c[w>>2]|0)+v|0]=W;v=e+148+((W&255)<<2)|0;b[v>>1]=(b[v>>1]|0)+1&65535;c[y>\
>2]=0}y=c[z>>2]|0;if((y|0)>-1){Y=(c[l>>2]|0)+y|0}else{Y=0}l=(f|0)==4;ba(A,Y,(c[\
k>>2]|0)-y|0,l&1);c[z>>2]=c[k>>2];k=c[B>>2]|0;z=k+28|0;y=c[z>>2]|0;Y=c[y+20>>2]\
|0;A=k+16|0;f=c[A>>2]|0;v=Y>>>0>f>>>0?f:Y;do{if((v|0)!=0){Y=k+12|0;f=c[Y>>2]|0;\
W=c[y+16>>2]|0;bn(f|0,W|0,v)|0;c[Y>>2]=(c[Y>>2]|0)+v;Y=(c[z>>2]|0)+16|0;c[Y>>2]\
=(c[Y>>2]|0)+v;Y=k+20|0;c[Y>>2]=(c[Y>>2]|0)+v;c[A>>2]=(c[A>>2]|0)-v;Y=(c[z>>2]|\
0)+20|0;c[Y>>2]=(c[Y>>2]|0)-v;Y=c[z>>2]|0;if((c[Y+20>>2]|0)!=0){break}c[Y+16>>2\
]=c[Y+8>>2]}}while(0);if((c[(c[B>>2]|0)+16>>2]|0)==0){H=l?2:0;return H|0}else{H\
=l?3:1;return H|0}}else if((I|0)==815){return H|0}else if((I|0)==816){return H|\
0}else if((I|0)==817){return H|0}return 0}function a9(d,f,g,h){d=d|0;f=f|0;g=g|\
0;h=h|0;var i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0;i=d+5820|0;\
j=c[i>>2]|0;k=h&65535;h=d+5816|0;l=e[h>>1]|0|k<<j;b[h>>1]=l&65535;if((j|0)>13){\
m=d+20|0;n=c[m>>2]|0;c[m>>2]=n+1;o=d+8|0;a[(c[o>>2]|0)+n|0]=l&255;n=(e[h>>1]|0)\
>>>8&255;p=c[m>>2]|0;c[m>>2]=p+1;a[(c[o>>2]|0)+p|0]=n;n=c[i>>2]|0;p=k>>>((16-n|\
0)>>>0);b[h>>1]=p&65535;q=n-13|0;r=p}else{q=j+3|0;r=l}l=r&255;c[i>>2]=q;do{if((\
q|0)>8){r=d+20|0;j=c[r>>2]|0;c[r>>2]=j+1;p=d+8|0;a[(c[p>>2]|0)+j|0]=l;j=(e[h>>1\
]|0)>>>8&255;n=c[r>>2]|0;c[r>>2]=n+1;a[(c[p>>2]|0)+n|0]=j;s=r;t=p}else{p=d+20|0\
;if((q|0)>0){r=c[p>>2]|0;c[p>>2]=r+1;j=d+8|0;a[(c[j>>2]|0)+r|0]=l;s=p;t=j;break\
}else{s=p;t=d+8|0;break}}}while(0);b[h>>1]=0;c[i>>2]=0;c[d+5812>>2]=8;d=c[s>>2]\
|0;c[s>>2]=d+1;a[(c[t>>2]|0)+d|0]=g&255;d=c[s>>2]|0;c[s>>2]=d+1;a[(c[t>>2]|0)+d\
|0]=g>>>8&255;d=g&65535^65535;i=c[s>>2]|0;c[s>>2]=i+1;a[(c[t>>2]|0)+i|0]=d&255;\
i=c[s>>2]|0;c[s>>2]=i+1;a[(c[t>>2]|0)+i|0]=d>>>8&255;if((g|0)==0){return}else{u\
=g;v=f}while(1){f=u-1|0;g=a[v]|0;d=c[s>>2]|0;c[s>>2]=d+1;a[(c[t>>2]|0)+d|0]=g;i\
f((f|0)==0){break}else{u=f;v=v+1|0}}return}function ba(f,g,h,i){f=f|0;g=g|0;h=h\
|0;i=i|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=\
0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0;if((c[f+132>>2]|0)>0){j=(c[f>\
>2]|0)+44|0;if((c[j>>2]|0)==2){k=-201342849;l=0;while(1){if((k&1|0)!=0){if((b[f\
+148+(l<<2)>>1]|0)!=0){m=0;break}}n=l+1|0;if((n|0)<32){k=k>>>1;l=n}else{o=838;b\
reak}}L1167:do{if((o|0)==838){if((b[f+184>>1]|0)!=0){m=1;break}if((b[f+188>>1]|\
0)!=0){m=1;break}if((b[f+200>>1]|0)==0){p=32}else{m=1;break}while(1){if((p|0)>=\
256){m=0;break L1167}if((b[f+148+(p<<2)>>1]|0)==0){p=p+1|0}else{m=1;break}}}}wh\
ile(0);c[j>>2]=m}bb(f,f+2840|0);bb(f,f+2852|0);be(f,f+148|0,c[f+2844>>2]|0);be(\
f,f+2440|0,c[f+2856>>2]|0);bb(f,f+2864|0);m=18;while(1){if((m|0)<=2){break}if((\
b[f+2684+(d[m+12504|0]<<2)+2>>1]|0)==0){m=m-1|0}else{break}}j=f+5800|0;p=(m*3&-\
1)+17+(c[j>>2]|0)|0;c[j>>2]=p;j=(p+10|0)>>>3;p=((c[f+5804>>2]|0)+10|0)>>>3;q=p>\
>>0>j>>>0?j:p;r=p;s=m}else{m=h+5|0;q=m;r=m;s=0}do{if((h+4|0)>>>0>q>>>0|(g|0)==0\
){m=f+5820|0;p=c[m>>2]|0;j=(p|0)>13;if((c[f+136>>2]|0)==4|(r|0)==(q|0)){o=i+2&6\
5535;l=f+5816|0;k=e[l>>1]|o<<p;b[l>>1]=k&65535;if(j){n=f+20|0;t=c[n>>2]|0;c[n>>\
2]=t+1;u=f+8|0;a[(c[u>>2]|0)+t|0]=k&255;k=(e[l>>1]|0)>>>8&255;t=c[n>>2]|0;c[n>>\
2]=t+1;a[(c[u>>2]|0)+t|0]=k;k=c[m>>2]|0;b[l>>1]=o>>>((16-k|0)>>>0)&65535;v=k-13\
|0}else{v=p+3|0}c[m>>2]=v;bc(f,16,1192);break}k=i+4&65535;o=f+5816|0;l=e[o>>1]|\
k<<p;t=l&65535;b[o>>1]=t;if(j){j=f+20|0;u=c[j>>2]|0;c[j>>2]=u+1;n=f+8|0;a[(c[n>\
>2]|0)+u|0]=l&255;l=(e[o>>1]|0)>>>8&255;u=c[j>>2]|0;c[j>>2]=u+1;a[(c[n>>2]|0)+u\
|0]=l;l=c[m>>2]|0;u=k>>>((16-l|0)>>>0)&65535;b[o>>1]=u;w=l-13|0;x=u}else{w=p+3|\
0;x=t}c[m>>2]=w;t=c[f+2844>>2]|0;p=c[f+2856>>2]|0;u=s+1|0;l=t+65280&65535;k=x&6\
5535|l<<w;n=k&65535;b[o>>1]=n;if((w|0)>11){j=f+20|0;y=c[j>>2]|0;c[j>>2]=y+1;z=f\
+8|0;a[(c[z>>2]|0)+y|0]=k&255;k=(e[o>>1]|0)>>>8&255;y=c[j>>2]|0;c[j>>2]=y+1;a[(\
c[z>>2]|0)+y|0]=k;k=c[m>>2]|0;y=l>>>((16-k|0)>>>0)&65535;b[o>>1]=y;A=k-11|0;B=y\
}else{A=w+5|0;B=n}c[m>>2]=A;n=p&65535;y=n<<A|B&65535;k=y&65535;b[o>>1]=k;if((A|\
0)>11){l=f+20|0;z=c[l>>2]|0;c[l>>2]=z+1;j=f+8|0;a[(c[j>>2]|0)+z|0]=y&255;y=(e[o\
>>1]|0)>>>8&255;z=c[l>>2]|0;c[l>>2]=z+1;a[(c[j>>2]|0)+z|0]=y;y=c[m>>2]|0;z=n>>>\
((16-y|0)>>>0)&65535;b[o>>1]=z;C=y-11|0;D=z}else{C=A+5|0;D=k}c[m>>2]=C;k=s+6553\
3&65535;z=k<<C|D&65535;y=z&65535;b[o>>1]=y;if((C|0)>12){n=f+20|0;j=c[n>>2]|0;c[\
n>>2]=j+1;l=f+8|0;a[(c[l>>2]|0)+j|0]=z&255;z=(e[o>>1]|0)>>>8&255;j=c[n>>2]|0;c[\
n>>2]=j+1;a[(c[l>>2]|0)+j|0]=z;z=c[m>>2]|0;j=k>>>((16-z|0)>>>0)&65535;b[o>>1]=j\
;E=z-12|0;F=j}else{E=C+4|0;F=y}c[m>>2]=E;if((u|0)>0){y=f+20|0;j=f+8|0;z=0;k=E;l\
=F;while(1){n=e[f+2684+(d[z+12504|0]<<2)+2>>1]|0;G=n<<k|l&65535;H=G&65535;b[o>>\
1]=H;if((k|0)>13){I=c[y>>2]|0;c[y>>2]=I+1;a[(c[j>>2]|0)+I|0]=G&255;G=(e[o>>1]|0\
)>>>8&255;I=c[y>>2]|0;c[y>>2]=I+1;a[(c[j>>2]|0)+I|0]=G;G=c[m>>2]|0;I=n>>>((16-G\
|0)>>>0)&65535;b[o>>1]=I;J=G-13|0;K=I}else{J=k+3|0;K=H}c[m>>2]=J;H=z+1|0;if((H|\
0)<(u|0)){z=H;k=J;l=K}else{break}}}l=f+148|0;bd(f,l,t);k=f+2440|0;bd(f,k,p);bc(\
f,l,k)}else{a9(f,g,h,i)}}while(0);a7(f);if((i|0)==0){return}i=f+5820|0;h=c[i>>2\
]|0;do{if((h|0)>8){g=f+5816|0;K=b[g>>1]&255;J=f+20|0;F=c[J>>2]|0;c[J>>2]=F+1;E=\
f+8|0;a[(c[E>>2]|0)+F|0]=K;K=(e[g>>1]|0)>>>8&255;F=c[J>>2]|0;c[J>>2]=F+1;a[(c[E\
>>2]|0)+F|0]=K;L=g}else{g=f+5816|0;if((h|0)<=0){L=g;break}K=b[g>>1]&255;F=f+20|\
0;E=c[F>>2]|0;c[F>>2]=E+1;a[(c[f+8>>2]|0)+E|0]=K;L=g}}while(0);b[L>>1]=0;c[i>>2\
]=0;return}function bb(f,g){f=f|0;g=g|0;var h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0\
,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=\
0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,_=0;h=i;i=i+32|0;j=h|\
0;k=g|0;l=c[k>>2]|0;m=g+8|0;n=c[m>>2]|0;o=c[n>>2]|0;p=c[n+12>>2]|0;n=f+5200|0;c\
[n>>2]=0;q=f+5204|0;c[q>>2]=573;if((p|0)>0){r=0;s=-1;while(1){if((b[l+(r<<2)>>1\
]|0)==0){b[l+(r<<2)+2>>1]=0;t=s}else{u=(c[n>>2]|0)+1|0;c[n>>2]=u;c[f+2908+(u<<2\
)>>2]=r;a[r+(f+5208)|0]=0;t=r}u=r+1|0;if((u|0)<(p|0)){r=u;s=t}else{break}}s=c[n\
>>2]|0;if((s|0)<2){v=s;w=t;x=886}else{y=t}}else{v=0;w=-1;x=886}if((x|0)==886){x\
=f+5800|0;t=f+5804|0;if((o|0)==0){s=w;r=v;while(1){u=(s|0)<2;z=s+1|0;A=u?z:s;B=\
u?z:0;z=r+1|0;c[n>>2]=z;c[f+2908+(z<<2)>>2]=B;b[l+(B<<2)>>1]=1;a[B+(f+5208)|0]=\
0;c[x>>2]=(c[x>>2]|0)-1;B=c[n>>2]|0;if((B|0)<2){s=A;r=B}else{y=A;break}}}else{r\
=w;w=v;while(1){v=(r|0)<2;s=r+1|0;A=v?s:r;B=v?s:0;s=w+1|0;c[n>>2]=s;c[f+2908+(s\
<<2)>>2]=B;b[l+(B<<2)>>1]=1;a[B+(f+5208)|0]=0;c[x>>2]=(c[x>>2]|0)-1;c[t>>2]=(c[\
t>>2]|0)-(e[o+(B<<2)+2>>1]|0);B=c[n>>2]|0;if((B|0)<2){r=A;w=B}else{y=A;break}}}\
}w=g+4|0;c[w>>2]=y;g=c[n>>2]|0;if((g|0)>1){r=(g|0)/2&-1;o=g;while(1){t=c[f+2908\
+(r<<2)>>2]|0;x=t+(f+5208)|0;A=r<<1;L1248:do{if((A|0)>(o|0)){C=r}else{B=l+(t<<2\
)|0;s=r;v=A;z=o;while(1){do{if((v|0)<(z|0)){u=v|1;D=c[f+2908+(u<<2)>>2]|0;E=b[l\
+(D<<2)>>1]|0;F=c[f+2908+(v<<2)>>2]|0;G=b[l+(F<<2)>>1]|0;if((E&65535)>=(G&65535\
)){if(E<<16>>16!=G<<16>>16){H=v;break}if((d[D+(f+5208)|0]|0)>(d[F+(f+5208)|0]|0\
)){H=v;break}}H=u}else{H=v}}while(0);u=b[B>>1]|0;F=c[f+2908+(H<<2)>>2]|0;D=b[l+\
(F<<2)>>1]|0;if((u&65535)<(D&65535)){C=s;break L1248}if(u<<16>>16==D<<16>>16){i\
f((d[x]|0)<=(d[F+(f+5208)|0]|0)){C=s;break L1248}}c[f+2908+(s<<2)>>2]=F;F=H<<1;\
D=c[n>>2]|0;if((F|0)>(D|0)){C=H;break}else{s=H;v=F;z=D}}}}while(0);c[f+2908+(C<\
<2)>>2]=t;x=r-1|0;A=c[n>>2]|0;if((x|0)>0){r=x;o=A}else{I=A;break}}}else{I=g}g=f\
+2912|0;o=p;p=I;while(1){I=c[g>>2]|0;r=p-1|0;c[n>>2]=r;C=c[f+2908+(p<<2)>>2]|0;\
c[g>>2]=C;H=C+(f+5208)|0;L1267:do{if((r|0)<2){J=1}else{A=l+(C<<2)|0;x=1;z=2;v=r\
;while(1){do{if((z|0)<(v|0)){s=z|1;B=c[f+2908+(s<<2)>>2]|0;D=b[l+(B<<2)>>1]|0;F\
=c[f+2908+(z<<2)>>2]|0;u=b[l+(F<<2)>>1]|0;if((D&65535)>=(u&65535)){if(D<<16>>16\
!=u<<16>>16){K=z;break}if((d[B+(f+5208)|0]|0)>(d[F+(f+5208)|0]|0)){K=z;break}}K\
=s}else{K=z}}while(0);s=b[A>>1]|0;F=c[f+2908+(K<<2)>>2]|0;B=b[l+(F<<2)>>1]|0;if\
((s&65535)<(B&65535)){J=x;break L1267}if(s<<16>>16==B<<16>>16){if((d[H]|0)<=(d[\
F+(f+5208)|0]|0)){J=x;break L1267}}c[f+2908+(x<<2)>>2]=F;F=K<<1;B=c[n>>2]|0;if(\
(F|0)>(B|0)){J=K;break}else{x=K;z=F;v=B}}}}while(0);c[f+2908+(J<<2)>>2]=C;H=c[g\
>>2]|0;r=(c[q>>2]|0)-1|0;c[q>>2]=r;c[f+2908+(r<<2)>>2]=I;r=(c[q>>2]|0)-1|0;c[q>\
>2]=r;c[f+2908+(r<<2)>>2]=H;r=l+(o<<2)|0;b[r>>1]=(b[l+(H<<2)>>1]|0)+(b[l+(I<<2)\
>>1]|0)&65535;t=a[I+(f+5208)|0]|0;v=a[H+(f+5208)|0]|0;z=o+(f+5208)|0;a[z]=((t&2\
55)<(v&255)?v:t)+1&255;t=o&65535;b[l+(H<<2)+2>>1]=t;b[l+(I<<2)+2>>1]=t;t=o+1|0;\
c[g>>2]=o;H=c[n>>2]|0;L1283:do{if((H|0)<2){L=1}else{v=1;x=2;A=H;while(1){do{if(\
(x|0)<(A|0)){B=x|1;F=c[f+2908+(B<<2)>>2]|0;s=b[l+(F<<2)>>1]|0;u=c[f+2908+(x<<2)\
>>2]|0;D=b[l+(u<<2)>>1]|0;if((s&65535)>=(D&65535)){if(s<<16>>16!=D<<16>>16){M=x\
;break}if((d[F+(f+5208)|0]|0)>(d[u+(f+5208)|0]|0)){M=x;break}}M=B}else{M=x}}whi\
le(0);B=b[r>>1]|0;u=c[f+2908+(M<<2)>>2]|0;F=b[l+(u<<2)>>1]|0;if((B&65535)<(F&65\
535)){L=v;break L1283}if(B<<16>>16==F<<16>>16){if((d[z]|0)<=(d[u+(f+5208)|0]|0)\
){L=v;break L1283}}c[f+2908+(v<<2)>>2]=u;u=M<<1;F=c[n>>2]|0;if((u|0)>(F|0)){L=M\
;break}else{v=M;x=u;A=F}}}}while(0);c[f+2908+(L<<2)>>2]=o;z=c[n>>2]|0;if((z|0)>\
1){o=t;p=z}else{break}}p=c[g>>2]|0;g=(c[q>>2]|0)-1|0;c[q>>2]=g;c[f+2908+(g<<2)>\
>2]=p;p=c[k>>2]|0;k=c[w>>2]|0;w=c[m>>2]|0;m=c[w>>2]|0;g=c[w+4>>2]|0;o=c[w+8>>2]\
|0;n=c[w+16>>2]|0;w=f+2876|0;bm(w|0,0,32);b[p+(c[f+2908+(c[q>>2]<<2)>>2]<<2)+2>\
>1]=0;L=(c[q>>2]|0)+1|0;L1299:do{if((L|0)<573){q=f+5800|0;M=f+5804|0;if((m|0)==\
0){J=0;K=L;while(1){z=c[f+2908+(K<<2)>>2]|0;r=p+(z<<2)+2|0;H=(e[p+(e[r>>1]<<2)+\
2>>1]|0)+1|0;I=(H|0)>(n|0);C=I?n:H;H=(I&1)+J|0;b[r>>1]=C&65535;if((z|0)<=(k|0))\
{r=f+2876+(C<<1)|0;b[r>>1]=(b[r>>1]|0)+1&65535;if((z|0)<(o|0)){N=0}else{N=c[g+(\
z-o<<2)>>2]|0}r=Z(e[p+(z<<2)>>1]|0,N+C|0)|0;c[q>>2]=r+(c[q>>2]|0)}r=K+1|0;if((r\
|0)<573){J=H;K=r}else{O=H;break}}}else{K=0;J=L;while(1){t=c[f+2908+(J<<2)>>2]|0\
;H=p+(t<<2)+2|0;r=(e[p+(e[H>>1]<<2)+2>>1]|0)+1|0;C=(r|0)>(n|0);z=C?n:r;r=(C&1)+\
K|0;b[H>>1]=z&65535;if((t|0)<=(k|0)){H=f+2876+(z<<1)|0;b[H>>1]=(b[H>>1]|0)+1&65\
535;if((t|0)<(o|0)){P=0}else{P=c[g+(t-o<<2)>>2]|0}H=e[p+(t<<2)>>1]|0;C=Z(H,P+z|\
0)|0;c[q>>2]=C+(c[q>>2]|0);C=Z((e[m+(t<<2)+2>>1]|0)+P|0,H)|0;c[M>>2]=C+(c[M>>2]\
|0)}C=J+1|0;if((C|0)<573){K=r;J=C}else{O=r;break}}}if((O|0)==0){break}J=f+2876+\
(n<<1)|0;K=O;do{M=n;while(1){r=M-1|0;Q=f+2876+(r<<1)|0;R=b[Q>>1]|0;if(R<<16>>16\
==0){M=r}else{break}}b[Q>>1]=R-1&65535;r=f+2876+(M<<1)|0;b[r>>1]=(b[r>>1]|0)+2&\
65535;S=(b[J>>1]|0)-1&65535;b[J>>1]=S;K=K-2|0;}while((K|0)>0);if((n|0)==0){brea\
k}else{T=n;U=573;V=S}while(1){K=T&65535;if(V<<16>>16==0){W=U}else{J=V&65535;r=U\
;while(1){C=r;do{C=C-1|0;X=c[f+2908+(C<<2)>>2]|0;}while((X|0)>(k|0));H=p+(X<<2)\
+2|0;t=e[H>>1]|0;if((t|0)!=(T|0)){z=Z(e[p+(X<<2)>>1]|0,T-t|0)|0;c[q>>2]=z+(c[q>\
>2]|0);b[H>>1]=K}H=J-1|0;if((H|0)==0){W=C;break}else{J=H;r=C}}}r=T-1|0;if((r|0)\
==0){break L1299}T=r;U=W;V=b[f+2876+(r<<1)>>1]|0}}}while(0);V=b[w>>1]<<1;b[j+2>\
>1]=V;w=((b[f+2878>>1]|0)+V&65535)<<1;b[j+4>>1]=w;V=(w+(b[f+2880>>1]|0)&65535)<\
<1;b[j+6>>1]=V;w=(V+(b[f+2882>>1]|0)&65535)<<1;b[j+8>>1]=w;V=(w+(b[f+2884>>1]|0\
)&65535)<<1;b[j+10>>1]=V;w=(V+(b[f+2886>>1]|0)&65535)<<1;b[j+12>>1]=w;V=(w+(b[f\
+2888>>1]|0)&65535)<<1;b[j+14>>1]=V;w=(V+(b[f+2890>>1]|0)&65535)<<1;b[j+16>>1]=\
w;V=(w+(b[f+2892>>1]|0)&65535)<<1;b[j+18>>1]=V;w=(V+(b[f+2894>>1]|0)&65535)<<1;\
b[j+20>>1]=w;V=(w+(b[f+2896>>1]|0)&65535)<<1;b[j+22>>1]=V;w=(V+(b[f+2898>>1]|0)\
&65535)<<1;b[j+24>>1]=w;V=(w+(b[f+2900>>1]|0)&65535)<<1;b[j+26>>1]=V;w=(V+(b[f+\
2902>>1]|0)&65535)<<1;b[j+28>>1]=w;b[j+30>>1]=(w+(b[f+2904>>1]|0)&65535)<<1;if(\
(y|0)<0){i=h;return}else{Y=0}do{f=b[l+(Y<<2)+2>>1]|0;w=f&65535;if(f<<16>>16!=0)\
{f=j+(w<<1)|0;V=b[f>>1]|0;b[f>>1]=V+1&65535;f=0;W=w;w=V&65535;while(1){_=f|w&1;\
V=W-1|0;if((V|0)>0){f=_<<1;W=V;w=w>>>1}else{break}}b[l+(Y<<2)>>1]=_&65535}Y=Y+1\
|0;}while((Y|0)<=(y|0));i=h;return}function bc(f,g,h){f=f|0;g=g|0;h=h|0;var i=0\
,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=\
0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0;i=f+5792|0;if((c[i>>2]|0)\
==0){j=c[f+5820>>2]|0;k=b[f+5816>>1]|0}else{l=f+5796|0;m=f+5784|0;n=f+5820|0;o=\
f+5816|0;p=f+20|0;q=f+8|0;r=0;while(1){s=b[(c[l>>2]|0)+(r<<1)>>1]|0;t=s&65535;u\
=r+1|0;v=d[(c[m>>2]|0)+r|0]|0;do{if(s<<16>>16==0){w=e[g+(v<<2)+2>>1]|0;x=c[n>>2\
]|0;y=e[g+(v<<2)>>1]|0;z=e[o>>1]|0|y<<x;A=z&65535;b[o>>1]=A;if((x|0)>(16-w|0)){\
B=c[p>>2]|0;c[p>>2]=B+1;a[(c[q>>2]|0)+B|0]=z&255;z=(e[o>>1]|0)>>>8&255;B=c[p>>2\
]|0;c[p>>2]=B+1;a[(c[q>>2]|0)+B|0]=z;z=c[n>>2]|0;B=y>>>((16-z|0)>>>0)&65535;b[o\
>>1]=B;y=w-16+z|0;c[n>>2]=y;C=y;D=B;break}else{B=x+w|0;c[n>>2]=B;C=B;D=A;break}\
}else{A=d[v+12952|0]|0;B=(A|256)+1|0;w=e[g+(B<<2)+2>>1]|0;x=c[n>>2]|0;y=e[g+(B<\
<2)>>1]|0;B=e[o>>1]|0|y<<x;z=B&65535;b[o>>1]=z;if((x|0)>(16-w|0)){E=c[p>>2]|0;c\
[p>>2]=E+1;a[(c[q>>2]|0)+E|0]=B&255;B=(e[o>>1]|0)>>>8&255;E=c[p>>2]|0;c[p>>2]=E\
+1;a[(c[q>>2]|0)+E|0]=B;B=c[n>>2]|0;E=y>>>((16-B|0)>>>0)&65535;b[o>>1]=E;F=w-16\
+B|0;G=E}else{F=x+w|0;G=z}c[n>>2]=F;z=c[3856+(A<<2)>>2]|0;do{if((A-8|0)>>>0<20)\
{w=v-(c[12528+(A<<2)>>2]|0)&65535;x=w<<F|G&65535;E=x&65535;b[o>>1]=E;if((F|0)>(\
16-z|0)){B=c[p>>2]|0;c[p>>2]=B+1;a[(c[q>>2]|0)+B|0]=x&255;x=(e[o>>1]|0)>>>8&255\
;B=c[p>>2]|0;c[p>>2]=B+1;a[(c[q>>2]|0)+B|0]=x;x=c[n>>2]|0;B=w>>>((16-x|0)>>>0)&\
65535;b[o>>1]=B;w=z-16+x|0;c[n>>2]=w;H=w;I=B;break}else{B=F+z|0;c[n>>2]=B;H=B;I\
=E;break}}else{H=F;I=G}}while(0);z=t-1|0;if(z>>>0<256){J=z}else{J=(z>>>7)+256|0\
}A=d[J+13680|0]|0;E=e[h+(A<<2)+2>>1]|0;B=e[h+(A<<2)>>1]|0;w=I&65535|B<<H;x=w&65\
535;b[o>>1]=x;if((H|0)>(16-E|0)){y=c[p>>2]|0;c[p>>2]=y+1;a[(c[q>>2]|0)+y|0]=w&2\
55;w=(e[o>>1]|0)>>>8&255;y=c[p>>2]|0;c[p>>2]=y+1;a[(c[q>>2]|0)+y|0]=w;w=c[n>>2]\
|0;y=B>>>((16-w|0)>>>0)&65535;b[o>>1]=y;K=E-16+w|0;L=y}else{K=H+E|0;L=x}c[n>>2]\
=K;x=c[3976+(A<<2)>>2]|0;if((A-4|0)>>>0>=26){C=K;D=L;break}E=z-(c[12648+(A<<2)>\
>2]|0)&65535;A=E<<K|L&65535;z=A&65535;b[o>>1]=z;if((K|0)>(16-x|0)){y=c[p>>2]|0;\
c[p>>2]=y+1;a[(c[q>>2]|0)+y|0]=A&255;A=(e[o>>1]|0)>>>8&255;y=c[p>>2]|0;c[p>>2]=\
y+1;a[(c[q>>2]|0)+y|0]=A;A=c[n>>2]|0;y=E>>>((16-A|0)>>>0)&65535;b[o>>1]=y;E=x-1\
6+A|0;c[n>>2]=E;C=E;D=y;break}else{y=K+x|0;c[n>>2]=y;C=y;D=z;break}}}while(0);i\
f(u>>>0<(c[i>>2]|0)>>>0){r=u}else{j=C;k=D;break}}}D=g+1026|0;C=e[D>>1]|0;r=f+58\
20|0;i=e[g+1024>>1]|0;g=f+5816|0;n=k&65535|i<<j;b[g>>1]=n&65535;if((j|0)>(16-C|\
0)){k=f+20|0;K=c[k>>2]|0;c[k>>2]=K+1;o=f+8|0;a[(c[o>>2]|0)+K|0]=n&255;n=(e[g>>1\
]|0)>>>8&255;K=c[k>>2]|0;c[k>>2]=K+1;a[(c[o>>2]|0)+K|0]=n;n=c[r>>2]|0;b[g>>1]=i\
>>>((16-n|0)>>>0)&65535;M=C-16+n|0;c[r>>2]=M;N=b[D>>1]|0;O=N&65535;P=f+5812|0;c\
[P>>2]=O;return}else{M=j+C|0;c[r>>2]=M;N=b[D>>1]|0;O=N&65535;P=f+5812|0;c[P>>2]\
=O;return}}function bd(d,f,g){d=d|0;f=f|0;g=g|0;var h=0,i=0,j=0,k=0,l=0,m=0,n=0\
,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=\
0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0;h=b[f+2>>\
1]|0;i=h<<16>>16==0;j=d+2754|0;k=d+5820|0;l=d+2752|0;m=d+5816|0;n=d+20|0;o=d+8|\
0;p=d+2758|0;q=d+2756|0;r=d+2750|0;s=d+2748|0;t=0;u=-1;v=h&65535;h=i?138:7;w=i?\
3:4;L1393:while(1){i=t;x=0;while(1){if((i|0)>(g|0)){break L1393}y=i+1|0;z=b[f+(\
y<<2)+2>>1]|0;A=z&65535;B=x+1|0;C=(v|0)==(A|0);if((B|0)<(h|0)&C){i=y;x=B}else{b\
reak}}do{if((B|0)<(w|0)){i=d+2684+(v<<2)+2|0;D=d+2684+(v<<2)|0;E=B;F=c[k>>2]|0;\
G=b[m>>1]|0;while(1){H=e[i>>1]|0;I=e[D>>1]|0;J=G&65535|I<<F;K=J&65535;b[m>>1]=K\
;if((F|0)>(16-H|0)){L=c[n>>2]|0;c[n>>2]=L+1;a[(c[o>>2]|0)+L|0]=J&255;J=(e[m>>1]\
|0)>>>8&255;L=c[n>>2]|0;c[n>>2]=L+1;a[(c[o>>2]|0)+L|0]=J;J=c[k>>2]|0;L=I>>>((16\
-J|0)>>>0)&65535;b[m>>1]=L;M=H-16+J|0;N=L}else{M=F+H|0;N=K}c[k>>2]=M;K=E-1|0;if\
((K|0)==0){break}else{E=K;F=M;G=N}}}else{if((v|0)!=0){if((v|0)==(u|0)){O=B;P=c[\
k>>2]|0;Q=b[m>>1]|0}else{G=e[d+2684+(v<<2)+2>>1]|0;F=c[k>>2]|0;E=e[d+2684+(v<<2\
)>>1]|0;D=e[m>>1]|0|E<<F;i=D&65535;b[m>>1]=i;if((F|0)>(16-G|0)){K=c[n>>2]|0;c[n\
>>2]=K+1;a[(c[o>>2]|0)+K|0]=D&255;D=(e[m>>1]|0)>>>8&255;K=c[n>>2]|0;c[n>>2]=K+1\
;a[(c[o>>2]|0)+K|0]=D;D=c[k>>2]|0;K=E>>>((16-D|0)>>>0)&65535;b[m>>1]=K;R=G-16+D\
|0;S=K}else{R=F+G|0;S=i}c[k>>2]=R;O=x;P=R;Q=S}i=e[r>>1]|0;G=e[s>>1]|0;F=Q&65535\
|G<<P;K=F&65535;b[m>>1]=K;if((P|0)>(16-i|0)){D=c[n>>2]|0;c[n>>2]=D+1;a[(c[o>>2]\
|0)+D|0]=F&255;F=(e[m>>1]|0)>>>8&255;D=c[n>>2]|0;c[n>>2]=D+1;a[(c[o>>2]|0)+D|0]\
=F;F=c[k>>2]|0;D=G>>>((16-F|0)>>>0)&65535;b[m>>1]=D;T=i-16+F|0;U=D}else{T=P+i|0\
;U=K}c[k>>2]=T;K=O+65533&65535;i=U&65535|K<<T;b[m>>1]=i&65535;if((T|0)>14){D=c[\
n>>2]|0;c[n>>2]=D+1;a[(c[o>>2]|0)+D|0]=i&255;i=(e[m>>1]|0)>>>8&255;D=c[n>>2]|0;\
c[n>>2]=D+1;a[(c[o>>2]|0)+D|0]=i;i=c[k>>2]|0;b[m>>1]=K>>>((16-i|0)>>>0)&65535;c\
[k>>2]=i-14;break}else{c[k>>2]=T+2;break}}if((B|0)<11){i=e[j>>1]|0;K=c[k>>2]|0;\
D=e[l>>1]|0;F=e[m>>1]|0|D<<K;G=F&65535;b[m>>1]=G;if((K|0)>(16-i|0)){E=c[n>>2]|0\
;c[n>>2]=E+1;a[(c[o>>2]|0)+E|0]=F&255;F=(e[m>>1]|0)>>>8&255;E=c[n>>2]|0;c[n>>2]\
=E+1;a[(c[o>>2]|0)+E|0]=F;F=c[k>>2]|0;E=D>>>((16-F|0)>>>0)&65535;b[m>>1]=E;V=i-\
16+F|0;W=E}else{V=K+i|0;W=G}c[k>>2]=V;G=x+65534&65535;i=W&65535|G<<V;b[m>>1]=i&\
65535;if((V|0)>13){K=c[n>>2]|0;c[n>>2]=K+1;a[(c[o>>2]|0)+K|0]=i&255;i=(e[m>>1]|\
0)>>>8&255;K=c[n>>2]|0;c[n>>2]=K+1;a[(c[o>>2]|0)+K|0]=i;i=c[k>>2]|0;b[m>>1]=G>>\
>((16-i|0)>>>0)&65535;c[k>>2]=i-13;break}else{c[k>>2]=V+3;break}}else{i=e[p>>1]\
|0;G=c[k>>2]|0;K=e[q>>1]|0;E=e[m>>1]|0|K<<G;F=E&65535;b[m>>1]=F;if((G|0)>(16-i|\
0)){D=c[n>>2]|0;c[n>>2]=D+1;a[(c[o>>2]|0)+D|0]=E&255;E=(e[m>>1]|0)>>>8&255;D=c[\
n>>2]|0;c[n>>2]=D+1;a[(c[o>>2]|0)+D|0]=E;E=c[k>>2]|0;D=K>>>((16-E|0)>>>0)&65535\
;b[m>>1]=D;X=i-16+E|0;Y=D}else{X=G+i|0;Y=F}c[k>>2]=X;F=x+65526&65535;i=Y&65535|\
F<<X;b[m>>1]=i&65535;if((X|0)>9){G=c[n>>2]|0;c[n>>2]=G+1;a[(c[o>>2]|0)+G|0]=i&2\
55;i=(e[m>>1]|0)>>>8&255;G=c[n>>2]|0;c[n>>2]=G+1;a[(c[o>>2]|0)+G|0]=i;i=c[k>>2]\
|0;b[m>>1]=F>>>((16-i|0)>>>0)&65535;c[k>>2]=i-9;break}else{c[k>>2]=X+7;break}}}\
}while(0);if(z<<16>>16==0){t=y;u=v;v=A;h=138;w=3;continue}t=y;u=v;v=A;h=C?6:7;w\
=C?3:4}return}function be(a,c,d){a=a|0;c=c|0;d=d|0;var f=0,g=0,h=0,i=0,j=0,k=0,\
l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0;f=b[c+2>>1]|0;g=f<<16>>16==0;b[c+(d+1<<2)+2>>1]\
=-1;h=a+2752|0;i=a+2756|0;j=a+2748|0;k=g?3:4;l=g?138:7;g=f&65535;f=0;m=-1;L1447\
:while(1){n=0;o=f;do{if((o|0)>(d|0)){break L1447}o=o+1|0;p=b[c+(o<<2)+2>>1]|0;q\
=p&65535;n=n+1|0;r=(g|0)==(q|0);}while((n|0)<(l|0)&r);do{if((n|0)<(k|0)){s=a+26\
84+(g<<2)|0;b[s>>1]=(e[s>>1]|0)+n&65535}else{if((g|0)==0){if((n|0)<11){b[h>>1]=\
(b[h>>1]|0)+1&65535;break}else{b[i>>1]=(b[i>>1]|0)+1&65535;break}}else{if((g|0)\
!=(m|0)){s=a+2684+(g<<2)|0;b[s>>1]=(b[s>>1]|0)+1&65535}b[j>>1]=(b[j>>1]|0)+1&65\
535;break}}}while(0);if(p<<16>>16==0){k=3;l=138;m=g;g=q;f=o;continue}k=r?3:4;l=\
r?6:7;m=g;g=q;f=o}return}function bf(a,b,c){a=a|0;b=b|0;c=c|0;return bk(Z(c,b)|\
0)|0}function bg(a,b){a=a|0;b=b|0;var d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n\
=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,\
H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0;if((b|0)==0){return}a=b-8|0;d=a;e=c[330\
6]|0;if(a>>>0<e>>>0){am()}f=c[b-4>>2]|0;g=f&3;if((g|0)==1){am()}h=f&-8;i=b+(h-8\
)|0;j=i;L1479:do{if((f&1|0)==0){k=c[a>>2]|0;if((g|0)==0){return}l=-8-k|0;m=b+l|\
0;n=m;o=k+h|0;if(m>>>0<e>>>0){am()}if((n|0)==(c[3307]|0)){p=b+(h-4)|0;if((c[p>>\
2]&3|0)!=3){q=n;r=o;break}c[3304]=o;c[p>>2]=c[p>>2]&-2;c[b+(l+4)>>2]=o|1;c[i>>2\
]=o;return}p=k>>>3;if(k>>>0<256){k=c[b+(l+8)>>2]|0;s=c[b+(l+12)>>2]|0;t=13248+(\
p<<1<<2)|0;do{if((k|0)!=(t|0)){if(k>>>0<e>>>0){am()}if((c[k+12>>2]|0)==(n|0)){b\
reak}am()}}while(0);if((s|0)==(k|0)){c[3302]=c[3302]&(1<<p^-1);q=n;r=o;break}do\
{if((s|0)==(t|0)){u=s+8|0}else{if(s>>>0<e>>>0){am()}v=s+8|0;if((c[v>>2]|0)==(n|\
0)){u=v;break}am()}}while(0);c[k+12>>2]=s;c[u>>2]=k;q=n;r=o;break}t=m;p=c[b+(l+\
24)>>2]|0;v=c[b+(l+12)>>2]|0;do{if((v|0)==(t|0)){w=b+(l+20)|0;x=c[w>>2]|0;if((x\
|0)==0){y=b+(l+16)|0;z=c[y>>2]|0;if((z|0)==0){A=0;break}else{B=z;C=y}}else{B=x;\
C=w}while(1){w=B+20|0;x=c[w>>2]|0;if((x|0)!=0){B=x;C=w;continue}w=B+16|0;x=c[w>\
>2]|0;if((x|0)==0){break}else{B=x;C=w}}if(C>>>0<e>>>0){am()}else{c[C>>2]=0;A=B;\
break}}else{w=c[b+(l+8)>>2]|0;if(w>>>0<e>>>0){am()}x=w+12|0;if((c[x>>2]|0)!=(t|\
0)){am()}y=v+8|0;if((c[y>>2]|0)==(t|0)){c[x>>2]=v;c[y>>2]=w;A=v;break}else{am()\
}}}while(0);if((p|0)==0){q=n;r=o;break}v=b+(l+28)|0;m=13512+(c[v>>2]<<2)|0;do{i\
f((t|0)==(c[m>>2]|0)){c[m>>2]=A;if((A|0)!=0){break}c[3303]=c[3303]&(1<<c[v>>2]^\
-1);q=n;r=o;break L1479}else{if(p>>>0<(c[3306]|0)>>>0){am()}k=p+16|0;if((c[k>>2\
]|0)==(t|0)){c[k>>2]=A}else{c[p+20>>2]=A}if((A|0)==0){q=n;r=o;break L1479}}}whi\
le(0);if(A>>>0<(c[3306]|0)>>>0){am()}c[A+24>>2]=p;t=c[b+(l+16)>>2]|0;do{if((t|0\
)!=0){if(t>>>0<(c[3306]|0)>>>0){am()}else{c[A+16>>2]=t;c[t+24>>2]=A;break}}}whi\
le(0);t=c[b+(l+20)>>2]|0;if((t|0)==0){q=n;r=o;break}if(t>>>0<(c[3306]|0)>>>0){a\
m()}else{c[A+20>>2]=t;c[t+24>>2]=A;q=n;r=o;break}}else{q=d;r=h}}while(0);d=q;if\
(d>>>0>=i>>>0){am()}A=b+(h-4)|0;e=c[A>>2]|0;if((e&1|0)==0){am()}do{if((e&2|0)==\
0){if((j|0)==(c[3308]|0)){B=(c[3305]|0)+r|0;c[3305]=B;c[3308]=q;c[q+4>>2]=B|1;i\
f((q|0)==(c[3307]|0)){c[3307]=0;c[3304]=0}if(B>>>0<=(c[3309]|0)>>>0){return}do{\
if((c[340]|0)==0){B=al(8)|0;if((B-1&B|0)==0){c[342]=B;c[341]=B;c[343]=-1;c[344]\
=2097152;c[345]=0;c[3413]=0;c[340]=(aF(0)|0)&-16^1431655768;break}else{am()}}}w\
hile(0);o=c[3308]|0;if((o|0)==0){return}n=c[3305]|0;do{if(n>>>0>40){l=c[342]|0;\
B=Z((((n-41+l|0)>>>0)/(l>>>0)>>>0)-1|0,l)|0;C=o;u=13656;while(1){g=c[u>>2]|0;if\
(g>>>0<=C>>>0){if((g+(c[u+4>>2]|0)|0)>>>0>C>>>0){D=u;break}}g=c[u+8>>2]|0;if((g\
|0)==0){D=0;break}else{u=g}}if((c[D+12>>2]&8|0)!=0){break}u=aB(0)|0;C=D+4|0;if(\
(u|0)!=((c[D>>2]|0)+(c[C>>2]|0)|0)){break}g=aB(-(B>>>0>2147483646?-2147483648-l\
|0:B)|0)|0;a=aB(0)|0;if(!((g|0)!=-1&a>>>0<u>>>0)){break}g=u-a|0;if((u|0)==(a|0)\
){break}c[C>>2]=(c[C>>2]|0)-g;c[3410]=(c[3410]|0)-g;C=c[3308]|0;a=(c[3305]|0)-g\
|0;g=C;u=C+8|0;if((u&7|0)==0){E=0}else{E=-u&7}u=a-E|0;c[3308]=g+E;c[3305]=u;c[g\
+(E+4)>>2]=u|1;c[g+(a+4)>>2]=40;c[3309]=c[344];return}}while(0);if((c[3305]|0)>\
>>0<=(c[3309]|0)>>>0){return}c[3309]=-1;return}if((j|0)==(c[3307]|0)){o=(c[3304\
]|0)+r|0;c[3304]=o;c[3307]=q;c[q+4>>2]=o|1;c[d+o>>2]=o;return}o=(e&-8)+r|0;n=e>\
>>3;L1613:do{if(e>>>0<256){a=c[b+h>>2]|0;g=c[b+(h|4)>>2]|0;u=13248+(n<<1<<2)|0;\
do{if((a|0)!=(u|0)){if(a>>>0<(c[3306]|0)>>>0){am()}if((c[a+12>>2]|0)==(j|0)){br\
eak}am()}}while(0);if((g|0)==(a|0)){c[3302]=c[3302]&(1<<n^-1);break}do{if((g|0)\
==(u|0)){F=g+8|0}else{if(g>>>0<(c[3306]|0)>>>0){am()}B=g+8|0;if((c[B>>2]|0)==(j\
|0)){F=B;break}am()}}while(0);c[a+12>>2]=g;c[F>>2]=a}else{u=i;B=c[b+(h+16)>>2]|\
0;l=c[b+(h|4)>>2]|0;do{if((l|0)==(u|0)){C=b+(h+12)|0;f=c[C>>2]|0;if((f|0)==0){t\
=b+(h+8)|0;p=c[t>>2]|0;if((p|0)==0){G=0;break}else{H=p;I=t}}else{H=f;I=C}while(\
1){C=H+20|0;f=c[C>>2]|0;if((f|0)!=0){H=f;I=C;continue}C=H+16|0;f=c[C>>2]|0;if((\
f|0)==0){break}else{H=f;I=C}}if(I>>>0<(c[3306]|0)>>>0){am()}else{c[I>>2]=0;G=H;\
break}}else{C=c[b+h>>2]|0;if(C>>>0<(c[3306]|0)>>>0){am()}f=C+12|0;if((c[f>>2]|0\
)!=(u|0)){am()}t=l+8|0;if((c[t>>2]|0)==(u|0)){c[f>>2]=l;c[t>>2]=C;G=l;break}els\
e{am()}}}while(0);if((B|0)==0){break}l=b+(h+20)|0;a=13512+(c[l>>2]<<2)|0;do{if(\
(u|0)==(c[a>>2]|0)){c[a>>2]=G;if((G|0)!=0){break}c[3303]=c[3303]&(1<<c[l>>2]^-1\
);break L1613}else{if(B>>>0<(c[3306]|0)>>>0){am()}g=B+16|0;if((c[g>>2]|0)==(u|0\
)){c[g>>2]=G}else{c[B+20>>2]=G}if((G|0)==0){break L1613}}}while(0);if(G>>>0<(c[\
3306]|0)>>>0){am()}c[G+24>>2]=B;u=c[b+(h+8)>>2]|0;do{if((u|0)!=0){if(u>>>0<(c[3\
306]|0)>>>0){am()}else{c[G+16>>2]=u;c[u+24>>2]=G;break}}}while(0);u=c[b+(h+12)>\
>2]|0;if((u|0)==0){break}if(u>>>0<(c[3306]|0)>>>0){am()}else{c[G+20>>2]=u;c[u+2\
4>>2]=G;break}}}while(0);c[q+4>>2]=o|1;c[d+o>>2]=o;if((q|0)!=(c[3307]|0)){J=o;b\
reak}c[3304]=o;return}else{c[A>>2]=e&-2;c[q+4>>2]=r|1;c[d+r>>2]=r;J=r}}while(0)\
;r=J>>>3;if(J>>>0<256){d=r<<1;e=13248+(d<<2)|0;A=c[3302]|0;G=1<<r;do{if((A&G|0)\
==0){c[3302]=A|G;K=e;L=13248+(d+2<<2)|0}else{r=13248+(d+2<<2)|0;h=c[r>>2]|0;if(\
h>>>0>=(c[3306]|0)>>>0){K=h;L=r;break}am()}}while(0);c[L>>2]=q;c[K+12>>2]=q;c[q\
+8>>2]=K;c[q+12>>2]=e;return}e=q;K=J>>>8;do{if((K|0)==0){M=0}else{if(J>>>0>1677\
7215){M=31;break}L=(K+1048320|0)>>>16&8;d=K<<L;G=(d+520192|0)>>>16&4;A=d<<G;d=(\
A+245760|0)>>>16&2;r=14-(G|L|d)+(A<<d>>>15)|0;M=J>>>((r+7|0)>>>0)&1|r<<1}}while\
(0);K=13512+(M<<2)|0;c[q+28>>2]=M;c[q+20>>2]=0;c[q+16>>2]=0;r=c[3303]|0;d=1<<M;\
do{if((r&d|0)==0){c[3303]=r|d;c[K>>2]=e;c[q+24>>2]=K;c[q+12>>2]=q;c[q+8>>2]=q}e\
lse{if((M|0)==31){N=0}else{N=25-(M>>>1)|0}A=J<<N;L=c[K>>2]|0;while(1){if((c[L+4\
>>2]&-8|0)==(J|0)){break}O=L+16+(A>>>31<<2)|0;G=c[O>>2]|0;if((G|0)==0){P=1200;b\
reak}else{A=A<<1;L=G}}if((P|0)==1200){if(O>>>0<(c[3306]|0)>>>0){am()}else{c[O>>\
2]=e;c[q+24>>2]=L;c[q+12>>2]=q;c[q+8>>2]=q;break}}A=L+8|0;o=c[A>>2]|0;G=c[3306]\
|0;if(L>>>0<G>>>0){am()}if(o>>>0<G>>>0){am()}else{c[o+12>>2]=e;c[A>>2]=e;c[q+8>\
>2]=o;c[q+12>>2]=L;c[q+24>>2]=0;break}}}while(0);q=(c[3310]|0)-1|0;c[3310]=q;if\
((q|0)==0){Q=13664}else{return}while(1){q=c[Q>>2]|0;if((q|0)==0){break}else{Q=q\
+8|0}}c[3310]=-1;return}function bh(a,b,c){a=a|0;b=b|0;c=c|0;var e=0,f=0,g=0,h=\
0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B\
=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,\
V=0;e=a>>>16;f=a&65535;if((c|0)==1){a=(d[b]|0)+f|0;g=a>>>0>65520?a-65521|0:a;a=\
g+e|0;h=(a>>>0>65520?a+15|0:a)<<16|g;return h|0}if((b|0)==0){h=1;return h|0}if(\
c>>>0<16){if((c|0)==0){i=f;j=e}else{g=f;a=b;k=c;l=e;while(1){m=k-1|0;n=(d[a]|0)\
+g|0;o=n+l|0;if((m|0)==0){i=n;j=o;break}else{g=n;a=a+1|0;k=m;l=o}}}h=(j>>>0)%65\
521>>>0<<16|(i>>>0>65520?i-65521|0:i);return h|0}do{if(c>>>0>5551){i=f;j=b;l=c;\
k=e;do{l=l-5552|0;a=347;g=k;o=j;m=i;while(1){n=(d[o]|0)+m|0;p=n+(d[o+1|0]|0)|0;\
q=p+(d[o+2|0]|0)|0;r=q+(d[o+3|0]|0)|0;s=r+(d[o+4|0]|0)|0;t=s+(d[o+5|0]|0)|0;u=t\
+(d[o+6|0]|0)|0;v=u+(d[o+7|0]|0)|0;w=v+(d[o+8|0]|0)|0;x=w+(d[o+9|0]|0)|0;y=x+(d\
[o+10|0]|0)|0;z=y+(d[o+11|0]|0)|0;A=z+(d[o+12|0]|0)|0;B=A+(d[o+13|0]|0)|0;C=B+(\
d[o+14|0]|0)|0;D=C+(d[o+15|0]|0)|0;E=n+g+p+q+r+s+t+u+v+w+x+y+z+A+B+C+D|0;C=a-1|\
0;if((C|0)==0){break}else{a=C;g=E;o=o+16|0;m=D}}j=j+5552|0;i=(D>>>0)%65521>>>0;\
k=(E>>>0)%65521>>>0;}while(l>>>0>5551);if((l|0)==0){F=k;G=i;break}if(l>>>0>15){\
H=i;I=j;J=l;K=k;L=1260}else{M=i;N=j;O=l;P=k;L=1261}}else{H=f;I=b;J=c;K=e;L=1260\
}}while(0);if((L|0)==1260){while(1){L=0;Q=J-16|0;e=(d[I]|0)+H|0;c=e+(d[I+1|0]|0\
)|0;b=c+(d[I+2|0]|0)|0;f=b+(d[I+3|0]|0)|0;E=f+(d[I+4|0]|0)|0;D=E+(d[I+5|0]|0)|0\
;m=D+(d[I+6|0]|0)|0;o=m+(d[I+7|0]|0)|0;g=o+(d[I+8|0]|0)|0;a=g+(d[I+9|0]|0)|0;C=\
a+(d[I+10|0]|0)|0;B=C+(d[I+11|0]|0)|0;A=B+(d[I+12|0]|0)|0;z=A+(d[I+13|0]|0)|0;y\
=z+(d[I+14|0]|0)|0;R=y+(d[I+15|0]|0)|0;S=e+K+c+b+f+E+D+m+o+g+a+C+B+A+z+y+R|0;T=\
I+16|0;if(Q>>>0>15){H=R;I=T;J=Q;K=S;L=1260}else{break}}if((Q|0)==0){U=R;V=S;L=1\
262}else{M=R;N=T;O=Q;P=S;L=1261}}if((L|0)==1261){while(1){L=0;S=O-1|0;Q=(d[N]|0\
)+M|0;T=Q+P|0;if((S|0)==0){U=Q;V=T;L=1262;break}else{M=Q;N=N+1|0;O=S;P=T;L=1261\
}}}if((L|0)==1262){F=(V>>>0)%65521>>>0;G=(U>>>0)%65521>>>0}h=F<<16|G;return h|0\
}function bi(a,b,e){a=a|0;b=b|0;e=e|0;var f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o\
=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0;if((b|0)==0){f=0;return f|0}g=a^-1;L1767:do{\
if((e|0)==0){h=g}else{a=b;i=e;j=g;while(1){if((a&3|0)==0){break}k=c[4192+(((d[a\
]|0)^j&255)<<2)>>2]^j>>>8;l=i-1|0;if((l|0)==0){h=k;break L1767}else{a=a+1|0;i=l\
;j=k}}k=a;if(i>>>0>31){l=i;m=j;n=k;while(1){o=c[n>>2]^m;p=c[6240+((o>>>8&255)<<\
2)>>2]^c[7264+((o&255)<<2)>>2]^c[5216+((o>>>16&255)<<2)>>2]^c[4192+(o>>>24<<2)>\
>2]^c[n+4>>2];o=c[6240+((p>>>8&255)<<2)>>2]^c[7264+((p&255)<<2)>>2]^c[5216+((p>\
>>16&255)<<2)>>2]^c[4192+(p>>>24<<2)>>2]^c[n+8>>2];p=c[6240+((o>>>8&255)<<2)>>2\
]^c[7264+((o&255)<<2)>>2]^c[5216+((o>>>16&255)<<2)>>2]^c[4192+(o>>>24<<2)>>2]^c\
[n+12>>2];o=c[6240+((p>>>8&255)<<2)>>2]^c[7264+((p&255)<<2)>>2]^c[5216+((p>>>16\
&255)<<2)>>2]^c[4192+(p>>>24<<2)>>2]^c[n+16>>2];p=c[6240+((o>>>8&255)<<2)>>2]^c\
[7264+((o&255)<<2)>>2]^c[5216+((o>>>16&255)<<2)>>2]^c[4192+(o>>>24<<2)>>2]^c[n+\
20>>2];o=c[6240+((p>>>8&255)<<2)>>2]^c[7264+((p&255)<<2)>>2]^c[5216+((p>>>16&25\
5)<<2)>>2]^c[4192+(p>>>24<<2)>>2]^c[n+24>>2];p=n+32|0;q=c[6240+((o>>>8&255)<<2)\
>>2]^c[7264+((o&255)<<2)>>2]^c[5216+((o>>>16&255)<<2)>>2]^c[4192+(o>>>24<<2)>>2\
]^c[n+28>>2];o=c[6240+((q>>>8&255)<<2)>>2]^c[7264+((q&255)<<2)>>2]^c[5216+((q>>\
>16&255)<<2)>>2]^c[4192+(q>>>24<<2)>>2];q=l-32|0;if(q>>>0>31){l=q;m=o;n=p}else{\
r=q;s=o;t=p;break}}}else{r=i;s=j;t=k}if(r>>>0>3){n=r;m=s;l=t;while(1){a=l+4|0;p\
=c[l>>2]^m;o=c[6240+((p>>>8&255)<<2)>>2]^c[7264+((p&255)<<2)>>2]^c[5216+((p>>>1\
6&255)<<2)>>2]^c[4192+(p>>>24<<2)>>2];p=n-4|0;if(p>>>0>3){n=p;m=o;l=a}else{u=p;\
v=o;w=a;break}}}else{u=r;v=s;w=t}if((u|0)==0){h=v;break}l=v;m=u;n=w;while(1){k=\
c[4192+(((d[n]|0)^l&255)<<2)>>2]^l>>>8;j=m-1|0;if((j|0)==0){h=k;break}else{l=k;\
m=j;n=n+1|0}}}}while(0);f=h^-1;return f|0}function bj(d,f,g,h,j,k){d=d|0;f=f|0;\
g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0\
,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=\
0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0;l=i;i=i+32|0;m=l|0;n=i;i=i+32|0;bm(m|0,0,\
32);o=(g|0)==0;if(!o){p=0;do{q=m+(e[f+(p<<1)>>1]<<1)|0;b[q>>1]=(b[q>>1]|0)+1&65\
535;p=p+1|0;}while(p>>>0<g>>>0)}p=c[j>>2]|0;q=15;while(1){if((q|0)==0){r=1290;b\
reak}if((b[m+(q<<1)>>1]|0)==0){q=q-1|0}else{break}}if((r|0)==1290){s=c[h>>2]|0;\
c[h>>2]=s+4;a[s|0]=64;a[s+1|0]=1;b[s+2>>1]=0;s=c[h>>2]|0;c[h>>2]=s+4;a[s|0]=64;\
a[s+1|0]=1;b[s+2>>1]=0;c[j>>2]=1;t=0;i=l;return t|0}s=p>>>0>q>>>0?q:p;p=1;while\
(1){if(p>>>0>=q>>>0){break}if((b[m+(p<<1)>>1]|0)==0){p=p+1|0}else{break}}u=s>>>\
0<p>>>0?p:s;s=1;v=1;while(1){if(v>>>0>=16){break}w=(s<<1)-(e[m+(v<<1)>>1]|0)|0;\
if((w|0)<0){t=-1;r=1339;break}else{s=w;v=v+1|0}}if((r|0)==1339){i=l;return t|0}\
do{if((s|0)>0){if((d|0)!=0&(q|0)==1){break}else{t=-1}i=l;return t|0}}while(0);b\
[n+2>>1]=0;s=b[m+2>>1]|0;b[n+4>>1]=s;v=(b[m+4>>1]|0)+s&65535;b[n+6>>1]=v;s=(b[m\
+6>>1]|0)+v&65535;b[n+8>>1]=s;v=(b[m+8>>1]|0)+s&65535;b[n+10>>1]=v;s=(b[m+10>>1\
]|0)+v&65535;b[n+12>>1]=s;v=(b[m+12>>1]|0)+s&65535;b[n+14>>1]=v;s=(b[m+14>>1]|0\
)+v&65535;b[n+16>>1]=s;v=(b[m+16>>1]|0)+s&65535;b[n+18>>1]=v;s=(b[m+18>>1]|0)+v\
&65535;b[n+20>>1]=s;v=(b[m+20>>1]|0)+s&65535;b[n+22>>1]=v;s=(b[m+22>>1]|0)+v&65\
535;b[n+24>>1]=s;v=(b[m+24>>1]|0)+s&65535;b[n+26>>1]=v;s=(b[m+26>>1]|0)+v&65535\
;b[n+28>>1]=s;b[n+30>>1]=(b[m+28>>1]|0)+s&65535;if(!o){o=0;do{s=b[f+(o<<1)>>1]|\
0;if(s<<16>>16!=0){v=n+((s&65535)<<1)|0;s=b[v>>1]|0;b[v>>1]=s+1&65535;b[k+((s&6\
5535)<<1)>>1]=o&65535}o=o+1|0;}while(o>>>0<g>>>0)}do{if((d|0)==0){x=0;y=1<<u;z=\
19;A=k;B=k;C=0}else if((d|0)==1){g=1<<u;if(g>>>0>851){t=1}else{x=1;y=g;z=256;A=\
870;B=934;C=0;break}i=l;return t|0}else{g=1<<u;o=(d|0)==2;if(o&g>>>0>591){t=1}e\
lse{x=0;y=g;z=-1;A=1512;B=1576;C=o;break}i=l;return t|0}}while(0);d=y-1|0;o=u&2\
55;g=c[h>>2]|0;n=-1;s=0;v=y;y=0;w=u;D=0;E=p;L1825:while(1){p=1<<w;F=s;G=D;H=E;w\
hile(1){I=H-y|0;J=I&255;K=b[k+(G<<1)>>1]|0;L=K&65535;do{if((L|0)<(z|0)){M=0;N=K\
}else{if((L|0)<=(z|0)){M=96;N=0;break}M=b[A+(L<<1)>>1]&255;N=b[B+(L<<1)>>1]|0}}\
while(0);L=1<<I;K=F>>>(y>>>0);O=p;while(1){P=O-L|0;Q=P+K|0;a[g+(Q<<2)|0]=M;a[g+\
(Q<<2)+1|0]=J;b[g+(Q<<2)+2>>1]=N;if((O|0)==(L|0)){break}else{O=P}}O=1<<H-1;whil\
e(1){if((O&F|0)==0){break}else{O=O>>>1}}if((O|0)==0){R=0}else{R=(O-1&F)+O|0}S=G\
+1|0;L=m+(H<<1)|0;K=(b[L>>1]|0)-1&65535;b[L>>1]=K;if(K<<16>>16==0){if((H|0)==(q\
|0)){break L1825}T=e[f+(e[k+(S<<1)>>1]<<1)>>1]|0}else{T=H}if(T>>>0<=u>>>0){F=R;\
G=S;H=T;continue}U=R&d;if((U|0)==(n|0)){F=R;G=S;H=T}else{break}}H=(y|0)==0?u:y;\
G=g+(p<<2)|0;F=T-H|0;L1848:do{if(T>>>0<q>>>0){K=F;L=1<<F;I=T;while(1){P=L-(e[m+\
(I<<1)>>1]|0)|0;if((P|0)<1){V=K;break L1848}Q=K+1|0;W=Q+H|0;if(W>>>0<q>>>0){K=Q\
;L=P<<1;I=W}else{V=Q;break}}}else{V=F}}while(0);F=(1<<V)+v|0;if(x&F>>>0>851|C&F\
>>>0>591){t=1;r=1343;break}a[(c[h>>2]|0)+(U<<2)|0]=V&255;a[(c[h>>2]|0)+(U<<2)+1\
|0]=o;p=c[h>>2]|0;b[p+(U<<2)+2>>1]=(G-p|0)>>>2&65535;g=G;n=U;s=R;v=F;y=H;w=V;D=\
S;E=T}if((r|0)==1343){i=l;return t|0}L1858:do{if((R|0)!=0){r=q;T=y;E=R;S=J;D=g;\
while(1){do{if((T|0)==0){X=D;Y=S;Z=0;_=r}else{if((E&d|0)==(n|0)){X=D;Y=S;Z=T;_=\
r;break}X=c[h>>2]|0;Y=o;Z=0;_=u}}while(0);V=E>>>(Z>>>0);a[X+(V<<2)|0]=64;a[X+(V\
<<2)+1|0]=Y;b[X+(V<<2)+2>>1]=0;V=1<<_-1;while(1){if((V&E|0)==0){break}else{V=V>\
>>1}}if((V|0)==0){break L1858}w=(V-1&E)+V|0;if((w|0)==0){break}else{r=_;T=Z;E=w\
;S=Y;D=X}}}}while(0);c[h>>2]=(c[h>>2]|0)+(v<<2);c[j>>2]=u;t=0;i=l;return t|0}fu\
nction bk(a){a=a|0;var b=0,d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,\
q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0\
,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,a\
b=0,ac=0,ad=0,ae=0,af=0,ag=0,ah=0,ai=0,aj=0,ak=0,an=0,ao=0,ap=0,aq=0,ar=0,as=0,\
at=0,au=0,av=0,aw=0,ax=0,ay=0,az=0,aA=0,aD=0,aE=0,aG=0,aH=0,aI=0,aJ=0,aK=0,aL=0\
;do{if(a>>>0<245){if(a>>>0<11){b=16}else{b=a+11&-8}d=b>>>3;e=c[3302]|0;f=e>>>(d\
>>>0);if((f&3|0)!=0){g=(f&1^1)+d|0;h=g<<1;i=13248+(h<<2)|0;j=13248+(h+2<<2)|0;h\
=c[j>>2]|0;k=h+8|0;l=c[k>>2]|0;do{if((i|0)==(l|0)){c[3302]=e&(1<<g^-1)}else{if(\
l>>>0<(c[3306]|0)>>>0){am();return 0;return 0}m=l+12|0;if((c[m>>2]|0)==(h|0)){c\
[m>>2]=i;c[j>>2]=l;break}else{am();return 0;return 0}}}while(0);l=g<<3;c[h+4>>2\
]=l|3;j=h+(l|4)|0;c[j>>2]=c[j>>2]|1;n=k;return n|0}if(b>>>0<=(c[3304]|0)>>>0){o\
=b;break}if((f|0)!=0){j=2<<d;l=f<<d&(j|-j);j=(l&-l)-1|0;l=j>>>12&16;i=j>>>(l>>>\
0);j=i>>>5&8;m=i>>>(j>>>0);i=m>>>2&4;p=m>>>(i>>>0);m=p>>>1&2;q=p>>>(m>>>0);p=q>\
>>1&1;r=(j|l|i|m|p)+(q>>>(p>>>0))|0;p=r<<1;q=13248+(p<<2)|0;m=13248+(p+2<<2)|0;\
p=c[m>>2]|0;i=p+8|0;l=c[i>>2]|0;do{if((q|0)==(l|0)){c[3302]=e&(1<<r^-1)}else{if\
(l>>>0<(c[3306]|0)>>>0){am();return 0;return 0}j=l+12|0;if((c[j>>2]|0)==(p|0)){\
c[j>>2]=q;c[m>>2]=l;break}else{am();return 0;return 0}}}while(0);l=r<<3;m=l-b|0\
;c[p+4>>2]=b|3;q=p;e=q+b|0;c[q+(b|4)>>2]=m|1;c[q+l>>2]=m;l=c[3304]|0;if((l|0)!=\
0){q=c[3307]|0;d=l>>>3;l=d<<1;f=13248+(l<<2)|0;k=c[3302]|0;h=1<<d;do{if((k&h|0)\
==0){c[3302]=k|h;s=f;t=13248+(l+2<<2)|0}else{d=13248+(l+2<<2)|0;g=c[d>>2]|0;if(\
g>>>0>=(c[3306]|0)>>>0){s=g;t=d;break}am();return 0;return 0}}while(0);c[t>>2]=\
q;c[s+12>>2]=q;c[q+8>>2]=s;c[q+12>>2]=f}c[3304]=m;c[3307]=e;n=i;return n|0}l=c[\
3303]|0;if((l|0)==0){o=b;break}h=(l&-l)-1|0;l=h>>>12&16;k=h>>>(l>>>0);h=k>>>5&8\
;p=k>>>(h>>>0);k=p>>>2&4;r=p>>>(k>>>0);p=r>>>1&2;d=r>>>(p>>>0);r=d>>>1&1;g=c[13\
512+((h|l|k|p|r)+(d>>>(r>>>0))<<2)>>2]|0;r=g;d=g;p=(c[g+4>>2]&-8)-b|0;while(1){\
g=c[r+16>>2]|0;if((g|0)==0){k=c[r+20>>2]|0;if((k|0)==0){break}else{u=k}}else{u=\
g}g=(c[u+4>>2]&-8)-b|0;k=g>>>0<p>>>0;r=u;d=k?u:d;p=k?g:p}r=d;i=c[3306]|0;if(r>>\
>0<i>>>0){am();return 0;return 0}e=r+b|0;m=e;if(r>>>0>=e>>>0){am();return 0;ret\
urn 0}e=c[d+24>>2]|0;f=c[d+12>>2]|0;do{if((f|0)==(d|0)){q=d+20|0;g=c[q>>2]|0;if\
((g|0)==0){k=d+16|0;l=c[k>>2]|0;if((l|0)==0){v=0;break}else{w=l;x=k}}else{w=g;x\
=q}while(1){q=w+20|0;g=c[q>>2]|0;if((g|0)!=0){w=g;x=q;continue}q=w+16|0;g=c[q>>\
2]|0;if((g|0)==0){break}else{w=g;x=q}}if(x>>>0<i>>>0){am();return 0;return 0}el\
se{c[x>>2]=0;v=w;break}}else{q=c[d+8>>2]|0;if(q>>>0<i>>>0){am();return 0;return\
 0}g=q+12|0;if((c[g>>2]|0)!=(d|0)){am();return 0;return 0}k=f+8|0;if((c[k>>2]|0\
)==(d|0)){c[g>>2]=f;c[k>>2]=q;v=f;break}else{am();return 0;return 0}}}while(0);\
L1949:do{if((e|0)!=0){f=d+28|0;i=13512+(c[f>>2]<<2)|0;do{if((d|0)==(c[i>>2]|0))\
{c[i>>2]=v;if((v|0)!=0){break}c[3303]=c[3303]&(1<<c[f>>2]^-1);break L1949}else{\
if(e>>>0<(c[3306]|0)>>>0){am();return 0;return 0}q=e+16|0;if((c[q>>2]|0)==(d|0)\
){c[q>>2]=v}else{c[e+20>>2]=v}if((v|0)==0){break L1949}}}while(0);if(v>>>0<(c[3\
306]|0)>>>0){am();return 0;return 0}c[v+24>>2]=e;f=c[d+16>>2]|0;do{if((f|0)!=0)\
{if(f>>>0<(c[3306]|0)>>>0){am();return 0;return 0}else{c[v+16>>2]=f;c[f+24>>2]=\
v;break}}}while(0);f=c[d+20>>2]|0;if((f|0)==0){break}if(f>>>0<(c[3306]|0)>>>0){\
am();return 0;return 0}else{c[v+20>>2]=f;c[f+24>>2]=v;break}}}while(0);if(p>>>0\
<16){e=p+b|0;c[d+4>>2]=e|3;f=r+(e+4)|0;c[f>>2]=c[f>>2]|1}else{c[d+4>>2]=b|3;c[r\
+(b|4)>>2]=p|1;c[r+(p+b)>>2]=p;f=c[3304]|0;if((f|0)!=0){e=c[3307]|0;i=f>>>3;f=i\
<<1;q=13248+(f<<2)|0;k=c[3302]|0;g=1<<i;do{if((k&g|0)==0){c[3302]=k|g;y=q;z=132\
48+(f+2<<2)|0}else{i=13248+(f+2<<2)|0;l=c[i>>2]|0;if(l>>>0>=(c[3306]|0)>>>0){y=\
l;z=i;break}am();return 0;return 0}}while(0);c[z>>2]=e;c[y+12>>2]=e;c[e+8>>2]=y\
;c[e+12>>2]=q}c[3304]=p;c[3307]=m}f=d+8|0;if((f|0)==0){o=b;break}else{n=f}retur\
n n|0}else{if(a>>>0>4294967231){o=-1;break}f=a+11|0;g=f&-8;k=c[3303]|0;if((k|0)\
==0){o=g;break}r=-g|0;i=f>>>8;do{if((i|0)==0){A=0}else{if(g>>>0>16777215){A=31;\
break}f=(i+1048320|0)>>>16&8;l=i<<f;h=(l+520192|0)>>>16&4;j=l<<h;l=(j+245760|0)\
>>>16&2;B=14-(h|f|l)+(j<<l>>>15)|0;A=g>>>((B+7|0)>>>0)&1|B<<1}}while(0);i=c[135\
12+(A<<2)>>2]|0;L1997:do{if((i|0)==0){C=0;D=r;E=0}else{if((A|0)==31){F=0}else{F\
=25-(A>>>1)|0}d=0;m=r;p=i;q=g<<F;e=0;while(1){B=c[p+4>>2]&-8;l=B-g|0;if(l>>>0<m\
>>>0){if((B|0)==(g|0)){C=p;D=l;E=p;break L1997}else{G=p;H=l}}else{G=d;H=m}l=c[p\
+20>>2]|0;B=c[p+16+(q>>>31<<2)>>2]|0;j=(l|0)==0|(l|0)==(B|0)?e:l;if((B|0)==0){C\
=G;D=H;E=j;break}else{d=G;m=H;p=B;q=q<<1;e=j}}}}while(0);if((E|0)==0&(C|0)==0){\
i=2<<A;r=k&(i|-i);if((r|0)==0){o=g;break}i=(r&-r)-1|0;r=i>>>12&16;e=i>>>(r>>>0)\
;i=e>>>5&8;q=e>>>(i>>>0);e=q>>>2&4;p=q>>>(e>>>0);q=p>>>1&2;m=p>>>(q>>>0);p=m>>>\
1&1;I=c[13512+((i|r|e|q|p)+(m>>>(p>>>0))<<2)>>2]|0}else{I=E}if((I|0)==0){J=D;K=\
C}else{p=I;m=D;q=C;while(1){e=(c[p+4>>2]&-8)-g|0;r=e>>>0<m>>>0;i=r?e:m;e=r?p:q;\
r=c[p+16>>2]|0;if((r|0)!=0){p=r;m=i;q=e;continue}r=c[p+20>>2]|0;if((r|0)==0){J=\
i;K=e;break}else{p=r;m=i;q=e}}}if((K|0)==0){o=g;break}if(J>>>0>=((c[3304]|0)-g|\
0)>>>0){o=g;break}q=K;m=c[3306]|0;if(q>>>0<m>>>0){am();return 0;return 0}p=q+g|\
0;k=p;if(q>>>0>=p>>>0){am();return 0;return 0}e=c[K+24>>2]|0;i=c[K+12>>2]|0;do{\
if((i|0)==(K|0)){r=K+20|0;d=c[r>>2]|0;if((d|0)==0){j=K+16|0;B=c[j>>2]|0;if((B|0\
)==0){L=0;break}else{M=B;N=j}}else{M=d;N=r}while(1){r=M+20|0;d=c[r>>2]|0;if((d|\
0)!=0){M=d;N=r;continue}r=M+16|0;d=c[r>>2]|0;if((d|0)==0){break}else{M=d;N=r}}i\
f(N>>>0<m>>>0){am();return 0;return 0}else{c[N>>2]=0;L=M;break}}else{r=c[K+8>>2\
]|0;if(r>>>0<m>>>0){am();return 0;return 0}d=r+12|0;if((c[d>>2]|0)!=(K|0)){am()\
;return 0;return 0}j=i+8|0;if((c[j>>2]|0)==(K|0)){c[d>>2]=i;c[j>>2]=r;L=i;break\
}else{am();return 0;return 0}}}while(0);L2047:do{if((e|0)!=0){i=K+28|0;m=13512+\
(c[i>>2]<<2)|0;do{if((K|0)==(c[m>>2]|0)){c[m>>2]=L;if((L|0)!=0){break}c[3303]=c\
[3303]&(1<<c[i>>2]^-1);break L2047}else{if(e>>>0<(c[3306]|0)>>>0){am();return 0\
;return 0}r=e+16|0;if((c[r>>2]|0)==(K|0)){c[r>>2]=L}else{c[e+20>>2]=L}if((L|0)=\
=0){break L2047}}}while(0);if(L>>>0<(c[3306]|0)>>>0){am();return 0;return 0}c[L\
+24>>2]=e;i=c[K+16>>2]|0;do{if((i|0)!=0){if(i>>>0<(c[3306]|0)>>>0){am();return \
0;return 0}else{c[L+16>>2]=i;c[i+24>>2]=L;break}}}while(0);i=c[K+20>>2]|0;if((i\
|0)==0){break}if(i>>>0<(c[3306]|0)>>>0){am();return 0;return 0}else{c[L+20>>2]=\
i;c[i+24>>2]=L;break}}}while(0);do{if(J>>>0<16){e=J+g|0;c[K+4>>2]=e|3;i=q+(e+4)\
|0;c[i>>2]=c[i>>2]|1}else{c[K+4>>2]=g|3;c[q+(g|4)>>2]=J|1;c[q+(J+g)>>2]=J;i=J>>\
>3;if(J>>>0<256){e=i<<1;m=13248+(e<<2)|0;r=c[3302]|0;j=1<<i;do{if((r&j|0)==0){c\
[3302]=r|j;O=m;P=13248+(e+2<<2)|0}else{i=13248+(e+2<<2)|0;d=c[i>>2]|0;if(d>>>0>\
=(c[3306]|0)>>>0){O=d;P=i;break}am();return 0;return 0}}while(0);c[P>>2]=k;c[O+\
12>>2]=k;c[q+(g+8)>>2]=O;c[q+(g+12)>>2]=m;break}e=p;j=J>>>8;do{if((j|0)==0){Q=0\
}else{if(J>>>0>16777215){Q=31;break}r=(j+1048320|0)>>>16&8;i=j<<r;d=(i+520192|0\
)>>>16&4;B=i<<d;i=(B+245760|0)>>>16&2;l=14-(d|r|i)+(B<<i>>>15)|0;Q=J>>>((l+7|0)\
>>>0)&1|l<<1}}while(0);j=13512+(Q<<2)|0;c[q+(g+28)>>2]=Q;c[q+(g+20)>>2]=0;c[q+(\
g+16)>>2]=0;m=c[3303]|0;l=1<<Q;if((m&l|0)==0){c[3303]=m|l;c[j>>2]=e;c[q+(g+24)>\
>2]=j;c[q+(g+12)>>2]=e;c[q+(g+8)>>2]=e;break}if((Q|0)==31){R=0}else{R=25-(Q>>>1\
)|0}l=J<<R;m=c[j>>2]|0;while(1){if((c[m+4>>2]&-8|0)==(J|0)){break}S=m+16+(l>>>3\
1<<2)|0;j=c[S>>2]|0;if((j|0)==0){T=1495;break}else{l=l<<1;m=j}}if((T|0)==1495){\
if(S>>>0<(c[3306]|0)>>>0){am();return 0;return 0}else{c[S>>2]=e;c[q+(g+24)>>2]=\
m;c[q+(g+12)>>2]=e;c[q+(g+8)>>2]=e;break}}l=m+8|0;j=c[l>>2]|0;i=c[3306]|0;if(m>\
>>0<i>>>0){am();return 0;return 0}if(j>>>0<i>>>0){am();return 0;return 0}else{c\
[j+12>>2]=e;c[l>>2]=e;c[q+(g+8)>>2]=j;c[q+(g+12)>>2]=m;c[q+(g+24)>>2]=0;break}}\
}while(0);q=K+8|0;if((q|0)==0){o=g;break}else{n=q}return n|0}}while(0);K=c[3304\
]|0;if(o>>>0<=K>>>0){S=K-o|0;J=c[3307]|0;if(S>>>0>15){R=J;c[3307]=R+o;c[3304]=S\
;c[R+(o+4)>>2]=S|1;c[R+K>>2]=S;c[J+4>>2]=o|3}else{c[3304]=0;c[3307]=0;c[J+4>>2]\
=K|3;S=J+(K+4)|0;c[S>>2]=c[S>>2]|1}n=J+8|0;return n|0}J=c[3305]|0;if(o>>>0<J>>>\
0){S=J-o|0;c[3305]=S;J=c[3308]|0;K=J;c[3308]=K+o;c[K+(o+4)>>2]=S|1;c[J+4>>2]=o|\
3;n=J+8|0;return n|0}do{if((c[340]|0)==0){J=al(8)|0;if((J-1&J|0)==0){c[342]=J;c\
[341]=J;c[343]=-1;c[344]=2097152;c[345]=0;c[3413]=0;c[340]=(aF(0)|0)&-16^143165\
5768;break}else{am();return 0;return 0}}}while(0);J=o+48|0;S=c[342]|0;K=o+47|0;\
R=S+K|0;Q=-S|0;S=R&Q;if(S>>>0<=o>>>0){n=0;return n|0}O=c[3412]|0;do{if((O|0)!=0\
){P=c[3410]|0;L=P+S|0;if(L>>>0<=P>>>0|L>>>0>O>>>0){n=0}else{break}return n|0}}w\
hile(0);L2139:do{if((c[3413]&4|0)==0){O=c[3308]|0;L2141:do{if((O|0)==0){T=1525}\
else{L=O;P=13656;while(1){U=P|0;M=c[U>>2]|0;if(M>>>0<=L>>>0){V=P+4|0;if((M+(c[V\
>>2]|0)|0)>>>0>L>>>0){break}}M=c[P+8>>2]|0;if((M|0)==0){T=1525;break L2141}else\
{P=M}}if((P|0)==0){T=1525;break}L=R-(c[3305]|0)&Q;if(L>>>0>=2147483647){W=0;bre\
ak}m=aB(L|0)|0;e=(m|0)==((c[U>>2]|0)+(c[V>>2]|0)|0);X=e?m:-1;Y=e?L:0;Z=m;_=L;T=\
1534}}while(0);do{if((T|0)==1525){O=aB(0)|0;if((O|0)==-1){W=0;break}g=O;L=c[341\
]|0;m=L-1|0;if((m&g|0)==0){$=S}else{$=S-g+(m+g&-L)|0}L=c[3410]|0;g=L+$|0;if(!($\
>>>0>o>>>0&$>>>0<2147483647)){W=0;break}m=c[3412]|0;if((m|0)!=0){if(g>>>0<=L>>>\
0|g>>>0>m>>>0){W=0;break}}m=aB($|0)|0;g=(m|0)==(O|0);X=g?O:-1;Y=g?$:0;Z=m;_=$;T\
=1534}}while(0);L2161:do{if((T|0)==1534){m=-_|0;if((X|0)!=-1){aa=Y;ab=X;T=1545;\
break L2139}do{if((Z|0)!=-1&_>>>0<2147483647&_>>>0<J>>>0){g=c[342]|0;O=K-_+g&-g\
;if(O>>>0>=2147483647){ac=_;break}if((aB(O|0)|0)==-1){aB(m|0)|0;W=Y;break L2161\
}else{ac=O+_|0;break}}else{ac=_}}while(0);if((Z|0)==-1){W=Y}else{aa=ac;ab=Z;T=1\
545;break L2139}}}while(0);c[3413]=c[3413]|4;ad=W;T=1542}else{ad=0;T=1542}}whil\
e(0);do{if((T|0)==1542){if(S>>>0>=2147483647){break}W=aB(S|0)|0;Z=aB(0)|0;if(!(\
(Z|0)!=-1&(W|0)!=-1&W>>>0<Z>>>0)){break}ac=Z-W|0;Z=ac>>>0>(o+40|0)>>>0;Y=Z?W:-1\
;if((Y|0)!=-1){aa=Z?ac:ad;ab=Y;T=1545}}}while(0);do{if((T|0)==1545){ad=(c[3410]\
|0)+aa|0;c[3410]=ad;if(ad>>>0>(c[3411]|0)>>>0){c[3411]=ad}ad=c[3308]|0;L2181:do\
{if((ad|0)==0){S=c[3306]|0;if((S|0)==0|ab>>>0<S>>>0){c[3306]=ab}c[3414]=ab;c[34\
15]=aa;c[3417]=0;c[3311]=c[340];c[3310]=-1;S=0;do{Y=S<<1;ac=13248+(Y<<2)|0;c[13\
248+(Y+3<<2)>>2]=ac;c[13248+(Y+2<<2)>>2]=ac;S=S+1|0;}while(S>>>0<32);S=ab+8|0;i\
f((S&7|0)==0){ae=0}else{ae=-S&7}S=aa-40-ae|0;c[3308]=ab+ae;c[3305]=S;c[ab+(ae+4\
)>>2]=S|1;c[ab+(aa-36)>>2]=40;c[3309]=c[344]}else{S=13656;while(1){af=c[S>>2]|0\
;ag=S+4|0;ah=c[ag>>2]|0;if((ab|0)==(af+ah|0)){T=1557;break}ac=c[S+8>>2]|0;if((a\
c|0)==0){break}else{S=ac}}do{if((T|0)==1557){if((c[S+12>>2]&8|0)!=0){break}ac=a\
d;if(!(ac>>>0>=af>>>0&ac>>>0<ab>>>0)){break}c[ag>>2]=ah+aa;ac=c[3308]|0;Y=(c[33\
05]|0)+aa|0;Z=ac;W=ac+8|0;if((W&7|0)==0){ai=0}else{ai=-W&7}W=Y-ai|0;c[3308]=Z+a\
i;c[3305]=W;c[Z+(ai+4)>>2]=W|1;c[Z+(Y+4)>>2]=40;c[3309]=c[344];break L2181}}whi\
le(0);if(ab>>>0<(c[3306]|0)>>>0){c[3306]=ab}S=ab+aa|0;Y=13656;while(1){aj=Y|0;i\
f((c[aj>>2]|0)==(S|0)){T=1567;break}Z=c[Y+8>>2]|0;if((Z|0)==0){break}else{Y=Z}}\
do{if((T|0)==1567){if((c[Y+12>>2]&8|0)!=0){break}c[aj>>2]=ab;S=Y+4|0;c[S>>2]=(c\
[S>>2]|0)+aa;S=ab+8|0;if((S&7|0)==0){ak=0}else{ak=-S&7}S=ab+(aa+8)|0;if((S&7|0)\
==0){an=0}else{an=-S&7}S=ab+(an+aa)|0;Z=S;W=ak+o|0;ac=ab+W|0;_=ac;K=S-(ab+ak)-o\
|0;c[ab+(ak+4)>>2]=o|3;do{if((Z|0)==(c[3308]|0)){J=(c[3305]|0)+K|0;c[3305]=J;c[\
3308]=_;c[ab+(W+4)>>2]=J|1}else{if((Z|0)==(c[3307]|0)){J=(c[3304]|0)+K|0;c[3304\
]=J;c[3307]=_;c[ab+(W+4)>>2]=J|1;c[ab+(J+W)>>2]=J;break}J=aa+4|0;X=c[ab+(J+an)>\
>2]|0;if((X&3|0)==1){$=X&-8;V=X>>>3;L2226:do{if(X>>>0<256){U=c[ab+((an|8)+aa)>>\
2]|0;Q=c[ab+(aa+12+an)>>2]|0;R=13248+(V<<1<<2)|0;do{if((U|0)!=(R|0)){if(U>>>0<(\
c[3306]|0)>>>0){am();return 0;return 0}if((c[U+12>>2]|0)==(Z|0)){break}am();ret\
urn 0;return 0}}while(0);if((Q|0)==(U|0)){c[3302]=c[3302]&(1<<V^-1);break}do{if\
((Q|0)==(R|0)){ao=Q+8|0}else{if(Q>>>0<(c[3306]|0)>>>0){am();return 0;return 0}m\
=Q+8|0;if((c[m>>2]|0)==(Z|0)){ao=m;break}am();return 0;return 0}}while(0);c[U+1\
2>>2]=Q;c[ao>>2]=U}else{R=S;m=c[ab+((an|24)+aa)>>2]|0;P=c[ab+(aa+12+an)>>2]|0;d\
o{if((P|0)==(R|0)){O=an|16;g=ab+(J+O)|0;L=c[g>>2]|0;if((L|0)==0){e=ab+(O+aa)|0;\
O=c[e>>2]|0;if((O|0)==0){ap=0;break}else{aq=O;ar=e}}else{aq=L;ar=g}while(1){g=a\
q+20|0;L=c[g>>2]|0;if((L|0)!=0){aq=L;ar=g;continue}g=aq+16|0;L=c[g>>2]|0;if((L|\
0)==0){break}else{aq=L;ar=g}}if(ar>>>0<(c[3306]|0)>>>0){am();return 0;return 0}\
else{c[ar>>2]=0;ap=aq;break}}else{g=c[ab+((an|8)+aa)>>2]|0;if(g>>>0<(c[3306]|0)\
>>>0){am();return 0;return 0}L=g+12|0;if((c[L>>2]|0)!=(R|0)){am();return 0;retu\
rn 0}e=P+8|0;if((c[e>>2]|0)==(R|0)){c[L>>2]=P;c[e>>2]=g;ap=P;break}else{am();re\
turn 0;return 0}}}while(0);if((m|0)==0){break}P=ab+(aa+28+an)|0;U=13512+(c[P>>2\
]<<2)|0;do{if((R|0)==(c[U>>2]|0)){c[U>>2]=ap;if((ap|0)!=0){break}c[3303]=c[3303\
]&(1<<c[P>>2]^-1);break L2226}else{if(m>>>0<(c[3306]|0)>>>0){am();return 0;retu\
rn 0}Q=m+16|0;if((c[Q>>2]|0)==(R|0)){c[Q>>2]=ap}else{c[m+20>>2]=ap}if((ap|0)==0\
){break L2226}}}while(0);if(ap>>>0<(c[3306]|0)>>>0){am();return 0;return 0}c[ap\
+24>>2]=m;R=an|16;P=c[ab+(R+aa)>>2]|0;do{if((P|0)!=0){if(P>>>0<(c[3306]|0)>>>0)\
{am();return 0;return 0}else{c[ap+16>>2]=P;c[P+24>>2]=ap;break}}}while(0);P=c[a\
b+(J+R)>>2]|0;if((P|0)==0){break}if(P>>>0<(c[3306]|0)>>>0){am();return 0;return\
 0}else{c[ap+20>>2]=P;c[P+24>>2]=ap;break}}}while(0);as=ab+(($|an)+aa)|0;at=$+K\
|0}else{as=Z;at=K}J=as+4|0;c[J>>2]=c[J>>2]&-2;c[ab+(W+4)>>2]=at|1;c[ab+(at+W)>>\
2]=at;J=at>>>3;if(at>>>0<256){V=J<<1;X=13248+(V<<2)|0;P=c[3302]|0;m=1<<J;do{if(\
(P&m|0)==0){c[3302]=P|m;au=X;av=13248+(V+2<<2)|0}else{J=13248+(V+2<<2)|0;U=c[J>\
>2]|0;if(U>>>0>=(c[3306]|0)>>>0){au=U;av=J;break}am();return 0;return 0}}while(\
0);c[av>>2]=_;c[au+12>>2]=_;c[ab+(W+8)>>2]=au;c[ab+(W+12)>>2]=X;break}V=ac;m=at\
>>>8;do{if((m|0)==0){aw=0}else{if(at>>>0>16777215){aw=31;break}P=(m+1048320|0)>\
>>16&8;$=m<<P;J=($+520192|0)>>>16&4;U=$<<J;$=(U+245760|0)>>>16&2;Q=14-(J|P|$)+(\
U<<$>>>15)|0;aw=at>>>((Q+7|0)>>>0)&1|Q<<1}}while(0);m=13512+(aw<<2)|0;c[ab+(W+2\
8)>>2]=aw;c[ab+(W+20)>>2]=0;c[ab+(W+16)>>2]=0;X=c[3303]|0;Q=1<<aw;if((X&Q|0)==0\
){c[3303]=X|Q;c[m>>2]=V;c[ab+(W+24)>>2]=m;c[ab+(W+12)>>2]=V;c[ab+(W+8)>>2]=V;br\
eak}if((aw|0)==31){ax=0}else{ax=25-(aw>>>1)|0}Q=at<<ax;X=c[m>>2]|0;while(1){if(\
(c[X+4>>2]&-8|0)==(at|0)){break}ay=X+16+(Q>>>31<<2)|0;m=c[ay>>2]|0;if((m|0)==0)\
{T=1640;break}else{Q=Q<<1;X=m}}if((T|0)==1640){if(ay>>>0<(c[3306]|0)>>>0){am();\
return 0;return 0}else{c[ay>>2]=V;c[ab+(W+24)>>2]=X;c[ab+(W+12)>>2]=V;c[ab+(W+8\
)>>2]=V;break}}Q=X+8|0;m=c[Q>>2]|0;$=c[3306]|0;if(X>>>0<$>>>0){am();return 0;re\
turn 0}if(m>>>0<$>>>0){am();return 0;return 0}else{c[m+12>>2]=V;c[Q>>2]=V;c[ab+\
(W+8)>>2]=m;c[ab+(W+12)>>2]=X;c[ab+(W+24)>>2]=0;break}}}while(0);n=ab+(ak|8)|0;\
return n|0}}while(0);Y=ad;W=13656;while(1){az=c[W>>2]|0;if(az>>>0<=Y>>>0){aA=c[\
W+4>>2]|0;aD=az+aA|0;if(aD>>>0>Y>>>0){break}}W=c[W+8>>2]|0}W=az+(aA-39)|0;if((W\
&7|0)==0){aE=0}else{aE=-W&7}W=az+(aA-47+aE)|0;ac=W>>>0<(ad+16|0)>>>0?Y:W;W=ac+8\
|0;_=ab+8|0;if((_&7|0)==0){aG=0}else{aG=-_&7}_=aa-40-aG|0;c[3308]=ab+aG;c[3305]\
=_;c[ab+(aG+4)>>2]=_|1;c[ab+(aa-36)>>2]=40;c[3309]=c[344];c[ac+4>>2]=27;c[W>>2]\
=c[3414];c[W+4>>2]=c[13660>>2];c[W+8>>2]=c[13664>>2];c[W+12>>2]=c[13668>>2];c[3\
414]=ab;c[3415]=aa;c[3417]=0;c[3416]=W;W=ac+28|0;c[W>>2]=7;if((ac+32|0)>>>0<aD>\
>>0){_=W;while(1){W=_+4|0;c[W>>2]=7;if((_+8|0)>>>0<aD>>>0){_=W}else{break}}}if(\
(ac|0)==(Y|0)){break}_=ac-ad|0;W=Y+(_+4)|0;c[W>>2]=c[W>>2]&-2;c[ad+4>>2]=_|1;c[\
Y+_>>2]=_;W=_>>>3;if(_>>>0<256){K=W<<1;Z=13248+(K<<2)|0;S=c[3302]|0;m=1<<W;do{i\
f((S&m|0)==0){c[3302]=S|m;aH=Z;aI=13248+(K+2<<2)|0}else{W=13248+(K+2<<2)|0;Q=c[\
W>>2]|0;if(Q>>>0>=(c[3306]|0)>>>0){aH=Q;aI=W;break}am();return 0;return 0}}whil\
e(0);c[aI>>2]=ad;c[aH+12>>2]=ad;c[ad+8>>2]=aH;c[ad+12>>2]=Z;break}K=ad;m=_>>>8;\
do{if((m|0)==0){aJ=0}else{if(_>>>0>16777215){aJ=31;break}S=(m+1048320|0)>>>16&8\
;Y=m<<S;ac=(Y+520192|0)>>>16&4;W=Y<<ac;Y=(W+245760|0)>>>16&2;Q=14-(ac|S|Y)+(W<<\
Y>>>15)|0;aJ=_>>>((Q+7|0)>>>0)&1|Q<<1}}while(0);m=13512+(aJ<<2)|0;c[ad+28>>2]=a\
J;c[ad+20>>2]=0;c[ad+16>>2]=0;Z=c[3303]|0;Q=1<<aJ;if((Z&Q|0)==0){c[3303]=Z|Q;c[\
m>>2]=K;c[ad+24>>2]=m;c[ad+12>>2]=ad;c[ad+8>>2]=ad;break}if((aJ|0)==31){aK=0}el\
se{aK=25-(aJ>>>1)|0}Q=_<<aK;Z=c[m>>2]|0;while(1){if((c[Z+4>>2]&-8|0)==(_|0)){br\
eak}aL=Z+16+(Q>>>31<<2)|0;m=c[aL>>2]|0;if((m|0)==0){T=1675;break}else{Q=Q<<1;Z=\
m}}if((T|0)==1675){if(aL>>>0<(c[3306]|0)>>>0){am();return 0;return 0}else{c[aL>\
>2]=K;c[ad+24>>2]=Z;c[ad+12>>2]=ad;c[ad+8>>2]=ad;break}}Q=Z+8|0;_=c[Q>>2]|0;m=c\
[3306]|0;if(Z>>>0<m>>>0){am();return 0;return 0}if(_>>>0<m>>>0){am();return 0;r\
eturn 0}else{c[_+12>>2]=K;c[Q>>2]=K;c[ad+8>>2]=_;c[ad+12>>2]=Z;c[ad+24>>2]=0;br\
eak}}}while(0);ad=c[3305]|0;if(ad>>>0<=o>>>0){break}_=ad-o|0;c[3305]=_;ad=c[330\
8]|0;Q=ad;c[3308]=Q+o;c[Q+(o+4)>>2]=_|1;c[ad+4>>2]=o|3;n=ad+8|0;return n|0}}whi\
le(0);c[(aC()|0)>>2]=12;n=0;return n|0}function bl(b){b=b|0;var c=0;c=b;while(a\
[c]|0){c=c+1|0}return c-b|0}function bm(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=\
0;f=b+e|0;if((e|0)>=20){d=d&255;e=b&3;g=d|d<<8|d<<16|d<<24;h=f&~3;if(e){e=b+4-e\
|0;while((b|0)<(e|0)){a[b]=d;b=b+1|0}}while((b|0)<(h|0)){c[b>>2]=g;b=b+4|0}}whi\
le((b|0)<(f|0)){a[b]=d;b=b+1|0}}function bn(b,d,e){b=b|0;d=d|0;e=e|0;var f=0;f=\
b|0;if((b&3)==(d&3)){while(b&3){if((e|0)==0)return f|0;a[b]=a[d]|0;b=b+1|0;d=d+\
1|0;e=e-1|0}while((e|0)>=4){c[b>>2]=c[d>>2];b=b+4|0;d=d+4|0;e=e-4|0}}while((e|0\
)>0){a[b]=a[d]|0;b=b+1|0;d=d+1|0;e=e-1|0}return f|0}function bo(a,b){a=a|0;b=b|\
0;return aH[a&1](b|0)|0}function bp(a,b){a=a|0;b=b|0;aI[a&1](b|0)}function bq(a\
,b,c){a=a|0;b=b|0;c=c|0;aJ[a&3](b|0,c|0)}function br(a,b,c,d){a=a|0;b=b|0;c=c|0\
;d=d|0;return aK[a&3](b|0,c|0,d|0)|0}function bs(a){a=a|0;aL[a&1]()}function bt\
(a,b,c){a=a|0;b=b|0;c=c|0;return aM[a&7](b|0,c|0)|0}function bu(a){a=a|0;_(0);r\
eturn 0}function bv(a){a=a|0;_(1)}function bw(a,b){a=a|0;b=b|0;_(2)}function bx\
(a,b,c){a=a|0;b=b|0;c=c|0;_(3);return 0}function by(){_(4)}function bz(a,b){a=a\
|0;b=b|0;_(5);return 0} var aH=[bu,bu];var aI=[bv,bv];var aJ=[bw,bw,bg,bw];var \
aK=[bx,bx,bf,bx];var aL=[by,by];var aM=[bz,bz,a5,bz,a8,bz,a4,bz];return{_malloc\
:bk,_strlen:bl,_memcpy:bn,_main:a2,_memset:bm,stackAlloc:aN,stackSave:aO,stackR\
estore:aP,setThrew:aQ,setTempRet0:aT,setTempRet1:aU,setTempRet2:aV,setTempRet3:\
aW,setTempRet4:aX,setTempRet5:aY,setTempRet6:aZ,setTempRet7:a_,setTempRet8:a$,s\
etTempRet9:a0,dynCall_ii:bo,dynCall_vi:bp,dynCall_vii:bq,dynCall_iiii:br,dynCal\
l_v:bs,dynCall_iii:bt} })({Math:Math,Int8Array:Int8Array,Int16Array:Int16Array,\
Int32Array:Int32Array,Uint8Array:Uint8Array,Uint16Array:Uint16Array,Uint32Array\
:Uint32Array,Float32Array:Float32Array,Float64Array:Float64Array},{abort:F,asse\
rt:z,asmPrintInt:function(a,b){Module.print("int "+a+","+b)},asmPrintFloat:func\
tion(a,b){Module.print("float "+a+","+b)},min:Wb,invoke_ii:function(a,b){try{re\
turn Module.dynCall_ii(a,b)}catch(c){"number"!==typeof c&&"longjmp"!==c&&j(c),$\
.setThrew(1,0)}}, invoke_vi:function(a,b){try{Module.dynCall_vi(a,b)}catch(c){"\
number"!==typeof c&&"longjmp"!==c&&j(c),$.setThrew(1,0)}},invoke_vii:function(a\
,b,c){try{Module.dynCall_vii(a,b,c)}catch(d){"number"!==typeof d&&"longjmp"!==d\
&&j(d),$.setThrew(1,0)}},invoke_iiii:function(a,b,c,d){try{return Module.dynCal\
l_iiii(a,b,c,d)}catch(e){"number"!==typeof e&&"longjmp"!==e&&j(e),$.setThrew(1,\
0)}},invoke_v:function(a){try{Module.dynCall_v(a)}catch(b){"number"!==typeof b&\
&"longjmp"!==b&&j(b),$.setThrew(1,0)}},invoke_iii:function(a, b,c){try{return M\
odule.dynCall_iii(a,b,c)}catch(d){"number"!==typeof d&&"longjmp"!==d&&j(d),$.se\
tThrew(1,0)}},_strncmp:Db,_llvm_lifetime_end:u(),_sysconf:function(a){switch(a)\
{case 8:return 4096;case 54:case 56:case 21:case 61:case 63:case 22:case 67:cas\
e 23:case 24:case 25:case 26:case 27:case 69:case 28:case 101:case 70:case 71:c\
ase 29:case 30:case 199:case 75:case 76:case 32:case 43:case 44:case 80:case 46\
:case 47:case 45:case 48:case 49:case 42:case 82:case 33:case 7:case 108:case 1\
09:case 107:case 112:case 119:case 121:return 200809; case 13:case 104:case 94:\
case 95:case 34:case 35:case 77:case 81:case 83:case 84:case 85:case 86:case 87\
:case 88:case 89:case 90:case 91:case 94:case 95:case 110:case 111:case 113:cas\
e 114:case 115:case 116:case 117:case 118:case 120:case 40:case 16:case 79:case\
 19:return-1;case 92:case 93:case 5:case 72:case 6:case 74:case 92:case 93:case\
 96:case 97:case 98:case 99:case 102:case 103:case 105:return 1;case 38:case 66\
:case 50:case 51:case 4:return 1024;case 15:case 64:case 41:return 32;case 55:c\
ase 37:case 17:return 2147483647; case 18:case 1:return 47839;case 59:case 57:r\
eturn 99;case 68:case 58:return 2048;case 0:return 2097152;case 3:return 65536;\
case 14:return 32768;case 73:return 32767;case 39:return 16384;case 60:return 1\
E3;case 106:return 700;case 52:return 256;case 62:return 255;case 2:return 100;\
case 65:return 64;case 36:return 20;case 100:return 16;case 20:return 6;case 53\
:return 4;case 10:return 1}V(db);return-1},_abort:function(){G=n;j("abort() at \
"+Error().stack)},_fprintf:Cb,_printf:function(a,b){},__reallyNegative:Ab,_fput\
c:Hb,_puts:function(a){},___setErrNo:V,_fwrite:yb,_send:vb,_write:xb,_fputs:Gb,\
__formatString:Bb,_free:u(),___assert_func:function(a,b,c,d){j("Assertion faile\
d: "+(d?I(d):"unknown condition")+", at: "+[a?I(a):"unknown filename",b,c?I(c):\
"unknown function"]+" at "+Error().stack)},_pwrite:wb,_sbrk:Z,___errno_location\
:function(){return eb},_llvm_lifetime_start:u(),_llvm_bswap_i32:function(a){ret\
urn(a&255)<<24|(a>> 8&255)<<16|(a>>16&255)<<8|a>>>24},_time:function(a){var b=M\
ath.floor(Date.now()/1E3);a&&(L[a>>2]=b);return b},_strcmp:function(a,b){return\
 Db(a,b,qa)},STACKTOP:w,STACK_MAX:Ja,tempDoublePtr:Za,ABORT:G,NaN:NaN,Infinity:\
Infinity},R),Ea=Module._malloc=$._malloc,zb=Module._strlen=$._strlen,Fb=Module.\
_memcpy=$._memcpy;Module._main=$._main;var Eb=Module._memset=$._memset;Module.d\
ynCall_ii=$.dynCall_ii;Module.dynCall_vi=$.dynCall_vi;Module.dynCall_vii=$.dynC\
all_vii;Module.dynCall_iiii=$.dynCall_iiii; Module.dynCall_v=$.dynCall_v;Module\
.dynCall_iii=$.dynCall_iii;na=function(a){return $.stackAlloc(a)};ha=function()\
{return $.stackSave()};ia=function(a){$.stackRestore(a)}; Module.callMain=funct\
ion(a){function b(){for(var a=0;3>a;a++)d.push(0)}z(0==T,"cannot call main when\
 async dependencies remain! (listen on __ATMAIN__)");z(!Module.preRun||0==Modul\
e.preRun.length,"cannot call main when preRun functions remain to be called");a\
=a||[];Pa||(Pa=n,La(Ma));var c=a.length+1,d=[P(S("/bin/this.program"),"i8",0)];\
b();for(var e=0;e<c-1;e+=1)d.push(P(S(a[e]),"i8",0)),b();d.push(0);var d=P(d,"i\
32",0),f,a=w;try{f=Module._main(c,d,0)}catch(g){if("ExitStatus"==g.name)return \
g.status; "SimulateInfiniteLoop"==g?Module.noExitRuntime=n:j(g)}finally{w=a}ret\
urn f}; function Ya(a){function b(){Pa||(Pa=n,La(Ma));La(Na);var b=0;Ta=n;Modul\
e._main&&Xa&&(b=Module.callMain(a),Module.noExitRuntime||La(Oa));if(Module.post\
Run)for("function"==typeof Module.postRun&&(Module.postRun=[Module.postRun]);0<\
Module.postRun.length;)Module.postRun.pop()();return b}a=a||Module.arguments;if\
(0<T)return Module.g("run() called, but dependencies remain, so not running"),0\
;if(Module.preRun){"function"==typeof Module.preRun&&(Module.preRun=[Module.pre\
Run]);var c=Module.preRun;Module.preRun= [];for(var d=c.length-1;0<=d;d--)c[d](\
);if(0<T)return 0}return Module.setStatus?(Module.setStatus("Running..."),setTi\
meout(function(){setTimeout(function(){Module.setStatus("")},1);G||b()},1),0):b\
()}Module.run=Module.X=Ya;if(Module.preInit)for("function"==typeof Module.preIn\
it&&(Module.preInit=[Module.preInit]);0<Module.preInit.length;)Module.preInit.p\
op()();var Xa=n;Module.noInitialRun&&(Xa=t); ');}

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

