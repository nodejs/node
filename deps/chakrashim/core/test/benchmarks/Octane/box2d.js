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
// box2d.js
/////////////////////////////////////////////////////////////

// Portions copyright 2013 Google, Inc

/*
* Copyright (c) 2006-2007 Erin Catto http://www.gphysics.com
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*/

/*
* Following code is beautified using http://jsbeautifier.org/
*/

var Box2D = {};
(function (F, G) {
    function K() {}
    if (!(Object.defineProperty instanceof Function) && Object.prototype.__defineGetter__ instanceof Function && Object.prototype.__defineSetter__ instanceof Function) Object.defineProperty = function (y, w, A) {
        A.get instanceof Function && y.__defineGetter__(w, A.get);
        A.set instanceof Function && y.__defineSetter__(w, A.set)
    };
    F.inherit = function (y, w) {
        K.prototype = w.prototype;
        y.prototype = new K;
        y.prototype.constructor = y
    };
    F.generateCallback = function (y, w) {
        return function () {
            w.apply(y, arguments)
        }
    };
    F.NVector = function (y) {
        if (y === G) y = 0;
        for (var w = Array(y || 0), A = 0; A < y; ++A) w[A] = 0;
        return w
    };
    F.is = function (y, w) {
        if (y === null) return false;
        if (w instanceof Function && y instanceof w) return true;
        if (y.constructor.__implements != G && y.constructor.__implements[w]) return true;
        return false
    };
    F.parseUInt = function (y) {
        return Math.abs(parseInt(y))
    }
})(Box2D);
var Vector = Array,
    Vector_a2j_Number = Box2D.NVector;
if (typeof Box2D === "undefined") Box2D = {};
if (typeof Box2D.Collision === "undefined") Box2D.Collision = {};
if (typeof Box2D.Collision.Shapes === "undefined") Box2D.Collision.Shapes = {};
if (typeof Box2D.Common === "undefined") Box2D.Common = {};
if (typeof Box2D.Common.Math === "undefined") Box2D.Common.Math = {};
if (typeof Box2D.Dynamics === "undefined") Box2D.Dynamics = {};
if (typeof Box2D.Dynamics.Contacts === "undefined") Box2D.Dynamics.Contacts = {};
if (typeof Box2D.Dynamics.Controllers === "undefined") Box2D.Dynamics.Controllers = {};
if (typeof Box2D.Dynamics.Joints === "undefined") Box2D.Dynamics.Joints = {};
(function () {
    function F() {
        F.b2AABB.apply(this, arguments)
    }

    function G() {
        G.b2Bound.apply(this, arguments)
    }

    function K() {
        K.b2BoundValues.apply(this, arguments);
        this.constructor === K && this.b2BoundValues.apply(this, arguments)
    }

    function y() {
        y.b2Collision.apply(this, arguments)
    }

    function w() {
        w.b2ContactID.apply(this, arguments);
        this.constructor === w && this.b2ContactID.apply(this, arguments)
    }

    function A() {
        A.b2ContactPoint.apply(this, arguments)
    }

    function U() {
        U.b2Distance.apply(this, arguments)
    }

    function p() {
        p.b2DistanceInput.apply(this,
            arguments)
    }

    function B() {
        B.b2DistanceOutput.apply(this, arguments)
    }

    function Q() {
        Q.b2DistanceProxy.apply(this, arguments)
    }

    function V() {
        V.b2DynamicTree.apply(this, arguments);
        this.constructor === V && this.b2DynamicTree.apply(this, arguments)
    }

    function M() {
        M.b2DynamicTreeBroadPhase.apply(this, arguments)
    }

    function L() {
        L.b2DynamicTreeNode.apply(this, arguments)
    }

    function I() {
        I.b2DynamicTreePair.apply(this, arguments)
    }

    function W() {
        W.b2Manifold.apply(this, arguments);
        this.constructor === W && this.b2Manifold.apply(this, arguments)
    }

    function Y() {
        Y.b2ManifoldPoint.apply(this, arguments);
        this.constructor === Y && this.b2ManifoldPoint.apply(this, arguments)
    }

    function k() {
        k.b2Point.apply(this, arguments)
    }

    function z() {
        z.b2RayCastInput.apply(this, arguments);
        this.constructor === z && this.b2RayCastInput.apply(this, arguments)
    }

    function u() {
        u.b2RayCastOutput.apply(this, arguments)
    }

    function D() {
        D.b2Segment.apply(this, arguments)
    }

    function H() {
        H.b2SeparationFunction.apply(this, arguments)
    }

    function O() {
        O.b2Simplex.apply(this, arguments);
        this.constructor === O && this.b2Simplex.apply(this, arguments)
    }

    function E() {
        E.b2SimplexCache.apply(this, arguments)
    }

    function R() {
        R.b2SimplexVertex.apply(this, arguments)
    }

    function N() {
        N.b2TimeOfImpact.apply(this, arguments)
    }

    function S() {
        S.b2TOIInput.apply(this, arguments)
    }

    function aa() {
        aa.b2WorldManifold.apply(this, arguments);
        this.constructor === aa && this.b2WorldManifold.apply(this, arguments)
    }

    function Z() {
        Z.ClipVertex.apply(this, arguments)
    }

    function d() {
        d.Features.apply(this, arguments)
    }

    function h() {
        h.b2CircleShape.apply(this, arguments);
        this.constructor === h && this.b2CircleShape.apply(this, arguments)
    }

    function l() {
        l.b2EdgeChainDef.apply(this, arguments);
        this.constructor === l && this.b2EdgeChainDef.apply(this, arguments)
    }

    function j() {
        j.b2EdgeShape.apply(this, arguments);
        this.constructor === j && this.b2EdgeShape.apply(this, arguments)
    }

    function o() {
        o.b2MassData.apply(this, arguments)
    }

    function q() {
        q.b2PolygonShape.apply(this, arguments);
        this.constructor === q && this.b2PolygonShape.apply(this, arguments)
    }

    function n() {
        n.b2Shape.apply(this, arguments);
        this.constructor === n && this.b2Shape.apply(this, arguments)
    }

    function a() {
        a.b2Color.apply(this, arguments);
        this.constructor === a && this.b2Color.apply(this, arguments)
    }

    function c() {
        c.b2Settings.apply(this, arguments)
    }

    function g() {
        g.b2Mat22.apply(this, arguments);
        this.constructor === g && this.b2Mat22.apply(this, arguments)
    }

    function b() {
        b.b2Mat33.apply(this, arguments);
        this.constructor === b && this.b2Mat33.apply(this, arguments)
    }

    function e() {
        e.b2Math.apply(this, arguments)
    }

    function f() {
        f.b2Sweep.apply(this, arguments)
    }

    function m() {
        m.b2Transform.apply(this,
            arguments);
        this.constructor === m && this.b2Transform.apply(this, arguments)
    }

    function r() {
        r.b2Vec2.apply(this, arguments);
        this.constructor === r && this.b2Vec2.apply(this, arguments)
    }

    function s() {
        s.b2Vec3.apply(this, arguments);
        this.constructor === s && this.b2Vec3.apply(this, arguments)
    }

    function v() {
        v.b2Body.apply(this, arguments);
        this.constructor === v && this.b2Body.apply(this, arguments)
    }

    function t() {
        t.b2BodyDef.apply(this, arguments);
        this.constructor === t && this.b2BodyDef.apply(this, arguments)
    }

    function x() {
        x.b2ContactFilter.apply(this,
            arguments)
    }

    function C() {
        C.b2ContactImpulse.apply(this, arguments)
    }

    function J() {
        J.b2ContactListener.apply(this, arguments)
    }

    function T() {
        T.b2ContactManager.apply(this, arguments);
        this.constructor === T && this.b2ContactManager.apply(this, arguments)
    }

    function P() {
        P.b2DebugDraw.apply(this, arguments);
        this.constructor === P && this.b2DebugDraw.apply(this, arguments)
    }

    function X() {
        X.b2DestructionListener.apply(this, arguments)
    }

    function $() {
        $.b2FilterData.apply(this, arguments)
    }

    function ba() {
        ba.b2Fixture.apply(this, arguments);
        this.constructor === ba && this.b2Fixture.apply(this, arguments)
    }

    function ca() {
        ca.b2FixtureDef.apply(this, arguments);
        this.constructor === ca && this.b2FixtureDef.apply(this, arguments)
    }

    function da() {
        da.b2Island.apply(this, arguments);
        this.constructor === da && this.b2Island.apply(this, arguments)
    }

    function Fa() {
        Fa.b2TimeStep.apply(this, arguments)
    }

    function ea() {
        ea.b2World.apply(this, arguments);
        this.constructor === ea && this.b2World.apply(this, arguments)
    }

    function Ga() {
        Ga.b2CircleContact.apply(this, arguments)
    }

    function fa() {
        fa.b2Contact.apply(this,
            arguments);
        this.constructor === fa && this.b2Contact.apply(this, arguments)
    }

    function ga() {
        ga.b2ContactConstraint.apply(this, arguments);
        this.constructor === ga && this.b2ContactConstraint.apply(this, arguments)
    }

    function Ha() {
        Ha.b2ContactConstraintPoint.apply(this, arguments)
    }

    function Ia() {
        Ia.b2ContactEdge.apply(this, arguments)
    }

    function ha() {
        ha.b2ContactFactory.apply(this, arguments);
        this.constructor === ha && this.b2ContactFactory.apply(this, arguments)
    }

    function Ja() {
        Ja.b2ContactRegister.apply(this, arguments)
    }

    function Ka() {
        Ka.b2ContactResult.apply(this,
            arguments)
    }

    function ia() {
        ia.b2ContactSolver.apply(this, arguments);
        this.constructor === ia && this.b2ContactSolver.apply(this, arguments)
    }

    function La() {
        La.b2EdgeAndCircleContact.apply(this, arguments)
    }

    function ja() {
        ja.b2NullContact.apply(this, arguments);
        this.constructor === ja && this.b2NullContact.apply(this, arguments)
    }

    function Ma() {
        Ma.b2PolyAndCircleContact.apply(this, arguments)
    }

    function Na() {
        Na.b2PolyAndEdgeContact.apply(this, arguments)
    }

    function Oa() {
        Oa.b2PolygonContact.apply(this, arguments)
    }

    function ka() {
        ka.b2PositionSolverManifold.apply(this,
            arguments);
        this.constructor === ka && this.b2PositionSolverManifold.apply(this, arguments)
    }

    function Pa() {
        Pa.b2BuoyancyController.apply(this, arguments)
    }

    function Qa() {
        Qa.b2ConstantAccelController.apply(this, arguments)
    }

    function Ra() {
        Ra.b2ConstantForceController.apply(this, arguments)
    }

    function Sa() {
        Sa.b2Controller.apply(this, arguments)
    }

    function Ta() {
        Ta.b2ControllerEdge.apply(this, arguments)
    }

    function Ua() {
        Ua.b2GravityController.apply(this, arguments)
    }

    function Va() {
        Va.b2TensorDampingController.apply(this, arguments)
    }

    function la() {
        la.b2DistanceJoint.apply(this, arguments);
        this.constructor === la && this.b2DistanceJoint.apply(this, arguments)
    }

    function ma() {
        ma.b2DistanceJointDef.apply(this, arguments);
        this.constructor === ma && this.b2DistanceJointDef.apply(this, arguments)
    }

    function na() {
        na.b2FrictionJoint.apply(this, arguments);
        this.constructor === na && this.b2FrictionJoint.apply(this, arguments)
    }

    function oa() {
        oa.b2FrictionJointDef.apply(this, arguments);
        this.constructor === oa && this.b2FrictionJointDef.apply(this, arguments)
    }

    function pa() {
        pa.b2GearJoint.apply(this,
            arguments);
        this.constructor === pa && this.b2GearJoint.apply(this, arguments)
    }

    function qa() {
        qa.b2GearJointDef.apply(this, arguments);
        this.constructor === qa && this.b2GearJointDef.apply(this, arguments)
    }

    function Wa() {
        Wa.b2Jacobian.apply(this, arguments)
    }

    function ra() {
        ra.b2Joint.apply(this, arguments);
        this.constructor === ra && this.b2Joint.apply(this, arguments)
    }

    function sa() {
        sa.b2JointDef.apply(this, arguments);
        this.constructor === sa && this.b2JointDef.apply(this, arguments)
    }

    function Xa() {
        Xa.b2JointEdge.apply(this, arguments)
    }

    function ta() {
        ta.b2LineJoint.apply(this, arguments);
        this.constructor === ta && this.b2LineJoint.apply(this, arguments)
    }

    function ua() {
        ua.b2LineJointDef.apply(this, arguments);
        this.constructor === ua && this.b2LineJointDef.apply(this, arguments)
    }

    function va() {
        va.b2MouseJoint.apply(this, arguments);
        this.constructor === va && this.b2MouseJoint.apply(this, arguments)
    }

    function wa() {
        wa.b2MouseJointDef.apply(this, arguments);
        this.constructor === wa && this.b2MouseJointDef.apply(this, arguments)
    }

    function xa() {
        xa.b2PrismaticJoint.apply(this,
            arguments);
        this.constructor === xa && this.b2PrismaticJoint.apply(this, arguments)
    }

    function ya() {
        ya.b2PrismaticJointDef.apply(this, arguments);
        this.constructor === ya && this.b2PrismaticJointDef.apply(this, arguments)
    }

    function za() {
        za.b2PulleyJoint.apply(this, arguments);
        this.constructor === za && this.b2PulleyJoint.apply(this, arguments)
    }

    function Aa() {
        Aa.b2PulleyJointDef.apply(this, arguments);
        this.constructor === Aa && this.b2PulleyJointDef.apply(this, arguments)
    }

    function Ba() {
        Ba.b2RevoluteJoint.apply(this, arguments);
        this.constructor === Ba && this.b2RevoluteJoint.apply(this, arguments)
    }

    function Ca() {
        Ca.b2RevoluteJointDef.apply(this, arguments);
        this.constructor === Ca && this.b2RevoluteJointDef.apply(this, arguments)
    }

    function Da() {
        Da.b2WeldJoint.apply(this, arguments);
        this.constructor === Da && this.b2WeldJoint.apply(this, arguments)
    }

    function Ea() {
        Ea.b2WeldJointDef.apply(this, arguments);
        this.constructor === Ea && this.b2WeldJointDef.apply(this, arguments)
    }
    Box2D.Collision.IBroadPhase = "Box2D.Collision.IBroadPhase";
    Box2D.Collision.b2AABB = F;
    Box2D.Collision.b2Bound = G;
    Box2D.Collision.b2BoundValues = K;
    Box2D.Collision.b2Collision = y;
    Box2D.Collision.b2ContactID = w;
    Box2D.Collision.b2ContactPoint = A;
    Box2D.Collision.b2Distance = U;
    Box2D.Collision.b2DistanceInput = p;
    Box2D.Collision.b2DistanceOutput = B;
    Box2D.Collision.b2DistanceProxy = Q;
    Box2D.Collision.b2DynamicTree = V;
    Box2D.Collision.b2DynamicTreeBroadPhase = M;
    Box2D.Collision.b2DynamicTreeNode = L;
    Box2D.Collision.b2DynamicTreePair = I;
    Box2D.Collision.b2Manifold = W;
    Box2D.Collision.b2ManifoldPoint = Y;
    Box2D.Collision.b2Point = k;
    Box2D.Collision.b2RayCastInput = z;
    Box2D.Collision.b2RayCastOutput = u;
    Box2D.Collision.b2Segment = D;
    Box2D.Collision.b2SeparationFunction = H;
    Box2D.Collision.b2Simplex = O;
    Box2D.Collision.b2SimplexCache = E;
    Box2D.Collision.b2SimplexVertex = R;
    Box2D.Collision.b2TimeOfImpact = N;
    Box2D.Collision.b2TOIInput = S;
    Box2D.Collision.b2WorldManifold = aa;
    Box2D.Collision.ClipVertex = Z;
    Box2D.Collision.Features = d;
    Box2D.Collision.Shapes.b2CircleShape = h;
    Box2D.Collision.Shapes.b2EdgeChainDef = l;
    Box2D.Collision.Shapes.b2EdgeShape = j;
    Box2D.Collision.Shapes.b2MassData = o;
    Box2D.Collision.Shapes.b2PolygonShape = q;
    Box2D.Collision.Shapes.b2Shape = n;
    Box2D.Common.b2internal = "Box2D.Common.b2internal";
    Box2D.Common.b2Color = a;
    Box2D.Common.b2Settings = c;
    Box2D.Common.Math.b2Mat22 = g;
    Box2D.Common.Math.b2Mat33 = b;
    Box2D.Common.Math.b2Math = e;
    Box2D.Common.Math.b2Sweep = f;
    Box2D.Common.Math.b2Transform = m;
    Box2D.Common.Math.b2Vec2 = r;
    Box2D.Common.Math.b2Vec3 = s;
    Box2D.Dynamics.b2Body = v;
    Box2D.Dynamics.b2BodyDef = t;
    Box2D.Dynamics.b2ContactFilter = x;
    Box2D.Dynamics.b2ContactImpulse = C;
    Box2D.Dynamics.b2ContactListener = J;
    Box2D.Dynamics.b2ContactManager = T;
    Box2D.Dynamics.b2DebugDraw = P;
    Box2D.Dynamics.b2DestructionListener = X;
    Box2D.Dynamics.b2FilterData = $;
    Box2D.Dynamics.b2Fixture = ba;
    Box2D.Dynamics.b2FixtureDef = ca;
    Box2D.Dynamics.b2Island = da;
    Box2D.Dynamics.b2TimeStep = Fa;
    Box2D.Dynamics.b2World = ea;
    Box2D.Dynamics.Contacts.b2CircleContact = Ga;
    Box2D.Dynamics.Contacts.b2Contact = fa;
    Box2D.Dynamics.Contacts.b2ContactConstraint = ga;
    Box2D.Dynamics.Contacts.b2ContactConstraintPoint = Ha;
    Box2D.Dynamics.Contacts.b2ContactEdge = Ia;
    Box2D.Dynamics.Contacts.b2ContactFactory = ha;
    Box2D.Dynamics.Contacts.b2ContactRegister = Ja;
    Box2D.Dynamics.Contacts.b2ContactResult = Ka;
    Box2D.Dynamics.Contacts.b2ContactSolver = ia;
    Box2D.Dynamics.Contacts.b2EdgeAndCircleContact = La;
    Box2D.Dynamics.Contacts.b2NullContact = ja;
    Box2D.Dynamics.Contacts.b2PolyAndCircleContact = Ma;
    Box2D.Dynamics.Contacts.b2PolyAndEdgeContact = Na;
    Box2D.Dynamics.Contacts.b2PolygonContact = Oa;
    Box2D.Dynamics.Contacts.b2PositionSolverManifold = ka;
    Box2D.Dynamics.Controllers.b2BuoyancyController = Pa;
    Box2D.Dynamics.Controllers.b2ConstantAccelController = Qa;
    Box2D.Dynamics.Controllers.b2ConstantForceController = Ra;
    Box2D.Dynamics.Controllers.b2Controller = Sa;
    Box2D.Dynamics.Controllers.b2ControllerEdge = Ta;
    Box2D.Dynamics.Controllers.b2GravityController = Ua;
    Box2D.Dynamics.Controllers.b2TensorDampingController = Va;
    Box2D.Dynamics.Joints.b2DistanceJoint = la;
    Box2D.Dynamics.Joints.b2DistanceJointDef = ma;
    Box2D.Dynamics.Joints.b2FrictionJoint = na;
    Box2D.Dynamics.Joints.b2FrictionJointDef = oa;
    Box2D.Dynamics.Joints.b2GearJoint = pa;
    Box2D.Dynamics.Joints.b2GearJointDef = qa;
    Box2D.Dynamics.Joints.b2Jacobian = Wa;
    Box2D.Dynamics.Joints.b2Joint = ra;
    Box2D.Dynamics.Joints.b2JointDef = sa;
    Box2D.Dynamics.Joints.b2JointEdge = Xa;
    Box2D.Dynamics.Joints.b2LineJoint = ta;
    Box2D.Dynamics.Joints.b2LineJointDef = ua;
    Box2D.Dynamics.Joints.b2MouseJoint = va;
    Box2D.Dynamics.Joints.b2MouseJointDef = wa;
    Box2D.Dynamics.Joints.b2PrismaticJoint = xa;
    Box2D.Dynamics.Joints.b2PrismaticJointDef = ya;
    Box2D.Dynamics.Joints.b2PulleyJoint = za;
    Box2D.Dynamics.Joints.b2PulleyJointDef = Aa;
    Box2D.Dynamics.Joints.b2RevoluteJoint = Ba;
    Box2D.Dynamics.Joints.b2RevoluteJointDef = Ca;
    Box2D.Dynamics.Joints.b2WeldJoint = Da;
    Box2D.Dynamics.Joints.b2WeldJointDef = Ea
})();
Box2D.postDefs = [];
(function () {
    var F = Box2D.Collision.Shapes.b2CircleShape,
        G = Box2D.Collision.Shapes.b2PolygonShape,
        K = Box2D.Collision.Shapes.b2Shape,
        y = Box2D.Common.b2Settings,
        w = Box2D.Common.Math.b2Math,
        A = Box2D.Common.Math.b2Sweep,
        U = Box2D.Common.Math.b2Transform,
        p = Box2D.Common.Math.b2Vec2,
        B = Box2D.Collision.b2AABB,
        Q = Box2D.Collision.b2Bound,
        V = Box2D.Collision.b2BoundValues,
        M = Box2D.Collision.b2Collision,
        L = Box2D.Collision.b2ContactID,
        I = Box2D.Collision.b2ContactPoint,
        W = Box2D.Collision.b2Distance,
        Y = Box2D.Collision.b2DistanceInput,
        k = Box2D.Collision.b2DistanceOutput,
        z = Box2D.Collision.b2DistanceProxy,
        u = Box2D.Collision.b2DynamicTree,
        D = Box2D.Collision.b2DynamicTreeBroadPhase,
        H = Box2D.Collision.b2DynamicTreeNode,
        O = Box2D.Collision.b2DynamicTreePair,
        E = Box2D.Collision.b2Manifold,
        R = Box2D.Collision.b2ManifoldPoint,
        N = Box2D.Collision.b2Point,
        S = Box2D.Collision.b2RayCastInput,
        aa = Box2D.Collision.b2RayCastOutput,
        Z = Box2D.Collision.b2Segment,
        d = Box2D.Collision.b2SeparationFunction,
        h = Box2D.Collision.b2Simplex,
        l = Box2D.Collision.b2SimplexCache,
        j = Box2D.Collision.b2SimplexVertex,
        o = Box2D.Collision.b2TimeOfImpact,
        q = Box2D.Collision.b2TOIInput,
        n = Box2D.Collision.b2WorldManifold,
        a = Box2D.Collision.ClipVertex,
        c = Box2D.Collision.Features,
        g = Box2D.Collision.IBroadPhase;
    B.b2AABB = function () {
        this.lowerBound = new p;
        this.upperBound = new p
    };
    B.prototype.IsValid = function () {
        var b = this.upperBound.y - this.lowerBound.y;
        return b = (b = this.upperBound.x - this.lowerBound.x >= 0 && b >= 0) && this.lowerBound.IsValid() && this.upperBound.IsValid()
    };
    B.prototype.GetCenter = function () {
        return new p((this.lowerBound.x + this.upperBound.x) / 2, (this.lowerBound.y + this.upperBound.y) / 2)
    };
    B.prototype.GetExtents = function () {
        return new p((this.upperBound.x - this.lowerBound.x) / 2, (this.upperBound.y - this.lowerBound.y) / 2)
    };
    B.prototype.Contains = function (b) {
        var e = true;
        return e = (e = (e = (e = e && this.lowerBound.x <= b.lowerBound.x) && this.lowerBound.y <= b.lowerBound.y) && b.upperBound.x <= this.upperBound.x) && b.upperBound.y <= this.upperBound.y
    };
    B.prototype.RayCast = function (b, e) {
        var f = -Number.MAX_VALUE,
            m = Number.MAX_VALUE,
            r = e.p1.x,
            s = e.p1.y,
            v = e.p2.x - e.p1.x,
            t = e.p2.y - e.p1.y,
            x = Math.abs(t),
            C = b.normal,
            J = 0,
            T = 0,
            P = J = 0;
        P = 0;
        if (Math.abs(v) < Number.MIN_VALUE) {
            if (r < this.lowerBound.x || this.upperBound.x < r) return false
        } else {
            J = 1 / v;
            T = (this.lowerBound.x - r) * J;
            J = (this.upperBound.x - r) * J;
            P = -1;
            if (T > J) {
                P = T;
                T = J;
                J = P;
                P = 1
            }
            if (T > f) {
                C.x = P;
                C.y = 0;
                f = T
            }
            m = Math.min(m, J);
            if (f > m) return false
        } if (x < Number.MIN_VALUE) {
            if (s < this.lowerBound.y || this.upperBound.y < s) return false
        } else {
            J = 1 / t;
            T = (this.lowerBound.y - s) * J;
            J = (this.upperBound.y - s) * J;
            P = -1;
            if (T > J) {
                P = T;
                T = J;
                J = P;
                P = 1
            }
            if (T > f) {
                C.y = P;
                C.x = 0;
                f = T
            }
            m = Math.min(m, J);
            if (f > m) return false
        }
        b.fraction = f;
        return true
    };
    B.prototype.TestOverlap = function (b) {
        var e = b.lowerBound.y - this.upperBound.y,
            f = this.lowerBound.y - b.upperBound.y;
        if (b.lowerBound.x - this.upperBound.x > 0 || e > 0) return false;
        if (this.lowerBound.x - b.upperBound.x > 0 || f > 0) return false;
        return true
    };
    B.Combine = function (b, e) {
        var f = new B;
        f.Combine(b, e);
        return f
    };
    B.prototype.Combine = function (b, e) {
        this.lowerBound.x = Math.min(b.lowerBound.x, e.lowerBound.x);
        this.lowerBound.y = Math.min(b.lowerBound.y, e.lowerBound.y);
        this.upperBound.x = Math.max(b.upperBound.x, e.upperBound.x);
        this.upperBound.y = Math.max(b.upperBound.y, e.upperBound.y)
    };
    Q.b2Bound = function () {};
    Q.prototype.IsLower = function () {
        return (this.value & 1) == 0
    };
    Q.prototype.IsUpper = function () {
        return (this.value & 1) == 1
    };
    Q.prototype.Swap = function (b) {
        var e = this.value,
            f = this.proxy,
            m = this.stabbingCount;
        this.value = b.value;
        this.proxy = b.proxy;
        this.stabbingCount = b.stabbingCount;
        b.value = e;
        b.proxy = f;
        b.stabbingCount = m
    };
    V.b2BoundValues = function () {};
    V.prototype.b2BoundValues = function () {
        this.lowerValues = new Vector_a2j_Number;
        this.lowerValues[0] = 0;
        this.lowerValues[1] = 0;
        this.upperValues = new Vector_a2j_Number;
        this.upperValues[0] = 0;
        this.upperValues[1] = 0
    };
    M.b2Collision = function () {};
    M.ClipSegmentToLine = function (b, e, f, m) {
        if (m === undefined) m = 0;
        var r, s = 0;
        r = e[0];
        var v = r.v;
        r = e[1];
        var t = r.v,
            x = f.x * v.x + f.y * v.y - m;
        r = f.x * t.x + f.y * t.y - m;
        x <= 0 && b[s++].Set(e[0]);
        r <= 0 && b[s++].Set(e[1]);
        if (x * r < 0) {
            f = x / (x - r);
            r = b[s];
            r = r.v;
            r.x = v.x + f * (t.x - v.x);
            r.y = v.y + f * (t.y - v.y);
            r = b[s];
            r.id = (x > 0 ? e[0] : e[1]).id;
            ++s
        }
        return s
    };
    M.EdgeSeparation = function (b, e, f, m, r) {
            if (f === undefined) f = 0;
            parseInt(b.m_vertexCount);
            var s = b.m_vertices;
            b = b.m_normals;
            var v = parseInt(m.m_vertexCount),
                t = m.m_vertices,
                x, C;
            x = e.R;
            C = b[f];
            b = x.col1.x * C.x + x.col2.x * C.y;
            m = x.col1.y * C.x + x.col2.y * C.y;
            x = r.R;
            var J = x.col1.x * b + x.col1.y * m;
            x = x.col2.x * b + x.col2.y * m;
            for (var T = 0, P = Number.MAX_VALUE, X = 0; X < v; ++X) {
                C = t[X];
                C = C.x * J + C.y * x;
                if (C < P) {
                    P = C;
                    T = X
                }
            }
            C = s[f];
            x = e.R;
            f = e.position.x + (x.col1.x * C.x + x.col2.x * C.y);
            e = e.position.y + (x.col1.y * C.x + x.col2.y * C.y);
            C = t[T];
            x = r.R;
            s = r.position.x + (x.col1.x * C.x + x.col2.x * C.y);
            r = r.position.y + (x.col1.y * C.x + x.col2.y * C.y);
            s -= f;
            r -= e;
            return s * b + r * m
    };
    M.FindMaxSeparation = function (b, e, f, m, r) {
        var s = parseInt(e.m_vertexCount),
            v = e.m_normals,
            t, x;
        x = r.R;
        t = m.m_centroid;
        var C = r.position.x + (x.col1.x * t.x + x.col2.x * t.y),
            J = r.position.y + (x.col1.y * t.x + x.col2.y * t.y);
        x = f.R;
        t = e.m_centroid;
        C -= f.position.x + (x.col1.x * t.x + x.col2.x * t.y);
        J -= f.position.y + (x.col1.y * t.x + x.col2.y * t.y);
        x = C * f.R.col1.x + J * f.R.col1.y;
        J = C * f.R.col2.x + J * f.R.col2.y;
        C = 0;
        for (var T = -Number.MAX_VALUE, P = 0; P < s; ++P) {
            t = v[P];
            t = t.x * x + t.y * J;
            if (t > T) {
                T = t;
                C = P
            }
        }
        v = M.EdgeSeparation(e, f, C, m, r);
        t = parseInt(C - 1 >= 0 ? C - 1 : s - 1);
        x = M.EdgeSeparation(e, f, t, m, r);
        J = parseInt(C + 1 < s ? C + 1 : 0);
        T = M.EdgeSeparation(e, f, J, m, r);
        var X = P = 0,
            $ = 0;
        if (x > v && x > T) {
            $ = -1;
            P = t;
            X = x
        } else if (T > v) {
            $ = 1;
            P = J;
            X = T
        } else {
            b[0] = C;
            return v
        }
        for (;;) {
            C = $ == -1 ? P - 1 >= 0 ? P - 1 : s - 1 : P + 1 < s ? P + 1 : 0;
            v = M.EdgeSeparation(e, f, C, m, r);
            if (v > X) {
                P = C;
                X = v
            } else break
        }
        b[0] = P;
        return X
    };
    M.FindIncidentEdge = function (b, e, f, m, r, s) {
        if (m === undefined) m = 0;
        parseInt(e.m_vertexCount);
        var v = e.m_normals,
            t = parseInt(r.m_vertexCount);
        e = r.m_vertices;
        r = r.m_normals;
        var x;
        x = f.R;
        f = v[m];
        v = x.col1.x * f.x + x.col2.x * f.y;
        var C = x.col1.y * f.x + x.col2.y * f.y;
        x = s.R;
        f = x.col1.x * v + x.col1.y * C;
        C = x.col2.x * v + x.col2.y * C;
        v = f;
        x = 0;
        for (var J = Number.MAX_VALUE, T = 0; T < t; ++T) {
            f = r[T];
            f = v * f.x + C * f.y;
            if (f < J) {
                J = f;
                x = T
            }
        }
        r = parseInt(x);
        v = parseInt(r + 1 < t ? r + 1 : 0);
        t = b[0];
        f = e[r];
        x = s.R;
        t.v.x = s.position.x + (x.col1.x * f.x + x.col2.x * f.y);
        t.v.y = s.position.y + (x.col1.y * f.x + x.col2.y * f.y);
        t.id.features.referenceEdge = m;
        t.id.features.incidentEdge = r;
        t.id.features.incidentVertex = 0;
        t = b[1];
        f = e[v];
        x = s.R;
        t.v.x = s.position.x + (x.col1.x * f.x + x.col2.x * f.y);
        t.v.y = s.position.y + (x.col1.y * f.x + x.col2.y * f.y);
        t.id.features.referenceEdge = m;
        t.id.features.incidentEdge = v;
        t.id.features.incidentVertex = 1
    };
    M.MakeClipPointVector = function () {
        var b = new Vector(2);
        b[0] = new a;
        b[1] = new a;
        return b
    };
    M.CollidePolygons = function (b, e, f, m, r) {
        var s;
        b.m_pointCount = 0;
        var v = e.m_radius + m.m_radius;
        s = 0;
        M.s_edgeAO[0] = s;
        var t = M.FindMaxSeparation(M.s_edgeAO, e, f, m, r);
        s = M.s_edgeAO[0];
        if (!(t > v)) {
            var x = 0;
            M.s_edgeBO[0] = x;
            var C = M.FindMaxSeparation(M.s_edgeBO,
                m, r, e, f);
            x = M.s_edgeBO[0];
            if (!(C > v)) {
                var J = 0,
                    T = 0;
                if (C > 0.98 * t + 0.0010) {
                    t = m;
                    m = e;
                    e = r;
                    f = f;
                    J = x;
                    b.m_type = E.e_faceB;
                    T = 1
                } else {
                    t = e;
                    m = m;
                    e = f;
                    f = r;
                    J = s;
                    b.m_type = E.e_faceA;
                    T = 0
                }
                s = M.s_incidentEdge;
                M.FindIncidentEdge(s, t, e, J, m, f);
                x = parseInt(t.m_vertexCount);
                r = t.m_vertices;
                t = r[J];
                var P;
                P = J + 1 < x ? r[parseInt(J + 1)] : r[0];
                J = M.s_localTangent;
                J.Set(P.x - t.x, P.y - t.y);
                J.Normalize();
                r = M.s_localNormal;
                r.x = J.y;
                r.y = -J.x;
                m = M.s_planePoint;
                m.Set(0.5 * (t.x + P.x), 0.5 * (t.y + P.y));
                C = M.s_tangent;
                x = e.R;
                C.x = x.col1.x * J.x + x.col2.x * J.y;
                C.y = x.col1.y * J.x + x.col2.y * J.y;
                var X = M.s_tangent2;
                X.x = -C.x;
                X.y = -C.y;
                J = M.s_normal;
                J.x = C.y;
                J.y = -C.x;
                var $ = M.s_v11,
                    ba = M.s_v12;
                $.x = e.position.x + (x.col1.x * t.x + x.col2.x * t.y);
                $.y = e.position.y + (x.col1.y * t.x + x.col2.y * t.y);
                ba.x = e.position.x + (x.col1.x * P.x + x.col2.x * P.y);
                ba.y = e.position.y + (x.col1.y * P.x + x.col2.y * P.y);
                e = J.x * $.x + J.y * $.y;
                x = C.x * ba.x + C.y * ba.y + v;
                P = M.s_clipPoints1;
                t = M.s_clipPoints2;
                ba = 0;
                ba = M.ClipSegmentToLine(P, s, X, -C.x * $.x - C.y * $.y + v);
                if (!(ba < 2)) {
                    ba = M.ClipSegmentToLine(t, P, C, x);
                    if (!(ba < 2)) {
                        b.m_localPlaneNormal.SetV(r);
                        b.m_localPoint.SetV(m);
                        for (m = r = 0; m < y.b2_maxManifoldPoints; ++m) {
                            s = t[m];
                            if (J.x * s.v.x + J.y * s.v.y - e <= v) {
                                C = b.m_points[r];
                                x = f.R;
                                X = s.v.x - f.position.x;
                                $ = s.v.y - f.position.y;
                                C.m_localPoint.x = X * x.col1.x + $ * x.col1.y;
                                C.m_localPoint.y = X * x.col2.x + $ * x.col2.y;
                                C.m_id.Set(s.id);
                                C.m_id.features.flip = T;
                                ++r
                            }
                        }
                        b.m_pointCount = r
                    }
                }
            }
        }
    };
    M.CollideCircles = function (b, e, f, m, r) {
        b.m_pointCount = 0;
        var s, v;
        s = f.R;
        v = e.m_p;
        var t = f.position.x + (s.col1.x * v.x + s.col2.x * v.y);
        f = f.position.y + (s.col1.y * v.x + s.col2.y * v.y);
        s = r.R;
        v = m.m_p;
        t = r.position.x + (s.col1.x * v.x + s.col2.x * v.y) - t;
        r = r.position.y + (s.col1.y * v.x + s.col2.y * v.y) - f;
        s = e.m_radius + m.m_radius;
        if (!(t * t + r * r > s * s)) {
            b.m_type = E.e_circles;
            b.m_localPoint.SetV(e.m_p);
            b.m_localPlaneNormal.SetZero();
            b.m_pointCount = 1;
            b.m_points[0].m_localPoint.SetV(m.m_p);
            b.m_points[0].m_id.key = 0
        }
    };
    M.CollidePolygonAndCircle = function (b, e, f, m, r) {
        var s = b.m_pointCount = 0,
            v = 0,
            t, x;
        x = r.R;
        t = m.m_p;
        var C = r.position.y + (x.col1.y * t.x + x.col2.y * t.y);
        s = r.position.x + (x.col1.x * t.x + x.col2.x * t.y) - f.position.x;
        v = C - f.position.y;
        x = f.R;
        f = s * x.col1.x + v * x.col1.y;
        x = s * x.col2.x + v * x.col2.y;
        var J = 0;
        C = -Number.MAX_VALUE;
        r = e.m_radius + m.m_radius;
        var T = parseInt(e.m_vertexCount),
            P = e.m_vertices;
        e = e.m_normals;
        for (var X = 0; X < T; ++X) {
            t = P[X];
            s = f - t.x;
            v = x - t.y;
            t = e[X];
            s = t.x * s + t.y * v;
            if (s > r) return;
            if (s > C) {
                C = s;
                J = X
            }
        }
        s = parseInt(J);
        v = parseInt(s + 1 < T ? s + 1 : 0);
        t = P[s];
        P = P[v];
        if (C < Number.MIN_VALUE) {
            b.m_pointCount = 1;
            b.m_type = E.e_faceA;
            b.m_localPlaneNormal.SetV(e[J]);
            b.m_localPoint.x = 0.5 * (t.x + P.x);
            b.m_localPoint.y = 0.5 * (t.y + P.y)
        } else {
            C = (f - P.x) * (t.x - P.x) + (x - P.y) * (t.y - P.y);
            if ((f - t.x) * (P.x - t.x) + (x - t.y) * (P.y - t.y) <= 0) {
                if ((f - t.x) * (f - t.x) + (x - t.y) * (x - t.y) > r * r) return;
                b.m_pointCount = 1;
                b.m_type = E.e_faceA;
                b.m_localPlaneNormal.x = f - t.x;
                b.m_localPlaneNormal.y = x - t.y;
                b.m_localPlaneNormal.Normalize();
                b.m_localPoint.SetV(t)
            } else if (C <= 0) {
                if ((f - P.x) * (f - P.x) + (x - P.y) * (x - P.y) > r * r) return;
                b.m_pointCount = 1;
                b.m_type = E.e_faceA;
                b.m_localPlaneNormal.x = f - P.x;
                b.m_localPlaneNormal.y = x - P.y;
                b.m_localPlaneNormal.Normalize();
                b.m_localPoint.SetV(P)
            } else {
                J = 0.5 * (t.x + P.x);
                t = 0.5 * (t.y + P.y);
                C = (f - J) * e[s].x + (x - t) * e[s].y;
                if (C > r) return;
                b.m_pointCount = 1;
                b.m_type = E.e_faceA;
                b.m_localPlaneNormal.x = e[s].x;
                b.m_localPlaneNormal.y = e[s].y;
                b.m_localPlaneNormal.Normalize();
                b.m_localPoint.Set(J, t)
            }
        }
        b.m_points[0].m_localPoint.SetV(m.m_p);
        b.m_points[0].m_id.key = 0
    };
    M.TestOverlap = function (b, e) {
        var f = e.lowerBound,
            m = b.upperBound,
            r = f.x - m.x,
            s = f.y - m.y;
        f = b.lowerBound;
        m = e.upperBound;
        var v = f.y - m.y;
        if (r > 0 || s > 0) return false;
        if (f.x - m.x > 0 || v > 0) return false;
        return true
    };
    Box2D.postDefs.push(function () {
        Box2D.Collision.b2Collision.s_incidentEdge = M.MakeClipPointVector();
        Box2D.Collision.b2Collision.s_clipPoints1 = M.MakeClipPointVector();
        Box2D.Collision.b2Collision.s_clipPoints2 = M.MakeClipPointVector();
        Box2D.Collision.b2Collision.s_edgeAO = new Vector_a2j_Number(1);
        Box2D.Collision.b2Collision.s_edgeBO = new Vector_a2j_Number(1);
        Box2D.Collision.b2Collision.s_localTangent = new p;
        Box2D.Collision.b2Collision.s_localNormal = new p;
        Box2D.Collision.b2Collision.s_planePoint = new p;
        Box2D.Collision.b2Collision.s_normal = new p;
        Box2D.Collision.b2Collision.s_tangent = new p;
        Box2D.Collision.b2Collision.s_tangent2 = new p;
        Box2D.Collision.b2Collision.s_v11 = new p;
        Box2D.Collision.b2Collision.s_v12 = new p;
        Box2D.Collision.b2Collision.b2CollidePolyTempVec = new p;
        Box2D.Collision.b2Collision.b2_nullFeature = 255
    });
    L.b2ContactID = function () {
        this.features = new c
    };
    L.prototype.b2ContactID = function () {
        this.features._m_id = this
    };
    L.prototype.Set = function (b) {
        this.key = b._key
    };
    L.prototype.Copy = function () {
        var b = new L;
        b.key = this.key;
        return b
    };
    Object.defineProperty(L.prototype, "key", {
        enumerable: false,
        configurable: true,
        get: function () {
            return this._key
        }
    });
    Object.defineProperty(L.prototype, "key", {
        enumerable: false,
        configurable: true,
        set: function (b) {
            if (b === undefined) b = 0;
            this._key = b;
            this.features._referenceEdge = this._key & 255;
            this.features._incidentEdge = (this._key & 65280) >> 8 & 255;
            this.features._incidentVertex = (this._key & 16711680) >> 16 & 255;
            this.features._flip = (this._key & 4278190080) >> 24 & 255
        }
    });
    I.b2ContactPoint = function () {
        this.position = new p;
        this.velocity = new p;
        this.normal = new p;
        this.id = new L
    };
    W.b2Distance = function () {};
    W.Distance = function (b, e, f) {
        ++W.b2_gjkCalls;
        var m = f.proxyA,
            r = f.proxyB,
            s = f.transformA,
            v = f.transformB,
            t = W.s_simplex;
        t.ReadCache(e, m, s, r, v);
        var x = t.m_vertices,
            C = W.s_saveA,
            J = W.s_saveB,
            T = 0;
        t.GetClosestPoint().LengthSquared();
        for (var P = 0, X, $ = 0; $ < 20;) {
            T = t.m_count;
            for (P = 0; P < T; P++) {
                C[P] = x[P].indexA;
                J[P] = x[P].indexB
            }
            switch (t.m_count) {
            case 1:
                break;
            case 2:
                t.Solve2();
                break;
            case 3:
                t.Solve3();
                break;
            default:
                y.b2Assert(false)
            }
            if (t.m_count == 3) break;
            X = t.GetClosestPoint();
            X.LengthSquared();
            P = t.GetSearchDirection();
            if (P.LengthSquared() < Number.MIN_VALUE * Number.MIN_VALUE) break;
            X = x[t.m_count];
            X.indexA = m.GetSupport(w.MulTMV(s.R, P.GetNegative()));
            X.wA = w.MulX(s, m.GetVertex(X.indexA));
            X.indexB = r.GetSupport(w.MulTMV(v.R, P));
            X.wB = w.MulX(v, r.GetVertex(X.indexB));
            X.w = w.SubtractVV(X.wB, X.wA);
            ++$;
            ++W.b2_gjkIters;
            var ba = false;
            for (P = 0; P < T; P++)
                if (X.indexA == C[P] && X.indexB == J[P]) {
                    ba = true;
                    break
                }
            if (ba) break;
            ++t.m_count
        }
        W.b2_gjkMaxIters = w.Max(W.b2_gjkMaxIters, $);
        t.GetWitnessPoints(b.pointA, b.pointB);
        b.distance = w.SubtractVV(b.pointA,
            b.pointB).Length();
        b.iterations = $;
        t.WriteCache(e);
        if (f.useRadii) {
            e = m.m_radius;
            r = r.m_radius;
            if (b.distance > e + r && b.distance > Number.MIN_VALUE) {
                b.distance -= e + r;
                f = w.SubtractVV(b.pointB, b.pointA);
                f.Normalize();
                b.pointA.x += e * f.x;
                b.pointA.y += e * f.y;
                b.pointB.x -= r * f.x;
                b.pointB.y -= r * f.y
            } else {
                X = new p;
                X.x = 0.5 * (b.pointA.x + b.pointB.x);
                X.y = 0.5 * (b.pointA.y + b.pointB.y);
                b.pointA.x = b.pointB.x = X.x;
                b.pointA.y = b.pointB.y = X.y;
                b.distance = 0
            }
        }
    };
    Box2D.postDefs.push(function () {
        Box2D.Collision.b2Distance.s_simplex = new h;
        Box2D.Collision.b2Distance.s_saveA = new Vector_a2j_Number(3);
        Box2D.Collision.b2Distance.s_saveB = new Vector_a2j_Number(3)
    });
    Y.b2DistanceInput = function () {};
    k.b2DistanceOutput = function () {
        this.pointA = new p;
        this.pointB = new p
    };
    z.b2DistanceProxy = function () {};
    z.prototype.Set = function (b) {
        switch (b.GetType()) {
        case K.e_circleShape:
            b = b instanceof F ? b : null;
            this.m_vertices = new Vector(1, true);
            this.m_vertices[0] = b.m_p;
            this.m_count = 1;
            this.m_radius = b.m_radius;
            break;
        case K.e_polygonShape:
            b = b instanceof G ? b : null;
            this.m_vertices = b.m_vertices;
            this.m_count = b.m_vertexCount;
            this.m_radius = b.m_radius;
            break;
        default:
            y.b2Assert(false)
        }
    };
    z.prototype.GetSupport = function (b) {
        for (var e = 0, f = this.m_vertices[0].x * b.x + this.m_vertices[0].y * b.y, m = 1; m < this.m_count; ++m) {
            var r = this.m_vertices[m].x * b.x + this.m_vertices[m].y * b.y;
            if (r > f) {
                e = m;
                f = r
            }
        }
        return e
    };
    z.prototype.GetSupportVertex = function (b) {
        for (var e = 0, f = this.m_vertices[0].x * b.x + this.m_vertices[0].y * b.y, m = 1; m < this.m_count; ++m) {
            var r = this.m_vertices[m].x * b.x + this.m_vertices[m].y * b.y;
            if (r > f) {
                e = m;
                f = r
            }
        }
        return this.m_vertices[e]
    };
    z.prototype.GetVertexCount = function () {
        return this.m_count
    };
    z.prototype.GetVertex = function (b) {
        if (b === undefined) b = 0;
        y.b2Assert(0 <= b && b < this.m_count);
        return this.m_vertices[b]
    };
    u.b2DynamicTree = function () {};
    u.prototype.b2DynamicTree = function () {
        this.m_freeList = this.m_root = null;
        this.m_insertionCount = this.m_path = 0
    };
    u.prototype.CreateProxy = function (b, e) {
        var f = this.AllocateNode(),
            m = y.b2_aabbExtension,
            r = y.b2_aabbExtension;
        f.aabb.lowerBound.x = b.lowerBound.x - m;
        f.aabb.lowerBound.y = b.lowerBound.y - r;
        f.aabb.upperBound.x = b.upperBound.x + m;
        f.aabb.upperBound.y = b.upperBound.y + r;
        f.userData = e;
        this.InsertLeaf(f);
        return f
    };
    u.prototype.DestroyProxy = function (b) {
        this.RemoveLeaf(b);
        this.FreeNode(b)
    };
    u.prototype.MoveProxy = function (b, e, f) {
        y.b2Assert(b.IsLeaf());
        if (b.aabb.Contains(e)) return false;
        this.RemoveLeaf(b);
        var m = y.b2_aabbExtension + y.b2_aabbMultiplier * (f.x > 0 ? f.x : -f.x);
        f = y.b2_aabbExtension + y.b2_aabbMultiplier * (f.y > 0 ? f.y : -f.y);
        b.aabb.lowerBound.x = e.lowerBound.x - m;
        b.aabb.lowerBound.y = e.lowerBound.y - f;
        b.aabb.upperBound.x = e.upperBound.x + m;
        b.aabb.upperBound.y = e.upperBound.y + f;
        this.InsertLeaf(b);
        return true
    };
    u.prototype.Rebalance = function (b) {
        if (b === undefined) b = 0;
        if (this.m_root != null) for (var e = 0; e < b; e++) {
                for (var f = this.m_root, m = 0; f.IsLeaf() == false;) {
                    f = this.m_path >> m & 1 ? f.child2 : f.child1;
                    m = m + 1 & 31
                }++this.m_path;
                this.RemoveLeaf(f);
                this.InsertLeaf(f)
            }
    };
    u.prototype.GetFatAABB = function (b) {
        return b.aabb
    };
    u.prototype.GetUserData = function (b) {
        return b.userData
    };
    u.prototype.Query = function (b, e) {
        if (this.m_root != null) {
            var f = new Vector,
                m = 0;
            for (f[m++] = this.m_root; m > 0;) {
                var r = f[--m];
                if (r.aabb.TestOverlap(e))
                    if (r.IsLeaf()) {
                        if (!b(r)) break
                    } else {
                        f[m++] = r.child1;
                        f[m++] = r.child2
                    }
            }
        }
    };
    u.prototype.RayCast = function (b, e) {
        if (this.m_root != null) {
            var f = e.p1,
                m = e.p2,
                r = w.SubtractVV(f, m);
            r.Normalize();
            r = w.CrossFV(1, r);
            var s = w.AbsV(r),
                v = e.maxFraction,
                t = new B,
                x = 0,
                C = 0;
            x = f.x + v * (m.x - f.x);
            C = f.y + v * (m.y - f.y);
            t.lowerBound.x = Math.min(f.x, x);
            t.lowerBound.y = Math.min(f.y, C);
            t.upperBound.x = Math.max(f.x, x);
            t.upperBound.y = Math.max(f.y, C);
            var J = new Vector,
                T = 0;
            for (J[T++] = this.m_root; T > 0;) {
                v = J[--T];
                if (v.aabb.TestOverlap(t) != false) {
                    x = v.aabb.GetCenter();
                    C = v.aabb.GetExtents();
                    if (!(Math.abs(r.x * (f.x - x.x) + r.y * (f.y - x.y)) - s.x * C.x - s.y * C.y > 0))
                        if (v.IsLeaf()) {
                            x = new S;
                            x.p1 = e.p1;
                            x.p2 = e.p2;
                            x.maxFraction = e.maxFraction;
                            v = b(x, v);
                            if (v == 0) break;
                            if (v > 0) {
                                x = f.x + v * (m.x - f.x);
                                C = f.y + v * (m.y - f.y);
                                t.lowerBound.x = Math.min(f.x, x);
                                t.lowerBound.y = Math.min(f.y, C);
                                t.upperBound.x = Math.max(f.x, x);
                                t.upperBound.y = Math.max(f.y, C)
                            }
                        } else {
                            J[T++] = v.child1;
                            J[T++] = v.child2
                        }
                }
            }
        }
    };
    u.prototype.AllocateNode = function () {
        if (this.m_freeList) {
            var b = this.m_freeList;
            this.m_freeList = b.parent;
            b.parent = null;
            b.child1 = null;
            b.child2 = null;
            return b
        }
        return new H
    };
    u.prototype.FreeNode = function (b) {
        b.parent = this.m_freeList;
        this.m_freeList = b
    };
    u.prototype.InsertLeaf = function (b) {
        ++this.m_insertionCount;
        if (this.m_root == null) {
            this.m_root = b;
            this.m_root.parent = null
        } else {
            var e = b.aabb.GetCenter(),
                f = this.m_root;
            if (f.IsLeaf() == false) {
                do {
                    var m = f.child1;
                    f = f.child2;
                    f = Math.abs((m.aabb.lowerBound.x + m.aabb.upperBound.x) / 2 - e.x) + Math.abs((m.aabb.lowerBound.y + m.aabb.upperBound.y) / 2 - e.y) < Math.abs((f.aabb.lowerBound.x + f.aabb.upperBound.x) / 2 - e.x) + Math.abs((f.aabb.lowerBound.y + f.aabb.upperBound.y) / 2 - e.y) ? m : f
                } while (f.IsLeaf() == false)
            }
            e = f.parent;
            m = this.AllocateNode();
            m.parent = e;
            m.userData = null;
            m.aabb.Combine(b.aabb, f.aabb);
            if (e) {
                if (f.parent.child1 == f) e.child1 = m;
                else e.child2 = m;
                m.child1 = f;
                m.child2 = b;
                f.parent = m;
                b.parent = m;
                do {
                    if (e.aabb.Contains(m.aabb)) break;
                    e.aabb.Combine(e.child1.aabb, e.child2.aabb);
                    m = e;
                    e = e.parent
                } while (e)
            } else {
                m.child1 = f;
                m.child2 = b;
                f.parent = m;
                this.m_root = b.parent = m
            }
        }
    };
    u.prototype.RemoveLeaf = function (b) {
        if (b == this.m_root) this.m_root = null;
        else {
            var e = b.parent,
                f = e.parent;
            b = e.child1 == b ? e.child2 : e.child1;
            if (f) {
                if (f.child1 == e) f.child1 = b;
                else f.child2 = b;
                b.parent = f;
                for (this.FreeNode(e); f;) {
                    e = f.aabb;
                    f.aabb = B.Combine(f.child1.aabb, f.child2.aabb);
                    if (e.Contains(f.aabb)) break;
                    f = f.parent
                }
            } else {
                this.m_root = b;
                b.parent = null;
                this.FreeNode(e)
            }
        }
    };
    D.b2DynamicTreeBroadPhase = function () {
        this.m_tree = new u;
        this.m_moveBuffer = new Vector;
        this.m_pairBuffer = new Vector;
        this.m_pairCount = 0
    };
    D.prototype.CreateProxy = function (b, e) {
        var f = this.m_tree.CreateProxy(b, e);
        ++this.m_proxyCount;
        this.BufferMove(f);
        return f
    };
    D.prototype.DestroyProxy = function (b) {
        this.UnBufferMove(b);
        --this.m_proxyCount;
        this.m_tree.DestroyProxy(b)
    };
    D.prototype.MoveProxy = function (b, e, f) {
        this.m_tree.MoveProxy(b, e, f) && this.BufferMove(b)
    };
    D.prototype.TestOverlap = function (b, e) {
        var f = this.m_tree.GetFatAABB(b),
            m = this.m_tree.GetFatAABB(e);
        return f.TestOverlap(m)
    };
    D.prototype.GetUserData = function (b) {
        return this.m_tree.GetUserData(b)
    };
    D.prototype.GetFatAABB = function (b) {
        return this.m_tree.GetFatAABB(b)
    };
    D.prototype.GetProxyCount = function () {
        return this.m_proxyCount
    };
    D.prototype.UpdatePairs = function (b) {
        var e = this;
        var f = e.m_pairCount = 0,
            m;
        for (f = 0; f < e.m_moveBuffer.length; ++f) {
            m = e.m_moveBuffer[f];
            var r = e.m_tree.GetFatAABB(m);
            e.m_tree.Query(function (t) {
                    if (t == m) return true;
                    if (e.m_pairCount == e.m_pairBuffer.length) e.m_pairBuffer[e.m_pairCount] = new O;
                    var x = e.m_pairBuffer[e.m_pairCount];
                    x.proxyA = t < m ? t : m;
                    x.proxyB = t >= m ? t : m;
                    ++e.m_pairCount;
                    return true
                },
                r)
        }
        for (f = e.m_moveBuffer.length = 0; f < e.m_pairCount;) {
            r = e.m_pairBuffer[f];
            var s = e.m_tree.GetUserData(r.proxyA),
                v = e.m_tree.GetUserData(r.proxyB);
            b(s, v);
            for (++f; f < e.m_pairCount;) {
                s = e.m_pairBuffer[f];
                if (s.proxyA != r.proxyA || s.proxyB != r.proxyB) break;
                ++f
            }
        }
    };
    D.prototype.Query = function (b, e) {
        this.m_tree.Query(b, e)
    };
    D.prototype.RayCast = function (b, e) {
        this.m_tree.RayCast(b, e)
    };
    D.prototype.Validate = function () {};
    D.prototype.Rebalance = function (b) {
        if (b === undefined) b = 0;
        this.m_tree.Rebalance(b)
    };
    D.prototype.BufferMove = function (b) {
            this.m_moveBuffer[this.m_moveBuffer.length] = b
    };
    D.prototype.UnBufferMove = function (b) {
        this.m_moveBuffer.splice(parseInt(this.m_moveBuffer.indexOf(b)), 1)
    };
    D.prototype.ComparePairs = function () {
        return 0
    };
    D.__implements = {};
    D.__implements[g] = true;
    H.b2DynamicTreeNode = function () {
        this.aabb = new B
    };
    H.prototype.IsLeaf = function () {
        return this.child1 == null
    };
    O.b2DynamicTreePair = function () {};
    E.b2Manifold = function () {
        this.m_pointCount = 0
    };
    E.prototype.b2Manifold = function () {
        this.m_points = new Vector(y.b2_maxManifoldPoints);
        for (var b = 0; b < y.b2_maxManifoldPoints; b++) this.m_points[b] = new R;
        this.m_localPlaneNormal = new p;
        this.m_localPoint = new p
    };
    E.prototype.Reset = function () {
        for (var b = 0; b < y.b2_maxManifoldPoints; b++)(this.m_points[b] instanceof R ? this.m_points[b] : null).Reset();
        this.m_localPlaneNormal.SetZero();
        this.m_localPoint.SetZero();
        this.m_pointCount = this.m_type = 0
    };
    E.prototype.Set = function (b) {
        this.m_pointCount = b.m_pointCount;
        for (var e = 0; e < y.b2_maxManifoldPoints; e++)(this.m_points[e] instanceof R ? this.m_points[e] : null).Set(b.m_points[e]);
        this.m_localPlaneNormal.SetV(b.m_localPlaneNormal);
        this.m_localPoint.SetV(b.m_localPoint);
        this.m_type = b.m_type
    };
    E.prototype.Copy = function () {
        var b = new E;
        b.Set(this);
        return b
    };
    Box2D.postDefs.push(function () {
        Box2D.Collision.b2Manifold.e_circles = 1;
        Box2D.Collision.b2Manifold.e_faceA = 2;
        Box2D.Collision.b2Manifold.e_faceB = 4
    });
    R.b2ManifoldPoint = function () {
        this.m_localPoint = new p;
        this.m_id = new L
    };
    R.prototype.b2ManifoldPoint = function () {
        this.Reset()
    };
    R.prototype.Reset = function () {
        this.m_localPoint.SetZero();
        this.m_tangentImpulse = this.m_normalImpulse = 0;
        this.m_id.key = 0
    };
    R.prototype.Set = function (b) {
        this.m_localPoint.SetV(b.m_localPoint);
        this.m_normalImpulse = b.m_normalImpulse;
        this.m_tangentImpulse = b.m_tangentImpulse;
        this.m_id.Set(b.m_id)
    };
    N.b2Point = function () {
        this.p = new p
    };
    N.prototype.Support = function () {
        return this.p
    };
    N.prototype.GetFirstVertex = function () {
        return this.p
    };
    S.b2RayCastInput = function () {
        this.p1 = new p;
        this.p2 = new p
    };
    S.prototype.b2RayCastInput = function (b, e, f) {
        if (b === undefined) b = null;
        if (e === undefined) e = null;
        if (f === undefined) f = 1;
        b && this.p1.SetV(b);
        e && this.p2.SetV(e);
        this.maxFraction = f
    };
    aa.b2RayCastOutput = function () {
        this.normal = new p
    };
    Z.b2Segment = function () {
        this.p1 = new p;
        this.p2 = new p
    };
    Z.prototype.TestSegment = function (b, e, f, m) {
        if (m === undefined) m = 0;
        var r = f.p1,
            s = f.p2.x - r.x,
            v = f.p2.y - r.y;
        f = this.p2.y - this.p1.y;
        var t = -(this.p2.x - this.p1.x),
            x = 100 * Number.MIN_VALUE,
            C = -(s * f + v * t);
        if (C > x) {
            var J = r.x - this.p1.x,
                T = r.y - this.p1.y;
            r = J * f + T * t;
            if (0 <= r && r <= m * C) {
                m = -s * T + v * J;
                if (-x * C <= m && m <= C * (1 + x)) {
                    r /= C;
                    m = Math.sqrt(f * f + t * t);
                    f /= m;
                    t /= m;
                    b[0] = r;
                    e.Set(f,
                        t);
                    return true
                }
            }
        }
        return false
    };
    Z.prototype.Extend = function (b) {
        this.ExtendForward(b);
        this.ExtendBackward(b)
    };
    Z.prototype.ExtendForward = function (b) {
        var e = this.p2.x - this.p1.x,
            f = this.p2.y - this.p1.y;
        b = Math.min(e > 0 ? (b.upperBound.x - this.p1.x) / e : e < 0 ? (b.lowerBound.x - this.p1.x) / e : Number.POSITIVE_INFINITY, f > 0 ? (b.upperBound.y - this.p1.y) / f : f < 0 ? (b.lowerBound.y - this.p1.y) / f : Number.POSITIVE_INFINITY);
        this.p2.x = this.p1.x + e * b;
        this.p2.y = this.p1.y + f * b
    };
    Z.prototype.ExtendBackward = function (b) {
        var e = -this.p2.x + this.p1.x,
            f = -this.p2.y + this.p1.y;
        b = Math.min(e > 0 ? (b.upperBound.x - this.p2.x) / e : e < 0 ? (b.lowerBound.x - this.p2.x) / e : Number.POSITIVE_INFINITY, f > 0 ? (b.upperBound.y - this.p2.y) / f : f < 0 ? (b.lowerBound.y - this.p2.y) / f : Number.POSITIVE_INFINITY);
        this.p1.x = this.p2.x + e * b;
        this.p1.y = this.p2.y + f * b
    };
    d.b2SeparationFunction = function () {
        this.m_localPoint = new p;
        this.m_axis = new p
    };
    d.prototype.Initialize = function (b, e, f, m, r) {
        this.m_proxyA = e;
        this.m_proxyB = m;
        var s = parseInt(b.count);
        y.b2Assert(0 < s && s < 3);
        var v, t, x, C, J = C = x = m = e = 0,
            T = 0;
        J = 0;
        if (s == 1) {
            this.m_type = d.e_points;
            v = this.m_proxyA.GetVertex(b.indexA[0]);
            t = this.m_proxyB.GetVertex(b.indexB[0]);
            s = v;
            b = f.R;
            e = f.position.x + (b.col1.x * s.x + b.col2.x * s.y);
            m = f.position.y + (b.col1.y * s.x + b.col2.y * s.y);
            s = t;
            b = r.R;
            x = r.position.x + (b.col1.x * s.x + b.col2.x * s.y);
            C = r.position.y + (b.col1.y * s.x + b.col2.y * s.y);
            this.m_axis.x = x - e;
            this.m_axis.y = C - m;
            this.m_axis.Normalize()
        } else {
            if (b.indexB[0] == b.indexB[1]) {
                this.m_type = d.e_faceA;
                e = this.m_proxyA.GetVertex(b.indexA[0]);
                m = this.m_proxyA.GetVertex(b.indexA[1]);
                t = this.m_proxyB.GetVertex(b.indexB[0]);
                this.m_localPoint.x = 0.5 * (e.x + m.x);
                this.m_localPoint.y = 0.5 * (e.y + m.y);
                this.m_axis = w.CrossVF(w.SubtractVV(m, e), 1);
                this.m_axis.Normalize();
                s = this.m_axis;
                b = f.R;
                J = b.col1.x * s.x + b.col2.x * s.y;
                T = b.col1.y * s.x + b.col2.y * s.y;
                s = this.m_localPoint;
                b = f.R;
                e = f.position.x + (b.col1.x * s.x + b.col2.x * s.y);
                m = f.position.y + (b.col1.y * s.x + b.col2.y * s.y);
                s = t;
                b = r.R;
                x = r.position.x + (b.col1.x * s.x + b.col2.x * s.y);
                C = r.position.y + (b.col1.y * s.x + b.col2.y * s.y);
                J = (x - e) * J + (C - m) * T
            } else if (b.indexA[0] == b.indexA[0]) {
                this.m_type = d.e_faceB;
                x = this.m_proxyB.GetVertex(b.indexB[0]);
                C = this.m_proxyB.GetVertex(b.indexB[1]);
                v = this.m_proxyA.GetVertex(b.indexA[0]);
                this.m_localPoint.x = 0.5 * (x.x + C.x);
                this.m_localPoint.y = 0.5 * (x.y + C.y);
                this.m_axis = w.CrossVF(w.SubtractVV(C, x), 1);
                this.m_axis.Normalize();
                s = this.m_axis;
                b = r.R;
                J = b.col1.x * s.x + b.col2.x * s.y;
                T = b.col1.y * s.x + b.col2.y * s.y;
                s = this.m_localPoint;
                b = r.R;
                x = r.position.x + (b.col1.x * s.x + b.col2.x * s.y);
                C = r.position.y + (b.col1.y * s.x + b.col2.y * s.y);
                s = v;
                b = f.R;
                e = f.position.x + (b.col1.x * s.x + b.col2.x * s.y);
                m = f.position.y + (b.col1.y * s.x + b.col2.y * s.y);
                J = (e - x) * J + (m - C) * T
            } else {
                e = this.m_proxyA.GetVertex(b.indexA[0]);
                m = this.m_proxyA.GetVertex(b.indexA[1]);
                x = this.m_proxyB.GetVertex(b.indexB[0]);
                C = this.m_proxyB.GetVertex(b.indexB[1]);
                w.MulX(f, v);
                v = w.MulMV(f.R, w.SubtractVV(m, e));
                w.MulX(r, t);
                J = w.MulMV(r.R, w.SubtractVV(C, x));
                r = v.x * v.x + v.y * v.y;
                t = J.x * J.x + J.y * J.y;
                b = w.SubtractVV(J, v);
                f = v.x * b.x + v.y * b.y;
                b = J.x * b.x + J.y * b.y;
                v = v.x * J.x + v.y * J.y;
                T = r * t - v * v;
                J = 0;
                if (T != 0) J = w.Clamp((v * b - f * t) / T, 0, 1);
                if ((v * J + b) / t < 0) J = w.Clamp((v - f) / r, 0, 1);
                v = new p;
                v.x = e.x + J * (m.x - e.x);
                v.y = e.y + J * (m.y - e.y);
                t = new p;
                t.x = x.x + J * (C.x - x.x);
                t.y = x.y + J * (C.y - x.y);
                if (J == 0 || J == 1) {
                    this.m_type = d.e_faceB;
                    this.m_axis = w.CrossVF(w.SubtractVV(C, x), 1);
                    this.m_axis.Normalize();
                    this.m_localPoint = t
                } else {
                    this.m_type = d.e_faceA;
                    this.m_axis = w.CrossVF(w.SubtractVV(m, e), 1);
                    this.m_localPoint = v
                }
            }
            J < 0 && this.m_axis.NegativeSelf()
        }
    };
    d.prototype.Evaluate = function (b, e) {
        var f, m, r = 0;
        switch (this.m_type) {
        case d.e_points:
            f = w.MulTMV(b.R, this.m_axis);
            m = w.MulTMV(e.R, this.m_axis.GetNegative());
            f = this.m_proxyA.GetSupportVertex(f);
            m = this.m_proxyB.GetSupportVertex(m);
            f = w.MulX(b, f);
            m = w.MulX(e, m);
            return r = (m.x - f.x) * this.m_axis.x + (m.y - f.y) * this.m_axis.y;
        case d.e_faceA:
            r = w.MulMV(b.R, this.m_axis);
            f = w.MulX(b, this.m_localPoint);
            m = w.MulTMV(e.R, r.GetNegative());
            m = this.m_proxyB.GetSupportVertex(m);
            m = w.MulX(e, m);
            return r = (m.x - f.x) * r.x + (m.y - f.y) * r.y;
        case d.e_faceB:
            r = w.MulMV(e.R, this.m_axis);
            m = w.MulX(e, this.m_localPoint);
            f = w.MulTMV(b.R, r.GetNegative());
            f = this.m_proxyA.GetSupportVertex(f);
            f = w.MulX(b, f);
            return r = (f.x - m.x) * r.x + (f.y - m.y) * r.y;
        default:
            y.b2Assert(false);
            return 0
        }
    };
    Box2D.postDefs.push(function () {
        Box2D.Collision.b2SeparationFunction.e_points = 1;
        Box2D.Collision.b2SeparationFunction.e_faceA = 2;
        Box2D.Collision.b2SeparationFunction.e_faceB = 4
    });
    h.b2Simplex = function () {
        this.m_v1 = new j;
        this.m_v2 = new j;
        this.m_v3 = new j;
        this.m_vertices = new Vector(3)
    };
    h.prototype.b2Simplex = function () {
        this.m_vertices[0] = this.m_v1;
        this.m_vertices[1] = this.m_v2;
        this.m_vertices[2] = this.m_v3
    };
    h.prototype.ReadCache = function (b, e, f, m, r) {
        y.b2Assert(0 <= b.count && b.count <= 3);
        var s, v;
        this.m_count = b.count;
        for (var t = this.m_vertices, x = 0; x < this.m_count; x++) {
            var C = t[x];
            C.indexA = b.indexA[x];
            C.indexB = b.indexB[x];
            s = e.GetVertex(C.indexA);
            v = m.GetVertex(C.indexB);
            C.wA = w.MulX(f, s);
            C.wB = w.MulX(r, v);
            C.w = w.SubtractVV(C.wB, C.wA);
            C.a = 0
        }
        if (this.m_count > 1) {
            b = b.metric;
            s = this.GetMetric();
            if (s < 0.5 * b || 2 * b < s || s < Number.MIN_VALUE) this.m_count = 0
        }
        if (this.m_count == 0) {
            C = t[0];
            C.indexA = 0;
            C.indexB = 0;
            s = e.GetVertex(0);
            v = m.GetVertex(0);
            C.wA = w.MulX(f, s);
            C.wB = w.MulX(r, v);
            C.w = w.SubtractVV(C.wB, C.wA);
            this.m_count = 1
        }
    };
    h.prototype.WriteCache = function (b) {
            b.metric = this.GetMetric();
            b.count = Box2D.parseUInt(this.m_count);
            for (var e = this.m_vertices, f = 0; f < this.m_count; f++) {
                b.indexA[f] = Box2D.parseUInt(e[f].indexA);
                b.indexB[f] = Box2D.parseUInt(e[f].indexB)
            }
    };
    h.prototype.GetSearchDirection = function () {
        switch (this.m_count) {
        case 1:
            return this.m_v1.w.GetNegative();
        case 2:
            var b = w.SubtractVV(this.m_v2.w, this.m_v1.w);
            return w.CrossVV(b, this.m_v1.w.GetNegative()) > 0 ? w.CrossFV(1, b) : w.CrossVF(b, 1);
        default:
            y.b2Assert(false);
            return new p
        }
    };
    h.prototype.GetClosestPoint = function () {
            switch (this.m_count) {
            case 0:
                y.b2Assert(false);
                return new p;
            case 1:
                return this.m_v1.w;
            case 2:
                return new p(this.m_v1.a * this.m_v1.w.x + this.m_v2.a * this.m_v2.w.x, this.m_v1.a * this.m_v1.w.y + this.m_v2.a * this.m_v2.w.y);
            default:
                y.b2Assert(false);
                return new p
            }
    };
    h.prototype.GetWitnessPoints = function (b, e) {
        switch (this.m_count) {
        case 0:
            y.b2Assert(false);
            break;
        case 1:
            b.SetV(this.m_v1.wA);
            e.SetV(this.m_v1.wB);
            break;
        case 2:
            b.x = this.m_v1.a * this.m_v1.wA.x + this.m_v2.a * this.m_v2.wA.x;
            b.y = this.m_v1.a * this.m_v1.wA.y + this.m_v2.a * this.m_v2.wA.y;
            e.x = this.m_v1.a * this.m_v1.wB.x + this.m_v2.a * this.m_v2.wB.x;
            e.y = this.m_v1.a * this.m_v1.wB.y + this.m_v2.a * this.m_v2.wB.y;
            break;
        case 3:
            e.x = b.x = this.m_v1.a * this.m_v1.wA.x + this.m_v2.a * this.m_v2.wA.x + this.m_v3.a * this.m_v3.wA.x;
            e.y = b.y = this.m_v1.a * this.m_v1.wA.y + this.m_v2.a * this.m_v2.wA.y + this.m_v3.a * this.m_v3.wA.y;
            break;
        default:
            y.b2Assert(false)
        }
    };
    h.prototype.GetMetric = function () {
        switch (this.m_count) {
        case 0:
            y.b2Assert(false);
            return 0;
        case 1:
            return 0;
        case 2:
            return w.SubtractVV(this.m_v1.w,
                this.m_v2.w).Length();
        case 3:
            return w.CrossVV(w.SubtractVV(this.m_v2.w, this.m_v1.w), w.SubtractVV(this.m_v3.w, this.m_v1.w));
        default:
            y.b2Assert(false);
            return 0
        }
    };
    h.prototype.Solve2 = function () {
        var b = this.m_v1.w,
            e = this.m_v2.w,
            f = w.SubtractVV(e, b);
        b = -(b.x * f.x + b.y * f.y);
        if (b <= 0) this.m_count = this.m_v1.a = 1;
        else {
            e = e.x * f.x + e.y * f.y;
            if (e <= 0) {
                this.m_count = this.m_v2.a = 1;
                this.m_v1.Set(this.m_v2)
            } else {
                f = 1 / (e + b);
                this.m_v1.a = e * f;
                this.m_v2.a = b * f;
                this.m_count = 2
            }
        }
    };
    h.prototype.Solve3 = function () {
        var b = this.m_v1.w,
            e = this.m_v2.w,
            f = this.m_v3.w,
            m = w.SubtractVV(e, b),
            r = w.Dot(b, m),
            s = w.Dot(e, m);
        r = -r;
        var v = w.SubtractVV(f, b),
            t = w.Dot(b, v),
            x = w.Dot(f, v);
        t = -t;
        var C = w.SubtractVV(f, e),
            J = w.Dot(e, C);
        C = w.Dot(f, C);
        J = -J;
        v = w.CrossVV(m, v);
        m = v * w.CrossVV(e, f);
        f = v * w.CrossVV(f, b);
        b = v * w.CrossVV(b, e);
        if (r <= 0 && t <= 0) this.m_count = this.m_v1.a = 1;
        else if (s > 0 && r > 0 && b <= 0) {
            x = 1 / (s + r);
            this.m_v1.a = s * x;
            this.m_v2.a = r * x;
            this.m_count = 2
        } else if (x > 0 && t > 0 && f <= 0) {
            s = 1 / (x + t);
            this.m_v1.a = x * s;
            this.m_v3.a = t * s;
            this.m_count = 2;
            this.m_v2.Set(this.m_v3)
        } else if (s <= 0 && J <= 0) {
            this.m_count = this.m_v2.a = 1;
            this.m_v1.Set(this.m_v2)
        } else if (x <= 0 && C <= 0) {
            this.m_count = this.m_v3.a = 1;
            this.m_v1.Set(this.m_v3)
        } else if (C > 0 && J > 0 && m <= 0) {
            s = 1 / (C + J);
            this.m_v2.a = C * s;
            this.m_v3.a = J * s;
            this.m_count = 2;
            this.m_v1.Set(this.m_v3)
        } else {
            s = 1 / (m + f + b);
            this.m_v1.a = m * s;
            this.m_v2.a = f * s;
            this.m_v3.a = b * s;
            this.m_count = 3
        }
    };
    l.b2SimplexCache = function () {
        this.indexA = new Vector_a2j_Number(3);
        this.indexB = new Vector_a2j_Number(3)
    };
    j.b2SimplexVertex = function () {};
    j.prototype.Set = function (b) {
        this.wA.SetV(b.wA);
        this.wB.SetV(b.wB);
        this.w.SetV(b.w);
        this.a = b.a;
        this.indexA = b.indexA;
        this.indexB = b.indexB
    };
    o.b2TimeOfImpact = function () {};
    o.TimeOfImpact = function (b) {
        ++o.b2_toiCalls;
        var e = b.proxyA,
            f = b.proxyB,
            m = b.sweepA,
            r = b.sweepB;
        y.b2Assert(m.t0 == r.t0);
        y.b2Assert(1 - m.t0 > Number.MIN_VALUE);
        var s = e.m_radius + f.m_radius;
        b = b.tolerance;
        var v = 0,
            t = 0,
            x = 0;
        o.s_cache.count = 0;
        for (o.s_distanceInput.useRadii = false;;) {
            m.GetTransform(o.s_xfA, v);
            r.GetTransform(o.s_xfB, v);
            o.s_distanceInput.proxyA = e;
            o.s_distanceInput.proxyB = f;
            o.s_distanceInput.transformA = o.s_xfA;
            o.s_distanceInput.transformB = o.s_xfB;
            W.Distance(o.s_distanceOutput, o.s_cache, o.s_distanceInput);
            if (o.s_distanceOutput.distance <= 0) {
                v = 1;
                break
            }
            o.s_fcn.Initialize(o.s_cache, e, o.s_xfA, f, o.s_xfB);
            var C = o.s_fcn.Evaluate(o.s_xfA, o.s_xfB);
            if (C <= 0) {
                v = 1;
                break
            }
            if (t == 0) x = C > s ? w.Max(s - b, 0.75 * s) : w.Max(C - b, 0.02 * s);
            if (C - x < 0.5 * b) {
                if (t == 0) {
                    v = 1;
                    break
                }
                break
            }
            var J = v,
                T = v,
                P = 1;
            C = C;
            m.GetTransform(o.s_xfA, P);
            r.GetTransform(o.s_xfB, P);
            var X = o.s_fcn.Evaluate(o.s_xfA, o.s_xfB);
            if (X >= x) {
                v = 1;
                break
            }
            for (var $ = 0;;) {
                var ba = 0;
                ba = $ & 1 ? T + (x - C) * (P - T) / (X - C) : 0.5 * (T + P);
                m.GetTransform(o.s_xfA, ba);
                r.GetTransform(o.s_xfB, ba);
                var ca = o.s_fcn.Evaluate(o.s_xfA, o.s_xfB);
                if (w.Abs(ca - x) < 0.025 * b) {
                    J = ba;
                    break
                }
                if (ca > x) {
                    T = ba;
                    C = ca
                } else {
                    P = ba;
                    X = ca
                }++$;
                ++o.b2_toiRootIters;
                if ($ == 50) break
            }
            o.b2_toiMaxRootIters = w.Max(o.b2_toiMaxRootIters, $);
            if (J < (1 + 100 * Number.MIN_VALUE) * v) break;
            v = J;
            t++;
            ++o.b2_toiIters;
            if (t == 1E3) break
        }
        o.b2_toiMaxIters = w.Max(o.b2_toiMaxIters, t);
        return v
    };
    Box2D.postDefs.push(function () {
        Box2D.Collision.b2TimeOfImpact.b2_toiCalls = 0;
        Box2D.Collision.b2TimeOfImpact.b2_toiIters = 0;
        Box2D.Collision.b2TimeOfImpact.b2_toiMaxIters = 0;
        Box2D.Collision.b2TimeOfImpact.b2_toiRootIters = 0;
        Box2D.Collision.b2TimeOfImpact.b2_toiMaxRootIters = 0;
        Box2D.Collision.b2TimeOfImpact.s_cache = new l;
        Box2D.Collision.b2TimeOfImpact.s_distanceInput = new Y;
        Box2D.Collision.b2TimeOfImpact.s_xfA = new U;
        Box2D.Collision.b2TimeOfImpact.s_xfB = new U;
        Box2D.Collision.b2TimeOfImpact.s_fcn = new d;
        Box2D.Collision.b2TimeOfImpact.s_distanceOutput = new k
    });
    q.b2TOIInput = function () {
        this.proxyA = new z;
        this.proxyB = new z;
        this.sweepA = new A;
        this.sweepB = new A
    };
    n.b2WorldManifold = function () {
        this.m_normal = new p
    };
    n.prototype.b2WorldManifold = function () {
        this.m_points = new Vector(y.b2_maxManifoldPoints);
        for (var b = 0; b < y.b2_maxManifoldPoints; b++) this.m_points[b] = new p
    };
    n.prototype.Initialize = function (b, e, f, m, r) {
        if (f === undefined) f = 0;
        if (r === undefined) r = 0;
        if (b.m_pointCount != 0) {
            var s = 0,
                v, t, x = 0,
                C = 0,
                J = 0,
                T = 0,
                P = 0;
            v = 0;
            switch (b.m_type) {
            case E.e_circles:
                t = e.R;
                v = b.m_localPoint;
                s = e.position.x + t.col1.x * v.x + t.col2.x * v.y;
                e = e.position.y + t.col1.y * v.x + t.col2.y * v.y;
                t = m.R;
                v = b.m_points[0].m_localPoint;
                b = m.position.x + t.col1.x * v.x + t.col2.x * v.y;
                m = m.position.y + t.col1.y * v.x + t.col2.y * v.y;
                v = b - s;
                t = m - e;
                x = v * v + t * t;
                if (x > Number.MIN_VALUE * Number.MIN_VALUE) {
                    x = Math.sqrt(x);
                    this.m_normal.x = v / x;
                    this.m_normal.y = t / x
                } else {
                    this.m_normal.x = 1;
                    this.m_normal.y = 0
                }
                v = e + f * this.m_normal.y;
                m = m - r * this.m_normal.y;
                this.m_points[0].x = 0.5 * (s + f * this.m_normal.x + (b - r * this.m_normal.x));
                this.m_points[0].y = 0.5 * (v + m);
                break;
            case E.e_faceA:
                t = e.R;
                v = b.m_localPlaneNormal;
                x = t.col1.x * v.x + t.col2.x * v.y;
                C = t.col1.y * v.x + t.col2.y * v.y;
                t = e.R;
                v = b.m_localPoint;
                J = e.position.x + t.col1.x * v.x + t.col2.x * v.y;
                T = e.position.y + t.col1.y * v.x + t.col2.y * v.y;
                this.m_normal.x = x;
                this.m_normal.y = C;
                for (s = 0; s < b.m_pointCount; s++) {
                    t = m.R;
                    v = b.m_points[s].m_localPoint;
                    P = m.position.x + t.col1.x * v.x + t.col2.x * v.y;
                    v = m.position.y + t.col1.y * v.x + t.col2.y * v.y;
                    this.m_points[s].x = P + 0.5 * (f - (P - J) * x - (v - T) * C - r) * x;
                    this.m_points[s].y = v + 0.5 * (f - (P - J) * x - (v - T) * C - r) * C
                }
                break;
            case E.e_faceB:
                t = m.R;
                v = b.m_localPlaneNormal;
                x = t.col1.x * v.x + t.col2.x * v.y;
                C = t.col1.y * v.x + t.col2.y * v.y;
                t = m.R;
                v = b.m_localPoint;
                J = m.position.x + t.col1.x * v.x + t.col2.x * v.y;
                T = m.position.y + t.col1.y * v.x + t.col2.y * v.y;
                this.m_normal.x = -x;
                this.m_normal.y = -C;
                for (s = 0; s < b.m_pointCount; s++) {
                    t = e.R;
                    v = b.m_points[s].m_localPoint;
                    P = e.position.x + t.col1.x * v.x + t.col2.x * v.y;
                    v = e.position.y + t.col1.y * v.x + t.col2.y * v.y;
                    this.m_points[s].x = P + 0.5 * (r - (P - J) * x - (v - T) * C - f) * x;
                    this.m_points[s].y = v + 0.5 * (r - (P - J) * x - (v - T) * C - f) * C
                }
            }
        }
    };
    a.ClipVertex = function () {
        this.v = new p;
        this.id = new L
    };
    a.prototype.Set = function (b) {
        this.v.SetV(b.v);
        this.id.Set(b.id)
    };
    c.Features = function () {};
    Object.defineProperty(c.prototype, "referenceEdge", {
        enumerable: false,
        configurable: true,
        get: function () {
            return this._referenceEdge
        }
    });
    Object.defineProperty(c.prototype, "referenceEdge", {
        enumerable: false,
        configurable: true,
        set: function (b) {
            if (b === undefined) b = 0;
            this._referenceEdge = b;
            this._m_id._key = this._m_id._key & 4294967040 | this._referenceEdge & 255
        }
    });
    Object.defineProperty(c.prototype, "incidentEdge", {
        enumerable: false,
        configurable: true,
        get: function () {
            return this._incidentEdge
        }
    });
    Object.defineProperty(c.prototype,
        "incidentEdge", {
            enumerable: false,
            configurable: true,
            set: function (b) {
                if (b === undefined) b = 0;
                this._incidentEdge = b;
                this._m_id._key = this._m_id._key & 4294902015 | this._incidentEdge << 8 & 65280
            }
        });
    Object.defineProperty(c.prototype, "incidentVertex", {
        enumerable: false,
        configurable: true,
        get: function () {
            return this._incidentVertex
        }
    });
    Object.defineProperty(c.prototype, "incidentVertex", {
        enumerable: false,
        configurable: true,
        set: function (b) {
            if (b === undefined) b = 0;
            this._incidentVertex = b;
            this._m_id._key = this._m_id._key & 4278255615 |
                this._incidentVertex << 16 & 16711680
        }
    });
    Object.defineProperty(c.prototype, "flip", {
        enumerable: false,
        configurable: true,
        get: function () {
            return this._flip
        }
    });
    Object.defineProperty(c.prototype, "flip", {
        enumerable: false,
        configurable: true,
        set: function (b) {
            if (b === undefined) b = 0;
            this._flip = b;
            this._m_id._key = this._m_id._key & 16777215 | this._flip << 24 & 4278190080
        }
    })
})();
(function () {
    var F = Box2D.Common.b2Settings,
        G = Box2D.Collision.Shapes.b2CircleShape,
        K = Box2D.Collision.Shapes.b2EdgeChainDef,
        y = Box2D.Collision.Shapes.b2EdgeShape,
        w = Box2D.Collision.Shapes.b2MassData,
        A = Box2D.Collision.Shapes.b2PolygonShape,
        U = Box2D.Collision.Shapes.b2Shape,
        p = Box2D.Common.Math.b2Mat22,
        B = Box2D.Common.Math.b2Math,
        Q = Box2D.Common.Math.b2Transform,
        V = Box2D.Common.Math.b2Vec2,
        M = Box2D.Collision.b2Distance,
        L = Box2D.Collision.b2DistanceInput,
        I = Box2D.Collision.b2DistanceOutput,
        W = Box2D.Collision.b2DistanceProxy,
        Y = Box2D.Collision.b2SimplexCache;
    Box2D.inherit(G, Box2D.Collision.Shapes.b2Shape);
    G.prototype.__super = Box2D.Collision.Shapes.b2Shape.prototype;
    G.b2CircleShape = function () {
        Box2D.Collision.Shapes.b2Shape.b2Shape.apply(this, arguments);
        this.m_p = new V
    };
    G.prototype.Copy = function () {
        var k = new G;
        k.Set(this);
        return k
    };
    G.prototype.Set = function (k) {
        this.__super.Set.call(this, k);
        if (Box2D.is(k, G)) this.m_p.SetV((k instanceof G ? k : null).m_p)
    };
    G.prototype.TestPoint = function (k, z) {
        var u = k.R,
            D = k.position.x + (u.col1.x * this.m_p.x + u.col2.x * this.m_p.y);
        u = k.position.y + (u.col1.y * this.m_p.x + u.col2.y * this.m_p.y);
        D = z.x - D;
        u = z.y - u;
        return D * D + u * u <= this.m_radius * this.m_radius
    };
    G.prototype.RayCast = function (k, z, u) {
        var D = u.R,
            H = z.p1.x - (u.position.x + (D.col1.x * this.m_p.x + D.col2.x * this.m_p.y));
        u = z.p1.y - (u.position.y + (D.col1.y * this.m_p.x + D.col2.y * this.m_p.y));
        D = z.p2.x - z.p1.x;
        var O = z.p2.y - z.p1.y,
            E = H * D + u * O,
            R = D * D + O * O,
            N = E * E - R * (H * H + u * u - this.m_radius * this.m_radius);
        if (N < 0 || R < Number.MIN_VALUE) return false;
        E = -(E + Math.sqrt(N));
        if (0 <= E && E <= z.maxFraction * R) {
            E /= R;
            k.fraction = E;
            k.normal.x = H + E * D;
            k.normal.y = u + E * O;
            k.normal.Normalize();
            return true
        }
        return false
    };
    G.prototype.ComputeAABB = function (k, z) {
        var u = z.R,
            D = z.position.x + (u.col1.x * this.m_p.x + u.col2.x * this.m_p.y);
        u = z.position.y + (u.col1.y * this.m_p.x + u.col2.y * this.m_p.y);
        k.lowerBound.Set(D - this.m_radius, u - this.m_radius);
        k.upperBound.Set(D + this.m_radius, u + this.m_radius)
    };
    G.prototype.ComputeMass = function (k, z) {
        if (z === undefined) z = 0;
        k.mass = z * F.b2_pi * this.m_radius * this.m_radius;
        k.center.SetV(this.m_p);
        k.I = k.mass * (0.5 * this.m_radius * this.m_radius + (this.m_p.x * this.m_p.x + this.m_p.y * this.m_p.y))
    };
    G.prototype.ComputeSubmergedArea = function (k, z, u, D) {
        if (z === undefined) z = 0;
        u = B.MulX(u, this.m_p);
        var H = -(B.Dot(k, u) - z);
        if (H < -this.m_radius + Number.MIN_VALUE) return 0;
        if (H > this.m_radius) {
            D.SetV(u);
            return Math.PI * this.m_radius * this.m_radius
        }
        z = this.m_radius * this.m_radius;
        var O = H * H;
        H = z * (Math.asin(H / this.m_radius) + Math.PI / 2) + H * Math.sqrt(z - O);
        z = -2 / 3 * Math.pow(z - O, 1.5) / H;
        D.x = u.x + k.x * z;
        D.y = u.y + k.y * z;
        return H
    };
    G.prototype.GetLocalPosition = function () {
            return this.m_p
    };
    G.prototype.SetLocalPosition = function (k) {
        this.m_p.SetV(k)
    };
    G.prototype.GetRadius = function () {
        return this.m_radius
    };
    G.prototype.SetRadius = function (k) {
        if (k === undefined) k = 0;
        this.m_radius = k
    };
    G.prototype.b2CircleShape = function (k) {
        if (k === undefined) k = 0;
        this.__super.b2Shape.call(this);
        this.m_type = U.e_circleShape;
        this.m_radius = k
    };
    K.b2EdgeChainDef = function () {};
    K.prototype.b2EdgeChainDef = function () {
        this.vertexCount = 0;
        this.isALoop = true;
        this.vertices = []
    };
    Box2D.inherit(y, Box2D.Collision.Shapes.b2Shape);
    y.prototype.__super = Box2D.Collision.Shapes.b2Shape.prototype;
    y.b2EdgeShape = function () {
        Box2D.Collision.Shapes.b2Shape.b2Shape.apply(this, arguments);
        this.s_supportVec = new V;
        this.m_v1 = new V;
        this.m_v2 = new V;
        this.m_coreV1 = new V;
        this.m_coreV2 = new V;
        this.m_normal = new V;
        this.m_direction = new V;
        this.m_cornerDir1 = new V;
        this.m_cornerDir2 = new V
    };
    y.prototype.TestPoint = function () {
        return false
    };
    y.prototype.RayCast = function (k, z, u) {
        var D, H = z.p2.x - z.p1.x,
            O = z.p2.y - z.p1.y;
        D = u.R;
        var E = u.position.x + (D.col1.x * this.m_v1.x + D.col2.x * this.m_v1.y),
            R = u.position.y + (D.col1.y * this.m_v1.x + D.col2.y * this.m_v1.y),
            N = u.position.y + (D.col1.y * this.m_v2.x + D.col2.y * this.m_v2.y) - R;
        u = -(u.position.x + (D.col1.x * this.m_v2.x + D.col2.x * this.m_v2.y) - E);
        D = 100 * Number.MIN_VALUE;
        var S = -(H * N + O * u);
        if (S > D) {
            E = z.p1.x - E;
            var aa = z.p1.y - R;
            R = E * N + aa * u;
            if (0 <= R && R <= z.maxFraction * S) {
                z = -H * aa + O * E;
                if (-D * S <= z && z <= S * (1 + D)) {
                    R /= S;
                    k.fraction = R;
                    z = Math.sqrt(N * N + u * u);
                    k.normal.x = N / z;
                    k.normal.y = u / z;
                    return true
                }
            }
        }
        return false
    };
    y.prototype.ComputeAABB = function (k, z) {
        var u = z.R,
            D = z.position.x + (u.col1.x * this.m_v1.x + u.col2.x * this.m_v1.y),
            H = z.position.y + (u.col1.y * this.m_v1.x + u.col2.y * this.m_v1.y),
            O = z.position.x + (u.col1.x * this.m_v2.x + u.col2.x * this.m_v2.y);
        u = z.position.y + (u.col1.y * this.m_v2.x + u.col2.y * this.m_v2.y);
        if (D < O) {
            k.lowerBound.x = D;
            k.upperBound.x = O
        } else {
            k.lowerBound.x = O;
            k.upperBound.x = D
        } if (H < u) {
            k.lowerBound.y = H;
            k.upperBound.y = u
        } else {
            k.lowerBound.y = u;
            k.upperBound.y = H
        }
    };
    y.prototype.ComputeMass = function (k) {
        k.mass = 0;
        k.center.SetV(this.m_v1);
        k.I = 0
    };
    y.prototype.ComputeSubmergedArea = function (k, z, u, D) {
            if (z === undefined) z = 0;
            var H = new V(k.x * z, k.y * z),
                O = B.MulX(u, this.m_v1);
            u = B.MulX(u, this.m_v2);
            var E = B.Dot(k, O) - z;
            k = B.Dot(k, u) - z;
            if (E > 0)
                if (k > 0) return 0;
                else {
                    O.x = -k / (E - k) * O.x + E / (E - k) * u.x;
                    O.y = -k / (E - k) * O.y + E / (E - k) * u.y
                } else if (k > 0) {
                u.x = -k / (E - k) * O.x + E / (E - k) * u.x;
                u.y = -k / (E - k) * O.y + E / (E - k) * u.y
            }
            D.x = (H.x + O.x + u.x) / 3;
            D.y = (H.y + O.y + u.y) / 3;
            return 0.5 * ((O.x - H.x) * (u.y - H.y) - (O.y - H.y) * (u.x - H.x))
    };
    y.prototype.GetLength = function () {
        return this.m_length
    };
    y.prototype.GetVertex1 = function () {
        return this.m_v1
    };
    y.prototype.GetVertex2 = function () {
            return this.m_v2
    };
    y.prototype.GetCoreVertex1 = function () {
        return this.m_coreV1
    };
    y.prototype.GetCoreVertex2 = function () {
        return this.m_coreV2
    };
    y.prototype.GetNormalVector = function () {
        return this.m_normal
    };
    y.prototype.GetDirectionVector = function () {
        return this.m_direction
    };
    y.prototype.GetCorner1Vector = function () {
        return this.m_cornerDir1
    };
    y.prototype.GetCorner2Vector = function () {
        return this.m_cornerDir2
    };
    y.prototype.Corner1IsConvex = function () {
        return this.m_cornerConvex1
    };
    y.prototype.Corner2IsConvex = function () {
            return this.m_cornerConvex2
    };
    y.prototype.GetFirstVertex = function (k) {
        var z = k.R;
        return new V(k.position.x + (z.col1.x * this.m_coreV1.x + z.col2.x * this.m_coreV1.y), k.position.y + (z.col1.y * this.m_coreV1.x + z.col2.y * this.m_coreV1.y))
    };
    y.prototype.GetNextEdge = function () {
        return this.m_nextEdge
    };
    y.prototype.GetPrevEdge = function () {
        return this.m_prevEdge
    };
    y.prototype.Support = function (k, z, u) {
        if (z === undefined) z = 0;
        if (u === undefined) u = 0;
        var D = k.R,
            H = k.position.x + (D.col1.x * this.m_coreV1.x + D.col2.x * this.m_coreV1.y),
            O = k.position.y + (D.col1.y * this.m_coreV1.x + D.col2.y * this.m_coreV1.y),
            E = k.position.x + (D.col1.x * this.m_coreV2.x + D.col2.x * this.m_coreV2.y);
        k = k.position.y + (D.col1.y * this.m_coreV2.x + D.col2.y * this.m_coreV2.y);
        if (H * z + O * u > E * z + k * u) {
            this.s_supportVec.x = H;
            this.s_supportVec.y = O
        } else {
            this.s_supportVec.x = E;
            this.s_supportVec.y = k
        }
        return this.s_supportVec
    };
    y.prototype.b2EdgeShape = function (k, z) {
        this.__super.b2Shape.call(this);
        this.m_type = U.e_edgeShape;
        this.m_nextEdge = this.m_prevEdge = null;
        this.m_v1 = k;
        this.m_v2 = z;
        this.m_direction.Set(this.m_v2.x - this.m_v1.x, this.m_v2.y - this.m_v1.y);
        this.m_length = this.m_direction.Normalize();
        this.m_normal.Set(this.m_direction.y, -this.m_direction.x);
        this.m_coreV1.Set(-F.b2_toiSlop * (this.m_normal.x - this.m_direction.x) + this.m_v1.x, -F.b2_toiSlop * (this.m_normal.y - this.m_direction.y) + this.m_v1.y);
        this.m_coreV2.Set(-F.b2_toiSlop * (this.m_normal.x + this.m_direction.x) + this.m_v2.x, -F.b2_toiSlop * (this.m_normal.y + this.m_direction.y) + this.m_v2.y);
        this.m_cornerDir1 = this.m_normal;
        this.m_cornerDir2.Set(-this.m_normal.x, -this.m_normal.y)
    };
    y.prototype.SetPrevEdge = function (k, z, u, D) {
        this.m_prevEdge = k;
        this.m_coreV1 = z;
        this.m_cornerDir1 = u;
        this.m_cornerConvex1 = D
    };
    y.prototype.SetNextEdge = function (k, z, u, D) {
        this.m_nextEdge = k;
        this.m_coreV2 = z;
        this.m_cornerDir2 = u;
        this.m_cornerConvex2 = D
    };
    w.b2MassData = function () {
        this.mass = 0;
        this.center = new V(0, 0);
        this.I = 0
    };
    Box2D.inherit(A, Box2D.Collision.Shapes.b2Shape);
    A.prototype.__super = Box2D.Collision.Shapes.b2Shape.prototype;
    A.b2PolygonShape = function () {
        Box2D.Collision.Shapes.b2Shape.b2Shape.apply(this, arguments)
    };
    A.prototype.Copy = function () {
        var k = new A;
        k.Set(this);
        return k
    };
    A.prototype.Set = function (k) {
        this.__super.Set.call(this, k);
        if (Box2D.is(k, A)) {
            k = k instanceof A ? k : null;
            this.m_centroid.SetV(k.m_centroid);
            this.m_vertexCount = k.m_vertexCount;
            this.Reserve(this.m_vertexCount);
            for (var z = 0; z < this.m_vertexCount; z++) {
                this.m_vertices[z].SetV(k.m_vertices[z]);
                this.m_normals[z].SetV(k.m_normals[z])
            }
        }
    };
    A.prototype.SetAsArray = function (k, z) {
        if (z === undefined) z = 0;
        var u = new Vector,
            D = 0,
            H;
        for (D = 0; D < k.length; ++D) {
            H = k[D];
            u.push(H)
        }
        this.SetAsVector(u,
            z)
    };
    A.AsArray = function (k, z) {
        if (z === undefined) z = 0;
        var u = new A;
        u.SetAsArray(k, z);
        return u
    };
    A.prototype.SetAsVector = function (k, z) {
        if (z === undefined) z = 0;
        if (z == 0) z = k.length;
        F.b2Assert(2 <= z);
        this.m_vertexCount = z;
        this.Reserve(z);
        var u = 0;
        for (u = 0; u < this.m_vertexCount; u++) this.m_vertices[u].SetV(k[u]);
        for (u = 0; u < this.m_vertexCount; ++u) {
            var D = parseInt(u),
                H = parseInt(u + 1 < this.m_vertexCount ? u + 1 : 0);
            D = B.SubtractVV(this.m_vertices[H], this.m_vertices[D]);
            F.b2Assert(D.LengthSquared() > Number.MIN_VALUE);
            this.m_normals[u].SetV(B.CrossVF(D,
                1));
            this.m_normals[u].Normalize()
        }
        this.m_centroid = A.ComputeCentroid(this.m_vertices, this.m_vertexCount)
    };
    A.AsVector = function (k, z) {
        if (z === undefined) z = 0;
        var u = new A;
        u.SetAsVector(k, z);
        return u
    };
    A.prototype.SetAsBox = function (k, z) {
        if (k === undefined) k = 0;
        if (z === undefined) z = 0;
        this.m_vertexCount = 4;
        this.Reserve(4);
        this.m_vertices[0].Set(-k, -z);
        this.m_vertices[1].Set(k, -z);
        this.m_vertices[2].Set(k, z);
        this.m_vertices[3].Set(-k, z);
        this.m_normals[0].Set(0, -1);
        this.m_normals[1].Set(1, 0);
        this.m_normals[2].Set(0,
            1);
        this.m_normals[3].Set(-1, 0);
        this.m_centroid.SetZero()
    };
    A.AsBox = function (k, z) {
        if (k === undefined) k = 0;
        if (z === undefined) z = 0;
        var u = new A;
        u.SetAsBox(k, z);
        return u
    };
    A.prototype.SetAsOrientedBox = function (k, z, u, D) {
        if (k === undefined) k = 0;
        if (z === undefined) z = 0;
        if (u === undefined) u = null;
        if (D === undefined) D = 0;
        this.m_vertexCount = 4;
        this.Reserve(4);
        this.m_vertices[0].Set(-k, -z);
        this.m_vertices[1].Set(k, -z);
        this.m_vertices[2].Set(k, z);
        this.m_vertices[3].Set(-k, z);
        this.m_normals[0].Set(0, -1);
        this.m_normals[1].Set(1, 0);
        this.m_normals[2].Set(0, 1);
        this.m_normals[3].Set(-1, 0);
        this.m_centroid = u;
        k = new Q;
        k.position = u;
        k.R.Set(D);
        for (u = 0; u < this.m_vertexCount; ++u) {
            this.m_vertices[u] = B.MulX(k, this.m_vertices[u]);
            this.m_normals[u] = B.MulMV(k.R, this.m_normals[u])
        }
    };
    A.AsOrientedBox = function (k, z, u, D) {
        if (k === undefined) k = 0;
        if (z === undefined) z = 0;
        if (u === undefined) u = null;
        if (D === undefined) D = 0;
        var H = new A;
        H.SetAsOrientedBox(k, z, u, D);
        return H
    };
    A.prototype.SetAsEdge = function (k, z) {
        this.m_vertexCount = 2;
        this.Reserve(2);
        this.m_vertices[0].SetV(k);
        this.m_vertices[1].SetV(z);
        this.m_centroid.x = 0.5 * (k.x + z.x);
        this.m_centroid.y = 0.5 * (k.y + z.y);
        this.m_normals[0] = B.CrossVF(B.SubtractVV(z, k), 1);
        this.m_normals[0].Normalize();
        this.m_normals[1].x = -this.m_normals[0].x;
        this.m_normals[1].y = -this.m_normals[0].y
    };
    A.AsEdge = function (k, z) {
        var u = new A;
        u.SetAsEdge(k, z);
        return u
    };
    A.prototype.TestPoint = function (k, z) {
        var u;
        u = k.R;
        for (var D = z.x - k.position.x, H = z.y - k.position.y, O = D * u.col1.x + H * u.col1.y, E = D * u.col2.x + H * u.col2.y, R = 0; R < this.m_vertexCount; ++R) {
            u = this.m_vertices[R];
            D = O - u.x;
            H = E - u.y;
            u = this.m_normals[R];
            if (u.x * D + u.y * H > 0) return false
        }
        return true
    };
    A.prototype.RayCast = function (k, z, u) {
        var D = 0,
            H = z.maxFraction,
            O = 0,
            E = 0,
            R, N;
        O = z.p1.x - u.position.x;
        E = z.p1.y - u.position.y;
        R = u.R;
        var S = O * R.col1.x + E * R.col1.y,
            aa = O * R.col2.x + E * R.col2.y;
        O = z.p2.x - u.position.x;
        E = z.p2.y - u.position.y;
        R = u.R;
        z = O * R.col1.x + E * R.col1.y - S;
        R = O * R.col2.x + E * R.col2.y - aa;
        for (var Z = parseInt(-1), d = 0; d < this.m_vertexCount; ++d) {
            N = this.m_vertices[d];
            O = N.x - S;
            E = N.y - aa;
            N = this.m_normals[d];
            O = N.x * O + N.y * E;
            E = N.x * z + N.y * R;
            if (E == 0) {
                if (O < 0) return false
            } else if (E < 0 && O < D * E) {
                D = O / E;
                Z = d
            } else if (E > 0 && O < H * E) H = O / E;
            if (H < D - Number.MIN_VALUE) return false
        }
        if (Z >= 0) {
            k.fraction = D;
            R = u.R;
            N = this.m_normals[Z];
            k.normal.x = R.col1.x * N.x + R.col2.x * N.y;
            k.normal.y = R.col1.y * N.x + R.col2.y * N.y;
            return true
        }
        return false
    };
    A.prototype.ComputeAABB = function (k, z) {
        for (var u = z.R, D = this.m_vertices[0], H = z.position.x + (u.col1.x * D.x + u.col2.x * D.y), O = z.position.y + (u.col1.y * D.x + u.col2.y * D.y), E = H, R = O, N = 1; N < this.m_vertexCount; ++N) {
            D = this.m_vertices[N];
            var S = z.position.x + (u.col1.x * D.x + u.col2.x * D.y);
            D = z.position.y + (u.col1.y * D.x + u.col2.y * D.y);
            H = H < S ? H : S;
            O = O < D ? O : D;
            E = E > S ? E : S;
            R = R > D ? R : D
        }
        k.lowerBound.x = H - this.m_radius;
        k.lowerBound.y = O - this.m_radius;
        k.upperBound.x = E + this.m_radius;
        k.upperBound.y = R + this.m_radius
    };
    A.prototype.ComputeMass = function (k, z) {
        if (z === undefined) z = 0;
        if (this.m_vertexCount == 2) {
            k.center.x = 0.5 * (this.m_vertices[0].x + this.m_vertices[1].x);
            k.center.y = 0.5 * (this.m_vertices[0].y + this.m_vertices[1].y);
            k.mass = 0;
            k.I = 0
        } else {
            for (var u = 0, D = 0, H = 0, O = 0, E = 1 / 3, R = 0; R < this.m_vertexCount; ++R) {
                var N = this.m_vertices[R],
                    S = R + 1 < this.m_vertexCount ? this.m_vertices[parseInt(R + 1)] : this.m_vertices[0],
                    aa = N.x - 0,
                    Z = N.y - 0,
                    d = S.x - 0,
                    h = S.y - 0,
                    l = aa * h - Z * d,
                    j = 0.5 * l;
                H += j;
                u += j * E * (0 + N.x + S.x);
                D += j * E * (0 + N.y + S.y);
                N = aa;
                Z = Z;
                d = d;
                h = h;
                O += l * (E * (0.25 * (N * N + d * N + d * d) + (0 * N + 0 * d)) + 0 + (E * (0.25 * (Z * Z + h * Z + h * h) + (0 * Z + 0 * h)) + 0))
            }
            k.mass = z * H;
            u *= 1 / H;
            D *= 1 / H;
            k.center.Set(u, D);
            k.I = z * O
        }
    };
    A.prototype.ComputeSubmergedArea = function (k, z, u, D) {
        if (z === undefined) z = 0;
        var H = B.MulTMV(u.R, k),
            O = z - B.Dot(k, u.position),
            E = new Vector_a2j_Number,
            R = 0,
            N = parseInt(-1);
        z = parseInt(-1);
        var S = false;
        for (k = k = 0; k < this.m_vertexCount; ++k) {
            E[k] = B.Dot(H, this.m_vertices[k]) - O;
            var aa = E[k] < -Number.MIN_VALUE;
            if (k > 0)
                if (aa) {
                    if (!S) {
                        N = k - 1;
                        R++
                    }
                } else if (S) {
                z = k - 1;
                R++
            }
            S = aa
        }
        switch (R) {
        case 0:
            if (S) {
                k = new w;
                this.ComputeMass(k, 1);
                D.SetV(B.MulX(u, k.center));
                return k.mass
            } else return 0;
        case 1:
            if (N == -1) N = this.m_vertexCount - 1;
            else z = this.m_vertexCount - 1
        }
        k = parseInt((N + 1) % this.m_vertexCount);
        H = parseInt((z + 1) % this.m_vertexCount);
        O = (0 - E[N]) / (E[k] - E[N]);
        E = (0 - E[z]) / (E[H] - E[z]);
        N = new V(this.m_vertices[N].x * (1 - O) + this.m_vertices[k].x * O, this.m_vertices[N].y * (1 - O) + this.m_vertices[k].y * O);
        z = new V(this.m_vertices[z].x * (1 - E) + this.m_vertices[H].x * E, this.m_vertices[z].y * (1 - E) + this.m_vertices[H].y * E);
        E = 0;
        O = new V;
        R = this.m_vertices[k];
        for (k = k; k != H;) {
            k = (k + 1) % this.m_vertexCount;
            S = k == H ? z : this.m_vertices[k];
            aa = 0.5 * ((R.x - N.x) * (S.y - N.y) - (R.y - N.y) * (S.x - N.x));
            E += aa;
            O.x += aa * (N.x + R.x + S.x) / 3;
            O.y += aa * (N.y + R.y + S.y) / 3;
            R = S
        }
        O.Multiply(1 / E);
        D.SetV(B.MulX(u, O));
        return E
    };
    A.prototype.GetVertexCount = function () {
        return this.m_vertexCount
    };
    A.prototype.GetVertices = function () {
        return this.m_vertices
    };
    A.prototype.GetNormals = function () {
        return this.m_normals
    };
    A.prototype.GetSupport = function (k) {
        for (var z = 0, u = this.m_vertices[0].x * k.x + this.m_vertices[0].y * k.y, D = 1; D < this.m_vertexCount; ++D) {
            var H = this.m_vertices[D].x * k.x + this.m_vertices[D].y * k.y;
            if (H > u) {
                z = D;
                u = H
            }
        }
        return z
    };
    A.prototype.GetSupportVertex = function (k) {
        for (var z = 0, u = this.m_vertices[0].x * k.x + this.m_vertices[0].y * k.y, D = 1; D < this.m_vertexCount; ++D) {
            var H = this.m_vertices[D].x * k.x + this.m_vertices[D].y * k.y;
            if (H > u) {
                z = D;
                u = H
            }
        }
        return this.m_vertices[z]
    };
    A.prototype.Validate = function () {
        return false
    };
    A.prototype.b2PolygonShape = function () {
        this.__super.b2Shape.call(this);
        this.m_type = U.e_polygonShape;
        this.m_centroid = new V;
        this.m_vertices = new Vector;
        this.m_normals = new Vector
    };
    A.prototype.Reserve = function (k) {
        if (k === undefined) k = 0;
        for (var z = parseInt(this.m_vertices.length); z < k; z++) {
            this.m_vertices[z] = new V;
            this.m_normals[z] = new V
        }
    };
    A.ComputeCentroid = function (k, z) {
        if (z === undefined) z = 0;
        for (var u = new V, D = 0, H = 1 / 3, O = 0; O < z; ++O) {
            var E = k[O],
                R = O + 1 < z ? k[parseInt(O + 1)] : k[0],
                N = 0.5 * ((E.x - 0) * (R.y - 0) - (E.y - 0) * (R.x - 0));
            D += N;
            u.x += N * H * (0 + E.x + R.x);
            u.y += N * H * (0 + E.y + R.y)
        }
        u.x *= 1 / D;
        u.y *= 1 / D;
        return u
    };
    A.ComputeOBB = function (k, z, u) {
        if (u === undefined) u = 0;
        var D = 0,
            H = new Vector(u + 1);
        for (D = 0; D < u; ++D) H[D] = z[D];
        H[u] = H[0];
        z = Number.MAX_VALUE;
        for (D = 1; D <= u; ++D) {
            var O = H[parseInt(D - 1)],
                E = H[D].x - O.x,
                R = H[D].y - O.y,
                N = Math.sqrt(E * E + R * R);
            E /= N;
            R /= N;
            for (var S = -R, aa = E, Z = N = Number.MAX_VALUE, d = -Number.MAX_VALUE, h = -Number.MAX_VALUE, l = 0; l < u; ++l) {
                var j = H[l].x - O.x,
                    o = H[l].y - O.y,
                    q = E * j + R * o;
                j = S * j + aa * o;
                if (q < N) N = q;
                if (j < Z) Z = j;
                if (q > d) d = q;
                if (j > h) h = j
            }
            l = (d - N) * (h - Z);
            if (l < 0.95 * z) {
                z = l;
                k.R.col1.x = E;
                k.R.col1.y = R;
                k.R.col2.x = S;
                k.R.col2.y = aa;
                E = 0.5 * (N + d);
                R = 0.5 * (Z + h);
                S = k.R;
                k.center.x = O.x + (S.col1.x * E + S.col2.x * R);
                k.center.y = O.y + (S.col1.y * E + S.col2.y * R);
                k.extents.x = 0.5 * (d - N);
                k.extents.y = 0.5 * (h - Z)
            }
        }
    };
    Box2D.postDefs.push(function () {
        Box2D.Collision.Shapes.b2PolygonShape.s_mat = new p
    });
    U.b2Shape = function () {};
    U.prototype.Copy = function () {
        return null
    };
    U.prototype.Set = function (k) {
        this.m_radius = k.m_radius
    };
    U.prototype.GetType = function () {
        return this.m_type
    };
    U.prototype.TestPoint = function () {
        return false
    };
    U.prototype.RayCast = function () {
        return false
    };
    U.prototype.ComputeAABB = function () {};
    U.prototype.ComputeMass = function () {};
    U.prototype.ComputeSubmergedArea = function () {
        return 0
    };
    U.TestOverlap = function (k, z, u, D) {
        var H = new L;
        H.proxyA = new W;
        H.proxyA.Set(k);
        H.proxyB = new W;
        H.proxyB.Set(u);
        H.transformA = z;
        H.transformB = D;
        H.useRadii = true;
        k = new Y;
        k.count = 0;
        z = new I;
        M.Distance(z, k, H);
        return z.distance < 10 * Number.MIN_VALUE
    };
    U.prototype.b2Shape = function () {
        this.m_type = U.e_unknownShape;
        this.m_radius = F.b2_linearSlop
    };
    Box2D.postDefs.push(function () {
        Box2D.Collision.Shapes.b2Shape.e_unknownShape = parseInt(-1);
        Box2D.Collision.Shapes.b2Shape.e_circleShape = 0;
        Box2D.Collision.Shapes.b2Shape.e_polygonShape = 1;
        Box2D.Collision.Shapes.b2Shape.e_edgeShape = 2;
        Box2D.Collision.Shapes.b2Shape.e_shapeTypeCount = 3;
        Box2D.Collision.Shapes.b2Shape.e_hitCollide = 1;
        Box2D.Collision.Shapes.b2Shape.e_missCollide = 0;
        Box2D.Collision.Shapes.b2Shape.e_startsInsideCollide = parseInt(-1)
    })
})();
(function () {
    var F = Box2D.Common.b2Color,
        G = Box2D.Common.b2Settings,
        K = Box2D.Common.Math.b2Math;
    F.b2Color = function () {
        this._b = this._g = this._r = 0
    };
    F.prototype.b2Color = function (y, w, A) {
        if (y === undefined) y = 0;
        if (w === undefined) w = 0;
        if (A === undefined) A = 0;
        this._r = Box2D.parseUInt(255 * K.Clamp(y, 0, 1));
        this._g = Box2D.parseUInt(255 * K.Clamp(w, 0, 1));
        this._b = Box2D.parseUInt(255 * K.Clamp(A, 0, 1))
    };
    F.prototype.Set = function (y, w, A) {
        if (y === undefined) y = 0;
        if (w === undefined) w = 0;
        if (A === undefined) A = 0;
        this._r = Box2D.parseUInt(255 * K.Clamp(y,
            0, 1));
        this._g = Box2D.parseUInt(255 * K.Clamp(w, 0, 1));
        this._b = Box2D.parseUInt(255 * K.Clamp(A, 0, 1))
    };
    Object.defineProperty(F.prototype, "r", {
        enumerable: false,
        configurable: true,
        set: function (y) {
            if (y === undefined) y = 0;
            this._r = Box2D.parseUInt(255 * K.Clamp(y, 0, 1))
        }
    });
    Object.defineProperty(F.prototype, "g", {
        enumerable: false,
        configurable: true,
        set: function (y) {
            if (y === undefined) y = 0;
            this._g = Box2D.parseUInt(255 * K.Clamp(y, 0, 1))
        }
    });
    Object.defineProperty(F.prototype, "b", {
        enumerable: false,
        configurable: true,
        set: function (y) {
            if (y === undefined) y = 0;
            this._b = Box2D.parseUInt(255 * K.Clamp(y, 0, 1))
        }
    });
    Object.defineProperty(F.prototype, "color", {
        enumerable: false,
        configurable: true,
        get: function () {
            return this._r << 16 | this._g << 8 | this._b
        }
    });
    G.b2Settings = function () {};
    G.b2MixFriction = function (y, w) {
        if (y === undefined) y = 0;
        if (w === undefined) w = 0;
        return Math.sqrt(y * w)
    };
    G.b2MixRestitution = function (y, w) {
        if (y === undefined) y = 0;
        if (w === undefined) w = 0;
        return y > w ? y : w
    };
    G.b2Assert = function (y) {
        if (!y) throw "Assertion Failed";
    };
    Box2D.postDefs.push(function () {
        Box2D.Common.b2Settings.VERSION = "2.1alpha";
        Box2D.Common.b2Settings.USHRT_MAX = 65535;
        Box2D.Common.b2Settings.b2_pi = Math.PI;
        Box2D.Common.b2Settings.b2_maxManifoldPoints = 2;
        Box2D.Common.b2Settings.b2_aabbExtension = 0.1;
        Box2D.Common.b2Settings.b2_aabbMultiplier = 2;
        Box2D.Common.b2Settings.b2_polygonRadius = 2 * G.b2_linearSlop;
        Box2D.Common.b2Settings.b2_linearSlop = 0.0050;
        Box2D.Common.b2Settings.b2_angularSlop = 2 / 180 * G.b2_pi;
        Box2D.Common.b2Settings.b2_toiSlop = 8 * G.b2_linearSlop;
        Box2D.Common.b2Settings.b2_maxTOIContactsPerIsland = 32;
        Box2D.Common.b2Settings.b2_maxTOIJointsPerIsland = 32;
        Box2D.Common.b2Settings.b2_velocityThreshold = 1;
        Box2D.Common.b2Settings.b2_maxLinearCorrection = 0.2;
        Box2D.Common.b2Settings.b2_maxAngularCorrection = 8 / 180 * G.b2_pi;
        Box2D.Common.b2Settings.b2_maxTranslation = 2;
        Box2D.Common.b2Settings.b2_maxTranslationSquared = G.b2_maxTranslation * G.b2_maxTranslation;
        Box2D.Common.b2Settings.b2_maxRotation = 0.5 * G.b2_pi;
        Box2D.Common.b2Settings.b2_maxRotationSquared = G.b2_maxRotation * G.b2_maxRotation;
        Box2D.Common.b2Settings.b2_contactBaumgarte = 0.2;
        Box2D.Common.b2Settings.b2_timeToSleep = 0.5;
        Box2D.Common.b2Settings.b2_linearSleepTolerance = 0.01;
        Box2D.Common.b2Settings.b2_angularSleepTolerance = 2 / 180 * G.b2_pi
    })
})();
(function () {
    var F = Box2D.Common.Math.b2Mat22,
        G = Box2D.Common.Math.b2Mat33,
        K = Box2D.Common.Math.b2Math,
        y = Box2D.Common.Math.b2Sweep,
        w = Box2D.Common.Math.b2Transform,
        A = Box2D.Common.Math.b2Vec2,
        U = Box2D.Common.Math.b2Vec3;
    F.b2Mat22 = function () {
        this.col1 = new A;
        this.col2 = new A
    };
    F.prototype.b2Mat22 = function () {
        this.SetIdentity()
    };
    F.FromAngle = function (p) {
        if (p === undefined) p = 0;
        var B = new F;
        B.Set(p);
        return B
    };
    F.FromVV = function (p, B) {
        var Q = new F;
        Q.SetVV(p, B);
        return Q
    };
    F.prototype.Set = function (p) {
        if (p === undefined) p = 0;
        var B = Math.cos(p);
        p = Math.sin(p);
        this.col1.x = B;
        this.col2.x = -p;
        this.col1.y = p;
        this.col2.y = B
    };
    F.prototype.SetVV = function (p, B) {
        this.col1.SetV(p);
        this.col2.SetV(B)
    };
    F.prototype.Copy = function () {
        var p = new F;
        p.SetM(this);
        return p
    };
    F.prototype.SetM = function (p) {
        this.col1.SetV(p.col1);
        this.col2.SetV(p.col2)
    };
    F.prototype.AddM = function (p) {
        this.col1.x += p.col1.x;
        this.col1.y += p.col1.y;
        this.col2.x += p.col2.x;
        this.col2.y += p.col2.y
    };
    F.prototype.SetIdentity = function () {
        this.col1.x = 1;
        this.col2.x = 0;
        this.col1.y = 0;
        this.col2.y = 1
    };
    F.prototype.SetZero = function () {
        this.col1.x = 0;
        this.col2.x = 0;
        this.col1.y = 0;
        this.col2.y = 0
    };
    F.prototype.GetAngle = function () {
        return Math.atan2(this.col1.y, this.col1.x)
    };
    F.prototype.GetInverse = function (p) {
        var B = this.col1.x,
            Q = this.col2.x,
            V = this.col1.y,
            M = this.col2.y,
            L = B * M - Q * V;
        if (L != 0) L = 1 / L;
        p.col1.x = L * M;
        p.col2.x = -L * Q;
        p.col1.y = -L * V;
        p.col2.y = L * B;
        return p
    };
    F.prototype.Solve = function (p, B, Q) {
        if (B === undefined) B = 0;
        if (Q === undefined) Q = 0;
        var V = this.col1.x,
            M = this.col2.x,
            L = this.col1.y,
            I = this.col2.y,
            W = V * I - M * L;
        if (W != 0) W = 1 / W;
        p.x = W * (I * B - M * Q);
        p.y = W * (V * Q - L * B);
        return p
    };
    F.prototype.Abs = function () {
        this.col1.Abs();
        this.col2.Abs()
    };
    G.b2Mat33 = function () {
        this.col1 = new U;
        this.col2 = new U;
        this.col3 = new U
    };
    G.prototype.b2Mat33 = function (p, B, Q) {
        if (p === undefined) p = null;
        if (B === undefined) B = null;
        if (Q === undefined) Q = null;
        if (!p && !B && !Q) {
            this.col1.SetZero();
            this.col2.SetZero();
            this.col3.SetZero()
        } else {
            this.col1.SetV(p);
            this.col2.SetV(B);
            this.col3.SetV(Q)
        }
    };
    G.prototype.SetVVV = function (p, B, Q) {
        this.col1.SetV(p);
        this.col2.SetV(B);
        this.col3.SetV(Q)
    };
    G.prototype.Copy = function () {
        return new G(this.col1, this.col2, this.col3)
    };
    G.prototype.SetM = function (p) {
        this.col1.SetV(p.col1);
        this.col2.SetV(p.col2);
        this.col3.SetV(p.col3)
    };
    G.prototype.AddM = function (p) {
        this.col1.x += p.col1.x;
        this.col1.y += p.col1.y;
        this.col1.z += p.col1.z;
        this.col2.x += p.col2.x;
        this.col2.y += p.col2.y;
        this.col2.z += p.col2.z;
        this.col3.x += p.col3.x;
        this.col3.y += p.col3.y;
        this.col3.z += p.col3.z
    };
    G.prototype.SetIdentity = function () {
        this.col1.x = 1;
        this.col2.x = 0;
        this.col3.x = 0;
        this.col1.y = 0;
        this.col2.y = 1;
        this.col3.y = 0;
        this.col1.z = 0;
        this.col2.z = 0;
        this.col3.z = 1
    };
    G.prototype.SetZero = function () {
        this.col1.x = 0;
        this.col2.x = 0;
        this.col3.x = 0;
        this.col1.y = 0;
        this.col2.y = 0;
        this.col3.y = 0;
        this.col1.z = 0;
        this.col2.z = 0;
        this.col3.z = 0
    };
    G.prototype.Solve22 = function (p, B, Q) {
        if (B === undefined) B = 0;
        if (Q === undefined) Q = 0;
        var V = this.col1.x,
            M = this.col2.x,
            L = this.col1.y,
            I = this.col2.y,
            W = V * I - M * L;
        if (W != 0) W = 1 / W;
        p.x = W * (I * B - M * Q);
        p.y = W * (V * Q - L * B);
        return p
    };
    G.prototype.Solve33 = function (p, B, Q, V) {
        if (B === undefined) B = 0;
        if (Q === undefined) Q = 0;
        if (V === undefined) V = 0;
        var M = this.col1.x,
            L = this.col1.y,
            I = this.col1.z,
            W = this.col2.x,
            Y = this.col2.y,
            k = this.col2.z,
            z = this.col3.x,
            u = this.col3.y,
            D = this.col3.z,
            H = M * (Y * D - k * u) + L * (k * z - W * D) + I * (W * u - Y * z);
        if (H != 0) H = 1 / H;
        p.x = H * (B * (Y * D - k * u) + Q * (k * z - W * D) + V * (W * u - Y * z));
        p.y = H * (M * (Q * D - V * u) + L * (V * z - B * D) + I * (B * u - Q * z));
        p.z = H * (M * (Y * V - k * Q) + L * (k * B - W * V) + I * (W * Q - Y * B));
        return p
    };
    K.b2Math = function () {};
    K.IsValid = function (p) {
        if (p === undefined) p = 0;
        return isFinite(p)
    };
    K.Dot = function (p, B) {
        return p.x * B.x + p.y * B.y
    };
    K.CrossVV = function (p, B) {
        return p.x * B.y - p.y * B.x
    };
    K.CrossVF = function (p, B) {
        if (B === undefined) B = 0;
        return new A(B * p.y, -B * p.x)
    };
    K.CrossFV = function (p, B) {
        if (p === undefined) p = 0;
        return new A(-p * B.y, p * B.x)
    };
    K.MulMV = function (p, B) {
        return new A(p.col1.x * B.x + p.col2.x * B.y, p.col1.y * B.x + p.col2.y * B.y)
    };
    K.MulTMV = function (p, B) {
        return new A(K.Dot(B, p.col1), K.Dot(B, p.col2))
    };
    K.MulX = function (p, B) {
        var Q = K.MulMV(p.R, B);
        Q.x += p.position.x;
        Q.y += p.position.y;
        return Q
    };
    K.MulXT = function (p, B) {
        var Q = K.SubtractVV(B, p.position),
            V = Q.x * p.R.col1.x + Q.y * p.R.col1.y;
        Q.y = Q.x * p.R.col2.x + Q.y * p.R.col2.y;
        Q.x = V;
        return Q
    };
    K.AddVV = function (p, B) {
        return new A(p.x + B.x, p.y + B.y)
    };
    K.SubtractVV = function (p, B) {
        return new A(p.x - B.x, p.y - B.y)
    };
    K.Distance = function (p, B) {
        var Q = p.x - B.x,
            V = p.y - B.y;
        return Math.sqrt(Q * Q + V * V)
    };
    K.DistanceSquared = function (p, B) {
        var Q = p.x - B.x,
            V = p.y - B.y;
        return Q * Q + V * V
    };
    K.MulFV = function (p, B) {
        if (p === undefined) p = 0;
        return new A(p * B.x, p * B.y)
    };
    K.AddMM = function (p, B) {
        return F.FromVV(K.AddVV(p.col1, B.col1), K.AddVV(p.col2, B.col2))
    };
    K.MulMM = function (p, B) {
        return F.FromVV(K.MulMV(p,
            B.col1), K.MulMV(p, B.col2))
    };
    K.MulTMM = function (p, B) {
        var Q = new A(K.Dot(p.col1, B.col1), K.Dot(p.col2, B.col1)),
            V = new A(K.Dot(p.col1, B.col2), K.Dot(p.col2, B.col2));
        return F.FromVV(Q, V)
    };
    K.Abs = function (p) {
        if (p === undefined) p = 0;
        return p > 0 ? p : -p
    };
    K.AbsV = function (p) {
        return new A(K.Abs(p.x), K.Abs(p.y))
    };
    K.AbsM = function (p) {
        return F.FromVV(K.AbsV(p.col1), K.AbsV(p.col2))
    };
    K.Min = function (p, B) {
        if (p === undefined) p = 0;
        if (B === undefined) B = 0;
        return p < B ? p : B
    };
    K.MinV = function (p, B) {
        return new A(K.Min(p.x, B.x), K.Min(p.y, B.y))
    };
    K.Max = function (p, B) {
        if (p === undefined) p = 0;
        if (B === undefined) B = 0;
        return p > B ? p : B
    };
    K.MaxV = function (p, B) {
        return new A(K.Max(p.x, B.x), K.Max(p.y, B.y))
    };
    K.Clamp = function (p, B, Q) {
        if (p === undefined) p = 0;
        if (B === undefined) B = 0;
        if (Q === undefined) Q = 0;
        return p < B ? B : p > Q ? Q : p
    };
    K.ClampV = function (p, B, Q) {
        return K.MaxV(B, K.MinV(p, Q))
    };
    K.Swap = function (p, B) {
        var Q = p[0];
        p[0] = B[0];
        B[0] = Q
    };
    K.Random = function () {
        return Math.random() * 2 - 1
    };
    K.RandomRange = function (p, B) {
        if (p === undefined) p = 0;
        if (B === undefined) B = 0;
        var Q = Math.random();
        return Q = (B - p) * Q + p
    };
    K.NextPowerOfTwo = function (p) {
        if (p === undefined) p = 0;
        p |= p >> 1 & 2147483647;
        p |= p >> 2 & 1073741823;
        p |= p >> 4 & 268435455;
        p |= p >> 8 & 16777215;
        p |= p >> 16 & 65535;
        return p + 1
    };
    K.IsPowerOfTwo = function (p) {
        if (p === undefined) p = 0;
        return p > 0 && (p & p - 1) == 0
    };
    Box2D.postDefs.push(function () {
        Box2D.Common.Math.b2Math.b2Vec2_zero = new A(0, 0);
        Box2D.Common.Math.b2Math.b2Mat22_identity = F.FromVV(new A(1, 0), new A(0, 1));
        Box2D.Common.Math.b2Math.b2Transform_identity = new w(K.b2Vec2_zero, K.b2Mat22_identity)
    });
    y.b2Sweep = function () {
        this.localCenter = new A;
        this.c0 = new A;
        this.c = new A
    };
    y.prototype.Set = function (p) {
        this.localCenter.SetV(p.localCenter);
        this.c0.SetV(p.c0);
        this.c.SetV(p.c);
        this.a0 = p.a0;
        this.a = p.a;
        this.t0 = p.t0
    };
    y.prototype.Copy = function () {
        var p = new y;
        p.localCenter.SetV(this.localCenter);
        p.c0.SetV(this.c0);
        p.c.SetV(this.c);
        p.a0 = this.a0;
        p.a = this.a;
        p.t0 = this.t0;
        return p
    };
    y.prototype.GetTransform = function (p, B) {
        if (B === undefined) B = 0;
        p.position.x = (1 - B) * this.c0.x + B * this.c.x;
        p.position.y = (1 - B) * this.c0.y + B * this.c.y;
        p.R.Set((1 - B) * this.a0 + B * this.a);
        var Q = p.R;
        p.position.x -= Q.col1.x * this.localCenter.x + Q.col2.x * this.localCenter.y;
        p.position.y -= Q.col1.y * this.localCenter.x + Q.col2.y * this.localCenter.y
    };
    y.prototype.Advance = function (p) {
        if (p === undefined) p = 0;
        if (this.t0 < p && 1 - this.t0 > Number.MIN_VALUE) {
            var B = (p - this.t0) / (1 - this.t0);
            this.c0.x = (1 - B) * this.c0.x + B * this.c.x;
            this.c0.y = (1 - B) * this.c0.y + B * this.c.y;
            this.a0 = (1 - B) * this.a0 + B * this.a;
            this.t0 = p
        }
    };
    w.b2Transform = function () {
        this.position = new A;
        this.R = new F
    };
    w.prototype.b2Transform = function (p, B) {
        if (p === undefined) p = null;
        if (B === undefined) B = null;
        if (p) {
            this.position.SetV(p);
            this.R.SetM(B)
        }
    };
    w.prototype.Initialize = function (p, B) {
        this.position.SetV(p);
        this.R.SetM(B)
    };
    w.prototype.SetIdentity = function () {
        this.position.SetZero();
        this.R.SetIdentity()
    };
    w.prototype.Set = function (p) {
        this.position.SetV(p.position);
        this.R.SetM(p.R)
    };
    w.prototype.GetAngle = function () {
        return Math.atan2(this.R.col1.y, this.R.col1.x)
    };
    A.b2Vec2 = function () {};
    A.prototype.b2Vec2 = function (p, B) {
        if (p === undefined) p = 0;
        if (B === undefined) B = 0;
        this.x = p;
        this.y = B
    };
    A.prototype.SetZero = function () {
        this.y = this.x = 0
    };
    A.prototype.Set = function (p, B) {
        if (p === undefined) p = 0;
        if (B === undefined) B = 0;
        this.x = p;
        this.y = B
    };
    A.prototype.SetV = function (p) {
        this.x = p.x;
        this.y = p.y
    };
    A.prototype.GetNegative = function () {
        return new A(-this.x, -this.y)
    };
    A.prototype.NegativeSelf = function () {
        this.x = -this.x;
        this.y = -this.y
    };
    A.Make = function (p, B) {
        if (p === undefined) p = 0;
        if (B === undefined) B = 0;
        return new A(p, B)
    };
    A.prototype.Copy = function () {
        return new A(this.x, this.y)
    };
    A.prototype.Add = function (p) {
        this.x += p.x;
        this.y += p.y
    };
    A.prototype.Subtract = function (p) {
        this.x -= p.x;
        this.y -= p.y
    };
    A.prototype.Multiply = function (p) {
        if (p === undefined) p = 0;
        this.x *= p;
        this.y *= p
    };
    A.prototype.MulM = function (p) {
        var B = this.x;
        this.x = p.col1.x * B + p.col2.x * this.y;
        this.y = p.col1.y * B + p.col2.y * this.y
    };
    A.prototype.MulTM = function (p) {
        var B = K.Dot(this, p.col1);
        this.y = K.Dot(this, p.col2);
        this.x = B
    };
    A.prototype.CrossVF = function (p) {
        if (p === undefined) p = 0;
        var B = this.x;
        this.x = p * this.y;
        this.y = -p * B
    };
    A.prototype.CrossFV = function (p) {
        if (p === undefined) p = 0;
        var B = this.x;
        this.x = -p * this.y;
        this.y = p * B
    };
    A.prototype.MinV = function (p) {
        this.x = this.x < p.x ? this.x : p.x;
        this.y = this.y < p.y ? this.y : p.y
    };
    A.prototype.MaxV = function (p) {
        this.x = this.x > p.x ? this.x : p.x;
        this.y = this.y > p.y ? this.y : p.y
    };
    A.prototype.Abs = function () {
        if (this.x < 0) this.x = -this.x;
        if (this.y < 0) this.y = -this.y
    };
    A.prototype.Length = function () {
        return Math.sqrt(this.x * this.x + this.y * this.y)
    };
    A.prototype.LengthSquared = function () {
        return this.x * this.x + this.y * this.y
    };
    A.prototype.Normalize = function () {
        var p = Math.sqrt(this.x * this.x + this.y * this.y);
        if (p < Number.MIN_VALUE) return 0;
        var B = 1 / p;
        this.x *= B;
        this.y *= B;
        return p
    };
    A.prototype.IsValid = function () {
        return K.IsValid(this.x) && K.IsValid(this.y)
    };
    U.b2Vec3 = function () {};
    U.prototype.b2Vec3 = function (p, B, Q) {
        if (p === undefined) p = 0;
        if (B === undefined) B = 0;
        if (Q === undefined) Q = 0;
        this.x = p;
        this.y = B;
        this.z = Q
    };
    U.prototype.SetZero = function () {
        this.x = this.y = this.z = 0
    };
    U.prototype.Set = function (p, B, Q) {
        if (p === undefined) p = 0;
        if (B === undefined) B = 0;
        if (Q === undefined) Q = 0;
        this.x = p;
        this.y = B;
        this.z = Q
    };
    U.prototype.SetV = function (p) {
            this.x = p.x;
            this.y = p.y;
            this.z = p.z
    };
    U.prototype.GetNegative = function () {
        return new U(-this.x, -this.y, -this.z)
    };
    U.prototype.NegativeSelf = function () {
        this.x = -this.x;
        this.y = -this.y;
        this.z = -this.z
    };
    U.prototype.Copy = function () {
        return new U(this.x, this.y, this.z)
    };
    U.prototype.Add = function (p) {
        this.x += p.x;
        this.y += p.y;
        this.z += p.z
    };
    U.prototype.Subtract = function (p) {
        this.x -= p.x;
        this.y -= p.y;
        this.z -= p.z
    };
    U.prototype.Multiply = function (p) {
        if (p === undefined) p = 0;
        this.x *= p;
        this.y *= p;
        this.z *= p
    }
})();
(function () {
    var F = Box2D.Common.Math.b2Math,
        G = Box2D.Common.Math.b2Sweep,
        K = Box2D.Common.Math.b2Transform,
        y = Box2D.Common.Math.b2Vec2,
        w = Box2D.Common.b2Color,
        A = Box2D.Common.b2Settings,
        U = Box2D.Collision.b2AABB,
        p = Box2D.Collision.b2ContactPoint,
        B = Box2D.Collision.b2DynamicTreeBroadPhase,
        Q = Box2D.Collision.b2RayCastInput,
        V = Box2D.Collision.b2RayCastOutput,
        M = Box2D.Collision.Shapes.b2CircleShape,
        L = Box2D.Collision.Shapes.b2EdgeShape,
        I = Box2D.Collision.Shapes.b2MassData,
        W = Box2D.Collision.Shapes.b2PolygonShape,
        Y = Box2D.Collision.Shapes.b2Shape,
        k = Box2D.Dynamics.b2Body,
        z = Box2D.Dynamics.b2BodyDef,
        u = Box2D.Dynamics.b2ContactFilter,
        D = Box2D.Dynamics.b2ContactImpulse,
        H = Box2D.Dynamics.b2ContactListener,
        O = Box2D.Dynamics.b2ContactManager,
        E = Box2D.Dynamics.b2DebugDraw,
        R = Box2D.Dynamics.b2DestructionListener,
        N = Box2D.Dynamics.b2FilterData,
        S = Box2D.Dynamics.b2Fixture,
        aa = Box2D.Dynamics.b2FixtureDef,
        Z = Box2D.Dynamics.b2Island,
        d = Box2D.Dynamics.b2TimeStep,
        h = Box2D.Dynamics.b2World,
        l = Box2D.Dynamics.Contacts.b2Contact,
        j = Box2D.Dynamics.Contacts.b2ContactFactory,
        o = Box2D.Dynamics.Contacts.b2ContactSolver,
        q = Box2D.Dynamics.Joints.b2Joint,
        n = Box2D.Dynamics.Joints.b2PulleyJoint;
    k.b2Body = function () {
        this.m_xf = new K;
        this.m_sweep = new G;
        this.m_linearVelocity = new y;
        this.m_force = new y
    };
    k.prototype.connectEdges = function (a, c, g) {
        if (g === undefined) g = 0;
        var b = Math.atan2(c.GetDirectionVector().y, c.GetDirectionVector().x);
        g = F.MulFV(Math.tan((b - g) * 0.5), c.GetDirectionVector());
        g = F.SubtractVV(g, c.GetNormalVector());
        g = F.MulFV(A.b2_toiSlop, g);
        g = F.AddVV(g, c.GetVertex1());
        var e = F.AddVV(a.GetDirectionVector(),
            c.GetDirectionVector());
        e.Normalize();
        var f = F.Dot(a.GetDirectionVector(), c.GetNormalVector()) > 0;
        a.SetNextEdge(c, g, e, f);
        c.SetPrevEdge(a, g, e, f);
        return b
    };
    k.prototype.CreateFixture = function (a) {
        if (this.m_world.IsLocked() == true) return null;
        var c = new S;
        c.Create(this, this.m_xf, a);
        this.m_flags & k.e_activeFlag && c.CreateProxy(this.m_world.m_contactManager.m_broadPhase, this.m_xf);
        c.m_next = this.m_fixtureList;
        this.m_fixtureList = c;
        ++this.m_fixtureCount;
        c.m_body = this;
        c.m_density > 0 && this.ResetMassData();
        this.m_world.m_flags |= h.e_newFixture;
        return c
    };
    k.prototype.CreateFixture2 = function (a, c) {
        if (c === undefined) c = 0;
        var g = new aa;
        g.shape = a;
        g.density = c;
        return this.CreateFixture(g)
    };
    k.prototype.DestroyFixture = function (a) {
        if (this.m_world.IsLocked() != true) {
            for (var c = this.m_fixtureList, g = null; c != null;) {
                if (c == a) {
                    if (g) g.m_next = a.m_next;
                    else this.m_fixtureList = a.m_next;
                    break
                }
                g = c;
                c = c.m_next
            }
            for (c = this.m_contactList; c;) {
                g = c.contact;
                c = c.next;
                var b = g.GetFixtureA(),
                    e = g.GetFixtureB();
                if (a == b || a == e) this.m_world.m_contactManager.Destroy(g)
            }
            this.m_flags &
                k.e_activeFlag && a.DestroyProxy(this.m_world.m_contactManager.m_broadPhase);
            a.Destroy();
            a.m_body = null;
            a.m_next = null;
            --this.m_fixtureCount;
            this.ResetMassData()
        }
    };
    k.prototype.SetPositionAndAngle = function (a, c) {
        if (c === undefined) c = 0;
        var g;
        if (this.m_world.IsLocked() != true) {
            this.m_xf.R.Set(c);
            this.m_xf.position.SetV(a);
            g = this.m_xf.R;
            var b = this.m_sweep.localCenter;
            this.m_sweep.c.x = g.col1.x * b.x + g.col2.x * b.y;
            this.m_sweep.c.y = g.col1.y * b.x + g.col2.y * b.y;
            this.m_sweep.c.x += this.m_xf.position.x;
            this.m_sweep.c.y += this.m_xf.position.y;
            this.m_sweep.c0.SetV(this.m_sweep.c);
            this.m_sweep.a0 = this.m_sweep.a = c;
            b = this.m_world.m_contactManager.m_broadPhase;
            for (g = this.m_fixtureList; g; g = g.m_next) g.Synchronize(b, this.m_xf, this.m_xf);
            this.m_world.m_contactManager.FindNewContacts()
        }
    };
    k.prototype.SetTransform = function (a) {
        this.SetPositionAndAngle(a.position, a.GetAngle())
    };
    k.prototype.GetTransform = function () {
        return this.m_xf
    };
    k.prototype.GetPosition = function () {
        return this.m_xf.position
    };
    k.prototype.SetPosition = function (a) {
        this.SetPositionAndAngle(a,
            this.GetAngle())
    };
    k.prototype.GetAngle = function () {
        return this.m_sweep.a
    };
    k.prototype.SetAngle = function (a) {
        if (a === undefined) a = 0;
        this.SetPositionAndAngle(this.GetPosition(), a)
    };
    k.prototype.GetWorldCenter = function () {
        return this.m_sweep.c
    };
    k.prototype.GetLocalCenter = function () {
        return this.m_sweep.localCenter
    };
    k.prototype.SetLinearVelocity = function (a) {
        this.m_type != k.b2_staticBody && this.m_linearVelocity.SetV(a)
    };
    k.prototype.GetLinearVelocity = function () {
        return this.m_linearVelocity
    };
    k.prototype.SetAngularVelocity = function (a) {
            if (a === undefined) a = 0;
            if (this.m_type != k.b2_staticBody) this.m_angularVelocity = a
    };
    k.prototype.GetAngularVelocity = function () {
        return this.m_angularVelocity
    };
    k.prototype.GetDefinition = function () {
        var a = new z;
        a.type = this.GetType();
        a.allowSleep = (this.m_flags & k.e_allowSleepFlag) == k.e_allowSleepFlag;
        a.angle = this.GetAngle();
        a.angularDamping = this.m_angularDamping;
        a.angularVelocity = this.m_angularVelocity;
        a.fixedRotation = (this.m_flags & k.e_fixedRotationFlag) == k.e_fixedRotationFlag;
        a.bullet = (this.m_flags &
            k.e_bulletFlag) == k.e_bulletFlag;
        a.awake = (this.m_flags & k.e_awakeFlag) == k.e_awakeFlag;
        a.linearDamping = this.m_linearDamping;
        a.linearVelocity.SetV(this.GetLinearVelocity());
        a.position = this.GetPosition();
        a.userData = this.GetUserData();
        return a
    };
    k.prototype.ApplyForce = function (a, c) {
        if (this.m_type == k.b2_dynamicBody) {
            this.IsAwake() == false && this.SetAwake(true);
            this.m_force.x += a.x;
            this.m_force.y += a.y;
            this.m_torque += (c.x - this.m_sweep.c.x) * a.y - (c.y - this.m_sweep.c.y) * a.x
        }
    };
    k.prototype.ApplyTorque = function (a) {
        if (a === undefined) a = 0;
        if (this.m_type == k.b2_dynamicBody) {
            this.IsAwake() == false && this.SetAwake(true);
            this.m_torque += a
        }
    };
    k.prototype.ApplyImpulse = function (a, c) {
        if (this.m_type == k.b2_dynamicBody) {
            this.IsAwake() == false && this.SetAwake(true);
            this.m_linearVelocity.x += this.m_invMass * a.x;
            this.m_linearVelocity.y += this.m_invMass * a.y;
            this.m_angularVelocity += this.m_invI * ((c.x - this.m_sweep.c.x) * a.y - (c.y - this.m_sweep.c.y) * a.x)
        }
    };
    k.prototype.Split = function (a) {
        for (var c = this.GetLinearVelocity().Copy(), g = this.GetAngularVelocity(),
                b = this.GetWorldCenter(), e = this.m_world.CreateBody(this.GetDefinition()), f, m = this.m_fixtureList; m;)
            if (a(m)) {
                var r = m.m_next;
                if (f) f.m_next = r;
                else this.m_fixtureList = r;
                this.m_fixtureCount--;
                m.m_next = e.m_fixtureList;
                e.m_fixtureList = m;
                e.m_fixtureCount++;
                m.m_body = e;
                m = r
            } else {
                f = m;
                m = m.m_next
            }
        this.ResetMassData();
        e.ResetMassData();
        f = this.GetWorldCenter();
        a = e.GetWorldCenter();
        f = F.AddVV(c, F.CrossFV(g, F.SubtractVV(f, b)));
        c = F.AddVV(c, F.CrossFV(g, F.SubtractVV(a, b)));
        this.SetLinearVelocity(f);
        e.SetLinearVelocity(c);
        this.SetAngularVelocity(g);
        e.SetAngularVelocity(g);
        this.SynchronizeFixtures();
        e.SynchronizeFixtures();
        return e
    };
    k.prototype.Merge = function (a) {
        var c;
        for (c = a.m_fixtureList; c;) {
            var g = c.m_next;
            a.m_fixtureCount--;
            c.m_next = this.m_fixtureList;
            this.m_fixtureList = c;
            this.m_fixtureCount++;
            c.m_body = e;
            c = g
        }
        b.m_fixtureCount = 0;
        var b = this,
            e = a;
        b.GetWorldCenter();
        e.GetWorldCenter();
        b.GetLinearVelocity().Copy();
        e.GetLinearVelocity().Copy();
        b.GetAngularVelocity();
        e.GetAngularVelocity();
        b.ResetMassData();
        this.SynchronizeFixtures()
    };
    k.prototype.GetMass = function () {
        return this.m_mass
    };
    k.prototype.GetInertia = function () {
        return this.m_I
    };
    k.prototype.GetMassData = function (a) {
        a.mass = this.m_mass;
        a.I = this.m_I;
        a.center.SetV(this.m_sweep.localCenter)
    };
    k.prototype.SetMassData = function (a) {
        A.b2Assert(this.m_world.IsLocked() == false);
        if (this.m_world.IsLocked() != true)
            if (this.m_type == k.b2_dynamicBody) {
                this.m_invI = this.m_I = this.m_invMass = 0;
                this.m_mass = a.mass;
                if (this.m_mass <= 0) this.m_mass = 1;
                this.m_invMass = 1 / this.m_mass;
                if (a.I > 0 && (this.m_flags & k.e_fixedRotationFlag) == 0) {
                    this.m_I = a.I - this.m_mass * (a.center.x * a.center.x + a.center.y * a.center.y);
                    this.m_invI = 1 / this.m_I
                }
                var c = this.m_sweep.c.Copy();
                this.m_sweep.localCenter.SetV(a.center);
                this.m_sweep.c0.SetV(F.MulX(this.m_xf, this.m_sweep.localCenter));
                this.m_sweep.c.SetV(this.m_sweep.c0);
                this.m_linearVelocity.x += this.m_angularVelocity * -(this.m_sweep.c.y - c.y);
                this.m_linearVelocity.y += this.m_angularVelocity * +(this.m_sweep.c.x - c.x)
            }
    };
    k.prototype.ResetMassData = function () {
        this.m_invI = this.m_I = this.m_invMass = this.m_mass = 0;
        this.m_sweep.localCenter.SetZero();
        if (!(this.m_type == k.b2_staticBody || this.m_type == k.b2_kinematicBody)) {
            for (var a = y.Make(0, 0), c = this.m_fixtureList; c; c = c.m_next)
                if (c.m_density != 0) {
                    var g = c.GetMassData();
                    this.m_mass += g.mass;
                    a.x += g.center.x * g.mass;
                    a.y += g.center.y * g.mass;
                    this.m_I += g.I
                }
            if (this.m_mass > 0) {
                this.m_invMass = 1 / this.m_mass;
                a.x *= this.m_invMass;
                a.y *= this.m_invMass
            } else this.m_invMass = this.m_mass = 1; if (this.m_I > 0 && (this.m_flags & k.e_fixedRotationFlag) == 0) {
                this.m_I -= this.m_mass * (a.x * a.x + a.y * a.y);
                this.m_I *= this.m_inertiaScale;
                A.b2Assert(this.m_I > 0);
                this.m_invI = 1 / this.m_I
            } else this.m_invI = this.m_I = 0;
            c = this.m_sweep.c.Copy();
            this.m_sweep.localCenter.SetV(a);
            this.m_sweep.c0.SetV(F.MulX(this.m_xf, this.m_sweep.localCenter));
            this.m_sweep.c.SetV(this.m_sweep.c0);
            this.m_linearVelocity.x += this.m_angularVelocity * -(this.m_sweep.c.y - c.y);
            this.m_linearVelocity.y += this.m_angularVelocity * +(this.m_sweep.c.x - c.x)
        }
    };
    k.prototype.GetWorldPoint = function (a) {
        var c = this.m_xf.R;
        a = new y(c.col1.x * a.x + c.col2.x * a.y, c.col1.y * a.x + c.col2.y * a.y);
        a.x += this.m_xf.position.x;
        a.y += this.m_xf.position.y;
        return a
    };
    k.prototype.GetWorldVector = function (a) {
        return F.MulMV(this.m_xf.R, a)
    };
    k.prototype.GetLocalPoint = function (a) {
        return F.MulXT(this.m_xf, a)
    };
    k.prototype.GetLocalVector = function (a) {
        return F.MulTMV(this.m_xf.R, a)
    };
    k.prototype.GetLinearVelocityFromWorldPoint = function (a) {
        return new y(this.m_linearVelocity.x - this.m_angularVelocity * (a.y - this.m_sweep.c.y), this.m_linearVelocity.y + this.m_angularVelocity * (a.x - this.m_sweep.c.x))
    };
    k.prototype.GetLinearVelocityFromLocalPoint = function (a) {
        var c = this.m_xf.R;
        a = new y(c.col1.x * a.x + c.col2.x * a.y, c.col1.y * a.x + c.col2.y * a.y);
        a.x += this.m_xf.position.x;
        a.y += this.m_xf.position.y;
        return new y(this.m_linearVelocity.x - this.m_angularVelocity * (a.y - this.m_sweep.c.y), this.m_linearVelocity.y + this.m_angularVelocity * (a.x - this.m_sweep.c.x))
    };
    k.prototype.GetLinearDamping = function () {
        return this.m_linearDamping
    };
    k.prototype.SetLinearDamping = function (a) {
        if (a === undefined) a = 0;
        this.m_linearDamping = a
    };
    k.prototype.GetAngularDamping = function () {
        return this.m_angularDamping
    };
    k.prototype.SetAngularDamping = function (a) {
        if (a === undefined) a = 0;
        this.m_angularDamping = a
    };
    k.prototype.SetType = function (a) {
        if (a === undefined) a = 0;
        if (this.m_type != a) {
            this.m_type = a;
            this.ResetMassData();
            if (this.m_type == k.b2_staticBody) {
                this.m_linearVelocity.SetZero();
                this.m_angularVelocity = 0
            }
            this.SetAwake(true);
            this.m_force.SetZero();
            this.m_torque = 0;
            for (a = this.m_contactList; a; a = a.next) a.contact.FlagForFiltering()
        }
    };
    k.prototype.GetType = function () {
        return this.m_type
    };
    k.prototype.SetBullet = function (a) {
        if (a) this.m_flags |= k.e_bulletFlag;
        else this.m_flags &= ~k.e_bulletFlag
    };
    k.prototype.IsBullet = function () {
        return (this.m_flags & k.e_bulletFlag) == k.e_bulletFlag
    };
    k.prototype.SetSleepingAllowed = function (a) {
        if (a) this.m_flags |= k.e_allowSleepFlag;
        else {
            this.m_flags &= ~k.e_allowSleepFlag;
            this.SetAwake(true)
        }
    };
    k.prototype.SetAwake = function (a) {
        if (a) {
            this.m_flags |= k.e_awakeFlag;
            this.m_sleepTime = 0
        } else {
            this.m_flags &= ~k.e_awakeFlag;
            this.m_sleepTime = 0;
            this.m_linearVelocity.SetZero();
            this.m_angularVelocity = 0;
            this.m_force.SetZero();
            this.m_torque = 0
        }
    };
    k.prototype.IsAwake = function () {
        return (this.m_flags & k.e_awakeFlag) == k.e_awakeFlag
    };
    k.prototype.SetFixedRotation = function (a) {
        if (a) this.m_flags |= k.e_fixedRotationFlag;
        else this.m_flags &= ~k.e_fixedRotationFlag;
        this.ResetMassData()
    };
    k.prototype.IsFixedRotation = function () {
        return (this.m_flags & k.e_fixedRotationFlag) == k.e_fixedRotationFlag
    };
    k.prototype.SetActive = function (a) {
        if (a != this.IsActive()) {
            var c;
            if (a) {
                this.m_flags |= k.e_activeFlag;
                a = this.m_world.m_contactManager.m_broadPhase;
                for (c = this.m_fixtureList; c; c = c.m_next) c.CreateProxy(a, this.m_xf)
            } else {
                this.m_flags &= ~k.e_activeFlag;
                a = this.m_world.m_contactManager.m_broadPhase;
                for (c = this.m_fixtureList; c; c = c.m_next) c.DestroyProxy(a);
                for (a = this.m_contactList; a;) {
                    c = a;
                    a = a.next;
                    this.m_world.m_contactManager.Destroy(c.contact)
                }
                this.m_contactList = null
            }
        }
    };
    k.prototype.IsActive = function () {
        return (this.m_flags & k.e_activeFlag) == k.e_activeFlag
    };
    k.prototype.IsSleepingAllowed = function () {
        return (this.m_flags & k.e_allowSleepFlag) == k.e_allowSleepFlag
    };
    k.prototype.GetFixtureList = function () {
            return this.m_fixtureList
    };
    k.prototype.GetJointList = function () {
        return this.m_jointList
    };
    k.prototype.GetControllerList = function () {
        return this.m_controllerList
    };
    k.prototype.GetContactList = function () {
        return this.m_contactList
    };
    k.prototype.GetNext = function () {
        return this.m_next
    };
    k.prototype.GetUserData = function () {
        return this.m_userData
    };
    k.prototype.SetUserData = function (a) {
        this.m_userData = a
    };
    k.prototype.GetWorld = function () {
        return this.m_world
    };
    k.prototype.b2Body = function (a, c) {
        this.m_flags = 0;
        if (a.bullet) this.m_flags |= k.e_bulletFlag;
        if (a.fixedRotation) this.m_flags |= k.e_fixedRotationFlag;
        if (a.allowSleep) this.m_flags |= k.e_allowSleepFlag;
        if (a.awake) this.m_flags |= k.e_awakeFlag;
        if (a.active) this.m_flags |= k.e_activeFlag;
        this.m_world = c;
        this.m_xf.position.SetV(a.position);
        this.m_xf.R.Set(a.angle);
        this.m_sweep.localCenter.SetZero();
        this.m_sweep.t0 = 1;
        this.m_sweep.a0 = this.m_sweep.a = a.angle;
        var g = this.m_xf.R,
            b = this.m_sweep.localCenter;
        this.m_sweep.c.x = g.col1.x * b.x + g.col2.x * b.y;
        this.m_sweep.c.y = g.col1.y * b.x + g.col2.y * b.y;
        this.m_sweep.c.x += this.m_xf.position.x;
        this.m_sweep.c.y += this.m_xf.position.y;
        this.m_sweep.c0.SetV(this.m_sweep.c);
        this.m_contactList = this.m_controllerList = this.m_jointList = null;
        this.m_controllerCount = 0;
        this.m_next = this.m_prev = null;
        this.m_linearVelocity.SetV(a.linearVelocity);
        this.m_angularVelocity = a.angularVelocity;
        this.m_linearDamping = a.linearDamping;
        this.m_angularDamping = a.angularDamping;
        this.m_force.Set(0, 0);
        this.m_sleepTime = this.m_torque = 0;
        this.m_type = a.type;
        if (this.m_type == k.b2_dynamicBody) this.m_invMass = this.m_mass = 1;
        else this.m_invMass = this.m_mass = 0;
        this.m_invI = this.m_I = 0;
        this.m_inertiaScale = a.inertiaScale;
        this.m_userData = a.userData;
        this.m_fixtureList = null;
        this.m_fixtureCount = 0
    };
    k.prototype.SynchronizeFixtures = function () {
        var a = k.s_xf1;
        a.R.Set(this.m_sweep.a0);
        var c = a.R,
            g = this.m_sweep.localCenter;
        a.position.x = this.m_sweep.c0.x - (c.col1.x * g.x + c.col2.x * g.y);
        a.position.y = this.m_sweep.c0.y - (c.col1.y * g.x + c.col2.y * g.y);
        g = this.m_world.m_contactManager.m_broadPhase;
        for (c = this.m_fixtureList; c; c = c.m_next) c.Synchronize(g, a, this.m_xf)
    };
    k.prototype.SynchronizeTransform = function () {
        this.m_xf.R.Set(this.m_sweep.a);
        var a = this.m_xf.R,
            c = this.m_sweep.localCenter;
        this.m_xf.position.x = this.m_sweep.c.x - (a.col1.x * c.x + a.col2.x * c.y);
        this.m_xf.position.y = this.m_sweep.c.y - (a.col1.y * c.x + a.col2.y * c.y)
    };
    k.prototype.ShouldCollide = function (a) {
        if (this.m_type != k.b2_dynamicBody && a.m_type != k.b2_dynamicBody) return false;
        for (var c = this.m_jointList; c; c = c.next)
            if (c.other == a)
                if (c.joint.m_collideConnected == false) return false;
        return true
    };
    k.prototype.Advance = function (a) {
        if (a === undefined) a = 0;
        this.m_sweep.Advance(a);
        this.m_sweep.c.SetV(this.m_sweep.c0);
        this.m_sweep.a = this.m_sweep.a0;
        this.SynchronizeTransform()
    };
    Box2D.postDefs.push(function () {
        Box2D.Dynamics.b2Body.s_xf1 = new K;
        Box2D.Dynamics.b2Body.e_islandFlag = 1;
        Box2D.Dynamics.b2Body.e_awakeFlag = 2;
        Box2D.Dynamics.b2Body.e_allowSleepFlag = 4;
        Box2D.Dynamics.b2Body.e_bulletFlag = 8;
        Box2D.Dynamics.b2Body.e_fixedRotationFlag = 16;
        Box2D.Dynamics.b2Body.e_activeFlag = 32;
        Box2D.Dynamics.b2Body.b2_staticBody = 0;
        Box2D.Dynamics.b2Body.b2_kinematicBody = 1;
        Box2D.Dynamics.b2Body.b2_dynamicBody = 2
    });
    z.b2BodyDef = function () {
        this.position = new y;
        this.linearVelocity = new y
    };
    z.prototype.b2BodyDef = function () {
        this.userData = null;
        this.position.Set(0, 0);
        this.angle = 0;
        this.linearVelocity.Set(0, 0);
        this.angularDamping = this.linearDamping = this.angularVelocity = 0;
        this.awake = this.allowSleep = true;
        this.bullet = this.fixedRotation = false;
        this.type = k.b2_staticBody;
        this.active = true;
        this.inertiaScale = 1
    };
    u.b2ContactFilter = function () {};
    u.prototype.ShouldCollide = function (a, c) {
            var g = a.GetFilterData(),
                b = c.GetFilterData();
            if (g.groupIndex == b.groupIndex && g.groupIndex != 0) return g.groupIndex > 0;
            return (g.maskBits & b.categoryBits) != 0 && (g.categoryBits & b.maskBits) != 0
    };
    u.prototype.RayCollide = function (a, c) {
        if (!a) return true;
        return this.ShouldCollide(a instanceof S ? a : null, c)
    };
    Box2D.postDefs.push(function () {
        Box2D.Dynamics.b2ContactFilter.b2_defaultFilter = new u
    });
    D.b2ContactImpulse = function () {
        this.normalImpulses = new Vector_a2j_Number(A.b2_maxManifoldPoints);
        this.tangentImpulses = new Vector_a2j_Number(A.b2_maxManifoldPoints)
    };
    H.b2ContactListener = function () {};
    H.prototype.BeginContact = function () {};
    H.prototype.EndContact = function () {};
    H.prototype.PreSolve = function () {};
    H.prototype.PostSolve = function () {};
    Box2D.postDefs.push(function () {
        Box2D.Dynamics.b2ContactListener.b2_defaultListener = new H
    });
    O.b2ContactManager = function () {};
    O.prototype.b2ContactManager = function () {
        this.m_world = null;
        this.m_contactCount = 0;
        this.m_contactFilter = u.b2_defaultFilter;
        this.m_contactListener = H.b2_defaultListener;
        this.m_contactFactory = new j(this.m_allocator);
        this.m_broadPhase = new B
    };
    O.prototype.AddPair = function (a, c) {
        var g = a instanceof S ? a : null,
            b = c instanceof S ? c : null,
            e = g.GetBody(),
            f = b.GetBody();
        if (e != f) {
            for (var m = f.GetContactList(); m;) {
                if (m.other == e) {
                    var r = m.contact.GetFixtureA(),
                        s = m.contact.GetFixtureB();
                    if (r == g && s == b) return;
                    if (r == b && s == g) return
                }
                m = m.next
            }
            if (f.ShouldCollide(e) != false)
                if (this.m_contactFilter.ShouldCollide(g, b) != false) {
                    m = this.m_contactFactory.Create(g, b);
                    g = m.GetFixtureA();
                    b = m.GetFixtureB();
                    e = g.m_body;
                    f = b.m_body;
                    m.m_prev = null;
                    m.m_next = this.m_world.m_contactList;
                    if (this.m_world.m_contactList != null) this.m_world.m_contactList.m_prev = m;
                    this.m_world.m_contactList = m;
                    m.m_nodeA.contact = m;
                    m.m_nodeA.other = f;
                    m.m_nodeA.prev = null;
                    m.m_nodeA.next = e.m_contactList;
                    if (e.m_contactList != null) e.m_contactList.prev = m.m_nodeA;
                    e.m_contactList = m.m_nodeA;
                    m.m_nodeB.contact = m;
                    m.m_nodeB.other = e;
                    m.m_nodeB.prev = null;
                    m.m_nodeB.next = f.m_contactList;
                    if (f.m_contactList != null) f.m_contactList.prev = m.m_nodeB;
                    f.m_contactList = m.m_nodeB;
                    ++this.m_world.m_contactCount
                }
        }
    };
    O.prototype.FindNewContacts = function () {
        this.m_broadPhase.UpdatePairs(Box2D.generateCallback(this, this.AddPair))
    };
    O.prototype.Destroy = function (a) {
        var c = a.GetFixtureA(),
            g = a.GetFixtureB();
        c = c.GetBody();
        g = g.GetBody();
        a.IsTouching() && this.m_contactListener.EndContact(a);
        if (a.m_prev) a.m_prev.m_next = a.m_next;
        if (a.m_next) a.m_next.m_prev = a.m_prev;
        if (a == this.m_world.m_contactList) this.m_world.m_contactList = a.m_next;
        if (a.m_nodeA.prev) a.m_nodeA.prev.next = a.m_nodeA.next;
        if (a.m_nodeA.next) a.m_nodeA.next.prev = a.m_nodeA.prev;
        if (a.m_nodeA == c.m_contactList) c.m_contactList = a.m_nodeA.next;
        if (a.m_nodeB.prev) a.m_nodeB.prev.next = a.m_nodeB.next;
        if (a.m_nodeB.next) a.m_nodeB.next.prev = a.m_nodeB.prev;
        if (a.m_nodeB == g.m_contactList) g.m_contactList = a.m_nodeB.next;
        this.m_contactFactory.Destroy(a);
        --this.m_contactCount
    };
    O.prototype.Collide = function () {
        for (var a = this.m_world.m_contactList; a;) {
            var c = a.GetFixtureA(),
                g = a.GetFixtureB(),
                b = c.GetBody(),
                e = g.GetBody();
            if (b.IsAwake() == false && e.IsAwake() == false) a = a.GetNext();
            else {
                if (a.m_flags & l.e_filterFlag) {
                    if (e.ShouldCollide(b) == false) {
                        c = a;
                        a = c.GetNext();
                        this.Destroy(c);
                        continue
                    }
                    if (this.m_contactFilter.ShouldCollide(c, g) == false) {
                        c = a;
                        a = c.GetNext();
                        this.Destroy(c);
                        continue
                    }
                    a.m_flags &= ~l.e_filterFlag
                }
                if (this.m_broadPhase.TestOverlap(c.m_proxy, g.m_proxy) == false) {
                    c = a;
                    a = c.GetNext();
                    this.Destroy(c)
                } else {
                    a.Update(this.m_contactListener);
                    a = a.GetNext()
                }
            }
        }
    };
    Box2D.postDefs.push(function () {
        Box2D.Dynamics.b2ContactManager.s_evalCP = new p
    });
    E.b2DebugDraw = function () {};
    E.prototype.b2DebugDraw = function () {};
    E.prototype.SetFlags = function () {};
    E.prototype.GetFlags = function () {};
    E.prototype.AppendFlags = function () {};
    E.prototype.ClearFlags = function () {};
    E.prototype.SetSprite = function () {};
    E.prototype.GetSprite = function () {};
    E.prototype.SetDrawScale = function () {};
    E.prototype.GetDrawScale = function () {};
    E.prototype.SetLineThickness = function () {};
    E.prototype.GetLineThickness = function () {};
    E.prototype.SetAlpha = function () {};
    E.prototype.GetAlpha = function () {};
    E.prototype.SetFillAlpha = function () {};
    E.prototype.GetFillAlpha = function () {};
    E.prototype.SetXFormScale = function () {};
    E.prototype.GetXFormScale = function () {};
    E.prototype.DrawPolygon = function () {};
    E.prototype.DrawSolidPolygon = function () {};
    E.prototype.DrawCircle = function () {};
    E.prototype.DrawSolidCircle = function () {};
    E.prototype.DrawSegment = function () {};
    E.prototype.DrawTransform = function () {};
    Box2D.postDefs.push(function () {
        Box2D.Dynamics.b2DebugDraw.e_shapeBit = 1;
        Box2D.Dynamics.b2DebugDraw.e_jointBit = 2;
        Box2D.Dynamics.b2DebugDraw.e_aabbBit = 4;
        Box2D.Dynamics.b2DebugDraw.e_pairBit = 8;
        Box2D.Dynamics.b2DebugDraw.e_centerOfMassBit = 16;
        Box2D.Dynamics.b2DebugDraw.e_controllerBit = 32
    });
    R.b2DestructionListener = function () {};
    R.prototype.SayGoodbyeJoint = function () {};
    R.prototype.SayGoodbyeFixture = function () {};
    N.b2FilterData = function () {
        this.categoryBits = 1;
        this.maskBits = 65535;
        this.groupIndex = 0
    };
    N.prototype.Copy = function () {
        var a = new N;
        a.categoryBits = this.categoryBits;
        a.maskBits = this.maskBits;
        a.groupIndex = this.groupIndex;
        return a
    };
    S.b2Fixture = function () {
        this.m_filter = new N
    };
    S.prototype.GetType = function () {
        return this.m_shape.GetType()
    };
    S.prototype.GetShape = function () {
        return this.m_shape
    };
    S.prototype.SetSensor = function (a) {
        if (this.m_isSensor != a) {
            this.m_isSensor = a;
            if (this.m_body != null) for (a = this.m_body.GetContactList(); a;) {
                    var c = a.contact,
                        g = c.GetFixtureA(),
                        b = c.GetFixtureB();
                    if (g == this || b == this) c.SetSensor(g.IsSensor() || b.IsSensor());
                    a = a.next
                }
        }
    };
    S.prototype.IsSensor = function () {
        return this.m_isSensor
    };
    S.prototype.SetFilterData = function (a) {
        this.m_filter = a.Copy();
        if (!this.m_body) for (a = this.m_body.GetContactList(); a;) {
                var c = a.contact,
                    g = c.GetFixtureA(),
                    b = c.GetFixtureB();
                if (g == this || b == this) c.FlagForFiltering();
                a = a.next
            }
    };
    S.prototype.GetFilterData = function () {
        return this.m_filter.Copy()
    };
    S.prototype.GetBody = function () {
        return this.m_body
    };
    S.prototype.GetNext = function () {
        return this.m_next
    };
    S.prototype.GetUserData = function () {
        return this.m_userData
    };
    S.prototype.SetUserData = function (a) {
        this.m_userData = a
    };
    S.prototype.TestPoint = function (a) {
        return this.m_shape.TestPoint(this.m_body.GetTransform(),
            a)
    };
    S.prototype.RayCast = function (a, c) {
        return this.m_shape.RayCast(a, c, this.m_body.GetTransform())
    };
    S.prototype.GetMassData = function (a) {
        if (a === undefined) a = null;
        if (a == null) a = new I;
        this.m_shape.ComputeMass(a, this.m_density);
        return a
    };
    S.prototype.SetDensity = function (a) {
        if (a === undefined) a = 0;
        this.m_density = a
    };
    S.prototype.GetDensity = function () {
        return this.m_density
    };
    S.prototype.GetFriction = function () {
        return this.m_friction
    };
    S.prototype.SetFriction = function (a) {
        if (a === undefined) a = 0;
        this.m_friction = a
    };
    S.prototype.GetRestitution = function () {
            return this.m_restitution
    };
    S.prototype.SetRestitution = function (a) {
        if (a === undefined) a = 0;
        this.m_restitution = a
    };
    S.prototype.GetAABB = function () {
        return this.m_aabb
    };
    S.prototype.b2Fixture = function () {
        this.m_aabb = new U;
        this.m_shape = this.m_next = this.m_body = this.m_userData = null;
        this.m_restitution = this.m_friction = this.m_density = 0
    };
    S.prototype.Create = function (a, c, g) {
        this.m_userData = g.userData;
        this.m_friction = g.friction;
        this.m_restitution = g.restitution;
        this.m_body = a;
        this.m_next = null;
        this.m_filter = g.filter.Copy();
        this.m_isSensor = g.isSensor;
        this.m_shape = g.shape.Copy();
        this.m_density = g.density
    };
    S.prototype.Destroy = function () {
        this.m_shape = null
    };
    S.prototype.CreateProxy = function (a, c) {
        this.m_shape.ComputeAABB(this.m_aabb, c);
        this.m_proxy = a.CreateProxy(this.m_aabb, this)
    };
    S.prototype.DestroyProxy = function (a) {
        if (this.m_proxy != null) {
            a.DestroyProxy(this.m_proxy);
            this.m_proxy = null
        }
    };
    S.prototype.Synchronize = function (a, c, g) {
        if (this.m_proxy) {
            var b = new U,
                e = new U;
            this.m_shape.ComputeAABB(b, c);
            this.m_shape.ComputeAABB(e, g);
            this.m_aabb.Combine(b, e);
            c = F.SubtractVV(g.position, c.position);
            a.MoveProxy(this.m_proxy, this.m_aabb, c)
        }
    };
    aa.b2FixtureDef = function () {
        this.filter = new N
    };
    aa.prototype.b2FixtureDef = function () {
        this.userData = this.shape = null;
        this.friction = 0.2;
        this.density = this.restitution = 0;
        this.filter.categoryBits = 1;
        this.filter.maskBits = 65535;
        this.filter.groupIndex = 0;
        this.isSensor = false
    };
    Z.b2Island = function () {};
    Z.prototype.b2Island = function () {
        this.m_bodies = new Vector;
        this.m_contacts = new Vector;
        this.m_joints = new Vector
    };
    Z.prototype.Initialize = function (a, c, g, b, e, f) {
        if (a === undefined) a = 0;
        if (c === undefined) c = 0;
        if (g === undefined) g = 0;
        var m = 0;
        this.m_bodyCapacity = a;
        this.m_contactCapacity = c;
        this.m_jointCapacity = g;
        this.m_jointCount = this.m_contactCount = this.m_bodyCount = 0;
        this.m_allocator = b;
        this.m_listener = e;
        this.m_contactSolver = f;
        for (m = this.m_bodies.length; m < a; m++) this.m_bodies[m] = null;
        for (m = this.m_contacts.length; m < c; m++) this.m_contacts[m] = null;
        for (m = this.m_joints.length; m < g; m++) this.m_joints[m] = null
    };
    Z.prototype.Clear = function () {
        this.m_jointCount = this.m_contactCount = this.m_bodyCount = 0
    };
    Z.prototype.Solve = function (a, c, g) {
        var b = 0,
            e = 0,
            f;
        for (b = 0; b < this.m_bodyCount; ++b) {
            e = this.m_bodies[b];
            if (e.GetType() == k.b2_dynamicBody) {
                e.m_linearVelocity.x += a.dt * (c.x + e.m_invMass * e.m_force.x);
                e.m_linearVelocity.y += a.dt * (c.y + e.m_invMass * e.m_force.y);
                e.m_angularVelocity += a.dt * e.m_invI * e.m_torque;
                e.m_linearVelocity.Multiply(F.Clamp(1 - a.dt * e.m_linearDamping, 0, 1));
                e.m_angularVelocity *= F.Clamp(1 - a.dt * e.m_angularDamping, 0, 1)
            }
        }
        this.m_contactSolver.Initialize(a, this.m_contacts,
            this.m_contactCount, this.m_allocator);
        c = this.m_contactSolver;
        c.InitVelocityConstraints(a);
        for (b = 0; b < this.m_jointCount; ++b) {
            f = this.m_joints[b];
            f.InitVelocityConstraints(a)
        }
        for (b = 0; b < a.velocityIterations; ++b) {
            for (e = 0; e < this.m_jointCount; ++e) {
                f = this.m_joints[e];
                f.SolveVelocityConstraints(a)
            }
            c.SolveVelocityConstraints()
        }
        for (b = 0; b < this.m_jointCount; ++b) {
            f = this.m_joints[b];
            f.FinalizeVelocityConstraints()
        }
        c.FinalizeVelocityConstraints();
        for (b = 0; b < this.m_bodyCount; ++b) {
            e = this.m_bodies[b];
            if (e.GetType() != k.b2_staticBody) {
                var m = a.dt * e.m_linearVelocity.x,
                    r = a.dt * e.m_linearVelocity.y;
                if (m * m + r * r > A.b2_maxTranslationSquared) {
                    e.m_linearVelocity.Normalize();
                    e.m_linearVelocity.x *= A.b2_maxTranslation * a.inv_dt;
                    e.m_linearVelocity.y *= A.b2_maxTranslation * a.inv_dt
                }
                m = a.dt * e.m_angularVelocity;
                if (m * m > A.b2_maxRotationSquared) e.m_angularVelocity = e.m_angularVelocity < 0 ? -A.b2_maxRotation * a.inv_dt : A.b2_maxRotation * a.inv_dt;
                e.m_sweep.c0.SetV(e.m_sweep.c);
                e.m_sweep.a0 = e.m_sweep.a;
                e.m_sweep.c.x += a.dt * e.m_linearVelocity.x;
                e.m_sweep.c.y += a.dt * e.m_linearVelocity.y;
                e.m_sweep.a += a.dt * e.m_angularVelocity;
                e.SynchronizeTransform()
            }
        }
        for (b = 0; b < a.positionIterations; ++b) {
            m = c.SolvePositionConstraints(A.b2_contactBaumgarte);
            r = true;
            for (e = 0; e < this.m_jointCount; ++e) {
                f = this.m_joints[e];
                f = f.SolvePositionConstraints(A.b2_contactBaumgarte);
                r = r && f
            }
            if (m && r) break
        }
        this.Report(c.m_constraints);
        if (g) {
            g = Number.MAX_VALUE;
            c = A.b2_linearSleepTolerance * A.b2_linearSleepTolerance;
            m = A.b2_angularSleepTolerance * A.b2_angularSleepTolerance;
            for (b = 0; b < this.m_bodyCount; ++b) {
                e = this.m_bodies[b];
                if (e.GetType() != k.b2_staticBody) {
                    if ((e.m_flags & k.e_allowSleepFlag) == 0) g = e.m_sleepTime = 0;
                    if ((e.m_flags & k.e_allowSleepFlag) == 0 || e.m_angularVelocity * e.m_angularVelocity > m || F.Dot(e.m_linearVelocity, e.m_linearVelocity) > c) g = e.m_sleepTime = 0;
                    else {
                        e.m_sleepTime += a.dt;
                        g = F.Min(g, e.m_sleepTime)
                    }
                }
            }
            if (g >= A.b2_timeToSleep)
                for (b = 0; b < this.m_bodyCount; ++b) {
                    e = this.m_bodies[b];
                    e.SetAwake(false)
                }
        }
    };
    Z.prototype.SolveTOI = function (a) {
        var c = 0,
            g = 0;
        this.m_contactSolver.Initialize(a, this.m_contacts, this.m_contactCount,
            this.m_allocator);
        var b = this.m_contactSolver;
        for (c = 0; c < this.m_jointCount; ++c) this.m_joints[c].InitVelocityConstraints(a);
        for (c = 0; c < a.velocityIterations; ++c) {
            b.SolveVelocityConstraints();
            for (g = 0; g < this.m_jointCount; ++g) this.m_joints[g].SolveVelocityConstraints(a)
        }
        for (c = 0; c < this.m_bodyCount; ++c) {
            g = this.m_bodies[c];
            if (g.GetType() != k.b2_staticBody) {
                var e = a.dt * g.m_linearVelocity.x,
                    f = a.dt * g.m_linearVelocity.y;
                if (e * e + f * f > A.b2_maxTranslationSquared) {
                    g.m_linearVelocity.Normalize();
                    g.m_linearVelocity.x *= A.b2_maxTranslation * a.inv_dt;
                    g.m_linearVelocity.y *= A.b2_maxTranslation * a.inv_dt
                }
                e = a.dt * g.m_angularVelocity;
                if (e * e > A.b2_maxRotationSquared) g.m_angularVelocity = g.m_angularVelocity < 0 ? -A.b2_maxRotation * a.inv_dt : A.b2_maxRotation * a.inv_dt;
                g.m_sweep.c0.SetV(g.m_sweep.c);
                g.m_sweep.a0 = g.m_sweep.a;
                g.m_sweep.c.x += a.dt * g.m_linearVelocity.x;
                g.m_sweep.c.y += a.dt * g.m_linearVelocity.y;
                g.m_sweep.a += a.dt * g.m_angularVelocity;
                g.SynchronizeTransform()
            }
        }
        for (c = 0; c < a.positionIterations; ++c) {
            e = b.SolvePositionConstraints(0.75);
            f = true;
            for (g = 0; g <
                this.m_jointCount; ++g) {
                var m = this.m_joints[g].SolvePositionConstraints(A.b2_contactBaumgarte);
                f = f && m
            }
            if (e && f) break
        }
        this.Report(b.m_constraints)
    };
    Z.prototype.Report = function (a) {
        if (this.m_listener != null)
            for (var c = 0; c < this.m_contactCount; ++c) {
                for (var g = this.m_contacts[c], b = a[c], e = 0; e < b.pointCount; ++e) {
                    Z.s_impulse.normalImpulses[e] = b.points[e].normalImpulse;
                    Z.s_impulse.tangentImpulses[e] = b.points[e].tangentImpulse
                }
                this.m_listener.PostSolve(g, Z.s_impulse)
            }
    };
    Z.prototype.AddBody = function (a) {
        a.m_islandIndex = this.m_bodyCount;
        this.m_bodies[this.m_bodyCount++] = a
    };
    Z.prototype.AddContact = function (a) {
        this.m_contacts[this.m_contactCount++] = a
    };
    Z.prototype.AddJoint = function (a) {
        this.m_joints[this.m_jointCount++] = a
    };
    Box2D.postDefs.push(function () {
        Box2D.Dynamics.b2Island.s_impulse = new D
    });
    d.b2TimeStep = function () {};
    d.prototype.Set = function (a) {
        this.dt = a.dt;
        this.inv_dt = a.inv_dt;
        this.positionIterations = a.positionIterations;
        this.velocityIterations = a.velocityIterations;
        this.warmStarting = a.warmStarting
    };
    h.b2World = function () {
        this.s_stack = new Vector;
        this.m_contactManager = new O;
        this.m_contactSolver = new o;
        this.m_island = new Z
    };
    h.prototype.b2World = function (a, c) {
        this.m_controllerList = this.m_jointList = this.m_contactList = this.m_bodyList = this.m_debugDraw = this.m_destructionListener = null;
        this.m_controllerCount = this.m_jointCount = this.m_contactCount = this.m_bodyCount = 0;
        h.m_warmStarting = true;
        h.m_continuousPhysics = true;
        this.m_allowSleep = c;
        this.m_gravity = a;
        this.m_inv_dt0 = 0;
        this.m_contactManager.m_world = this;
        this.m_groundBody = this.CreateBody(new z)
    };
    h.prototype.SetDestructionListener = function (a) {
        this.m_destructionListener = a
    };
    h.prototype.SetContactFilter = function (a) {
        this.m_contactManager.m_contactFilter = a
    };
    h.prototype.SetContactListener = function (a) {
        this.m_contactManager.m_contactListener = a
    };
    h.prototype.SetDebugDraw = function (a) {
        this.m_debugDraw = a
    };
    h.prototype.SetBroadPhase = function (a) {
        var c = this.m_contactManager.m_broadPhase;
        this.m_contactManager.m_broadPhase = a;
        for (var g = this.m_bodyList; g; g = g.m_next)
            for (var b = g.m_fixtureList; b; b = b.m_next) b.m_proxy = a.CreateProxy(c.GetFatAABB(b.m_proxy), b)
    };
    h.prototype.Validate = function () {
        this.m_contactManager.m_broadPhase.Validate()
    };
    h.prototype.GetProxyCount = function () {
        return this.m_contactManager.m_broadPhase.GetProxyCount()
    };
    h.prototype.CreateBody = function (a) {
        if (this.IsLocked() == true) return null;
        a = new k(a, this);
        a.m_prev = null;
        if (a.m_next = this.m_bodyList) this.m_bodyList.m_prev = a;
        this.m_bodyList = a;
        ++this.m_bodyCount;
        return a
    };
    h.prototype.DestroyBody = function (a) {
        if (this.IsLocked() != true) {
            for (var c = a.m_jointList; c;) {
                var g = c;
                c = c.next;
                this.m_destructionListener && this.m_destructionListener.SayGoodbyeJoint(g.joint);
                this.DestroyJoint(g.joint)
            }
            for (c = a.m_controllerList; c;) {
                g = c;
                c = c.nextController;
                g.controller.RemoveBody(a)
            }
            for (c = a.m_contactList; c;) {
                g = c;
                c = c.next;
                this.m_contactManager.Destroy(g.contact)
            }
            a.m_contactList = null;
            for (c = a.m_fixtureList; c;) {
                g = c;
                c = c.m_next;
                this.m_destructionListener && this.m_destructionListener.SayGoodbyeFixture(g);
                g.DestroyProxy(this.m_contactManager.m_broadPhase);
                g.Destroy()
            }
            a.m_fixtureList = null;
            a.m_fixtureCount = 0;
            if (a.m_prev) a.m_prev.m_next = a.m_next;
            if (a.m_next) a.m_next.m_prev = a.m_prev;
            if (a == this.m_bodyList) this.m_bodyList = a.m_next;
            --this.m_bodyCount
        }
    };
    h.prototype.CreateJoint = function (a) {
        var c = q.Create(a, null);
        c.m_prev = null;
        if (c.m_next = this.m_jointList) this.m_jointList.m_prev = c;
        this.m_jointList = c;
        ++this.m_jointCount;
        c.m_edgeA.joint = c;
        c.m_edgeA.other = c.m_bodyB;
        c.m_edgeA.prev = null;
        if (c.m_edgeA.next = c.m_bodyA.m_jointList) c.m_bodyA.m_jointList.prev = c.m_edgeA;
        c.m_bodyA.m_jointList = c.m_edgeA;
        c.m_edgeB.joint = c;
        c.m_edgeB.other = c.m_bodyA;
        c.m_edgeB.prev = null;
        if (c.m_edgeB.next = c.m_bodyB.m_jointList) c.m_bodyB.m_jointList.prev = c.m_edgeB;
        c.m_bodyB.m_jointList = c.m_edgeB;
        var g = a.bodyA,
            b = a.bodyB;
        if (a.collideConnected == false)
            for (a = b.GetContactList(); a;) {
                a.other == g && a.contact.FlagForFiltering();
                a = a.next
            }
        return c
    };
    h.prototype.DestroyJoint = function (a) {
        var c = a.m_collideConnected;
        if (a.m_prev) a.m_prev.m_next = a.m_next;
        if (a.m_next) a.m_next.m_prev = a.m_prev;
        if (a == this.m_jointList) this.m_jointList = a.m_next;
        var g = a.m_bodyA,
            b = a.m_bodyB;
        g.SetAwake(true);
        b.SetAwake(true);
        if (a.m_edgeA.prev) a.m_edgeA.prev.next = a.m_edgeA.next;
        if (a.m_edgeA.next) a.m_edgeA.next.prev = a.m_edgeA.prev;
        if (a.m_edgeA == g.m_jointList) g.m_jointList = a.m_edgeA.next;
        a.m_edgeA.prev = null;
        a.m_edgeA.next = null;
        if (a.m_edgeB.prev) a.m_edgeB.prev.next = a.m_edgeB.next;
        if (a.m_edgeB.next) a.m_edgeB.next.prev = a.m_edgeB.prev;
        if (a.m_edgeB == b.m_jointList) b.m_jointList = a.m_edgeB.next;
        a.m_edgeB.prev = null;
        a.m_edgeB.next = null;
        q.Destroy(a, null);
        --this.m_jointCount;
        if (c == false)
            for (a = b.GetContactList(); a;) {
                a.other == g && a.contact.FlagForFiltering();
                a = a.next
            }
    };
    h.prototype.AddController = function (a) {
        a.m_next = this.m_controllerList;
        a.m_prev = null;
        this.m_controllerList = a;
        a.m_world = this;
        this.m_controllerCount++;
        return a
    };
    h.prototype.RemoveController = function (a) {
        if (a.m_prev) a.m_prev.m_next = a.m_next;
        if (a.m_next) a.m_next.m_prev = a.m_prev;
        if (this.m_controllerList == a) this.m_controllerList = a.m_next;
        this.m_controllerCount--
    };
    h.prototype.CreateController = function (a) {
        if (a.m_world != this) throw Error("Controller can only be a member of one world");
        a.m_next = this.m_controllerList;
        a.m_prev = null;
        if (this.m_controllerList) this.m_controllerList.m_prev = a;
        this.m_controllerList = a;
        ++this.m_controllerCount;
        a.m_world = this;
        return a
    };
    h.prototype.DestroyController = function (a) {
        a.Clear();
        if (a.m_next) a.m_next.m_prev = a.m_prev;
        if (a.m_prev) a.m_prev.m_next = a.m_next;
        if (a == this.m_controllerList) this.m_controllerList = a.m_next;
        --this.m_controllerCount
    };
    h.prototype.SetWarmStarting = function (a) {
        h.m_warmStarting = a
    };
    h.prototype.SetContinuousPhysics = function (a) {
        h.m_continuousPhysics = a
    };
    h.prototype.GetBodyCount = function () {
        return this.m_bodyCount
    };
    h.prototype.GetJointCount = function () {
        return this.m_jointCount
    };
    h.prototype.GetContactCount = function () {
        return this.m_contactCount
    };
    h.prototype.SetGravity = function (a) {
        this.m_gravity = a
    };
    h.prototype.GetGravity = function () {
        return this.m_gravity
    };
    h.prototype.GetGroundBody = function () {
        return this.m_groundBody
    };
    h.prototype.Step = function (a, c, g) {
        if (a === undefined) a = 0;
        if (c === undefined) c = 0;
        if (g === undefined) g = 0;
        if (this.m_flags & h.e_newFixture) {
            this.m_contactManager.FindNewContacts();
            this.m_flags &= ~h.e_newFixture
        }
        this.m_flags |= h.e_locked;
        var b = h.s_timestep2;
        b.dt = a;
        b.velocityIterations = c;
        b.positionIterations = g;
        b.inv_dt = a > 0 ? 1 / a : 0;
        b.dtRatio = this.m_inv_dt0 * a;
        b.warmStarting = h.m_warmStarting;
        this.m_contactManager.Collide();
        b.dt > 0 && this.Solve(b);
        h.m_continuousPhysics && b.dt > 0 && this.SolveTOI(b);
        if (b.dt > 0) this.m_inv_dt0 = b.inv_dt;
        this.m_flags &= ~h.e_locked
    };
    h.prototype.ClearForces = function () {
        for (var a = this.m_bodyList; a; a = a.m_next) {
            a.m_force.SetZero();
            a.m_torque = 0
        }
    };
    h.prototype.DrawDebugData = function () {
            if (this.m_debugDraw != null) {
                this.m_debugDraw.m_sprite.graphics.clear();
                var a = this.m_debugDraw.GetFlags(),
                    c, g, b;
                new y;
                new y;
                new y;
                var e;
                new U;
                new U;
                e = [new y, new y, new y, new y];
                var f = new w(0, 0, 0);
                if (a & E.e_shapeBit)
                    for (c = this.m_bodyList; c; c = c.m_next) {
                        e = c.m_xf;
                        for (g = c.GetFixtureList(); g; g = g.m_next) {
                            b = g.GetShape();
                            if (c.IsActive() == false) f.Set(0.5, 0.5, 0.3);
                            else if (c.GetType() == k.b2_staticBody) f.Set(0.5, 0.9, 0.5);
                            else if (c.GetType() == k.b2_kinematicBody) f.Set(0.5, 0.5, 0.9);
                            else c.IsAwake() == false ?
                                f.Set(0.6, 0.6, 0.6) : f.Set(0.9, 0.7, 0.7);
                            this.DrawShape(b, e, f)
                        }
                    }
                if (a & E.e_jointBit)
                    for (c = this.m_jointList; c; c = c.m_next) this.DrawJoint(c);
                if (a & E.e_controllerBit)
                    for (c = this.m_controllerList; c; c = c.m_next) c.Draw(this.m_debugDraw);
                if (a & E.e_pairBit) {
                    f.Set(0.3, 0.9, 0.9);
                    for (c = this.m_contactManager.m_contactList; c; c = c.GetNext()) {
                        b = c.GetFixtureA();
                        g = c.GetFixtureB();
                        b = b.GetAABB().GetCenter();
                        g = g.GetAABB().GetCenter();
                        this.m_debugDraw.DrawSegment(b, g, f)
                    }
                }
                if (a & E.e_aabbBit) {
                    b = this.m_contactManager.m_broadPhase;
                    e = [new y,
                        new y, new y, new y
                    ];
                    for (c = this.m_bodyList; c; c = c.GetNext())
                        if (c.IsActive() != false)
                            for (g = c.GetFixtureList(); g; g = g.GetNext()) {
                                var m = b.GetFatAABB(g.m_proxy);
                                e[0].Set(m.lowerBound.x, m.lowerBound.y);
                                e[1].Set(m.upperBound.x, m.lowerBound.y);
                                e[2].Set(m.upperBound.x, m.upperBound.y);
                                e[3].Set(m.lowerBound.x, m.upperBound.y);
                                this.m_debugDraw.DrawPolygon(e, 4, f)
                            }
                }
                if (a & E.e_centerOfMassBit)
                    for (c = this.m_bodyList; c; c = c.m_next) {
                        e = h.s_xf;
                        e.R = c.m_xf.R;
                        e.position = c.GetWorldCenter();
                        this.m_debugDraw.DrawTransform(e)
                    }
            }
    };
    h.prototype.QueryAABB = function (a, c) {
            var g = this.m_contactManager.m_broadPhase;
            g.Query(function (b) {
                return a(g.GetUserData(b))
            }, c)
    };
    h.prototype.QueryShape = function (a, c, g) {
        if (g === undefined) g = null;
        if (g == null) {
            g = new K;
            g.SetIdentity()
        }
        var b = this.m_contactManager.m_broadPhase,
            e = new U;
        c.ComputeAABB(e, g);
        b.Query(function (f) {
            f = b.GetUserData(f) instanceof S ? b.GetUserData(f) : null;
            if (Y.TestOverlap(c, g, f.GetShape(), f.GetBody().GetTransform())) return a(f);
            return true
        }, e)
    };
    h.prototype.QueryPoint = function (a, c) {
        var g = this.m_contactManager.m_broadPhase,
            b = new U;
        b.lowerBound.Set(c.x - A.b2_linearSlop, c.y - A.b2_linearSlop);
        b.upperBound.Set(c.x + A.b2_linearSlop, c.y + A.b2_linearSlop);
        g.Query(function (e) {
            e = g.GetUserData(e) instanceof S ? g.GetUserData(e) : null;
            if (e.TestPoint(c)) return a(e);
            return true
        }, b)
    };
    h.prototype.RayCast = function (a, c, g) {
        var b = this.m_contactManager.m_broadPhase,
            e = new V,
            f = new Q(c, g);
        b.RayCast(function (m, r) {
            var s = b.GetUserData(r);
            s = s instanceof S ? s : null;
            if (s.RayCast(e, m)) {
                var v = e.fraction,
                    t = new y((1 - v) * c.x + v * g.x, (1 - v) * c.y + v * g.y);
                return a(s,
                    t, e.normal, v)
            }
            return m.maxFraction
        }, f)
    };
    h.prototype.RayCastOne = function (a, c) {
        var g;
        this.RayCast(function (b, e, f, m) {
            if (m === undefined) m = 0;
            g = b;
            return m
        }, a, c);
        return g
    };
    h.prototype.RayCastAll = function (a, c) {
        var g = new Vector;
        this.RayCast(function (b) {
            g[g.length] = b;
            return 1
        }, a, c);
        return g
    };
    h.prototype.GetBodyList = function () {
        return this.m_bodyList
    };
    h.prototype.GetJointList = function () {
        return this.m_jointList
    };
    h.prototype.GetContactList = function () {
        return this.m_contactList
    };
    h.prototype.IsLocked = function () {
        return (this.m_flags &
            h.e_locked) > 0
    };
    h.prototype.Solve = function (a) {
        for (var c, g = this.m_controllerList; g; g = g.m_next) g.Step(a);
        g = this.m_island;
        g.Initialize(this.m_bodyCount, this.m_contactCount, this.m_jointCount, null, this.m_contactManager.m_contactListener, this.m_contactSolver);
        for (c = this.m_bodyList; c; c = c.m_next) c.m_flags &= ~k.e_islandFlag;
        for (var b = this.m_contactList; b; b = b.m_next) b.m_flags &= ~l.e_islandFlag;
        for (b = this.m_jointList; b; b = b.m_next) b.m_islandFlag = false;
        parseInt(this.m_bodyCount);
        b = this.s_stack;
        for (var e = this.m_bodyList; e; e = e.m_next)
            if (!(e.m_flags & k.e_islandFlag))
                if (!(e.IsAwake() == false || e.IsActive() == false))
                    if (e.GetType() != k.b2_staticBody) {
                        g.Clear();
                        var f = 0;
                        b[f++] = e;
                        for (e.m_flags |= k.e_islandFlag; f > 0;) {
                            c = b[--f];
                            g.AddBody(c);
                            c.IsAwake() == false && c.SetAwake(true);
                            if (c.GetType() != k.b2_staticBody) {
                                for (var m, r = c.m_contactList; r; r = r.next)
                                    if (!(r.contact.m_flags & l.e_islandFlag))
                                        if (!(r.contact.IsSensor() == true || r.contact.IsEnabled() == false || r.contact.IsTouching() == false)) {
                                            g.AddContact(r.contact);
                                            r.contact.m_flags |= l.e_islandFlag;
                                            m = r.other;
                                            if (!(m.m_flags & k.e_islandFlag)) {
                                                b[f++] = m;
                                                m.m_flags |= k.e_islandFlag
                                            }
                                        }
                                for (c = c.m_jointList; c; c = c.next)
                                    if (c.joint.m_islandFlag != true) {
                                        m = c.other;
                                        if (m.IsActive() != false) {
                                            g.AddJoint(c.joint);
                                            c.joint.m_islandFlag = true;
                                            if (!(m.m_flags & k.e_islandFlag)) {
                                                b[f++] = m;
                                                m.m_flags |= k.e_islandFlag
                                            }
                                        }
                                    }
                            }
                        }
                        g.Solve(a, this.m_gravity, this.m_allowSleep);
                        for (f = 0; f < g.m_bodyCount; ++f) {
                            c = g.m_bodies[f];
                            if (c.GetType() == k.b2_staticBody) c.m_flags &= ~k.e_islandFlag
                        }
                    }
        for (f = 0; f < b.length; ++f) {
            if (!b[f]) break;
            b[f] = null
        }
        for (c = this.m_bodyList; c; c = c.m_next) c.IsAwake() == false || c.IsActive() == false || c.GetType() != k.b2_staticBody && c.SynchronizeFixtures();
        this.m_contactManager.FindNewContacts()
    };
    h.prototype.SolveTOI = function (a) {
        var c, g, b, e = this.m_island;
        e.Initialize(this.m_bodyCount, A.b2_maxTOIContactsPerIsland, A.b2_maxTOIJointsPerIsland, null, this.m_contactManager.m_contactListener, this.m_contactSolver);
        var f = h.s_queue;
        for (c = this.m_bodyList; c; c = c.m_next) {
            c.m_flags &= ~k.e_islandFlag;
            c.m_sweep.t0 = 0
        }
        for (b = this.m_contactList; b; b = b.m_next) b.m_flags &= ~(l.e_toiFlag |
            l.e_islandFlag);
        for (b = this.m_jointList; b; b = b.m_next) b.m_islandFlag = false;
        for (;;) {
            var m = null,
                r = 1;
            for (b = this.m_contactList; b; b = b.m_next)
                if (!(b.IsSensor() == true || b.IsEnabled() == false || b.IsContinuous() == false)) {
                    c = 1;
                    if (b.m_flags & l.e_toiFlag) c = b.m_toi;
                    else {
                        c = b.m_fixtureA;
                        g = b.m_fixtureB;
                        c = c.m_body;
                        g = g.m_body;
                        if ((c.GetType() != k.b2_dynamicBody || c.IsAwake() == false) && (g.GetType() != k.b2_dynamicBody || g.IsAwake() == false)) continue;
                        var s = c.m_sweep.t0;
                        if (c.m_sweep.t0 < g.m_sweep.t0) {
                            s = g.m_sweep.t0;
                            c.m_sweep.Advance(s)
                        } else if (g.m_sweep.t0 <
                            c.m_sweep.t0) {
                            s = c.m_sweep.t0;
                            g.m_sweep.Advance(s)
                        }
                        c = b.ComputeTOI(c.m_sweep, g.m_sweep);
                        A.b2Assert(0 <= c && c <= 1);
                        if (c > 0 && c < 1) {
                            c = (1 - c) * s + c;
                            if (c > 1) c = 1
                        }
                        b.m_toi = c;
                        b.m_flags |= l.e_toiFlag
                    } if (Number.MIN_VALUE < c && c < r) {
                        m = b;
                        r = c
                    }
                }
            if (m == null || 1 - 100 * Number.MIN_VALUE < r) break;
            c = m.m_fixtureA;
            g = m.m_fixtureB;
            c = c.m_body;
            g = g.m_body;
            h.s_backupA.Set(c.m_sweep);
            h.s_backupB.Set(g.m_sweep);
            c.Advance(r);
            g.Advance(r);
            m.Update(this.m_contactManager.m_contactListener);
            m.m_flags &= ~l.e_toiFlag;
            if (m.IsSensor() == true || m.IsEnabled() == false) {
                c.m_sweep.Set(h.s_backupA);
                g.m_sweep.Set(h.s_backupB);
                c.SynchronizeTransform();
                g.SynchronizeTransform()
            } else if (m.IsTouching() != false) {
                c = c;
                if (c.GetType() != k.b2_dynamicBody) c = g;
                e.Clear();
                m = b = 0;
                f[b + m++] = c;
                for (c.m_flags |= k.e_islandFlag; m > 0;) {
                    c = f[b++];
                    --m;
                    e.AddBody(c);
                    c.IsAwake() == false && c.SetAwake(true);
                    if (c.GetType() == k.b2_dynamicBody) {
                        for (g = c.m_contactList; g; g = g.next) {
                            if (e.m_contactCount == e.m_contactCapacity) break;
                            if (!(g.contact.m_flags & l.e_islandFlag))
                                if (!(g.contact.IsSensor() == true || g.contact.IsEnabled() == false || g.contact.IsTouching() == false)) {
                                    e.AddContact(g.contact);
                                    g.contact.m_flags |= l.e_islandFlag;
                                    s = g.other;
                                    if (!(s.m_flags & k.e_islandFlag)) {
                                        if (s.GetType() != k.b2_staticBody) {
                                            s.Advance(r);
                                            s.SetAwake(true)
                                        }
                                        f[b + m] = s;
                                        ++m;
                                        s.m_flags |= k.e_islandFlag
                                    }
                                }
                        }
                        for (c = c.m_jointList; c; c = c.next)
                            if (e.m_jointCount != e.m_jointCapacity)
                                if (c.joint.m_islandFlag != true) {
                                    s = c.other;
                                    if (s.IsActive() != false) {
                                        e.AddJoint(c.joint);
                                        c.joint.m_islandFlag = true;
                                        if (!(s.m_flags & k.e_islandFlag)) {
                                            if (s.GetType() != k.b2_staticBody) {
                                                s.Advance(r);
                                                s.SetAwake(true)
                                            }
                                            f[b + m] = s;
                                            ++m;
                                            s.m_flags |= k.e_islandFlag
                                        }
                                    }
                                }
                    }
                }
                b = h.s_timestep;
                b.warmStarting = false;
                b.dt = (1 - r) * a.dt;
                b.inv_dt = 1 / b.dt;
                b.dtRatio = 0;
                b.velocityIterations = a.velocityIterations;
                b.positionIterations = a.positionIterations;
                e.SolveTOI(b);
                for (r = r = 0; r < e.m_bodyCount; ++r) {
                    c = e.m_bodies[r];
                    c.m_flags &= ~k.e_islandFlag;
                    if (c.IsAwake() != false)
                        if (c.GetType() == k.b2_dynamicBody) {
                            c.SynchronizeFixtures();
                            for (g = c.m_contactList; g; g = g.next) g.contact.m_flags &= ~l.e_toiFlag
                        }
                }
                for (r = 0; r < e.m_contactCount; ++r) {
                    b = e.m_contacts[r];
                    b.m_flags &= ~(l.e_toiFlag |
                        l.e_islandFlag)
                }
                for (r = 0; r < e.m_jointCount; ++r) {
                    b = e.m_joints[r];
                    b.m_islandFlag = false
                }
                this.m_contactManager.FindNewContacts()
            }
        }
    };
    h.prototype.DrawJoint = function (a) {
        var c = a.GetBodyA(),
            g = a.GetBodyB(),
            b = c.m_xf.position,
            e = g.m_xf.position,
            f = a.GetAnchorA(),
            m = a.GetAnchorB(),
            r = h.s_jointColor;
        switch (a.m_type) {
        case q.e_distanceJoint:
            this.m_debugDraw.DrawSegment(f, m, r);
            break;
        case q.e_pulleyJoint:
            c = a instanceof n ? a : null;
            a = c.GetGroundAnchorA();
            c = c.GetGroundAnchorB();
            this.m_debugDraw.DrawSegment(a, f, r);
            this.m_debugDraw.DrawSegment(c,
                m, r);
            this.m_debugDraw.DrawSegment(a, c, r);
            break;
        case q.e_mouseJoint:
            this.m_debugDraw.DrawSegment(f, m, r);
            break;
        default:
            c != this.m_groundBody && this.m_debugDraw.DrawSegment(b, f, r);
            this.m_debugDraw.DrawSegment(f, m, r);
            g != this.m_groundBody && this.m_debugDraw.DrawSegment(e, m, r)
        }
    };
    h.prototype.DrawShape = function (a, c, g) {
        switch (a.m_type) {
        case Y.e_circleShape:
            var b = a instanceof M ? a : null;
            this.m_debugDraw.DrawSolidCircle(F.MulX(c, b.m_p), b.m_radius, c.R.col1, g);
            break;
        case Y.e_polygonShape:
            b = 0;
            b = a instanceof W ? a : null;
            a = parseInt(b.GetVertexCount());
            var e = b.GetVertices(),
                f = new Vector(a);
            for (b = 0; b < a; ++b) f[b] = F.MulX(c, e[b]);
            this.m_debugDraw.DrawSolidPolygon(f, a, g);
            break;
        case Y.e_edgeShape:
            b = a instanceof L ? a : null;
            this.m_debugDraw.DrawSegment(F.MulX(c, b.GetVertex1()), F.MulX(c, b.GetVertex2()), g)
        }
    };
    Box2D.postDefs.push(function () {
        Box2D.Dynamics.b2World.s_timestep2 = new d;
        Box2D.Dynamics.b2World.s_xf = new K;
        Box2D.Dynamics.b2World.s_backupA = new G;
        Box2D.Dynamics.b2World.s_backupB = new G;
        Box2D.Dynamics.b2World.s_timestep = new d;
        Box2D.Dynamics.b2World.s_queue = new Vector;
        Box2D.Dynamics.b2World.s_jointColor = new w(0.5, 0.8, 0.8);
        Box2D.Dynamics.b2World.e_newFixture = 1;
        Box2D.Dynamics.b2World.e_locked = 2
    })
})();
(function () {
    var F = Box2D.Collision.Shapes.b2CircleShape,
        G = Box2D.Collision.Shapes.b2EdgeShape,
        K = Box2D.Collision.Shapes.b2PolygonShape,
        y = Box2D.Collision.Shapes.b2Shape,
        w = Box2D.Dynamics.Contacts.b2CircleContact,
        A = Box2D.Dynamics.Contacts.b2Contact,
        U = Box2D.Dynamics.Contacts.b2ContactConstraint,
        p = Box2D.Dynamics.Contacts.b2ContactConstraintPoint,
        B = Box2D.Dynamics.Contacts.b2ContactEdge,
        Q = Box2D.Dynamics.Contacts.b2ContactFactory,
        V = Box2D.Dynamics.Contacts.b2ContactRegister,
        M = Box2D.Dynamics.Contacts.b2ContactResult,
        L = Box2D.Dynamics.Contacts.b2ContactSolver,
        I = Box2D.Dynamics.Contacts.b2EdgeAndCircleContact,
        W = Box2D.Dynamics.Contacts.b2NullContact,
        Y = Box2D.Dynamics.Contacts.b2PolyAndCircleContact,
        k = Box2D.Dynamics.Contacts.b2PolyAndEdgeContact,
        z = Box2D.Dynamics.Contacts.b2PolygonContact,
        u = Box2D.Dynamics.Contacts.b2PositionSolverManifold,
        D = Box2D.Dynamics.b2Body,
        H = Box2D.Dynamics.b2TimeStep,
        O = Box2D.Common.b2Settings,
        E = Box2D.Common.Math.b2Mat22,
        R = Box2D.Common.Math.b2Math,
        N = Box2D.Common.Math.b2Vec2,
        S = Box2D.Collision.b2Collision,
        aa = Box2D.Collision.b2ContactID,
        Z = Box2D.Collision.b2Manifold,
        d = Box2D.Collision.b2TimeOfImpact,
        h = Box2D.Collision.b2TOIInput,
        l = Box2D.Collision.b2WorldManifold;
    Box2D.inherit(w, Box2D.Dynamics.Contacts.b2Contact);
    w.prototype.__super = Box2D.Dynamics.Contacts.b2Contact.prototype;
    w.b2CircleContact = function () {
        Box2D.Dynamics.Contacts.b2Contact.b2Contact.apply(this, arguments)
    };
    w.Create = function () {
        return new w
    };
    w.Destroy = function () {};
    w.prototype.Reset = function (j, o) {
        this.__super.Reset.call(this, j, o)
    };
    w.prototype.Evaluate = function () {
            var j = this.m_fixtureA.GetBody(),
                o = this.m_fixtureB.GetBody();
            S.CollideCircles(this.m_manifold, this.m_fixtureA.GetShape() instanceof F ? this.m_fixtureA.GetShape() : null, j.m_xf, this.m_fixtureB.GetShape() instanceof F ? this.m_fixtureB.GetShape() : null, o.m_xf)
    };
    A.b2Contact = function () {
        this.m_nodeA = new B;
        this.m_nodeB = new B;
        this.m_manifold = new Z;
        this.m_oldManifold = new Z
    };
    A.prototype.GetManifold = function () {
        return this.m_manifold
    };
    A.prototype.GetWorldManifold = function (j) {
        var o = this.m_fixtureA.GetBody(),
            q = this.m_fixtureB.GetBody(),
            n = this.m_fixtureA.GetShape(),
            a = this.m_fixtureB.GetShape();
        j.Initialize(this.m_manifold, o.GetTransform(), n.m_radius, q.GetTransform(), a.m_radius)
    };
    A.prototype.IsTouching = function () {
        return (this.m_flags & A.e_touchingFlag) == A.e_touchingFlag
    };
    A.prototype.IsContinuous = function () {
        return (this.m_flags & A.e_continuousFlag) == A.e_continuousFlag
    };
    A.prototype.SetSensor = function (j) {
        if (j) this.m_flags |= A.e_sensorFlag;
        else this.m_flags &= ~A.e_sensorFlag
    };
    A.prototype.IsSensor = function () {
        return (this.m_flags &
            A.e_sensorFlag) == A.e_sensorFlag
    };
    A.prototype.SetEnabled = function (j) {
        if (j) this.m_flags |= A.e_enabledFlag;
        else this.m_flags &= ~A.e_enabledFlag
    };
    A.prototype.IsEnabled = function () {
        return (this.m_flags & A.e_enabledFlag) == A.e_enabledFlag
    };
    A.prototype.GetNext = function () {
        return this.m_next
    };
    A.prototype.GetFixtureA = function () {
        return this.m_fixtureA
    };
    A.prototype.GetFixtureB = function () {
        return this.m_fixtureB
    };
    A.prototype.FlagForFiltering = function () {
        this.m_flags |= A.e_filterFlag
    };
    A.prototype.b2Contact = function () {};
    A.prototype.Reset = function (j, o) {
        if (j === undefined) j = null;
        if (o === undefined) o = null;
        this.m_flags = A.e_enabledFlag;
        if (!j || !o) this.m_fixtureB = this.m_fixtureA = null;
        else {
            if (j.IsSensor() || o.IsSensor()) this.m_flags |= A.e_sensorFlag;
            var q = j.GetBody(),
                n = o.GetBody();
            if (q.GetType() != D.b2_dynamicBody || q.IsBullet() || n.GetType() != D.b2_dynamicBody || n.IsBullet()) this.m_flags |= A.e_continuousFlag;
            this.m_fixtureA = j;
            this.m_fixtureB = o;
            this.m_manifold.m_pointCount = 0;
            this.m_next = this.m_prev = null;
            this.m_nodeA.contact = null;
            this.m_nodeA.prev = null;
            this.m_nodeA.next = null;
            this.m_nodeA.other = null;
            this.m_nodeB.contact = null;
            this.m_nodeB.prev = null;
            this.m_nodeB.next = null;
            this.m_nodeB.other = null
        }
    };
    A.prototype.Update = function (j) {
        var o = this.m_oldManifold;
        this.m_oldManifold = this.m_manifold;
        this.m_manifold = o;
        this.m_flags |= A.e_enabledFlag;
        var q = false;
        o = (this.m_flags & A.e_touchingFlag) == A.e_touchingFlag;
        var n = this.m_fixtureA.m_body,
            a = this.m_fixtureB.m_body,
            c = this.m_fixtureA.m_aabb.TestOverlap(this.m_fixtureB.m_aabb);
        if (this.m_flags & A.e_sensorFlag) {
            if (c) {
                q = this.m_fixtureA.GetShape();
                c = this.m_fixtureB.GetShape();
                n = n.GetTransform();
                a = a.GetTransform();
                q = y.TestOverlap(q, n, c, a)
            }
            this.m_manifold.m_pointCount = 0
        } else {
            if (n.GetType() != D.b2_dynamicBody || n.IsBullet() || a.GetType() != D.b2_dynamicBody || a.IsBullet()) this.m_flags |= A.e_continuousFlag;
            else this.m_flags &= ~A.e_continuousFlag; if (c) {
                this.Evaluate();
                q = this.m_manifold.m_pointCount > 0;
                for (c = 0; c < this.m_manifold.m_pointCount; ++c) {
                    var g = this.m_manifold.m_points[c];
                    g.m_normalImpulse = 0;
                    g.m_tangentImpulse = 0;
                    for (var b = g.m_id, e = 0; e < this.m_oldManifold.m_pointCount; ++e) {
                        var f = this.m_oldManifold.m_points[e];
                        if (f.m_id.key == b.key) {
                            g.m_normalImpulse = f.m_normalImpulse;
                            g.m_tangentImpulse = f.m_tangentImpulse;
                            break
                        }
                    }
                }
            } else this.m_manifold.m_pointCount = 0; if (q != o) {
                n.SetAwake(true);
                a.SetAwake(true)
            }
        } if (q) this.m_flags |= A.e_touchingFlag;
        else this.m_flags &= ~A.e_touchingFlag;
        o == false && q == true && j.BeginContact(this);
        o == true && q == false && j.EndContact(this);
        (this.m_flags & A.e_sensorFlag) == 0 && j.PreSolve(this, this.m_oldManifold)
    };
    A.prototype.Evaluate = function () {};
    A.prototype.ComputeTOI = function (j, o) {
        A.s_input.proxyA.Set(this.m_fixtureA.GetShape());
        A.s_input.proxyB.Set(this.m_fixtureB.GetShape());
        A.s_input.sweepA = j;
        A.s_input.sweepB = o;
        A.s_input.tolerance = O.b2_linearSlop;
        return d.TimeOfImpact(A.s_input)
    };
    Box2D.postDefs.push(function () {
        Box2D.Dynamics.Contacts.b2Contact.e_sensorFlag = 1;
        Box2D.Dynamics.Contacts.b2Contact.e_continuousFlag = 2;
        Box2D.Dynamics.Contacts.b2Contact.e_islandFlag = 4;
        Box2D.Dynamics.Contacts.b2Contact.e_toiFlag = 8;
        Box2D.Dynamics.Contacts.b2Contact.e_touchingFlag = 16;
        Box2D.Dynamics.Contacts.b2Contact.e_enabledFlag = 32;
        Box2D.Dynamics.Contacts.b2Contact.e_filterFlag = 64;
        Box2D.Dynamics.Contacts.b2Contact.s_input = new h
    });
    U.b2ContactConstraint = function () {
        this.localPlaneNormal = new N;
        this.localPoint = new N;
        this.normal = new N;
        this.normalMass = new E;
        this.K = new E
    };
    U.prototype.b2ContactConstraint = function () {
        this.points = new Vector(O.b2_maxManifoldPoints);
        for (var j = 0; j < O.b2_maxManifoldPoints; j++) this.points[j] = new p
    };
    p.b2ContactConstraintPoint = function () {
        this.localPoint = new N;
        this.rA = new N;
        this.rB = new N
    };
    B.b2ContactEdge = function () {};
    Q.b2ContactFactory = function () {};
    Q.prototype.b2ContactFactory = function (j) {
        this.m_allocator = j;
        this.InitializeRegisters()
    };
    Q.prototype.AddType = function (j, o, q, n) {
        if (q === undefined) q = 0;
        if (n === undefined) n = 0;
        this.m_registers[q][n].createFcn = j;
        this.m_registers[q][n].destroyFcn = o;
        this.m_registers[q][n].primary = true;
        if (q != n) {
            this.m_registers[n][q].createFcn = j;
            this.m_registers[n][q].destroyFcn = o;
            this.m_registers[n][q].primary = false
        }
    };
    Q.prototype.InitializeRegisters = function () {
            this.m_registers = new Vector(y.e_shapeTypeCount);
            for (var j = 0; j < y.e_shapeTypeCount; j++) {
                this.m_registers[j] = new Vector(y.e_shapeTypeCount);
                for (var o = 0; o < y.e_shapeTypeCount; o++) this.m_registers[j][o] = new V
            }
            this.AddType(w.Create, w.Destroy, y.e_circleShape, y.e_circleShape);
            this.AddType(Y.Create, Y.Destroy, y.e_polygonShape, y.e_circleShape);
            this.AddType(z.Create, z.Destroy, y.e_polygonShape, y.e_polygonShape);
            this.AddType(I.Create, I.Destroy, y.e_edgeShape, y.e_circleShape);
            this.AddType(k.Create, k.Destroy,
                y.e_polygonShape, y.e_edgeShape)
    };
    Q.prototype.Create = function (j, o) {
        var q = parseInt(j.GetType()),
            n = parseInt(o.GetType());
        q = this.m_registers[q][n];
        if (q.pool) {
            n = q.pool;
            q.pool = n.m_next;
            q.poolCount--;
            n.Reset(j, o);
            return n
        }
        n = q.createFcn;
        if (n != null) {
            if (q.primary) {
                n = n(this.m_allocator);
                n.Reset(j, o)
            } else {
                n = n(this.m_allocator);
                n.Reset(o, j)
            }
            return n
        } else return null
    };
    Q.prototype.Destroy = function (j) {
        if (j.m_manifold.m_pointCount > 0) {
            j.m_fixtureA.m_body.SetAwake(true);
            j.m_fixtureB.m_body.SetAwake(true)
        }
        var o = parseInt(j.m_fixtureA.GetType()),
            q = parseInt(j.m_fixtureB.GetType());
        o = this.m_registers[o][q];
        o.poolCount++;
        j.m_next = o.pool;
        o.pool = j;
        o = o.destroyFcn;
        o(j, this.m_allocator)
    };
    V.b2ContactRegister = function () {};
    M.b2ContactResult = function () {
        this.position = new N;
        this.normal = new N;
        this.id = new aa
    };
    L.b2ContactSolver = function () {
        this.m_step = new H;
        this.m_constraints = new Vector
    };
    L.prototype.b2ContactSolver = function () {};
    L.prototype.Initialize = function (j, o, q, n) {
        if (q === undefined) q = 0;
        var a;
        this.m_step.Set(j);
        this.m_allocator = n;
        j = 0;
        for (this.m_constraintCount = q; this.m_constraints.length < this.m_constraintCount;) this.m_constraints[this.m_constraints.length] = new U;
        for (j = 0; j < q; ++j) {
            a = o[j];
            n = a.m_fixtureA;
            var c = a.m_fixtureB,
                g = n.m_shape.m_radius,
                b = c.m_shape.m_radius,
                e = n.m_body,
                f = c.m_body,
                m = a.GetManifold(),
                r = O.b2MixFriction(n.GetFriction(), c.GetFriction()),
                s = O.b2MixRestitution(n.GetRestitution(), c.GetRestitution()),
                v = e.m_linearVelocity.x,
                t = e.m_linearVelocity.y,
                x = f.m_linearVelocity.x,
                C = f.m_linearVelocity.y,
                J = e.m_angularVelocity,
                T = f.m_angularVelocity;
            O.b2Assert(m.m_pointCount > 0);
            L.s_worldManifold.Initialize(m, e.m_xf, g, f.m_xf, b);
            c = L.s_worldManifold.m_normal.x;
            a = L.s_worldManifold.m_normal.y;
            n = this.m_constraints[j];
            n.bodyA = e;
            n.bodyB = f;
            n.manifold = m;
            n.normal.x = c;
            n.normal.y = a;
            n.pointCount = m.m_pointCount;
            n.friction = r;
            n.restitution = s;
            n.localPlaneNormal.x = m.m_localPlaneNormal.x;
            n.localPlaneNormal.y = m.m_localPlaneNormal.y;
            n.localPoint.x = m.m_localPoint.x;
            n.localPoint.y = m.m_localPoint.y;
            n.radius = g + b;
            n.type = m.m_type;
            for (g = 0; g < n.pointCount; ++g) {
                r = m.m_points[g];
                b = n.points[g];
                b.normalImpulse = r.m_normalImpulse;
                b.tangentImpulse = r.m_tangentImpulse;
                b.localPoint.SetV(r.m_localPoint);
                r = b.rA.x = L.s_worldManifold.m_points[g].x - e.m_sweep.c.x;
                s = b.rA.y = L.s_worldManifold.m_points[g].y - e.m_sweep.c.y;
                var P = b.rB.x = L.s_worldManifold.m_points[g].x - f.m_sweep.c.x,
                    X = b.rB.y = L.s_worldManifold.m_points[g].y - f.m_sweep.c.y,
                    $ = r * a - s * c,
                    ba = P * a - X * c;
                $ *= $;
                ba *= ba;
                b.normalMass = 1 / (e.m_invMass + f.m_invMass + e.m_invI * $ + f.m_invI * ba);
                var ca = e.m_mass * e.m_invMass + f.m_mass * f.m_invMass;
                ca += e.m_mass * e.m_invI * $ + f.m_mass * f.m_invI * ba;
                b.equalizedMass = 1 / ca;
                ba = a;
                ca = -c;
                $ = r * ca - s * ba;
                ba = P * ca - X * ba;
                $ *= $;
                ba *= ba;
                b.tangentMass = 1 / (e.m_invMass + f.m_invMass + e.m_invI * $ + f.m_invI * ba);
                b.velocityBias = 0;
                r = n.normal.x * (x + -T * X - v - -J * s) + n.normal.y * (C + T * P - t - J * r);
                if (r < -O.b2_velocityThreshold) b.velocityBias += -n.restitution * r
            }
            if (n.pointCount == 2) {
                C = n.points[0];
                x = n.points[1];
                m = e.m_invMass;
                e = e.m_invI;
                v = f.m_invMass;
                f = f.m_invI;
                t = C.rA.x * a - C.rA.y * c;
                C = C.rB.x * a - C.rB.y * c;
                J = x.rA.x * a - x.rA.y * c;
                x = x.rB.x * a - x.rB.y * c;
                c = m + v + e * t * t + f * C * C;
                a = m + v + e * J * J + f * x * x;
                f = m + v + e * t * J + f * C * x;
                if (c * c < 100 * (c * a - f * f)) {
                    n.K.col1.Set(c, f);
                    n.K.col2.Set(f, a);
                    n.K.GetInverse(n.normalMass)
                } else n.pointCount = 1
            }
        }
    };
    L.prototype.InitVelocityConstraints = function (j) {
        for (var o = 0; o < this.m_constraintCount; ++o) {
            var q = this.m_constraints[o],
                n = q.bodyA,
                a = q.bodyB,
                c = n.m_invMass,
                g = n.m_invI,
                b = a.m_invMass,
                e = a.m_invI,
                f = q.normal.x,
                m = q.normal.y,
                r = m,
                s = -f,
                v = 0,
                t = 0;
            if (j.warmStarting) {
                t = q.pointCount;
                for (v = 0; v < t; ++v) {
                    var x = q.points[v];
                    x.normalImpulse *= j.dtRatio;
                    x.tangentImpulse *= j.dtRatio;
                    var C = x.normalImpulse * f + x.tangentImpulse * r,
                        J = x.normalImpulse * m + x.tangentImpulse * s;
                    n.m_angularVelocity -= g * (x.rA.x * J - x.rA.y * C);
                    n.m_linearVelocity.x -= c * C;
                    n.m_linearVelocity.y -= c * J;
                    a.m_angularVelocity += e * (x.rB.x * J - x.rB.y * C);
                    a.m_linearVelocity.x += b * C;
                    a.m_linearVelocity.y += b * J
                }
            } else {
                t = q.pointCount;
                for (v = 0; v < t; ++v) {
                    n = q.points[v];
                    n.normalImpulse = 0;
                    n.tangentImpulse = 0
                }
            }
        }
    };
    L.prototype.SolveVelocityConstraints = function () {
        for (var j = 0, o, q = 0, n = 0, a = 0, c = n = n = q = q = 0, g = q = q = 0, b = q = a = 0, e = 0, f, m = 0; m < this.m_constraintCount; ++m) {
            a = this.m_constraints[m];
            var r = a.bodyA,
                s = a.bodyB,
                v = r.m_angularVelocity,
                t = s.m_angularVelocity,
                x = r.m_linearVelocity,
                C = s.m_linearVelocity,
                J = r.m_invMass,
                T = r.m_invI,
                P = s.m_invMass,
                X = s.m_invI;
            b = a.normal.x;
            var $ = e = a.normal.y;
            f = -b;
            g = a.friction;
            for (j = 0; j < a.pointCount; j++) {
                o = a.points[j];
                q = C.x - t * o.rB.y - x.x + v * o.rA.y;
                n = C.y + t * o.rB.x - x.y - v * o.rA.x;
                q = q * $ + n * f;
                q = o.tangentMass * -q;
                n = g * o.normalImpulse;
                n = R.Clamp(o.tangentImpulse + q, -n, n);
                q = n - o.tangentImpulse;
                c = q * $;
                q = q * f;
                x.x -= J * c;
                x.y -= J * q;
                v -= T * (o.rA.x * q - o.rA.y * c);
                C.x += P * c;
                C.y += P * q;
                t += X * (o.rB.x * q - o.rB.y * c);
                o.tangentImpulse = n
            }
            parseInt(a.pointCount);
            if (a.pointCount == 1) {
                o = a.points[0];
                q = C.x + -t * o.rB.y - x.x - -v * o.rA.y;
                n = C.y + t * o.rB.x - x.y - v * o.rA.x;
                a = q * b + n * e;
                q = -o.normalMass * (a - o.velocityBias);
                n = o.normalImpulse + q;
                n = n > 0 ? n : 0;
                q = n - o.normalImpulse;
                c = q * b;
                q = q * e;
                x.x -= J * c;
                x.y -= J * q;
                v -= T * (o.rA.x * q - o.rA.y * c);
                C.x += P * c;
                C.y += P * q;
                t += X * (o.rB.x * q - o.rB.y * c);
                o.normalImpulse = n
            } else {
                o = a.points[0];
                j = a.points[1];
                q = o.normalImpulse;
                g = j.normalImpulse;
                var ba = (C.x - t * o.rB.y - x.x + v * o.rA.y) * b + (C.y + t * o.rB.x - x.y - v * o.rA.x) * e,
                    ca = (C.x - t * j.rB.y - x.x + v * j.rA.y) * b + (C.y + t * j.rB.x - x.y - v * j.rA.x) * e;
                n = ba - o.velocityBias;
                c = ca - j.velocityBias;
                f = a.K;
                n -= f.col1.x * q + f.col2.x * g;
                for (c -= f.col1.y * q + f.col2.y * g;;) {
                    f = a.normalMass;
                    $ = -(f.col1.x * n + f.col2.x * c);
                    f = -(f.col1.y * n + f.col2.y * c);
                    if ($ >= 0 && f >= 0) {
                        q = $ - q;
                        g = f - g;
                        a = q * b;
                        q = q * e;
                        b = g * b;
                        e = g * e;
                        x.x -= J * (a + b);
                        x.y -= J * (q + e);
                        v -= T * (o.rA.x * q - o.rA.y * a + j.rA.x * e - j.rA.y * b);
                        C.x += P * (a + b);
                        C.y += P * (q + e);
                        t += X * (o.rB.x * q - o.rB.y * a + j.rB.x * e - j.rB.y * b);
                        o.normalImpulse = $;
                        j.normalImpulse = f;
                        break
                    }
                    $ = -o.normalMass * n;
                    f = 0;
                    ca = a.K.col1.y * $ + c;
                    if ($ >= 0 && ca >= 0) {
                        q = $ - q;
                        g = f - g;
                        a = q * b;
                        q = q * e;
                        b = g * b;
                        e = g * e;
                        x.x -= J * (a + b);
                        x.y -= J * (q + e);
                        v -= T * (o.rA.x * q - o.rA.y * a + j.rA.x * e - j.rA.y * b);
                        C.x += P * (a + b);
                        C.y += P * (q + e);
                        t += X * (o.rB.x * q - o.rB.y * a + j.rB.x * e - j.rB.y * b);
                        o.normalImpulse = $;
                        j.normalImpulse = f;
                        break
                    }
                    $ = 0;
                    f = -j.normalMass * c;
                    ba = a.K.col2.x * f + n;
                    if (f >= 0 && ba >= 0) {
                        q = $ - q;
                        g = f - g;
                        a = q * b;
                        q = q * e;
                        b = g * b;
                        e = g * e;
                        x.x -= J * (a + b);
                        x.y -= J * (q + e);
                        v -= T * (o.rA.x * q - o.rA.y * a + j.rA.x * e - j.rA.y * b);
                        C.x += P * (a + b);
                        C.y += P * (q + e);
                        t += X * (o.rB.x * q - o.rB.y * a + j.rB.x * e - j.rB.y * b);
                        o.normalImpulse = $;
                        j.normalImpulse = f;
                        break
                    }
                    f = $ = 0;
                    ba = n;
                    ca = c;
                    if (ba >= 0 && ca >= 0) {
                        q = $ - q;
                        g = f - g;
                        a = q * b;
                        q = q * e;
                        b = g * b;
                        e = g * e;
                        x.x -= J * (a + b);
                        x.y -= J * (q + e);
                        v -= T * (o.rA.x * q - o.rA.y * a + j.rA.x * e - j.rA.y * b);
                        C.x += P * (a + b);
                        C.y += P * (q + e);
                        t += X * (o.rB.x * q - o.rB.y * a + j.rB.x * e - j.rB.y * b);
                        o.normalImpulse = $;
                        j.normalImpulse = f;
                        break
                    }
                    break
                }
            }
            r.m_angularVelocity = v;
            s.m_angularVelocity = t
        }
    };
    L.prototype.FinalizeVelocityConstraints = function () {
        for (var j = 0; j < this.m_constraintCount; ++j) for (var o = this.m_constraints[j], q = o.manifold, n = 0; n < o.pointCount; ++n) {
                var a = q.m_points[n],
                    c = o.points[n];
                a.m_normalImpulse = c.normalImpulse;
                a.m_tangentImpulse = c.tangentImpulse
            }
    };
    L.prototype.SolvePositionConstraints = function (j) {
        if (j === undefined) j = 0;
        for (var o = 0, q = 0; q < this.m_constraintCount; q++) {
            var n = this.m_constraints[q],
                a = n.bodyA,
                c = n.bodyB,
                g = a.m_mass * a.m_invMass,
                b = a.m_mass * a.m_invI,
                e = c.m_mass * c.m_invMass,
                f = c.m_mass * c.m_invI;
            L.s_psm.Initialize(n);
            for (var m = L.s_psm.m_normal, r = 0; r < n.pointCount; r++) {
                var s = n.points[r],
                    v = L.s_psm.m_points[r],
                    t = L.s_psm.m_separations[r],
                    x = v.x - a.m_sweep.c.x,
                    C = v.y - a.m_sweep.c.y,
                    J = v.x - c.m_sweep.c.x;
                v = v.y - c.m_sweep.c.y;
                o = o < t ? o : t;
                t = R.Clamp(j * (t + O.b2_linearSlop), -O.b2_maxLinearCorrection, 0);
                t = -s.equalizedMass * t;
                s = t * m.x;
                t = t * m.y;
                a.m_sweep.c.x -= g * s;
                a.m_sweep.c.y -= g * t;
                a.m_sweep.a -= b * (x * t - C * s);
                a.SynchronizeTransform();
                c.m_sweep.c.x += e * s;
                c.m_sweep.c.y += e * t;
                c.m_sweep.a += f * (J * t - v * s);
                c.SynchronizeTransform()
            }
        }
        return o > -1.5 * O.b2_linearSlop
    };
    Box2D.postDefs.push(function () {
        Box2D.Dynamics.Contacts.b2ContactSolver.s_worldManifold = new l;
        Box2D.Dynamics.Contacts.b2ContactSolver.s_psm = new u
    });
    Box2D.inherit(I, Box2D.Dynamics.Contacts.b2Contact);
    I.prototype.__super = Box2D.Dynamics.Contacts.b2Contact.prototype;
    I.b2EdgeAndCircleContact = function () {
        Box2D.Dynamics.Contacts.b2Contact.b2Contact.apply(this, arguments)
    };
    I.Create = function () {
        return new I
    };
    I.Destroy = function () {};
    I.prototype.Reset = function (j, o) {
        this.__super.Reset.call(this, j, o)
    };
    I.prototype.Evaluate = function () {
        var j = this.m_fixtureA.GetBody(),
            o = this.m_fixtureB.GetBody();
        this.b2CollideEdgeAndCircle(this.m_manifold, this.m_fixtureA.GetShape() instanceof G ? this.m_fixtureA.GetShape() : null, j.m_xf,
            this.m_fixtureB.GetShape() instanceof F ? this.m_fixtureB.GetShape() : null, o.m_xf)
    };
    I.prototype.b2CollideEdgeAndCircle = function () {};
    Box2D.inherit(W, Box2D.Dynamics.Contacts.b2Contact);
    W.prototype.__super = Box2D.Dynamics.Contacts.b2Contact.prototype;
    W.b2NullContact = function () {
        Box2D.Dynamics.Contacts.b2Contact.b2Contact.apply(this, arguments)
    };
    W.prototype.b2NullContact = function () {
        this.__super.b2Contact.call(this)
    };
    W.prototype.Evaluate = function () {};
    Box2D.inherit(Y, Box2D.Dynamics.Contacts.b2Contact);
    Y.prototype.__super = Box2D.Dynamics.Contacts.b2Contact.prototype;
    Y.b2PolyAndCircleContact = function () {
        Box2D.Dynamics.Contacts.b2Contact.b2Contact.apply(this, arguments)
    };
    Y.Create = function () {
        return new Y
    };
    Y.Destroy = function () {};
    Y.prototype.Reset = function (j, o) {
        this.__super.Reset.call(this, j, o);
        O.b2Assert(j.GetType() == y.e_polygonShape);
        O.b2Assert(o.GetType() == y.e_circleShape)
    };
    Y.prototype.Evaluate = function () {
        var j = this.m_fixtureA.m_body,
            o = this.m_fixtureB.m_body;
        S.CollidePolygonAndCircle(this.m_manifold, this.m_fixtureA.GetShape() instanceof K ? this.m_fixtureA.GetShape() : null, j.m_xf, this.m_fixtureB.GetShape() instanceof F ? this.m_fixtureB.GetShape() : null, o.m_xf)
    };
    Box2D.inherit(k, Box2D.Dynamics.Contacts.b2Contact);
    k.prototype.__super = Box2D.Dynamics.Contacts.b2Contact.prototype;
    k.b2PolyAndEdgeContact = function () {
        Box2D.Dynamics.Contacts.b2Contact.b2Contact.apply(this, arguments)
    };
    k.Create = function () {
        return new k
    };
    k.Destroy = function () {};
    k.prototype.Reset = function (j, o) {
        this.__super.Reset.call(this, j, o);
        O.b2Assert(j.GetType() == y.e_polygonShape);
        O.b2Assert(o.GetType() == y.e_edgeShape)
    };
    k.prototype.Evaluate = function () {
        var j = this.m_fixtureA.GetBody(),
            o = this.m_fixtureB.GetBody();
        this.b2CollidePolyAndEdge(this.m_manifold, this.m_fixtureA.GetShape() instanceof K ? this.m_fixtureA.GetShape() : null, j.m_xf, this.m_fixtureB.GetShape() instanceof G ? this.m_fixtureB.GetShape() : null, o.m_xf)
    };
    k.prototype.b2CollidePolyAndEdge = function () {};
    Box2D.inherit(z, Box2D.Dynamics.Contacts.b2Contact);
    z.prototype.__super = Box2D.Dynamics.Contacts.b2Contact.prototype;
    z.b2PolygonContact = function () {
            Box2D.Dynamics.Contacts.b2Contact.b2Contact.apply(this, arguments)
    };
    z.Create = function () {
        return new z
    };
    z.Destroy = function () {};
    z.prototype.Reset = function (j, o) {
        this.__super.Reset.call(this, j, o)
    };
    z.prototype.Evaluate = function () {
        var j = this.m_fixtureA.GetBody(),
            o = this.m_fixtureB.GetBody();
        S.CollidePolygons(this.m_manifold, this.m_fixtureA.GetShape() instanceof K ? this.m_fixtureA.GetShape() : null, j.m_xf, this.m_fixtureB.GetShape() instanceof K ? this.m_fixtureB.GetShape() : null, o.m_xf)
    };
    u.b2PositionSolverManifold = function () {};
    u.prototype.b2PositionSolverManifold = function () {
        this.m_normal = new N;
        this.m_separations = new Vector_a2j_Number(O.b2_maxManifoldPoints);
        this.m_points = new Vector(O.b2_maxManifoldPoints);
        for (var j = 0; j < O.b2_maxManifoldPoints; j++) this.m_points[j] = new N
    };
    u.prototype.Initialize = function (j) {
        O.b2Assert(j.pointCount > 0);
        var o = 0,
            q = 0,
            n = 0,
            a, c = 0,
            g = 0;
        switch (j.type) {
        case Z.e_circles:
            a = j.bodyA.m_xf.R;
            n = j.localPoint;
            o = j.bodyA.m_xf.position.x + (a.col1.x * n.x + a.col2.x * n.y);
            q = j.bodyA.m_xf.position.y + (a.col1.y * n.x + a.col2.y * n.y);
            a = j.bodyB.m_xf.R;
            n = j.points[0].localPoint;
            c = j.bodyB.m_xf.position.x + (a.col1.x * n.x + a.col2.x * n.y);
            a = j.bodyB.m_xf.position.y + (a.col1.y * n.x + a.col2.y * n.y);
            n = c - o;
            g = a - q;
            var b = n * n + g * g;
            if (b > Number.MIN_VALUE * Number.MIN_VALUE) {
                b = Math.sqrt(b);
                this.m_normal.x = n / b;
                this.m_normal.y = g / b
            } else {
                this.m_normal.x = 1;
                this.m_normal.y = 0
            }
            this.m_points[0].x = 0.5 * (o + c);
            this.m_points[0].y = 0.5 * (q + a);
            this.m_separations[0] = n * this.m_normal.x + g * this.m_normal.y - j.radius;
            break;
        case Z.e_faceA:
            a = j.bodyA.m_xf.R;
            n = j.localPlaneNormal;
            this.m_normal.x = a.col1.x * n.x + a.col2.x * n.y;
            this.m_normal.y = a.col1.y * n.x + a.col2.y * n.y;
            a = j.bodyA.m_xf.R;
            n = j.localPoint;
            c = j.bodyA.m_xf.position.x + (a.col1.x * n.x + a.col2.x * n.y);
            g = j.bodyA.m_xf.position.y + (a.col1.y * n.x + a.col2.y * n.y);
            a = j.bodyB.m_xf.R;
            for (o = 0; o < j.pointCount; ++o) {
                n = j.points[o].localPoint;
                q = j.bodyB.m_xf.position.x + (a.col1.x * n.x + a.col2.x * n.y);
                n = j.bodyB.m_xf.position.y + (a.col1.y * n.x + a.col2.y * n.y);
                this.m_separations[o] = (q - c) * this.m_normal.x + (n - g) * this.m_normal.y - j.radius;
                this.m_points[o].x = q;
                this.m_points[o].y = n
            }
            break;
        case Z.e_faceB:
            a = j.bodyB.m_xf.R;
            n = j.localPlaneNormal;
            this.m_normal.x = a.col1.x * n.x + a.col2.x * n.y;
            this.m_normal.y = a.col1.y * n.x + a.col2.y * n.y;
            a = j.bodyB.m_xf.R;
            n = j.localPoint;
            c = j.bodyB.m_xf.position.x + (a.col1.x * n.x + a.col2.x * n.y);
            g = j.bodyB.m_xf.position.y + (a.col1.y * n.x + a.col2.y * n.y);
            a = j.bodyA.m_xf.R;
            for (o = 0; o < j.pointCount; ++o) {
                n = j.points[o].localPoint;
                q = j.bodyA.m_xf.position.x + (a.col1.x * n.x + a.col2.x * n.y);
                n = j.bodyA.m_xf.position.y + (a.col1.y * n.x + a.col2.y * n.y);
                this.m_separations[o] = (q - c) * this.m_normal.x + (n - g) * this.m_normal.y - j.radius;
                this.m_points[o].Set(q, n)
            }
            this.m_normal.x *= -1;
            this.m_normal.y *= -1
        }
    };
    Box2D.postDefs.push(function () {
        Box2D.Dynamics.Contacts.b2PositionSolverManifold.circlePointA = new N;
        Box2D.Dynamics.Contacts.b2PositionSolverManifold.circlePointB = new N
    })
})();
(function () {
    var F = Box2D.Common.Math.b2Mat22,
        G = Box2D.Common.Math.b2Math,
        K = Box2D.Common.Math.b2Vec2,
        y = Box2D.Common.b2Color,
        w = Box2D.Dynamics.Controllers.b2BuoyancyController,
        A = Box2D.Dynamics.Controllers.b2ConstantAccelController,
        U = Box2D.Dynamics.Controllers.b2ConstantForceController,
        p = Box2D.Dynamics.Controllers.b2Controller,
        B = Box2D.Dynamics.Controllers.b2ControllerEdge,
        Q = Box2D.Dynamics.Controllers.b2GravityController,
        V = Box2D.Dynamics.Controllers.b2TensorDampingController;
    Box2D.inherit(w, Box2D.Dynamics.Controllers.b2Controller);
    w.prototype.__super = Box2D.Dynamics.Controllers.b2Controller.prototype;
    w.b2BuoyancyController = function () {
        Box2D.Dynamics.Controllers.b2Controller.b2Controller.apply(this, arguments);
        this.normal = new K(0, -1);
        this.density = this.offset = 0;
        this.velocity = new K(0, 0);
        this.linearDrag = 2;
        this.angularDrag = 1;
        this.useDensity = false;
        this.useWorldGravity = true;
        this.gravity = null
    };
    w.prototype.Step = function () {
        if (this.m_bodyList) {
            if (this.useWorldGravity) this.gravity = this.GetWorld().GetGravity().Copy();
            for (var M = this.m_bodyList; M; M = M.nextBody) {
                var L = M.body;
                if (L.IsAwake() != false) {
                    for (var I = new K, W = new K, Y = 0, k = 0, z = L.GetFixtureList(); z; z = z.GetNext()) {
                        var u = new K,
                            D = z.GetShape().ComputeSubmergedArea(this.normal, this.offset, L.GetTransform(), u);
                        Y += D;
                        I.x += D * u.x;
                        I.y += D * u.y;
                        var H = 0;
                        H = 1;
                        k += D * H;
                        W.x += D * u.x * H;
                        W.y += D * u.y * H
                    }
                    I.x /= Y;
                    I.y /= Y;
                    W.x /= k;
                    W.y /= k;
                    if (!(Y < Number.MIN_VALUE)) {
                        k = this.gravity.GetNegative();
                        k.Multiply(this.density * Y);
                        L.ApplyForce(k, W);
                        W = L.GetLinearVelocityFromWorldPoint(I);
                        W.Subtract(this.velocity);
                        W.Multiply(-this.linearDrag * Y);
                        L.ApplyForce(W, I);
                        L.ApplyTorque(-L.GetInertia() / L.GetMass() * Y * L.GetAngularVelocity() * this.angularDrag)
                    }
                }
            }
        }
    };
    w.prototype.Draw = function (M) {
        var L = new K,
            I = new K;
        L.x = this.normal.x * this.offset + this.normal.y * 1E3;
        L.y = this.normal.y * this.offset - this.normal.x * 1E3;
        I.x = this.normal.x * this.offset - this.normal.y * 1E3;
        I.y = this.normal.y * this.offset + this.normal.x * 1E3;
        var W = new y(0, 0, 1);
        M.DrawSegment(L, I, W)
    };
    Box2D.inherit(A, Box2D.Dynamics.Controllers.b2Controller);
    A.prototype.__super = Box2D.Dynamics.Controllers.b2Controller.prototype;
    A.b2ConstantAccelController = function () {
        Box2D.Dynamics.Controllers.b2Controller.b2Controller.apply(this, arguments);
        this.A = new K(0, 0)
    };
    A.prototype.Step = function (M) {
        M = new K(this.A.x * M.dt, this.A.y * M.dt);
        for (var L = this.m_bodyList; L; L = L.nextBody) {
            var I = L.body;
            I.IsAwake() && I.SetLinearVelocity(new K(I.GetLinearVelocity().x + M.x, I.GetLinearVelocity().y + M.y))
        }
    };
    Box2D.inherit(U, Box2D.Dynamics.Controllers.b2Controller);
    U.prototype.__super = Box2D.Dynamics.Controllers.b2Controller.prototype;
    U.b2ConstantForceController = function () {
            Box2D.Dynamics.Controllers.b2Controller.b2Controller.apply(this, arguments);
            this.F = new K(0, 0)
    };
    U.prototype.Step = function () {
        for (var M = this.m_bodyList; M; M = M.nextBody) {
            var L = M.body;
            L.IsAwake() && L.ApplyForce(this.F, L.GetWorldCenter())
        }
    };
    p.b2Controller = function () {};
    p.prototype.Step = function () {};
    p.prototype.Draw = function () {};
    p.prototype.AddBody = function (M) {
        var L = new B;
        L.controller = this;
        L.body = M;
        L.nextBody = this.m_bodyList;
        L.prevBody = null;
        this.m_bodyList = L;
        if (L.nextBody) L.nextBody.prevBody = L;
        this.m_bodyCount++;
        L.nextController = M.m_controllerList;
        L.prevController = null;
        M.m_controllerList = L;
        if (L.nextController) L.nextController.prevController = L;
        M.m_controllerCount++
    };
    p.prototype.RemoveBody = function (M) {
        for (var L = M.m_controllerList; L && L.controller != this;) L = L.nextController;
        if (L.prevBody) L.prevBody.nextBody = L.nextBody;
        if (L.nextBody) L.nextBody.prevBody = L.prevBody;
        if (L.nextController) L.nextController.prevController = L.prevController;
        if (L.prevController) L.prevController.nextController = L.nextController;
        if (this.m_bodyList == L) this.m_bodyList = L.nextBody;
        if (M.m_controllerList == L) M.m_controllerList = L.nextController;
        M.m_controllerCount--;
        this.m_bodyCount--
    };
    p.prototype.Clear = function () {
        for (; this.m_bodyList;) this.RemoveBody(this.m_bodyList.body)
    };
    p.prototype.GetNext = function () {
        return this.m_next
    };
    p.prototype.GetWorld = function () {
        return this.m_world
    };
    p.prototype.GetBodyList = function () {
        return this.m_bodyList
    };
    B.b2ControllerEdge = function () {};
    Box2D.inherit(Q, Box2D.Dynamics.Controllers.b2Controller);
    Q.prototype.__super = Box2D.Dynamics.Controllers.b2Controller.prototype;
    Q.b2GravityController = function () {
        Box2D.Dynamics.Controllers.b2Controller.b2Controller.apply(this, arguments);
        this.G = 1;
        this.invSqr = true
    };
    Q.prototype.Step = function () {
        var M = null,
            L = null,
            I = null,
            W = 0,
            Y = null,
            k = null,
            z = null,
            u = 0,
            D = 0,
            H = 0;
        u = null;
        if (this.invSqr) for (M = this.m_bodyList; M; M = M.nextBody) {
                L = M.body;
                I = L.GetWorldCenter();
                W = L.GetMass();
                for (Y = this.m_bodyList; Y != M; Y = Y.nextBody) {
                    k = Y.body;
                    z = k.GetWorldCenter();
                    u = z.x - I.x;
                    D = z.y - I.y;
                    H = u * u + D * D;
                    if (!(H < Number.MIN_VALUE)) {
                        u = new K(u, D);
                        u.Multiply(this.G / H / Math.sqrt(H) * W * k.GetMass());
                        L.IsAwake() && L.ApplyForce(u, I);
                        u.Multiply(-1);
                        k.IsAwake() && k.ApplyForce(u, z)
                    }
                }
            } else
                for (M = this.m_bodyList; M; M = M.nextBody) {
                    L = M.body;
                    I = L.GetWorldCenter();
                    W = L.GetMass();
                    for (Y = this.m_bodyList; Y != M; Y = Y.nextBody) {
                        k = Y.body;
                        z = k.GetWorldCenter();
                        u = z.x - I.x;
                        D = z.y - I.y;
                        H = u * u + D * D;
                        if (!(H < Number.MIN_VALUE)) {
                            u = new K(u, D);
                            u.Multiply(this.G / H * W * k.GetMass());
                            L.IsAwake() && L.ApplyForce(u, I);
                            u.Multiply(-1);
                            k.IsAwake() && k.ApplyForce(u, z)
                        }
                    }
                }
    };
    Box2D.inherit(V, Box2D.Dynamics.Controllers.b2Controller);
    V.prototype.__super = Box2D.Dynamics.Controllers.b2Controller.prototype;
    V.b2TensorDampingController = function () {
        Box2D.Dynamics.Controllers.b2Controller.b2Controller.apply(this, arguments);
        this.T = new F;
        this.maxTimestep = 0
    };
    V.prototype.SetAxisAligned = function (M, L) {
        if (M === undefined) M = 0;
        if (L === undefined) L = 0;
        this.T.col1.x = -M;
        this.T.col1.y = 0;
        this.T.col2.x = 0;
        this.T.col2.y = -L;
        this.maxTimestep = M > 0 || L > 0 ? 1 / Math.max(M, L) : 0
    };
    V.prototype.Step = function (M) {
        M = M.dt;
        if (!(M <= Number.MIN_VALUE)) {
            if (M > this.maxTimestep && this.maxTimestep > 0) M = this.maxTimestep;
            for (var L = this.m_bodyList; L; L = L.nextBody) {
                var I = L.body;
                if (I.IsAwake()) {
                    var W = I.GetWorldVector(G.MulMV(this.T, I.GetLocalVector(I.GetLinearVelocity())));
                    I.SetLinearVelocity(new K(I.GetLinearVelocity().x + W.x * M, I.GetLinearVelocity().y + W.y * M))
                }
            }
        }
    }
})();
(function () {
    var F = Box2D.Common.b2Settings,
        G = Box2D.Common.Math.b2Mat22,
        K = Box2D.Common.Math.b2Mat33,
        y = Box2D.Common.Math.b2Math,
        w = Box2D.Common.Math.b2Vec2,
        A = Box2D.Common.Math.b2Vec3,
        U = Box2D.Dynamics.Joints.b2DistanceJoint,
        p = Box2D.Dynamics.Joints.b2DistanceJointDef,
        B = Box2D.Dynamics.Joints.b2FrictionJoint,
        Q = Box2D.Dynamics.Joints.b2FrictionJointDef,
        V = Box2D.Dynamics.Joints.b2GearJoint,
        M = Box2D.Dynamics.Joints.b2GearJointDef,
        L = Box2D.Dynamics.Joints.b2Jacobian,
        I = Box2D.Dynamics.Joints.b2Joint,
        W = Box2D.Dynamics.Joints.b2JointDef,
        Y = Box2D.Dynamics.Joints.b2JointEdge,
        k = Box2D.Dynamics.Joints.b2LineJoint,
        z = Box2D.Dynamics.Joints.b2LineJointDef,
        u = Box2D.Dynamics.Joints.b2MouseJoint,
        D = Box2D.Dynamics.Joints.b2MouseJointDef,
        H = Box2D.Dynamics.Joints.b2PrismaticJoint,
        O = Box2D.Dynamics.Joints.b2PrismaticJointDef,
        E = Box2D.Dynamics.Joints.b2PulleyJoint,
        R = Box2D.Dynamics.Joints.b2PulleyJointDef,
        N = Box2D.Dynamics.Joints.b2RevoluteJoint,
        S = Box2D.Dynamics.Joints.b2RevoluteJointDef,
        aa = Box2D.Dynamics.Joints.b2WeldJoint,
        Z = Box2D.Dynamics.Joints.b2WeldJointDef;
    Box2D.inherit(U, Box2D.Dynamics.Joints.b2Joint);
    U.prototype.__super = Box2D.Dynamics.Joints.b2Joint.prototype;
    U.b2DistanceJoint = function () {
        Box2D.Dynamics.Joints.b2Joint.b2Joint.apply(this, arguments);
        this.m_localAnchor1 = new w;
        this.m_localAnchor2 = new w;
        this.m_u = new w
    };
    U.prototype.GetAnchorA = function () {
        return this.m_bodyA.GetWorldPoint(this.m_localAnchor1)
    };
    U.prototype.GetAnchorB = function () {
        return this.m_bodyB.GetWorldPoint(this.m_localAnchor2)
    };
    U.prototype.GetReactionForce = function (d) {
        if (d === undefined) d = 0;
        return new w(d * this.m_impulse * this.m_u.x, d * this.m_impulse * this.m_u.y)
    };
    U.prototype.GetReactionTorque = function () {
        return 0
    };
    U.prototype.GetLength = function () {
        return this.m_length
    };
    U.prototype.SetLength = function (d) {
        if (d === undefined) d = 0;
        this.m_length = d
    };
    U.prototype.GetFrequency = function () {
        return this.m_frequencyHz
    };
    U.prototype.SetFrequency = function (d) {
        if (d === undefined) d = 0;
        this.m_frequencyHz = d
    };
    U.prototype.GetDampingRatio = function () {
        return this.m_dampingRatio
    };
    U.prototype.SetDampingRatio = function (d) {
        if (d === undefined) d = 0;
        this.m_dampingRatio = d
    };
    U.prototype.b2DistanceJoint = function (d) {
        this.__super.b2Joint.call(this, d);
        this.m_localAnchor1.SetV(d.localAnchorA);
        this.m_localAnchor2.SetV(d.localAnchorB);
        this.m_length = d.length;
        this.m_frequencyHz = d.frequencyHz;
        this.m_dampingRatio = d.dampingRatio;
        this.m_bias = this.m_gamma = this.m_impulse = 0
    };
    U.prototype.InitVelocityConstraints = function (d) {
        var h, l = 0,
            j = this.m_bodyA,
            o = this.m_bodyB;
        h = j.m_xf.R;
        var q = this.m_localAnchor1.x - j.m_sweep.localCenter.x,
            n = this.m_localAnchor1.y - j.m_sweep.localCenter.y;
        l = h.col1.x * q + h.col2.x * n;
        n = h.col1.y * q + h.col2.y * n;
        q = l;
        h = o.m_xf.R;
        var a = this.m_localAnchor2.x - o.m_sweep.localCenter.x,
            c = this.m_localAnchor2.y - o.m_sweep.localCenter.y;
        l = h.col1.x * a + h.col2.x * c;
        c = h.col1.y * a + h.col2.y * c;
        a = l;
        this.m_u.x = o.m_sweep.c.x + a - j.m_sweep.c.x - q;
        this.m_u.y = o.m_sweep.c.y + c - j.m_sweep.c.y - n;
        l = Math.sqrt(this.m_u.x * this.m_u.x + this.m_u.y * this.m_u.y);
        l > F.b2_linearSlop ? this.m_u.Multiply(1 / l) : this.m_u.SetZero();
        h = q * this.m_u.y - n * this.m_u.x;
        var g = a * this.m_u.y - c * this.m_u.x;
        h = j.m_invMass + j.m_invI * h * h + o.m_invMass + o.m_invI * g * g;
        this.m_mass = h != 0 ? 1 / h : 0;
        if (this.m_frequencyHz > 0) {
            l = l - this.m_length;
            g = 2 * Math.PI * this.m_frequencyHz;
            var b = this.m_mass * g * g;
            this.m_gamma = d.dt * (2 * this.m_mass * this.m_dampingRatio * g + d.dt * b);
            this.m_gamma = this.m_gamma != 0 ? 1 / this.m_gamma : 0;
            this.m_bias = l * d.dt * b * this.m_gamma;
            this.m_mass = h + this.m_gamma;
            this.m_mass = this.m_mass != 0 ? 1 / this.m_mass : 0
        }
        if (d.warmStarting) {
            this.m_impulse *= d.dtRatio;
            d = this.m_impulse * this.m_u.x;
            h = this.m_impulse * this.m_u.y;
            j.m_linearVelocity.x -= j.m_invMass * d;
            j.m_linearVelocity.y -= j.m_invMass * h;
            j.m_angularVelocity -= j.m_invI * (q * h - n * d);
            o.m_linearVelocity.x += o.m_invMass * d;
            o.m_linearVelocity.y += o.m_invMass * h;
            o.m_angularVelocity += o.m_invI * (a * h - c * d)
        } else this.m_impulse = 0
    };
    U.prototype.SolveVelocityConstraints = function () {
        var d, h = this.m_bodyA,
            l = this.m_bodyB;
        d = h.m_xf.R;
        var j = this.m_localAnchor1.x - h.m_sweep.localCenter.x,
            o = this.m_localAnchor1.y - h.m_sweep.localCenter.y,
            q = d.col1.x * j + d.col2.x * o;
        o = d.col1.y * j + d.col2.y * o;
        j = q;
        d = l.m_xf.R;
        var n = this.m_localAnchor2.x - l.m_sweep.localCenter.x,
            a = this.m_localAnchor2.y - l.m_sweep.localCenter.y;
        q = d.col1.x * n + d.col2.x * a;
        a = d.col1.y * n + d.col2.y * a;
        n = q;
        q = -this.m_mass * (this.m_u.x * (l.m_linearVelocity.x + -l.m_angularVelocity * a - (h.m_linearVelocity.x + -h.m_angularVelocity * o)) + this.m_u.y * (l.m_linearVelocity.y + l.m_angularVelocity * n - (h.m_linearVelocity.y + h.m_angularVelocity * j)) + this.m_bias + this.m_gamma * this.m_impulse);
        this.m_impulse += q;
        d = q * this.m_u.x;
        q = q * this.m_u.y;
        h.m_linearVelocity.x -= h.m_invMass * d;
        h.m_linearVelocity.y -= h.m_invMass * q;
        h.m_angularVelocity -= h.m_invI * (j * q - o * d);
        l.m_linearVelocity.x += l.m_invMass * d;
        l.m_linearVelocity.y += l.m_invMass * q;
        l.m_angularVelocity += l.m_invI * (n * q - a * d)
    };
    U.prototype.SolvePositionConstraints = function () {
        var d;
        if (this.m_frequencyHz > 0) return true;
        var h = this.m_bodyA,
            l = this.m_bodyB;
        d = h.m_xf.R;
        var j = this.m_localAnchor1.x - h.m_sweep.localCenter.x,
            o = this.m_localAnchor1.y - h.m_sweep.localCenter.y,
            q = d.col1.x * j + d.col2.x * o;
        o = d.col1.y * j + d.col2.y * o;
        j = q;
        d = l.m_xf.R;
        var n = this.m_localAnchor2.x - l.m_sweep.localCenter.x,
            a = this.m_localAnchor2.y - l.m_sweep.localCenter.y;
        q = d.col1.x * n + d.col2.x * a;
        a = d.col1.y * n + d.col2.y * a;
        n = q;
        q = l.m_sweep.c.x + n - h.m_sweep.c.x - j;
        var c = l.m_sweep.c.y + a - h.m_sweep.c.y - o;
        d = Math.sqrt(q * q + c * c);
        q /= d;
        c /= d;
        d = d - this.m_length;
        d = y.Clamp(d, -F.b2_maxLinearCorrection, F.b2_maxLinearCorrection);
        var g = -this.m_mass * d;
        this.m_u.Set(q, c);
        q = g * this.m_u.x;
        c = g * this.m_u.y;
        h.m_sweep.c.x -= h.m_invMass * q;
        h.m_sweep.c.y -= h.m_invMass * c;
        h.m_sweep.a -= h.m_invI * (j * c - o * q);
        l.m_sweep.c.x += l.m_invMass * q;
        l.m_sweep.c.y += l.m_invMass * c;
        l.m_sweep.a += l.m_invI * (n * c - a * q);
        h.SynchronizeTransform();
        l.SynchronizeTransform();
        return y.Abs(d) < F.b2_linearSlop
    };
    Box2D.inherit(p, Box2D.Dynamics.Joints.b2JointDef);
    p.prototype.__super = Box2D.Dynamics.Joints.b2JointDef.prototype;
    p.b2DistanceJointDef = function () {
        Box2D.Dynamics.Joints.b2JointDef.b2JointDef.apply(this, arguments);
        this.localAnchorA = new w;
        this.localAnchorB = new w
    };
    p.prototype.b2DistanceJointDef = function () {
        this.__super.b2JointDef.call(this);
        this.type = I.e_distanceJoint;
        this.length = 1;
        this.dampingRatio = this.frequencyHz = 0
    };
    p.prototype.Initialize = function (d, h, l, j) {
        this.bodyA = d;
        this.bodyB = h;
        this.localAnchorA.SetV(this.bodyA.GetLocalPoint(l));
        this.localAnchorB.SetV(this.bodyB.GetLocalPoint(j));
        d = j.x - l.x;
        l = j.y - l.y;
        this.length = Math.sqrt(d * d + l * l);
        this.dampingRatio = this.frequencyHz = 0
    };
    Box2D.inherit(B, Box2D.Dynamics.Joints.b2Joint);
    B.prototype.__super = Box2D.Dynamics.Joints.b2Joint.prototype;
    B.b2FrictionJoint = function () {
        Box2D.Dynamics.Joints.b2Joint.b2Joint.apply(this, arguments);
        this.m_localAnchorA = new w;
        this.m_localAnchorB = new w;
        this.m_linearMass = new G;
        this.m_linearImpulse = new w
    };
    B.prototype.GetAnchorA = function () {
        return this.m_bodyA.GetWorldPoint(this.m_localAnchorA)
    };
    B.prototype.GetAnchorB = function () {
        return this.m_bodyB.GetWorldPoint(this.m_localAnchorB)
    };
    B.prototype.GetReactionForce = function (d) {
        if (d === undefined) d = 0;
        return new w(d * this.m_linearImpulse.x, d * this.m_linearImpulse.y)
    };
    B.prototype.GetReactionTorque = function (d) {
        if (d === undefined) d = 0;
        return d * this.m_angularImpulse
    };
    B.prototype.SetMaxForce = function (d) {
            if (d === undefined) d = 0;
            this.m_maxForce = d
    };
    B.prototype.GetMaxForce = function () {
        return this.m_maxForce
    };
    B.prototype.SetMaxTorque = function (d) {
        if (d === undefined) d = 0;
        this.m_maxTorque = d
    };
    B.prototype.GetMaxTorque = function () {
        return this.m_maxTorque
    };
    B.prototype.b2FrictionJoint = function (d) {
        this.__super.b2Joint.call(this, d);
        this.m_localAnchorA.SetV(d.localAnchorA);
        this.m_localAnchorB.SetV(d.localAnchorB);
        this.m_linearMass.SetZero();
        this.m_angularMass = 0;
        this.m_linearImpulse.SetZero();
        this.m_angularImpulse = 0;
        this.m_maxForce = d.maxForce;
        this.m_maxTorque = d.maxTorque
    };
    B.prototype.InitVelocityConstraints = function (d) {
        var h, l = 0,
            j = this.m_bodyA,
            o = this.m_bodyB;
        h = j.m_xf.R;
        var q = this.m_localAnchorA.x - j.m_sweep.localCenter.x,
            n = this.m_localAnchorA.y - j.m_sweep.localCenter.y;
        l = h.col1.x * q + h.col2.x * n;
        n = h.col1.y * q + h.col2.y * n;
        q = l;
        h = o.m_xf.R;
        var a = this.m_localAnchorB.x - o.m_sweep.localCenter.x,
            c = this.m_localAnchorB.y - o.m_sweep.localCenter.y;
        l = h.col1.x * a + h.col2.x * c;
        c = h.col1.y * a + h.col2.y * c;
        a = l;
        h = j.m_invMass;
        l = o.m_invMass;
        var g = j.m_invI,
            b = o.m_invI,
            e = new G;
        e.col1.x = h + l;
        e.col2.x = 0;
        e.col1.y = 0;
        e.col2.y = h + l;
        e.col1.x += g * n * n;
        e.col2.x += -g * q * n;
        e.col1.y += -g * q * n;
        e.col2.y += g * q * q;
        e.col1.x += b * c * c;
        e.col2.x += -b * a * c;
        e.col1.y += -b * a * c;
        e.col2.y += b * a * a;
        e.GetInverse(this.m_linearMass);
        this.m_angularMass = g + b;
        if (this.m_angularMass > 0) this.m_angularMass = 1 / this.m_angularMass;
        if (d.warmStarting) {
            this.m_linearImpulse.x *= d.dtRatio;
            this.m_linearImpulse.y *= d.dtRatio;
            this.m_angularImpulse *= d.dtRatio;
            d = this.m_linearImpulse;
            j.m_linearVelocity.x -= h * d.x;
            j.m_linearVelocity.y -= h * d.y;
            j.m_angularVelocity -= g * (q * d.y - n * d.x + this.m_angularImpulse);
            o.m_linearVelocity.x += l * d.x;
            o.m_linearVelocity.y += l * d.y;
            o.m_angularVelocity += b * (a * d.y - c * d.x + this.m_angularImpulse)
        } else {
            this.m_linearImpulse.SetZero();
            this.m_angularImpulse = 0
        }
    };
    B.prototype.SolveVelocityConstraints = function (d) {
        var h, l = 0,
            j = this.m_bodyA,
            o = this.m_bodyB,
            q = j.m_linearVelocity,
            n = j.m_angularVelocity,
            a = o.m_linearVelocity,
            c = o.m_angularVelocity,
            g = j.m_invMass,
            b = o.m_invMass,
            e = j.m_invI,
            f = o.m_invI;
        h = j.m_xf.R;
        var m = this.m_localAnchorA.x - j.m_sweep.localCenter.x,
            r = this.m_localAnchorA.y - j.m_sweep.localCenter.y;
        l = h.col1.x * m + h.col2.x * r;
        r = h.col1.y * m + h.col2.y * r;
        m = l;
        h = o.m_xf.R;
        var s = this.m_localAnchorB.x - o.m_sweep.localCenter.x,
            v = this.m_localAnchorB.y - o.m_sweep.localCenter.y;
        l = h.col1.x * s + h.col2.x * v;
        v = h.col1.y * s + h.col2.y * v;
        s = l;
        h = 0;
        l = -this.m_angularMass * (c - n);
        var t = this.m_angularImpulse;
        h = d.dt * this.m_maxTorque;
        this.m_angularImpulse = y.Clamp(this.m_angularImpulse + l, -h, h);
        l = this.m_angularImpulse - t;
        n -= e * l;
        c += f * l;
        h = y.MulMV(this.m_linearMass,
            new w(-(a.x - c * v - q.x + n * r), -(a.y + c * s - q.y - n * m)));
        l = this.m_linearImpulse.Copy();
        this.m_linearImpulse.Add(h);
        h = d.dt * this.m_maxForce;
        if (this.m_linearImpulse.LengthSquared() > h * h) {
            this.m_linearImpulse.Normalize();
            this.m_linearImpulse.Multiply(h)
        }
        h = y.SubtractVV(this.m_linearImpulse, l);
        q.x -= g * h.x;
        q.y -= g * h.y;
        n -= e * (m * h.y - r * h.x);
        a.x += b * h.x;
        a.y += b * h.y;
        c += f * (s * h.y - v * h.x);
        j.m_angularVelocity = n;
        o.m_angularVelocity = c
    };
    B.prototype.SolvePositionConstraints = function () {
        return true
    };
    Box2D.inherit(Q, Box2D.Dynamics.Joints.b2JointDef);
    Q.prototype.__super = Box2D.Dynamics.Joints.b2JointDef.prototype;
    Q.b2FrictionJointDef = function () {
        Box2D.Dynamics.Joints.b2JointDef.b2JointDef.apply(this, arguments);
        this.localAnchorA = new w;
        this.localAnchorB = new w
    };
    Q.prototype.b2FrictionJointDef = function () {
        this.__super.b2JointDef.call(this);
        this.type = I.e_frictionJoint;
        this.maxTorque = this.maxForce = 0
    };
    Q.prototype.Initialize = function (d, h, l) {
        this.bodyA = d;
        this.bodyB = h;
        this.localAnchorA.SetV(this.bodyA.GetLocalPoint(l));
        this.localAnchorB.SetV(this.bodyB.GetLocalPoint(l))
    };
    Box2D.inherit(V, Box2D.Dynamics.Joints.b2Joint);
    V.prototype.__super = Box2D.Dynamics.Joints.b2Joint.prototype;
    V.b2GearJoint = function () {
        Box2D.Dynamics.Joints.b2Joint.b2Joint.apply(this, arguments);
        this.m_groundAnchor1 = new w;
        this.m_groundAnchor2 = new w;
        this.m_localAnchor1 = new w;
        this.m_localAnchor2 = new w;
        this.m_J = new L
    };
    V.prototype.GetAnchorA = function () {
        return this.m_bodyA.GetWorldPoint(this.m_localAnchor1)
    };
    V.prototype.GetAnchorB = function () {
        return this.m_bodyB.GetWorldPoint(this.m_localAnchor2)
    };
    V.prototype.GetReactionForce = function (d) {
            if (d === undefined) d = 0;
            return new w(d * this.m_impulse * this.m_J.linearB.x, d * this.m_impulse * this.m_J.linearB.y)
    };
    V.prototype.GetReactionTorque = function (d) {
        if (d === undefined) d = 0;
        var h = this.m_bodyB.m_xf.R,
            l = this.m_localAnchor1.x - this.m_bodyB.m_sweep.localCenter.x,
            j = this.m_localAnchor1.y - this.m_bodyB.m_sweep.localCenter.y,
            o = h.col1.x * l + h.col2.x * j;
        j = h.col1.y * l + h.col2.y * j;
        l = o;
        return d * (this.m_impulse * this.m_J.angularB - l * this.m_impulse * this.m_J.linearB.y + j * this.m_impulse * this.m_J.linearB.x)
    };
    V.prototype.GetRatio = function () {
            return this.m_ratio
    };
    V.prototype.SetRatio = function (d) {
        if (d === undefined) d = 0;
        this.m_ratio = d
    };
    V.prototype.b2GearJoint = function (d) {
        this.__super.b2Joint.call(this, d);
        var h = parseInt(d.joint1.m_type),
            l = parseInt(d.joint2.m_type);
        this.m_prismatic2 = this.m_revolute2 = this.m_prismatic1 = this.m_revolute1 = null;
        var j = 0,
            o = 0;
        this.m_ground1 = d.joint1.GetBodyA();
        this.m_bodyA = d.joint1.GetBodyB();
        if (h == I.e_revoluteJoint) {
            this.m_revolute1 = d.joint1 instanceof N ? d.joint1 : null;
            this.m_groundAnchor1.SetV(this.m_revolute1.m_localAnchor1);
            this.m_localAnchor1.SetV(this.m_revolute1.m_localAnchor2);
            j = this.m_revolute1.GetJointAngle()
        } else {
            this.m_prismatic1 = d.joint1 instanceof H ? d.joint1 : null;
            this.m_groundAnchor1.SetV(this.m_prismatic1.m_localAnchor1);
            this.m_localAnchor1.SetV(this.m_prismatic1.m_localAnchor2);
            j = this.m_prismatic1.GetJointTranslation()
        }
        this.m_ground2 = d.joint2.GetBodyA();
        this.m_bodyB = d.joint2.GetBodyB();
        if (l == I.e_revoluteJoint) {
            this.m_revolute2 = d.joint2 instanceof N ? d.joint2 : null;
            this.m_groundAnchor2.SetV(this.m_revolute2.m_localAnchor1);
            this.m_localAnchor2.SetV(this.m_revolute2.m_localAnchor2);
            o = this.m_revolute2.GetJointAngle()
        } else {
            this.m_prismatic2 = d.joint2 instanceof H ? d.joint2 : null;
            this.m_groundAnchor2.SetV(this.m_prismatic2.m_localAnchor1);
            this.m_localAnchor2.SetV(this.m_prismatic2.m_localAnchor2);
            o = this.m_prismatic2.GetJointTranslation()
        }
        this.m_ratio = d.ratio;
        this.m_constant = j + this.m_ratio * o;
        this.m_impulse = 0
    };
    V.prototype.InitVelocityConstraints = function (d) {
        var h = this.m_ground1,
            l = this.m_ground2,
            j = this.m_bodyA,
            o = this.m_bodyB,
            q = 0,
            n = 0,
            a = 0,
            c = 0,
            g = a = 0,
            b = 0;
        this.m_J.SetZero();
        if (this.m_revolute1) {
            this.m_J.angularA = -1;
            b += j.m_invI
        } else {
            h = h.m_xf.R;
            n = this.m_prismatic1.m_localXAxis1;
            q = h.col1.x * n.x + h.col2.x * n.y;
            n = h.col1.y * n.x + h.col2.y * n.y;
            h = j.m_xf.R;
            a = this.m_localAnchor1.x - j.m_sweep.localCenter.x;
            c = this.m_localAnchor1.y - j.m_sweep.localCenter.y;
            g = h.col1.x * a + h.col2.x * c;
            c = h.col1.y * a + h.col2.y * c;
            a = g;
            a = a * n - c * q;
            this.m_J.linearA.Set(-q, -n);
            this.m_J.angularA = -a;
            b += j.m_invMass + j.m_invI * a * a
        } if (this.m_revolute2) {
            this.m_J.angularB = -this.m_ratio;
            b += this.m_ratio * this.m_ratio * o.m_invI
        } else {
            h = l.m_xf.R;
            n = this.m_prismatic2.m_localXAxis1;
            q = h.col1.x * n.x + h.col2.x * n.y;
            n = h.col1.y * n.x + h.col2.y * n.y;
            h = o.m_xf.R;
            a = this.m_localAnchor2.x - o.m_sweep.localCenter.x;
            c = this.m_localAnchor2.y - o.m_sweep.localCenter.y;
            g = h.col1.x * a + h.col2.x * c;
            c = h.col1.y * a + h.col2.y * c;
            a = g;
            a = a * n - c * q;
            this.m_J.linearB.Set(-this.m_ratio * q, -this.m_ratio * n);
            this.m_J.angularB = -this.m_ratio * a;
            b += this.m_ratio * this.m_ratio * (o.m_invMass + o.m_invI * a * a)
        }
        this.m_mass = b > 0 ? 1 / b : 0;
        if (d.warmStarting) {
            j.m_linearVelocity.x += j.m_invMass * this.m_impulse * this.m_J.linearA.x;
            j.m_linearVelocity.y += j.m_invMass * this.m_impulse * this.m_J.linearA.y;
            j.m_angularVelocity += j.m_invI * this.m_impulse * this.m_J.angularA;
            o.m_linearVelocity.x += o.m_invMass * this.m_impulse * this.m_J.linearB.x;
            o.m_linearVelocity.y += o.m_invMass * this.m_impulse * this.m_J.linearB.y;
            o.m_angularVelocity += o.m_invI * this.m_impulse * this.m_J.angularB
        } else this.m_impulse = 0
    };
    V.prototype.SolveVelocityConstraints = function () {
        var d = this.m_bodyA,
            h = this.m_bodyB,
            l = -this.m_mass * this.m_J.Compute(d.m_linearVelocity,
                d.m_angularVelocity, h.m_linearVelocity, h.m_angularVelocity);
        this.m_impulse += l;
        d.m_linearVelocity.x += d.m_invMass * l * this.m_J.linearA.x;
        d.m_linearVelocity.y += d.m_invMass * l * this.m_J.linearA.y;
        d.m_angularVelocity += d.m_invI * l * this.m_J.angularA;
        h.m_linearVelocity.x += h.m_invMass * l * this.m_J.linearB.x;
        h.m_linearVelocity.y += h.m_invMass * l * this.m_J.linearB.y;
        h.m_angularVelocity += h.m_invI * l * this.m_J.angularB
    };
    V.prototype.SolvePositionConstraints = function () {
        var d = this.m_bodyA,
            h = this.m_bodyB,
            l = 0,
            j = 0;
        l = this.m_revolute1 ?
            this.m_revolute1.GetJointAngle() : this.m_prismatic1.GetJointTranslation();
        j = this.m_revolute2 ? this.m_revolute2.GetJointAngle() : this.m_prismatic2.GetJointTranslation();
        l = -this.m_mass * (this.m_constant - (l + this.m_ratio * j));
        d.m_sweep.c.x += d.m_invMass * l * this.m_J.linearA.x;
        d.m_sweep.c.y += d.m_invMass * l * this.m_J.linearA.y;
        d.m_sweep.a += d.m_invI * l * this.m_J.angularA;
        h.m_sweep.c.x += h.m_invMass * l * this.m_J.linearB.x;
        h.m_sweep.c.y += h.m_invMass * l * this.m_J.linearB.y;
        h.m_sweep.a += h.m_invI * l * this.m_J.angularB;
        d.SynchronizeTransform();
        h.SynchronizeTransform();
        return 0 < F.b2_linearSlop
    };
    Box2D.inherit(M, Box2D.Dynamics.Joints.b2JointDef);
    M.prototype.__super = Box2D.Dynamics.Joints.b2JointDef.prototype;
    M.b2GearJointDef = function () {
        Box2D.Dynamics.Joints.b2JointDef.b2JointDef.apply(this, arguments)
    };
    M.prototype.b2GearJointDef = function () {
        this.__super.b2JointDef.call(this);
        this.type = I.e_gearJoint;
        this.joint2 = this.joint1 = null;
        this.ratio = 1
    };
    L.b2Jacobian = function () {
        this.linearA = new w;
        this.linearB = new w
    };
    L.prototype.SetZero = function () {
        this.linearA.SetZero();
        this.angularA = 0;
        this.linearB.SetZero();
        this.angularB = 0
    };
    L.prototype.Set = function (d, h, l, j) {
        if (h === undefined) h = 0;
        if (j === undefined) j = 0;
        this.linearA.SetV(d);
        this.angularA = h;
        this.linearB.SetV(l);
        this.angularB = j
    };
    L.prototype.Compute = function (d, h, l, j) {
        if (h === undefined) h = 0;
        if (j === undefined) j = 0;
        return this.linearA.x * d.x + this.linearA.y * d.y + this.angularA * h + (this.linearB.x * l.x + this.linearB.y * l.y) + this.angularB * j
    };
    I.b2Joint = function () {
        this.m_edgeA = new Y;
        this.m_edgeB = new Y;
        this.m_localCenterA = new w;
        this.m_localCenterB = new w
    };
    I.prototype.GetType = function () {
        return this.m_type
    };
    I.prototype.GetAnchorA = function () {
        return null
    };
    I.prototype.GetAnchorB = function () {
        return null
    };
    I.prototype.GetReactionForce = function () {
        return null
    };
    I.prototype.GetReactionTorque = function () {
        return 0
    };
    I.prototype.GetBodyA = function () {
        return this.m_bodyA
    };
    I.prototype.GetBodyB = function () {
        return this.m_bodyB
    };
    I.prototype.GetNext = function () {
        return this.m_next
    };
    I.prototype.GetUserData = function () {
        return this.m_userData
    };
    I.prototype.SetUserData = function (d) {
        this.m_userData = d
    };
    I.prototype.IsActive = function () {
        return this.m_bodyA.IsActive() && this.m_bodyB.IsActive()
    };
    I.Create = function (d) {
        var h = null;
        switch (d.type) {
        case I.e_distanceJoint:
            h = new U(d instanceof p ? d : null);
            break;
        case I.e_mouseJoint:
            h = new u(d instanceof D ? d : null);
            break;
        case I.e_prismaticJoint:
            h = new H(d instanceof O ? d : null);
            break;
        case I.e_revoluteJoint:
            h = new N(d instanceof S ? d : null);
            break;
        case I.e_pulleyJoint:
            h = new E(d instanceof R ? d : null);
            break;
        case I.e_gearJoint:
            h = new V(d instanceof M ? d : null);
            break;
        case I.e_lineJoint:
            h = new k(d instanceof z ? d : null);
            break;
        case I.e_weldJoint:
            h = new aa(d instanceof Z ? d : null);
            break;
        case I.e_frictionJoint:
            h = new B(d instanceof Q ? d : null)
        }
        return h
    };
    I.Destroy = function () {};
    I.prototype.b2Joint = function (d) {
        F.b2Assert(d.bodyA != d.bodyB);
        this.m_type = d.type;
        this.m_next = this.m_prev = null;
        this.m_bodyA = d.bodyA;
        this.m_bodyB = d.bodyB;
        this.m_collideConnected = d.collideConnected;
        this.m_islandFlag = false;
        this.m_userData = d.userData
    };
    I.prototype.InitVelocityConstraints = function () {};
    I.prototype.SolveVelocityConstraints = function () {};
    I.prototype.FinalizeVelocityConstraints = function () {};
    I.prototype.SolvePositionConstraints = function () {
        return false
    };
    Box2D.postDefs.push(function () {
        Box2D.Dynamics.Joints.b2Joint.e_unknownJoint = 0;
        Box2D.Dynamics.Joints.b2Joint.e_revoluteJoint = 1;
        Box2D.Dynamics.Joints.b2Joint.e_prismaticJoint = 2;
        Box2D.Dynamics.Joints.b2Joint.e_distanceJoint = 3;
        Box2D.Dynamics.Joints.b2Joint.e_pulleyJoint = 4;
        Box2D.Dynamics.Joints.b2Joint.e_mouseJoint = 5;
        Box2D.Dynamics.Joints.b2Joint.e_gearJoint = 6;
        Box2D.Dynamics.Joints.b2Joint.e_lineJoint = 7;
        Box2D.Dynamics.Joints.b2Joint.e_weldJoint = 8;
        Box2D.Dynamics.Joints.b2Joint.e_frictionJoint = 9;
        Box2D.Dynamics.Joints.b2Joint.e_inactiveLimit = 0;
        Box2D.Dynamics.Joints.b2Joint.e_atLowerLimit = 1;
        Box2D.Dynamics.Joints.b2Joint.e_atUpperLimit = 2;
        Box2D.Dynamics.Joints.b2Joint.e_equalLimits = 3
    });
    W.b2JointDef = function () {};
    W.prototype.b2JointDef = function () {
        this.type = I.e_unknownJoint;
        this.bodyB = this.bodyA = this.userData = null;
        this.collideConnected = false
    };
    Y.b2JointEdge = function () {};
    Box2D.inherit(k, Box2D.Dynamics.Joints.b2Joint);
    k.prototype.__super = Box2D.Dynamics.Joints.b2Joint.prototype;
    k.b2LineJoint = function () {
        Box2D.Dynamics.Joints.b2Joint.b2Joint.apply(this, arguments);
        this.m_localAnchor1 = new w;
        this.m_localAnchor2 = new w;
        this.m_localXAxis1 = new w;
        this.m_localYAxis1 = new w;
        this.m_axis = new w;
        this.m_perp = new w;
        this.m_K = new G;
        this.m_impulse = new w
    };
    k.prototype.GetAnchorA = function () {
        return this.m_bodyA.GetWorldPoint(this.m_localAnchor1)
    };
    k.prototype.GetAnchorB = function () {
        return this.m_bodyB.GetWorldPoint(this.m_localAnchor2)
    };
    k.prototype.GetReactionForce = function (d) {
            if (d === undefined) d = 0;
            return new w(d * (this.m_impulse.x * this.m_perp.x + (this.m_motorImpulse + this.m_impulse.y) * this.m_axis.x), d * (this.m_impulse.x * this.m_perp.y + (this.m_motorImpulse + this.m_impulse.y) * this.m_axis.y))
    };
    k.prototype.GetReactionTorque = function (d) {
        if (d === undefined) d = 0;
        return d * this.m_impulse.y
    };
    k.prototype.GetJointTranslation = function () {
        var d = this.m_bodyA,
            h = this.m_bodyB,
            l = d.GetWorldPoint(this.m_localAnchor1),
            j = h.GetWorldPoint(this.m_localAnchor2);
        h = j.x - l.x;
        l = j.y - l.y;
        d = d.GetWorldVector(this.m_localXAxis1);
        return d.x * h + d.y * l
    };
    k.prototype.GetJointSpeed = function () {
        var d = this.m_bodyA,
            h = this.m_bodyB,
            l;
        l = d.m_xf.R;
        var j = this.m_localAnchor1.x - d.m_sweep.localCenter.x,
            o = this.m_localAnchor1.y - d.m_sweep.localCenter.y,
            q = l.col1.x * j + l.col2.x * o;
        o = l.col1.y * j + l.col2.y * o;
        j = q;
        l = h.m_xf.R;
        var n = this.m_localAnchor2.x - h.m_sweep.localCenter.x,
            a = this.m_localAnchor2.y - h.m_sweep.localCenter.y;
        q = l.col1.x * n + l.col2.x * a;
        a = l.col1.y * n + l.col2.y * a;
        n = q;
        l = h.m_sweep.c.x + n - (d.m_sweep.c.x + j);
        q = h.m_sweep.c.y + a - (d.m_sweep.c.y + o);
        var c = d.GetWorldVector(this.m_localXAxis1),
            g = d.m_linearVelocity,
            b = h.m_linearVelocity;
        d = d.m_angularVelocity;
        h = h.m_angularVelocity;
        return l * -d * c.y + q * d * c.x + (c.x * (b.x + -h * a - g.x - -d * o) + c.y * (b.y + h * n - g.y - d * j))
    };
    k.prototype.IsLimitEnabled = function () {
        return this.m_enableLimit
    };
    k.prototype.EnableLimit = function (d) {
        this.m_bodyA.SetAwake(true);
        this.m_bodyB.SetAwake(true);
        this.m_enableLimit = d
    };
    k.prototype.GetLowerLimit = function () {
        return this.m_lowerTranslation
    };
    k.prototype.GetUpperLimit = function () {
        return this.m_upperTranslation
    };
    k.prototype.SetLimits = function (d,
        h) {
        if (d === undefined) d = 0;
        if (h === undefined) h = 0;
        this.m_bodyA.SetAwake(true);
        this.m_bodyB.SetAwake(true);
        this.m_lowerTranslation = d;
        this.m_upperTranslation = h
    };
    k.prototype.IsMotorEnabled = function () {
        return this.m_enableMotor
    };
    k.prototype.EnableMotor = function (d) {
        this.m_bodyA.SetAwake(true);
        this.m_bodyB.SetAwake(true);
        this.m_enableMotor = d
    };
    k.prototype.SetMotorSpeed = function (d) {
        if (d === undefined) d = 0;
        this.m_bodyA.SetAwake(true);
        this.m_bodyB.SetAwake(true);
        this.m_motorSpeed = d
    };
    k.prototype.GetMotorSpeed = function () {
        return this.m_motorSpeed
    };
    k.prototype.SetMaxMotorForce = function (d) {
        if (d === undefined) d = 0;
        this.m_bodyA.SetAwake(true);
        this.m_bodyB.SetAwake(true);
        this.m_maxMotorForce = d
    };
    k.prototype.GetMaxMotorForce = function () {
        return this.m_maxMotorForce
    };
    k.prototype.GetMotorForce = function () {
        return this.m_motorImpulse
    };
    k.prototype.b2LineJoint = function (d) {
        this.__super.b2Joint.call(this, d);
        this.m_localAnchor1.SetV(d.localAnchorA);
        this.m_localAnchor2.SetV(d.localAnchorB);
        this.m_localXAxis1.SetV(d.localAxisA);
        this.m_localYAxis1.x = -this.m_localXAxis1.y;
        this.m_localYAxis1.y = this.m_localXAxis1.x;
        this.m_impulse.SetZero();
        this.m_motorImpulse = this.m_motorMass = 0;
        this.m_lowerTranslation = d.lowerTranslation;
        this.m_upperTranslation = d.upperTranslation;
        this.m_maxMotorForce = d.maxMotorForce;
        this.m_motorSpeed = d.motorSpeed;
        this.m_enableLimit = d.enableLimit;
        this.m_enableMotor = d.enableMotor;
        this.m_limitState = I.e_inactiveLimit;
        this.m_axis.SetZero();
        this.m_perp.SetZero()
    };
    k.prototype.InitVelocityConstraints = function (d) {
        var h = this.m_bodyA,
            l = this.m_bodyB,
            j, o = 0;
        this.m_localCenterA.SetV(h.GetLocalCenter());
        this.m_localCenterB.SetV(l.GetLocalCenter());
        var q = h.GetTransform();
        l.GetTransform();
        j = h.m_xf.R;
        var n = this.m_localAnchor1.x - this.m_localCenterA.x,
            a = this.m_localAnchor1.y - this.m_localCenterA.y;
        o = j.col1.x * n + j.col2.x * a;
        a = j.col1.y * n + j.col2.y * a;
        n = o;
        j = l.m_xf.R;
        var c = this.m_localAnchor2.x - this.m_localCenterB.x,
            g = this.m_localAnchor2.y - this.m_localCenterB.y;
        o = j.col1.x * c + j.col2.x * g;
        g = j.col1.y * c + j.col2.y * g;
        c = o;
        j = l.m_sweep.c.x + c - h.m_sweep.c.x - n;
        o = l.m_sweep.c.y + g - h.m_sweep.c.y - a;
        this.m_invMassA = h.m_invMass;
        this.m_invMassB = l.m_invMass;
        this.m_invIA = h.m_invI;
        this.m_invIB = l.m_invI;
        this.m_axis.SetV(y.MulMV(q.R, this.m_localXAxis1));
        this.m_a1 = (j + n) * this.m_axis.y - (o + a) * this.m_axis.x;
        this.m_a2 = c * this.m_axis.y - g * this.m_axis.x;
        this.m_motorMass = this.m_invMassA + this.m_invMassB + this.m_invIA * this.m_a1 * this.m_a1 + this.m_invIB * this.m_a2 * this.m_a2;
        this.m_motorMass = this.m_motorMass > Number.MIN_VALUE ? 1 / this.m_motorMass : 0;
        this.m_perp.SetV(y.MulMV(q.R, this.m_localYAxis1));
        this.m_s1 = (j + n) * this.m_perp.y - (o + a) * this.m_perp.x;
        this.m_s2 = c * this.m_perp.y - g * this.m_perp.x;
        q = this.m_invMassA;
        n = this.m_invMassB;
        a = this.m_invIA;
        c = this.m_invIB;
        this.m_K.col1.x = q + n + a * this.m_s1 * this.m_s1 + c * this.m_s2 * this.m_s2;
        this.m_K.col1.y = a * this.m_s1 * this.m_a1 + c * this.m_s2 * this.m_a2;
        this.m_K.col2.x = this.m_K.col1.y;
        this.m_K.col2.y = q + n + a * this.m_a1 * this.m_a1 + c * this.m_a2 * this.m_a2;
        if (this.m_enableLimit) {
            j = this.m_axis.x * j + this.m_axis.y * o;
            if (y.Abs(this.m_upperTranslation - this.m_lowerTranslation) < 2 * F.b2_linearSlop) this.m_limitState = I.e_equalLimits;
            else if (j <= this.m_lowerTranslation) {
                if (this.m_limitState != I.e_atLowerLimit) {
                    this.m_limitState = I.e_atLowerLimit;
                    this.m_impulse.y = 0
                }
            } else if (j >= this.m_upperTranslation) {
                if (this.m_limitState != I.e_atUpperLimit) {
                    this.m_limitState = I.e_atUpperLimit;
                    this.m_impulse.y = 0
                }
            } else {
                this.m_limitState = I.e_inactiveLimit;
                this.m_impulse.y = 0
            }
        } else this.m_limitState = I.e_inactiveLimit; if (this.m_enableMotor == false) this.m_motorImpulse = 0;
        if (d.warmStarting) {
            this.m_impulse.x *= d.dtRatio;
            this.m_impulse.y *= d.dtRatio;
            this.m_motorImpulse *= d.dtRatio;
            d = this.m_impulse.x * this.m_perp.x + (this.m_motorImpulse + this.m_impulse.y) * this.m_axis.x;
            j = this.m_impulse.x * this.m_perp.y + (this.m_motorImpulse + this.m_impulse.y) * this.m_axis.y;
            o = this.m_impulse.x * this.m_s1 + (this.m_motorImpulse + this.m_impulse.y) * this.m_a1;
            q = this.m_impulse.x * this.m_s2 + (this.m_motorImpulse + this.m_impulse.y) * this.m_a2;
            h.m_linearVelocity.x -= this.m_invMassA * d;
            h.m_linearVelocity.y -= this.m_invMassA * j;
            h.m_angularVelocity -= this.m_invIA * o;
            l.m_linearVelocity.x += this.m_invMassB * d;
            l.m_linearVelocity.y += this.m_invMassB * j;
            l.m_angularVelocity += this.m_invIB * q
        } else {
            this.m_impulse.SetZero();
            this.m_motorImpulse = 0
        }
    };
    k.prototype.SolveVelocityConstraints = function (d) {
        var h = this.m_bodyA,
            l = this.m_bodyB,
            j = h.m_linearVelocity,
            o = h.m_angularVelocity,
            q = l.m_linearVelocity,
            n = l.m_angularVelocity,
            a = 0,
            c = 0,
            g = 0,
            b = 0;
        if (this.m_enableMotor && this.m_limitState != I.e_equalLimits) {
            b = this.m_motorMass * (this.m_motorSpeed - (this.m_axis.x * (q.x - j.x) + this.m_axis.y * (q.y - j.y) + this.m_a2 * n - this.m_a1 * o));
            a = this.m_motorImpulse;
            c = d.dt * this.m_maxMotorForce;
            this.m_motorImpulse = y.Clamp(this.m_motorImpulse + b, -c, c);
            b = this.m_motorImpulse - a;
            a = b * this.m_axis.x;
            c = b * this.m_axis.y;
            g = b * this.m_a1;
            b = b * this.m_a2;
            j.x -= this.m_invMassA * a;
            j.y -= this.m_invMassA * c;
            o -= this.m_invIA * g;
            q.x += this.m_invMassB * a;
            q.y += this.m_invMassB * c;
            n += this.m_invIB * b
        }
        c = this.m_perp.x * (q.x - j.x) + this.m_perp.y * (q.y - j.y) + this.m_s2 * n - this.m_s1 * o;
        if (this.m_enableLimit && this.m_limitState != I.e_inactiveLimit) {
            g = this.m_axis.x * (q.x - j.x) + this.m_axis.y * (q.y - j.y) + this.m_a2 * n - this.m_a1 * o;
            a = this.m_impulse.Copy();
            d = this.m_K.Solve(new w, -c, -g);
            this.m_impulse.Add(d);
            if (this.m_limitState == I.e_atLowerLimit) this.m_impulse.y = y.Max(this.m_impulse.y, 0);
            else if (this.m_limitState == I.e_atUpperLimit) this.m_impulse.y = y.Min(this.m_impulse.y, 0);
            c = -c - (this.m_impulse.y - a.y) * this.m_K.col2.x;
            g = 0;
            g = this.m_K.col1.x != 0 ? c / this.m_K.col1.x + a.x : a.x;
            this.m_impulse.x = g;
            d.x = this.m_impulse.x - a.x;
            d.y = this.m_impulse.y - a.y;
            a = d.x * this.m_perp.x + d.y * this.m_axis.x;
            c = d.x * this.m_perp.y + d.y * this.m_axis.y;
            g = d.x * this.m_s1 + d.y * this.m_a1;
            b = d.x * this.m_s2 + d.y * this.m_a2
        } else {
            d = 0;
            d = this.m_K.col1.x != 0 ? -c / this.m_K.col1.x : 0;
            this.m_impulse.x += d;
            a = d * this.m_perp.x;
            c = d * this.m_perp.y;
            g = d * this.m_s1;
            b = d * this.m_s2
        }
        j.x -= this.m_invMassA * a;
        j.y -= this.m_invMassA * c;
        o -= this.m_invIA * g;
        q.x += this.m_invMassB * a;
        q.y += this.m_invMassB * c;
        n += this.m_invIB * b;
        h.m_linearVelocity.SetV(j);
        h.m_angularVelocity = o;
        l.m_linearVelocity.SetV(q);
        l.m_angularVelocity = n
    };
    k.prototype.SolvePositionConstraints = function () {
        var d = this.m_bodyA,
            h = this.m_bodyB,
            l = d.m_sweep.c,
            j = d.m_sweep.a,
            o = h.m_sweep.c,
            q = h.m_sweep.a,
            n, a = 0,
            c = 0,
            g = 0,
            b = 0,
            e = n = 0,
            f = 0;
        c = false;
        var m = 0,
            r = G.FromAngle(j);
        g = G.FromAngle(q);
        n = r;
        f = this.m_localAnchor1.x - this.m_localCenterA.x;
        var s = this.m_localAnchor1.y - this.m_localCenterA.y;
        a = n.col1.x * f + n.col2.x * s;
        s = n.col1.y * f + n.col2.y * s;
        f = a;
        n = g;
        g = this.m_localAnchor2.x - this.m_localCenterB.x;
        b = this.m_localAnchor2.y - this.m_localCenterB.y;
        a = n.col1.x * g + n.col2.x * b;
        b = n.col1.y * g + n.col2.y * b;
        g = a;
        n = o.x + g - l.x - f;
        a = o.y + b - l.y - s;
        if (this.m_enableLimit) {
            this.m_axis = y.MulMV(r, this.m_localXAxis1);
            this.m_a1 = (n + f) * this.m_axis.y - (a + s) * this.m_axis.x;
            this.m_a2 = g * this.m_axis.y - b * this.m_axis.x;
            var v = this.m_axis.x * n + this.m_axis.y * a;
            if (y.Abs(this.m_upperTranslation - this.m_lowerTranslation) < 2 * F.b2_linearSlop) {
                m = y.Clamp(v, -F.b2_maxLinearCorrection, F.b2_maxLinearCorrection);
                e = y.Abs(v);
                c = true
            } else if (v <= this.m_lowerTranslation) {
                m = y.Clamp(v - this.m_lowerTranslation + F.b2_linearSlop, -F.b2_maxLinearCorrection, 0);
                e = this.m_lowerTranslation - v;
                c = true
            } else if (v >= this.m_upperTranslation) {
                m = y.Clamp(v - this.m_upperTranslation + F.b2_linearSlop, 0, F.b2_maxLinearCorrection);
                e = v - this.m_upperTranslation;
                c = true
            }
        }
        this.m_perp = y.MulMV(r, this.m_localYAxis1);
        this.m_s1 = (n + f) * this.m_perp.y - (a + s) * this.m_perp.x;
        this.m_s2 = g * this.m_perp.y - b * this.m_perp.x;
        r = new w;
        s = this.m_perp.x * n + this.m_perp.y * a;
        e = y.Max(e, y.Abs(s));
        f = 0;
        if (c) {
            c = this.m_invMassA;
            g = this.m_invMassB;
            b = this.m_invIA;
            n = this.m_invIB;
            this.m_K.col1.x = c + g + b * this.m_s1 * this.m_s1 + n * this.m_s2 * this.m_s2;
            this.m_K.col1.y = b * this.m_s1 * this.m_a1 + n * this.m_s2 * this.m_a2;
            this.m_K.col2.x = this.m_K.col1.y;
            this.m_K.col2.y = c + g + b * this.m_a1 * this.m_a1 + n * this.m_a2 * this.m_a2;
            this.m_K.Solve(r, -s, -m)
        } else {
            c = this.m_invMassA;
            g = this.m_invMassB;
            b = this.m_invIA;
            n = this.m_invIB;
            m = c + g + b * this.m_s1 * this.m_s1 + n * this.m_s2 * this.m_s2;
            c = 0;
            c = m != 0 ? -s / m : 0;
            r.x = c;
            r.y = 0
        }
        m = r.x * this.m_perp.x + r.y * this.m_axis.x;
        c = r.x * this.m_perp.y + r.y * this.m_axis.y;
        s = r.x * this.m_s1 + r.y * this.m_a1;
        r = r.x * this.m_s2 + r.y * this.m_a2;
        l.x -= this.m_invMassA * m;
        l.y -= this.m_invMassA * c;
        j -= this.m_invIA * s;
        o.x += this.m_invMassB * m;
        o.y += this.m_invMassB * c;
        q += this.m_invIB * r;
        d.m_sweep.a = j;
        h.m_sweep.a = q;
        d.SynchronizeTransform();
        h.SynchronizeTransform();
        return e <= F.b2_linearSlop && f <= F.b2_angularSlop
    };
    Box2D.inherit(z, Box2D.Dynamics.Joints.b2JointDef);
    z.prototype.__super = Box2D.Dynamics.Joints.b2JointDef.prototype;
    z.b2LineJointDef = function () {
        Box2D.Dynamics.Joints.b2JointDef.b2JointDef.apply(this, arguments);
        this.localAnchorA = new w;
        this.localAnchorB = new w;
        this.localAxisA = new w
    };
    z.prototype.b2LineJointDef = function () {
        this.__super.b2JointDef.call(this);
        this.type = I.e_lineJoint;
        this.localAxisA.Set(1, 0);
        this.enableLimit = false;
        this.upperTranslation = this.lowerTranslation = 0;
        this.enableMotor = false;
        this.motorSpeed = this.maxMotorForce = 0
    };
    z.prototype.Initialize = function (d, h, l, j) {
        this.bodyA = d;
        this.bodyB = h;
        this.localAnchorA = this.bodyA.GetLocalPoint(l);
        this.localAnchorB = this.bodyB.GetLocalPoint(l);
        this.localAxisA = this.bodyA.GetLocalVector(j)
    };
    Box2D.inherit(u, Box2D.Dynamics.Joints.b2Joint);
    u.prototype.__super = Box2D.Dynamics.Joints.b2Joint.prototype;
    u.b2MouseJoint = function () {
        Box2D.Dynamics.Joints.b2Joint.b2Joint.apply(this, arguments);
        this.K = new G;
        this.K1 = new G;
        this.K2 = new G;
        this.m_localAnchor = new w;
        this.m_target = new w;
        this.m_impulse = new w;
        this.m_mass = new G;
        this.m_C = new w
    };
    u.prototype.GetAnchorA = function () {
        return this.m_target
    };
    u.prototype.GetAnchorB = function () {
        return this.m_bodyB.GetWorldPoint(this.m_localAnchor)
    };
    u.prototype.GetReactionForce = function (d) {
        if (d === undefined) d = 0;
        return new w(d * this.m_impulse.x, d * this.m_impulse.y)
    };
    u.prototype.GetReactionTorque = function () {
        return 0
    };
    u.prototype.GetTarget = function () {
        return this.m_target
    };
    u.prototype.SetTarget = function (d) {
        this.m_bodyB.IsAwake() == false && this.m_bodyB.SetAwake(true);
        this.m_target = d
    };
    u.prototype.GetMaxForce = function () {
        return this.m_maxForce
    };
    u.prototype.SetMaxForce = function (d) {
        if (d === undefined) d = 0;
        this.m_maxForce = d
    };
    u.prototype.GetFrequency = function () {
        return this.m_frequencyHz
    };
    u.prototype.SetFrequency = function (d) {
        if (d === undefined) d = 0;
        this.m_frequencyHz = d
    };
    u.prototype.GetDampingRatio = function () {
        return this.m_dampingRatio
    };
    u.prototype.SetDampingRatio = function (d) {
        if (d === undefined) d = 0;
        this.m_dampingRatio = d
    };
    u.prototype.b2MouseJoint = function (d) {
            this.__super.b2Joint.call(this, d);
            this.m_target.SetV(d.target);
            var h = this.m_target.x - this.m_bodyB.m_xf.position.x,
                l = this.m_target.y - this.m_bodyB.m_xf.position.y,
                j = this.m_bodyB.m_xf.R;
            this.m_localAnchor.x = h * j.col1.x + l * j.col1.y;
            this.m_localAnchor.y = h * j.col2.x + l * j.col2.y;
            this.m_maxForce = d.maxForce;
            this.m_impulse.SetZero();
            this.m_frequencyHz = d.frequencyHz;
            this.m_dampingRatio = d.dampingRatio;
            this.m_gamma = this.m_beta = 0
    };
    u.prototype.InitVelocityConstraints = function (d) {
        var h = this.m_bodyB,
            l = h.GetMass(),
            j = 2 * Math.PI * this.m_frequencyHz,
            o = l * j * j;
        this.m_gamma = d.dt * (2 * l * this.m_dampingRatio * j + d.dt * o);
        this.m_gamma = this.m_gamma != 0 ? 1 / this.m_gamma : 0;
        this.m_beta = d.dt * o * this.m_gamma;
        o = h.m_xf.R;
        l = this.m_localAnchor.x - h.m_sweep.localCenter.x;
        j = this.m_localAnchor.y - h.m_sweep.localCenter.y;
        var q = o.col1.x * l + o.col2.x * j;
        j = o.col1.y * l + o.col2.y * j;
        l = q;
        o = h.m_invMass;
        q = h.m_invI;
        this.K1.col1.x = o;
        this.K1.col2.x = 0;
        this.K1.col1.y = 0;
        this.K1.col2.y = o;
        this.K2.col1.x = q * j * j;
        this.K2.col2.x = -q * l * j;
        this.K2.col1.y = -q * l * j;
        this.K2.col2.y = q * l * l;
        this.K.SetM(this.K1);
        this.K.AddM(this.K2);
        this.K.col1.x += this.m_gamma;
        this.K.col2.y += this.m_gamma;
        this.K.GetInverse(this.m_mass);
        this.m_C.x = h.m_sweep.c.x + l - this.m_target.x;
        this.m_C.y = h.m_sweep.c.y + j - this.m_target.y;
        h.m_angularVelocity *= 0.98;
        this.m_impulse.x *= d.dtRatio;
        this.m_impulse.y *= d.dtRatio;
        h.m_linearVelocity.x += o * this.m_impulse.x;
        h.m_linearVelocity.y += o * this.m_impulse.y;
        h.m_angularVelocity += q * (l * this.m_impulse.y - j * this.m_impulse.x)
    };
    u.prototype.SolveVelocityConstraints = function (d) {
        var h = this.m_bodyB,
            l, j = 0,
            o = 0;
        l = h.m_xf.R;
        var q = this.m_localAnchor.x - h.m_sweep.localCenter.x,
            n = this.m_localAnchor.y - h.m_sweep.localCenter.y;
        j = l.col1.x * q + l.col2.x * n;
        n = l.col1.y * q + l.col2.y * n;
        q = j;
        j = h.m_linearVelocity.x + -h.m_angularVelocity * n;
        var a = h.m_linearVelocity.y + h.m_angularVelocity * q;
        l = this.m_mass;
        j = j + this.m_beta * this.m_C.x + this.m_gamma * this.m_impulse.x;
        o = a + this.m_beta * this.m_C.y + this.m_gamma * this.m_impulse.y;
        a = -(l.col1.x * j + l.col2.x * o);
        o = -(l.col1.y * j + l.col2.y * o);
        l = this.m_impulse.x;
        j = this.m_impulse.y;
        this.m_impulse.x += a;
        this.m_impulse.y += o;
        d = d.dt * this.m_maxForce;
        this.m_impulse.LengthSquared() > d * d && this.m_impulse.Multiply(d / this.m_impulse.Length());
        a = this.m_impulse.x - l;
        o = this.m_impulse.y - j;
        h.m_linearVelocity.x += h.m_invMass * a;
        h.m_linearVelocity.y += h.m_invMass * o;
        h.m_angularVelocity += h.m_invI * (q * o - n * a)
    };
    u.prototype.SolvePositionConstraints = function () {
        return true
    };
    Box2D.inherit(D, Box2D.Dynamics.Joints.b2JointDef);
    D.prototype.__super = Box2D.Dynamics.Joints.b2JointDef.prototype;
    D.b2MouseJointDef = function () {
        Box2D.Dynamics.Joints.b2JointDef.b2JointDef.apply(this,
            arguments);
        this.target = new w
    };
    D.prototype.b2MouseJointDef = function () {
        this.__super.b2JointDef.call(this);
        this.type = I.e_mouseJoint;
        this.maxForce = 0;
        this.frequencyHz = 5;
        this.dampingRatio = 0.7
    };
    Box2D.inherit(H, Box2D.Dynamics.Joints.b2Joint);
    H.prototype.__super = Box2D.Dynamics.Joints.b2Joint.prototype;
    H.b2PrismaticJoint = function () {
        Box2D.Dynamics.Joints.b2Joint.b2Joint.apply(this, arguments);
        this.m_localAnchor1 = new w;
        this.m_localAnchor2 = new w;
        this.m_localXAxis1 = new w;
        this.m_localYAxis1 = new w;
        this.m_axis = new w;
        this.m_perp = new w;
        this.m_K = new K;
        this.m_impulse = new A
    };
    H.prototype.GetAnchorA = function () {
        return this.m_bodyA.GetWorldPoint(this.m_localAnchor1)
    };
    H.prototype.GetAnchorB = function () {
        return this.m_bodyB.GetWorldPoint(this.m_localAnchor2)
    };
    H.prototype.GetReactionForce = function (d) {
        if (d === undefined) d = 0;
        return new w(d * (this.m_impulse.x * this.m_perp.x + (this.m_motorImpulse + this.m_impulse.z) * this.m_axis.x), d * (this.m_impulse.x * this.m_perp.y + (this.m_motorImpulse + this.m_impulse.z) * this.m_axis.y))
    };
    H.prototype.GetReactionTorque = function (d) {
            if (d === undefined) d = 0;
            return d * this.m_impulse.y
    };
    H.prototype.GetJointTranslation = function () {
        var d = this.m_bodyA,
            h = this.m_bodyB,
            l = d.GetWorldPoint(this.m_localAnchor1),
            j = h.GetWorldPoint(this.m_localAnchor2);
        h = j.x - l.x;
        l = j.y - l.y;
        d = d.GetWorldVector(this.m_localXAxis1);
        return d.x * h + d.y * l
    };
    H.prototype.GetJointSpeed = function () {
        var d = this.m_bodyA,
            h = this.m_bodyB,
            l;
        l = d.m_xf.R;
        var j = this.m_localAnchor1.x - d.m_sweep.localCenter.x,
            o = this.m_localAnchor1.y - d.m_sweep.localCenter.y,
            q = l.col1.x * j + l.col2.x * o;
        o = l.col1.y * j + l.col2.y * o;
        j = q;
        l = h.m_xf.R;
        var n = this.m_localAnchor2.x - h.m_sweep.localCenter.x,
            a = this.m_localAnchor2.y - h.m_sweep.localCenter.y;
        q = l.col1.x * n + l.col2.x * a;
        a = l.col1.y * n + l.col2.y * a;
        n = q;
        l = h.m_sweep.c.x + n - (d.m_sweep.c.x + j);
        q = h.m_sweep.c.y + a - (d.m_sweep.c.y + o);
        var c = d.GetWorldVector(this.m_localXAxis1),
            g = d.m_linearVelocity,
            b = h.m_linearVelocity;
        d = d.m_angularVelocity;
        h = h.m_angularVelocity;
        return l * -d * c.y + q * d * c.x + (c.x * (b.x + -h * a - g.x - -d * o) + c.y * (b.y + h * n - g.y - d * j))
    };
    H.prototype.IsLimitEnabled = function () {
        return this.m_enableLimit
    };
    H.prototype.EnableLimit = function (d) {
        this.m_bodyA.SetAwake(true);
        this.m_bodyB.SetAwake(true);
        this.m_enableLimit = d
    };
    H.prototype.GetLowerLimit = function () {
        return this.m_lowerTranslation
    };
    H.prototype.GetUpperLimit = function () {
        return this.m_upperTranslation
    };
    H.prototype.SetLimits = function (d, h) {
        if (d === undefined) d = 0;
        if (h === undefined) h = 0;
        this.m_bodyA.SetAwake(true);
        this.m_bodyB.SetAwake(true);
        this.m_lowerTranslation = d;
        this.m_upperTranslation = h
    };
    H.prototype.IsMotorEnabled = function () {
        return this.m_enableMotor
    };
    H.prototype.EnableMotor = function (d) {
        this.m_bodyA.SetAwake(true);
        this.m_bodyB.SetAwake(true);
        this.m_enableMotor = d
    };
    H.prototype.SetMotorSpeed = function (d) {
        if (d === undefined) d = 0;
        this.m_bodyA.SetAwake(true);
        this.m_bodyB.SetAwake(true);
        this.m_motorSpeed = d
    };
    H.prototype.GetMotorSpeed = function () {
        return this.m_motorSpeed
    };
    H.prototype.SetMaxMotorForce = function (d) {
        if (d === undefined) d = 0;
        this.m_bodyA.SetAwake(true);
        this.m_bodyB.SetAwake(true);
        this.m_maxMotorForce = d
    };
    H.prototype.GetMotorForce = function () {
        return this.m_motorImpulse
    };
    H.prototype.b2PrismaticJoint = function (d) {
        this.__super.b2Joint.call(this, d);
        this.m_localAnchor1.SetV(d.localAnchorA);
        this.m_localAnchor2.SetV(d.localAnchorB);
        this.m_localXAxis1.SetV(d.localAxisA);
        this.m_localYAxis1.x = -this.m_localXAxis1.y;
        this.m_localYAxis1.y = this.m_localXAxis1.x;
        this.m_refAngle = d.referenceAngle;
        this.m_impulse.SetZero();
        this.m_motorImpulse = this.m_motorMass = 0;
        this.m_lowerTranslation = d.lowerTranslation;
        this.m_upperTranslation = d.upperTranslation;
        this.m_maxMotorForce = d.maxMotorForce;
        this.m_motorSpeed = d.motorSpeed;
        this.m_enableLimit = d.enableLimit;
        this.m_enableMotor = d.enableMotor;
        this.m_limitState = I.e_inactiveLimit;
        this.m_axis.SetZero();
        this.m_perp.SetZero()
    };
    H.prototype.InitVelocityConstraints = function (d) {
        var h = this.m_bodyA,
            l = this.m_bodyB,
            j, o = 0;
        this.m_localCenterA.SetV(h.GetLocalCenter());
        this.m_localCenterB.SetV(l.GetLocalCenter());
        var q = h.GetTransform();
        l.GetTransform();
        j = h.m_xf.R;
        var n = this.m_localAnchor1.x - this.m_localCenterA.x,
            a = this.m_localAnchor1.y - this.m_localCenterA.y;
        o = j.col1.x * n + j.col2.x * a;
        a = j.col1.y * n + j.col2.y * a;
        n = o;
        j = l.m_xf.R;
        var c = this.m_localAnchor2.x - this.m_localCenterB.x,
            g = this.m_localAnchor2.y - this.m_localCenterB.y;
        o = j.col1.x * c + j.col2.x * g;
        g = j.col1.y * c + j.col2.y * g;
        c = o;
        j = l.m_sweep.c.x + c - h.m_sweep.c.x - n;
        o = l.m_sweep.c.y + g - h.m_sweep.c.y - a;
        this.m_invMassA = h.m_invMass;
        this.m_invMassB = l.m_invMass;
        this.m_invIA = h.m_invI;
        this.m_invIB = l.m_invI;
        this.m_axis.SetV(y.MulMV(q.R, this.m_localXAxis1));
        this.m_a1 = (j + n) * this.m_axis.y - (o + a) * this.m_axis.x;
        this.m_a2 = c * this.m_axis.y - g * this.m_axis.x;
        this.m_motorMass = this.m_invMassA + this.m_invMassB + this.m_invIA * this.m_a1 * this.m_a1 + this.m_invIB * this.m_a2 * this.m_a2;
        if (this.m_motorMass > Number.MIN_VALUE) this.m_motorMass = 1 / this.m_motorMass;
        this.m_perp.SetV(y.MulMV(q.R, this.m_localYAxis1));
        this.m_s1 = (j + n) * this.m_perp.y - (o + a) * this.m_perp.x;
        this.m_s2 = c * this.m_perp.y - g * this.m_perp.x;
        q = this.m_invMassA;
        n = this.m_invMassB;
        a = this.m_invIA;
        c = this.m_invIB;
        this.m_K.col1.x = q + n + a * this.m_s1 * this.m_s1 + c * this.m_s2 * this.m_s2;
        this.m_K.col1.y = a * this.m_s1 + c * this.m_s2;
        this.m_K.col1.z = a * this.m_s1 * this.m_a1 + c * this.m_s2 * this.m_a2;
        this.m_K.col2.x = this.m_K.col1.y;
        this.m_K.col2.y = a + c;
        this.m_K.col2.z = a * this.m_a1 + c * this.m_a2;
        this.m_K.col3.x = this.m_K.col1.z;
        this.m_K.col3.y = this.m_K.col2.z;
        this.m_K.col3.z = q + n + a * this.m_a1 * this.m_a1 + c * this.m_a2 * this.m_a2;
        if (this.m_enableLimit) {
            j = this.m_axis.x * j + this.m_axis.y * o;
            if (y.Abs(this.m_upperTranslation - this.m_lowerTranslation) < 2 * F.b2_linearSlop) this.m_limitState = I.e_equalLimits;
            else if (j <= this.m_lowerTranslation) {
                if (this.m_limitState != I.e_atLowerLimit) {
                    this.m_limitState = I.e_atLowerLimit;
                    this.m_impulse.z = 0
                }
            } else if (j >= this.m_upperTranslation) {
                if (this.m_limitState != I.e_atUpperLimit) {
                    this.m_limitState = I.e_atUpperLimit;
                    this.m_impulse.z = 0
                }
            } else {
                this.m_limitState = I.e_inactiveLimit;
                this.m_impulse.z = 0
            }
        } else this.m_limitState = I.e_inactiveLimit; if (this.m_enableMotor == false) this.m_motorImpulse = 0;
        if (d.warmStarting) {
            this.m_impulse.x *= d.dtRatio;
            this.m_impulse.y *= d.dtRatio;
            this.m_motorImpulse *= d.dtRatio;
            d = this.m_impulse.x * this.m_perp.x + (this.m_motorImpulse + this.m_impulse.z) * this.m_axis.x;
            j = this.m_impulse.x * this.m_perp.y + (this.m_motorImpulse + this.m_impulse.z) * this.m_axis.y;
            o = this.m_impulse.x * this.m_s1 + this.m_impulse.y + (this.m_motorImpulse + this.m_impulse.z) * this.m_a1;
            q = this.m_impulse.x * this.m_s2 + this.m_impulse.y + (this.m_motorImpulse + this.m_impulse.z) * this.m_a2;
            h.m_linearVelocity.x -= this.m_invMassA * d;
            h.m_linearVelocity.y -= this.m_invMassA * j;
            h.m_angularVelocity -= this.m_invIA * o;
            l.m_linearVelocity.x += this.m_invMassB * d;
            l.m_linearVelocity.y += this.m_invMassB * j;
            l.m_angularVelocity += this.m_invIB * q
        } else {
            this.m_impulse.SetZero();
            this.m_motorImpulse = 0
        }
    };
    H.prototype.SolveVelocityConstraints = function (d) {
        var h = this.m_bodyA,
            l = this.m_bodyB,
            j = h.m_linearVelocity,
            o = h.m_angularVelocity,
            q = l.m_linearVelocity,
            n = l.m_angularVelocity,
            a = 0,
            c = 0,
            g = 0,
            b = 0;
        if (this.m_enableMotor && this.m_limitState != I.e_equalLimits) {
            b = this.m_motorMass * (this.m_motorSpeed - (this.m_axis.x * (q.x - j.x) + this.m_axis.y * (q.y - j.y) + this.m_a2 * n - this.m_a1 * o));
            a = this.m_motorImpulse;
            d = d.dt * this.m_maxMotorForce;
            this.m_motorImpulse = y.Clamp(this.m_motorImpulse + b, -d, d);
            b = this.m_motorImpulse - a;
            a = b * this.m_axis.x;
            c = b * this.m_axis.y;
            g = b * this.m_a1;
            b = b * this.m_a2;
            j.x -= this.m_invMassA * a;
            j.y -= this.m_invMassA * c;
            o -= this.m_invIA * g;
            q.x += this.m_invMassB * a;
            q.y += this.m_invMassB * c;
            n += this.m_invIB * b
        }
        g = this.m_perp.x * (q.x - j.x) + this.m_perp.y * (q.y - j.y) + this.m_s2 * n - this.m_s1 * o;
        c = n - o;
        if (this.m_enableLimit && this.m_limitState != I.e_inactiveLimit) {
            d = this.m_axis.x * (q.x - j.x) + this.m_axis.y * (q.y - j.y) + this.m_a2 * n - this.m_a1 * o;
            a = this.m_impulse.Copy();
            d = this.m_K.Solve33(new A, -g, -c, -d);
            this.m_impulse.Add(d);
            if (this.m_limitState == I.e_atLowerLimit) this.m_impulse.z = y.Max(this.m_impulse.z, 0);
            else if (this.m_limitState == I.e_atUpperLimit) this.m_impulse.z = y.Min(this.m_impulse.z, 0);
            g = -g - (this.m_impulse.z - a.z) * this.m_K.col3.x;
            c = -c - (this.m_impulse.z - a.z) * this.m_K.col3.y;
            c = this.m_K.Solve22(new w, g, c);
            c.x += a.x;
            c.y += a.y;
            this.m_impulse.x = c.x;
            this.m_impulse.y = c.y;
            d.x = this.m_impulse.x - a.x;
            d.y = this.m_impulse.y - a.y;
            d.z = this.m_impulse.z - a.z;
            a = d.x * this.m_perp.x + d.z * this.m_axis.x;
            c = d.x * this.m_perp.y + d.z * this.m_axis.y;
            g = d.x * this.m_s1 + d.y + d.z * this.m_a1;
            b = d.x * this.m_s2 + d.y + d.z * this.m_a2
        } else {
            d = this.m_K.Solve22(new w, -g, -c);
            this.m_impulse.x += d.x;
            this.m_impulse.y += d.y;
            a = d.x * this.m_perp.x;
            c = d.x * this.m_perp.y;
            g = d.x * this.m_s1 + d.y;
            b = d.x * this.m_s2 + d.y
        }
        j.x -= this.m_invMassA * a;
        j.y -= this.m_invMassA * c;
        o -= this.m_invIA * g;
        q.x += this.m_invMassB * a;
        q.y += this.m_invMassB * c;
        n += this.m_invIB * b;
        h.m_linearVelocity.SetV(j);
        h.m_angularVelocity = o;
        l.m_linearVelocity.SetV(q);
        l.m_angularVelocity = n
    };
    H.prototype.SolvePositionConstraints = function () {
        var d = this.m_bodyA,
            h = this.m_bodyB,
            l = d.m_sweep.c,
            j = d.m_sweep.a,
            o = h.m_sweep.c,
            q = h.m_sweep.a,
            n, a = 0,
            c = 0,
            g = 0,
            b = a = n = 0,
            e = 0;
        c = false;
        var f = 0,
            m = G.FromAngle(j),
            r = G.FromAngle(q);
        n = m;
        e = this.m_localAnchor1.x - this.m_localCenterA.x;
        var s = this.m_localAnchor1.y - this.m_localCenterA.y;
        a = n.col1.x * e + n.col2.x * s;
        s = n.col1.y * e + n.col2.y * s;
        e = a;
        n = r;
        r = this.m_localAnchor2.x - this.m_localCenterB.x;
        g = this.m_localAnchor2.y - this.m_localCenterB.y;
        a = n.col1.x * r + n.col2.x * g;
        g = n.col1.y * r + n.col2.y * g;
        r = a;
        n = o.x + r - l.x - e;
        a = o.y + g - l.y - s;
        if (this.m_enableLimit) {
            this.m_axis = y.MulMV(m, this.m_localXAxis1);
            this.m_a1 = (n + e) * this.m_axis.y - (a + s) * this.m_axis.x;
            this.m_a2 = r * this.m_axis.y - g * this.m_axis.x;
            var v = this.m_axis.x * n + this.m_axis.y * a;
            if (y.Abs(this.m_upperTranslation - this.m_lowerTranslation) < 2 * F.b2_linearSlop) {
                f = y.Clamp(v, -F.b2_maxLinearCorrection, F.b2_maxLinearCorrection);
                b = y.Abs(v);
                c = true
            } else if (v <= this.m_lowerTranslation) {
                f = y.Clamp(v - this.m_lowerTranslation + F.b2_linearSlop, -F.b2_maxLinearCorrection, 0);
                b = this.m_lowerTranslation - v;
                c = true
            } else if (v >= this.m_upperTranslation) {
                f = y.Clamp(v - this.m_upperTranslation + F.b2_linearSlop, 0, F.b2_maxLinearCorrection);
                b = v - this.m_upperTranslation;
                c = true
            }
        }
        this.m_perp = y.MulMV(m, this.m_localYAxis1);
        this.m_s1 = (n + e) * this.m_perp.y - (a + s) * this.m_perp.x;
        this.m_s2 = r * this.m_perp.y - g * this.m_perp.x;
        m = new A;
        s = this.m_perp.x * n + this.m_perp.y * a;
        r = q - j - this.m_refAngle;
        b = y.Max(b, y.Abs(s));
        e = y.Abs(r);
        if (c) {
            c = this.m_invMassA;
            g = this.m_invMassB;
            n = this.m_invIA;
            a = this.m_invIB;
            this.m_K.col1.x = c + g + n * this.m_s1 * this.m_s1 + a * this.m_s2 * this.m_s2;
            this.m_K.col1.y = n * this.m_s1 + a * this.m_s2;
            this.m_K.col1.z = n * this.m_s1 * this.m_a1 + a * this.m_s2 * this.m_a2;
            this.m_K.col2.x = this.m_K.col1.y;
            this.m_K.col2.y = n + a;
            this.m_K.col2.z = n * this.m_a1 + a * this.m_a2;
            this.m_K.col3.x = this.m_K.col1.z;
            this.m_K.col3.y = this.m_K.col2.z;
            this.m_K.col3.z = c + g + n * this.m_a1 * this.m_a1 + a * this.m_a2 * this.m_a2;
            this.m_K.Solve33(m, -s, -r, -f)
        } else {
            c = this.m_invMassA;
            g = this.m_invMassB;
            n = this.m_invIA;
            a = this.m_invIB;
            f = n * this.m_s1 + a * this.m_s2;
            v = n + a;
            this.m_K.col1.Set(c + g + n * this.m_s1 * this.m_s1 + a * this.m_s2 * this.m_s2, f, 0);
            this.m_K.col2.Set(f,
                v, 0);
            f = this.m_K.Solve22(new w, -s, -r);
            m.x = f.x;
            m.y = f.y;
            m.z = 0
        }
        f = m.x * this.m_perp.x + m.z * this.m_axis.x;
        c = m.x * this.m_perp.y + m.z * this.m_axis.y;
        s = m.x * this.m_s1 + m.y + m.z * this.m_a1;
        m = m.x * this.m_s2 + m.y + m.z * this.m_a2;
        l.x -= this.m_invMassA * f;
        l.y -= this.m_invMassA * c;
        j -= this.m_invIA * s;
        o.x += this.m_invMassB * f;
        o.y += this.m_invMassB * c;
        q += this.m_invIB * m;
        d.m_sweep.a = j;
        h.m_sweep.a = q;
        d.SynchronizeTransform();
        h.SynchronizeTransform();
        return b <= F.b2_linearSlop && e <= F.b2_angularSlop
    };
    Box2D.inherit(O, Box2D.Dynamics.Joints.b2JointDef);
    O.prototype.__super = Box2D.Dynamics.Joints.b2JointDef.prototype;
    O.b2PrismaticJointDef = function () {
        Box2D.Dynamics.Joints.b2JointDef.b2JointDef.apply(this, arguments);
        this.localAnchorA = new w;
        this.localAnchorB = new w;
        this.localAxisA = new w
    };
    O.prototype.b2PrismaticJointDef = function () {
        this.__super.b2JointDef.call(this);
        this.type = I.e_prismaticJoint;
        this.localAxisA.Set(1, 0);
        this.referenceAngle = 0;
        this.enableLimit = false;
        this.upperTranslation = this.lowerTranslation = 0;
        this.enableMotor = false;
        this.motorSpeed = this.maxMotorForce = 0
    };
    O.prototype.Initialize = function (d, h, l, j) {
        this.bodyA = d;
        this.bodyB = h;
        this.localAnchorA = this.bodyA.GetLocalPoint(l);
        this.localAnchorB = this.bodyB.GetLocalPoint(l);
        this.localAxisA = this.bodyA.GetLocalVector(j);
        this.referenceAngle = this.bodyB.GetAngle() - this.bodyA.GetAngle()
    };
    Box2D.inherit(E, Box2D.Dynamics.Joints.b2Joint);
    E.prototype.__super = Box2D.Dynamics.Joints.b2Joint.prototype;
    E.b2PulleyJoint = function () {
        Box2D.Dynamics.Joints.b2Joint.b2Joint.apply(this, arguments);
        this.m_groundAnchor1 = new w;
        this.m_groundAnchor2 = new w;
        this.m_localAnchor1 = new w;
        this.m_localAnchor2 = new w;
        this.m_u1 = new w;
        this.m_u2 = new w
    };
    E.prototype.GetAnchorA = function () {
        return this.m_bodyA.GetWorldPoint(this.m_localAnchor1)
    };
    E.prototype.GetAnchorB = function () {
        return this.m_bodyB.GetWorldPoint(this.m_localAnchor2)
    };
    E.prototype.GetReactionForce = function (d) {
        if (d === undefined) d = 0;
        return new w(d * this.m_impulse * this.m_u2.x, d * this.m_impulse * this.m_u2.y)
    };
    E.prototype.GetReactionTorque = function () {
        return 0
    };
    E.prototype.GetGroundAnchorA = function () {
        var d = this.m_ground.m_xf.position.Copy();
        d.Add(this.m_groundAnchor1);
        return d
    };
    E.prototype.GetGroundAnchorB = function () {
        var d = this.m_ground.m_xf.position.Copy();
        d.Add(this.m_groundAnchor2);
        return d
    };
    E.prototype.GetLength1 = function () {
        var d = this.m_bodyA.GetWorldPoint(this.m_localAnchor1),
            h = d.x - (this.m_ground.m_xf.position.x + this.m_groundAnchor1.x);
        d = d.y - (this.m_ground.m_xf.position.y + this.m_groundAnchor1.y);
        return Math.sqrt(h * h + d * d)
    };
    E.prototype.GetLength2 = function () {
        var d = this.m_bodyB.GetWorldPoint(this.m_localAnchor2),
            h = d.x - (this.m_ground.m_xf.position.x + this.m_groundAnchor2.x);
        d = d.y - (this.m_ground.m_xf.position.y + this.m_groundAnchor2.y);
        return Math.sqrt(h * h + d * d)
    };
    E.prototype.GetRatio = function () {
        return this.m_ratio
    };
    E.prototype.b2PulleyJoint = function (d) {
        this.__super.b2Joint.call(this, d);
        this.m_ground = this.m_bodyA.m_world.m_groundBody;
        this.m_groundAnchor1.x = d.groundAnchorA.x - this.m_ground.m_xf.position.x;
        this.m_groundAnchor1.y = d.groundAnchorA.y - this.m_ground.m_xf.position.y;
        this.m_groundAnchor2.x = d.groundAnchorB.x - this.m_ground.m_xf.position.x;
        this.m_groundAnchor2.y = d.groundAnchorB.y - this.m_ground.m_xf.position.y;
        this.m_localAnchor1.SetV(d.localAnchorA);
        this.m_localAnchor2.SetV(d.localAnchorB);
        this.m_ratio = d.ratio;
        this.m_constant = d.lengthA + this.m_ratio * d.lengthB;
        this.m_maxLength1 = y.Min(d.maxLengthA, this.m_constant - this.m_ratio * E.b2_minPulleyLength);
        this.m_maxLength2 = y.Min(d.maxLengthB, (this.m_constant - E.b2_minPulleyLength) / this.m_ratio);
        this.m_limitImpulse2 = this.m_limitImpulse1 = this.m_impulse = 0
    };
    E.prototype.InitVelocityConstraints = function (d) {
            var h = this.m_bodyA,
                l = this.m_bodyB,
                j;
            j = h.m_xf.R;
            var o = this.m_localAnchor1.x - h.m_sweep.localCenter.x,
                q = this.m_localAnchor1.y - h.m_sweep.localCenter.y,
                n = j.col1.x * o + j.col2.x * q;
            q = j.col1.y * o + j.col2.y * q;
            o = n;
            j = l.m_xf.R;
            var a = this.m_localAnchor2.x - l.m_sweep.localCenter.x,
                c = this.m_localAnchor2.y - l.m_sweep.localCenter.y;
            n = j.col1.x * a + j.col2.x * c;
            c = j.col1.y * a + j.col2.y * c;
            a = n;
            j = l.m_sweep.c.x + a;
            n = l.m_sweep.c.y + c;
            var g = this.m_ground.m_xf.position.x + this.m_groundAnchor2.x,
                b = this.m_ground.m_xf.position.y + this.m_groundAnchor2.y;
            this.m_u1.Set(h.m_sweep.c.x + o - (this.m_ground.m_xf.position.x + this.m_groundAnchor1.x), h.m_sweep.c.y + q - (this.m_ground.m_xf.position.y + this.m_groundAnchor1.y));
            this.m_u2.Set(j - g, n - b);
            j = this.m_u1.Length();
            n = this.m_u2.Length();
            j > F.b2_linearSlop ? this.m_u1.Multiply(1 / j) : this.m_u1.SetZero();
            n > F.b2_linearSlop ? this.m_u2.Multiply(1 / n) : this.m_u2.SetZero();
            if (this.m_constant - j - this.m_ratio * n > 0) {
                this.m_state = I.e_inactiveLimit;
                this.m_impulse = 0
            } else this.m_state = I.e_atUpperLimit; if (j < this.m_maxLength1) {
                this.m_limitState1 = I.e_inactiveLimit;
                this.m_limitImpulse1 = 0
            } else this.m_limitState1 = I.e_atUpperLimit; if (n < this.m_maxLength2) {
                this.m_limitState2 = I.e_inactiveLimit;
                this.m_limitImpulse2 = 0
            } else this.m_limitState2 = I.e_atUpperLimit;
            j = o * this.m_u1.y - q * this.m_u1.x;
            n = a * this.m_u2.y - c * this.m_u2.x;
            this.m_limitMass1 = h.m_invMass + h.m_invI * j * j;
            this.m_limitMass2 = l.m_invMass + l.m_invI * n * n;
            this.m_pulleyMass = this.m_limitMass1 + this.m_ratio * this.m_ratio * this.m_limitMass2;
            this.m_limitMass1 = 1 / this.m_limitMass1;
            this.m_limitMass2 = 1 / this.m_limitMass2;
            this.m_pulleyMass = 1 / this.m_pulleyMass;
            if (d.warmStarting) {
                this.m_impulse *= d.dtRatio;
                this.m_limitImpulse1 *= d.dtRatio;
                this.m_limitImpulse2 *= d.dtRatio;
                d = (-this.m_impulse - this.m_limitImpulse1) * this.m_u1.x;
                j = (-this.m_impulse - this.m_limitImpulse1) * this.m_u1.y;
                n = (-this.m_ratio * this.m_impulse - this.m_limitImpulse2) * this.m_u2.x;
                g = (-this.m_ratio * this.m_impulse - this.m_limitImpulse2) * this.m_u2.y;
                h.m_linearVelocity.x += h.m_invMass * d;
                h.m_linearVelocity.y += h.m_invMass * j;
                h.m_angularVelocity += h.m_invI * (o * j - q * d);
                l.m_linearVelocity.x += l.m_invMass * n;
                l.m_linearVelocity.y += l.m_invMass * g;
                l.m_angularVelocity += l.m_invI * (a * g - c * n)
            } else this.m_limitImpulse2 = this.m_limitImpulse1 = this.m_impulse = 0
    };
    E.prototype.SolveVelocityConstraints = function () {
        var d = this.m_bodyA,
            h = this.m_bodyB,
            l;
        l = d.m_xf.R;
        var j = this.m_localAnchor1.x - d.m_sweep.localCenter.x,
            o = this.m_localAnchor1.y - d.m_sweep.localCenter.y,
            q = l.col1.x * j + l.col2.x * o;
        o = l.col1.y * j + l.col2.y * o;
        j = q;
        l = h.m_xf.R;
        var n = this.m_localAnchor2.x - h.m_sweep.localCenter.x,
            a = this.m_localAnchor2.y - h.m_sweep.localCenter.y;
        q = l.col1.x * n + l.col2.x * a;
        a = l.col1.y * n + l.col2.y * a;
        n = q;
        var c = q = l = 0,
            g = 0;
        l = g = l = g = c = q = l = 0;
        if (this.m_state == I.e_atUpperLimit) {
            l = d.m_linearVelocity.x + -d.m_angularVelocity * o;
            q = d.m_linearVelocity.y + d.m_angularVelocity * j;
            c = h.m_linearVelocity.x + -h.m_angularVelocity * a;
            g = h.m_linearVelocity.y + h.m_angularVelocity * n;
            l = -(this.m_u1.x * l + this.m_u1.y * q) - this.m_ratio * (this.m_u2.x * c + this.m_u2.y * g);
            g = this.m_pulleyMass * -l;
            l = this.m_impulse;
            this.m_impulse = y.Max(0, this.m_impulse + g);
            g = this.m_impulse - l;
            l = -g * this.m_u1.x;
            q = -g * this.m_u1.y;
            c = -this.m_ratio * g * this.m_u2.x;
            g = -this.m_ratio * g * this.m_u2.y;
            d.m_linearVelocity.x += d.m_invMass * l;
            d.m_linearVelocity.y += d.m_invMass * q;
            d.m_angularVelocity += d.m_invI * (j * q - o * l);
            h.m_linearVelocity.x += h.m_invMass * c;
            h.m_linearVelocity.y += h.m_invMass * g;
            h.m_angularVelocity += h.m_invI * (n * g - a * c)
        }
        if (this.m_limitState1 == I.e_atUpperLimit) {
            l = d.m_linearVelocity.x + -d.m_angularVelocity * o;
            q = d.m_linearVelocity.y + d.m_angularVelocity * j;
            l = -(this.m_u1.x * l + this.m_u1.y * q);
            g = -this.m_limitMass1 * l;
            l = this.m_limitImpulse1;
            this.m_limitImpulse1 = y.Max(0, this.m_limitImpulse1 + g);
            g = this.m_limitImpulse1 - l;
            l = -g * this.m_u1.x;
            q = -g * this.m_u1.y;
            d.m_linearVelocity.x += d.m_invMass * l;
            d.m_linearVelocity.y += d.m_invMass * q;
            d.m_angularVelocity += d.m_invI * (j * q - o * l)
        }
        if (this.m_limitState2 == I.e_atUpperLimit) {
            c = h.m_linearVelocity.x + -h.m_angularVelocity * a;
            g = h.m_linearVelocity.y + h.m_angularVelocity * n;
            l = -(this.m_u2.x * c + this.m_u2.y * g);
            g = -this.m_limitMass2 * l;
            l = this.m_limitImpulse2;
            this.m_limitImpulse2 = y.Max(0, this.m_limitImpulse2 + g);
            g = this.m_limitImpulse2 - l;
            c = -g * this.m_u2.x;
            g = -g * this.m_u2.y;
            h.m_linearVelocity.x += h.m_invMass * c;
            h.m_linearVelocity.y += h.m_invMass * g;
            h.m_angularVelocity += h.m_invI * (n * g - a * c)
        }
    };
    E.prototype.SolvePositionConstraints = function () {
        var d = this.m_bodyA,
            h = this.m_bodyB,
            l, j = this.m_ground.m_xf.position.x + this.m_groundAnchor1.x,
            o = this.m_ground.m_xf.position.y + this.m_groundAnchor1.y,
            q = this.m_ground.m_xf.position.x + this.m_groundAnchor2.x,
            n = this.m_ground.m_xf.position.y + this.m_groundAnchor2.y,
            a = 0,
            c = 0,
            g = 0,
            b = 0,
            e = l = 0,
            f = 0,
            m = 0,
            r = e = m = l = e = l = 0;
        if (this.m_state == I.e_atUpperLimit) {
            l = d.m_xf.R;
            a = this.m_localAnchor1.x - d.m_sweep.localCenter.x;
            c = this.m_localAnchor1.y - d.m_sweep.localCenter.y;
            e = l.col1.x * a + l.col2.x * c;
            c = l.col1.y * a + l.col2.y * c;
            a = e;
            l = h.m_xf.R;
            g = this.m_localAnchor2.x - h.m_sweep.localCenter.x;
            b = this.m_localAnchor2.y - h.m_sweep.localCenter.y;
            e = l.col1.x * g + l.col2.x * b;
            b = l.col1.y * g + l.col2.y * b;
            g = e;
            l = d.m_sweep.c.x + a;
            e = d.m_sweep.c.y + c;
            f = h.m_sweep.c.x + g;
            m = h.m_sweep.c.y + b;
            this.m_u1.Set(l - j, e - o);
            this.m_u2.Set(f - q, m - n);
            l = this.m_u1.Length();
            e = this.m_u2.Length();
            l > F.b2_linearSlop ? this.m_u1.Multiply(1 / l) : this.m_u1.SetZero();
            e > F.b2_linearSlop ? this.m_u2.Multiply(1 / e) : this.m_u2.SetZero();
            l = this.m_constant - l - this.m_ratio * e;
            r = y.Max(r, -l);
            l = y.Clamp(l + F.b2_linearSlop, -F.b2_maxLinearCorrection, 0);
            m = -this.m_pulleyMass * l;
            l = -m * this.m_u1.x;
            e = -m * this.m_u1.y;
            f = -this.m_ratio * m * this.m_u2.x;
            m = -this.m_ratio * m * this.m_u2.y;
            d.m_sweep.c.x += d.m_invMass * l;
            d.m_sweep.c.y += d.m_invMass * e;
            d.m_sweep.a += d.m_invI * (a * e - c * l);
            h.m_sweep.c.x += h.m_invMass * f;
            h.m_sweep.c.y += h.m_invMass * m;
            h.m_sweep.a += h.m_invI * (g * m - b * f);
            d.SynchronizeTransform();
            h.SynchronizeTransform()
        }
        if (this.m_limitState1 == I.e_atUpperLimit) {
            l = d.m_xf.R;
            a = this.m_localAnchor1.x - d.m_sweep.localCenter.x;
            c = this.m_localAnchor1.y - d.m_sweep.localCenter.y;
            e = l.col1.x * a + l.col2.x * c;
            c = l.col1.y * a + l.col2.y * c;
            a = e;
            l = d.m_sweep.c.x + a;
            e = d.m_sweep.c.y + c;
            this.m_u1.Set(l - j, e - o);
            l = this.m_u1.Length();
            if (l > F.b2_linearSlop) {
                this.m_u1.x *= 1 / l;
                this.m_u1.y *= 1 / l
            } else this.m_u1.SetZero();
            l = this.m_maxLength1 - l;
            r = y.Max(r, -l);
            l = y.Clamp(l + F.b2_linearSlop, -F.b2_maxLinearCorrection, 0);
            m = -this.m_limitMass1 * l;
            l = -m * this.m_u1.x;
            e = -m * this.m_u1.y;
            d.m_sweep.c.x += d.m_invMass * l;
            d.m_sweep.c.y += d.m_invMass * e;
            d.m_sweep.a += d.m_invI * (a * e - c * l);
            d.SynchronizeTransform()
        }
        if (this.m_limitState2 == I.e_atUpperLimit) {
            l = h.m_xf.R;
            g = this.m_localAnchor2.x - h.m_sweep.localCenter.x;
            b = this.m_localAnchor2.y - h.m_sweep.localCenter.y;
            e = l.col1.x * g + l.col2.x * b;
            b = l.col1.y * g + l.col2.y * b;
            g = e;
            f = h.m_sweep.c.x + g;
            m = h.m_sweep.c.y + b;
            this.m_u2.Set(f - q, m - n);
            e = this.m_u2.Length();
            if (e > F.b2_linearSlop) {
                this.m_u2.x *= 1 / e;
                this.m_u2.y *= 1 / e
            } else this.m_u2.SetZero();
            l = this.m_maxLength2 - e;
            r = y.Max(r, -l);
            l = y.Clamp(l + F.b2_linearSlop, -F.b2_maxLinearCorrection, 0);
            m = -this.m_limitMass2 * l;
            f = -m * this.m_u2.x;
            m = -m * this.m_u2.y;
            h.m_sweep.c.x += h.m_invMass * f;
            h.m_sweep.c.y += h.m_invMass * m;
            h.m_sweep.a += h.m_invI * (g * m - b * f);
            h.SynchronizeTransform()
        }
        return r < F.b2_linearSlop
    };
    Box2D.postDefs.push(function () {
        Box2D.Dynamics.Joints.b2PulleyJoint.b2_minPulleyLength = 2
    });
    Box2D.inherit(R, Box2D.Dynamics.Joints.b2JointDef);
    R.prototype.__super = Box2D.Dynamics.Joints.b2JointDef.prototype;
    R.b2PulleyJointDef = function () {
            Box2D.Dynamics.Joints.b2JointDef.b2JointDef.apply(this, arguments);
            this.groundAnchorA = new w;
            this.groundAnchorB = new w;
            this.localAnchorA = new w;
            this.localAnchorB = new w
    };
    R.prototype.b2PulleyJointDef = function () {
        this.__super.b2JointDef.call(this);
        this.type = I.e_pulleyJoint;
        this.groundAnchorA.Set(-1, 1);
        this.groundAnchorB.Set(1, 1);
        this.localAnchorA.Set(-1, 0);
        this.localAnchorB.Set(1, 0);
        this.maxLengthB = this.lengthB = this.maxLengthA = this.lengthA = 0;
        this.ratio = 1;
        this.collideConnected = true
    };
    R.prototype.Initialize = function (d, h, l, j, o, q, n) {
            if (n === undefined) n = 0;
            this.bodyA = d;
            this.bodyB = h;
            this.groundAnchorA.SetV(l);
            this.groundAnchorB.SetV(j);
            this.localAnchorA = this.bodyA.GetLocalPoint(o);
            this.localAnchorB = this.bodyB.GetLocalPoint(q);
            d = o.x - l.x;
            l = o.y - l.y;
            this.lengthA = Math.sqrt(d * d + l * l);
            l = q.x - j.x;
            j = q.y - j.y;
            this.lengthB = Math.sqrt(l * l + j * j);
            this.ratio = n;
            n = this.lengthA + this.ratio * this.lengthB;
            this.maxLengthA = n - this.ratio * E.b2_minPulleyLength;
            this.maxLengthB = (n - E.b2_minPulleyLength) / this.ratio
    };
    Box2D.inherit(N, Box2D.Dynamics.Joints.b2Joint);
    N.prototype.__super = Box2D.Dynamics.Joints.b2Joint.prototype;
    N.b2RevoluteJoint = function () {
        Box2D.Dynamics.Joints.b2Joint.b2Joint.apply(this, arguments);
        this.K = new G;
        this.K1 = new G;
        this.K2 = new G;
        this.K3 = new G;
        this.impulse3 = new A;
        this.impulse2 = new w;
        this.reduced = new w;
        this.m_localAnchor1 = new w;
        this.m_localAnchor2 = new w;
        this.m_impulse = new A;
        this.m_mass = new K
    };
    N.prototype.GetAnchorA = function () {
        return this.m_bodyA.GetWorldPoint(this.m_localAnchor1)
    };
    N.prototype.GetAnchorB = function () {
        return this.m_bodyB.GetWorldPoint(this.m_localAnchor2)
    };
    N.prototype.GetReactionForce = function (d) {
        if (d === undefined) d = 0;
        return new w(d * this.m_impulse.x, d * this.m_impulse.y)
    };
    N.prototype.GetReactionTorque = function (d) {
        if (d === undefined) d = 0;
        return d * this.m_impulse.z
    };
    N.prototype.GetJointAngle = function () {
        return this.m_bodyB.m_sweep.a - this.m_bodyA.m_sweep.a - this.m_referenceAngle
    };
    N.prototype.GetJointSpeed = function () {
        return this.m_bodyB.m_angularVelocity - this.m_bodyA.m_angularVelocity
    };
    N.prototype.IsLimitEnabled = function () {
        return this.m_enableLimit
    };
    N.prototype.EnableLimit = function (d) {
            this.m_enableLimit = d
    };
    N.prototype.GetLowerLimit = function () {
        return this.m_lowerAngle
    };
    N.prototype.GetUpperLimit = function () {
        return this.m_upperAngle
    };
    N.prototype.SetLimits = function (d, h) {
        if (d === undefined) d = 0;
        if (h === undefined) h = 0;
        this.m_lowerAngle = d;
        this.m_upperAngle = h
    };
    N.prototype.IsMotorEnabled = function () {
        this.m_bodyA.SetAwake(true);
        this.m_bodyB.SetAwake(true);
        return this.m_enableMotor
    };
    N.prototype.EnableMotor = function (d) {
        this.m_enableMotor = d
    };
    N.prototype.SetMotorSpeed = function (d) {
        if (d === undefined) d = 0;
        this.m_bodyA.SetAwake(true);
        this.m_bodyB.SetAwake(true);
        this.m_motorSpeed = d
    };
    N.prototype.GetMotorSpeed = function () {
        return this.m_motorSpeed
    };
    N.prototype.SetMaxMotorTorque = function (d) {
        if (d === undefined) d = 0;
        this.m_maxMotorTorque = d
    };
    N.prototype.GetMotorTorque = function () {
        return this.m_maxMotorTorque
    };
    N.prototype.b2RevoluteJoint = function (d) {
        this.__super.b2Joint.call(this, d);
        this.m_localAnchor1.SetV(d.localAnchorA);
        this.m_localAnchor2.SetV(d.localAnchorB);
        this.m_referenceAngle = d.referenceAngle;
        this.m_impulse.SetZero();
        this.m_motorImpulse = 0;
        this.m_lowerAngle = d.lowerAngle;
        this.m_upperAngle = d.upperAngle;
        this.m_maxMotorTorque = d.maxMotorTorque;
        this.m_motorSpeed = d.motorSpeed;
        this.m_enableLimit = d.enableLimit;
        this.m_enableMotor = d.enableMotor;
        this.m_limitState = I.e_inactiveLimit
    };
    N.prototype.InitVelocityConstraints = function (d) {
        var h = this.m_bodyA,
            l = this.m_bodyB,
            j, o = 0;
        j = h.m_xf.R;
        var q = this.m_localAnchor1.x - h.m_sweep.localCenter.x,
            n = this.m_localAnchor1.y - h.m_sweep.localCenter.y;
        o = j.col1.x * q + j.col2.x * n;
        n = j.col1.y * q + j.col2.y * n;
        q = o;
        j = l.m_xf.R;
        var a = this.m_localAnchor2.x - l.m_sweep.localCenter.x,
            c = this.m_localAnchor2.y - l.m_sweep.localCenter.y;
        o = j.col1.x * a + j.col2.x * c;
        c = j.col1.y * a + j.col2.y * c;
        a = o;
        j = h.m_invMass;
        o = l.m_invMass;
        var g = h.m_invI,
            b = l.m_invI;
        this.m_mass.col1.x = j + o + n * n * g + c * c * b;
        this.m_mass.col2.x = -n * q * g - c * a * b;
        this.m_mass.col3.x = -n * g - c * b;
        this.m_mass.col1.y = this.m_mass.col2.x;
        this.m_mass.col2.y = j + o + q * q * g + a * a * b;
        this.m_mass.col3.y = q * g + a * b;
        this.m_mass.col1.z = this.m_mass.col3.x;
        this.m_mass.col2.z = this.m_mass.col3.y;
        this.m_mass.col3.z = g + b;
        this.m_motorMass = 1 / (g + b);
        if (this.m_enableMotor == false) this.m_motorImpulse = 0;
        if (this.m_enableLimit) {
            var e = l.m_sweep.a - h.m_sweep.a - this.m_referenceAngle;
            if (y.Abs(this.m_upperAngle - this.m_lowerAngle) < 2 * F.b2_angularSlop) this.m_limitState = I.e_equalLimits;
            else if (e <= this.m_lowerAngle) {
                if (this.m_limitState != I.e_atLowerLimit) this.m_impulse.z = 0;
                this.m_limitState = I.e_atLowerLimit
            } else if (e >= this.m_upperAngle) {
                if (this.m_limitState != I.e_atUpperLimit) this.m_impulse.z = 0;
                this.m_limitState = I.e_atUpperLimit
            } else {
                this.m_limitState = I.e_inactiveLimit;
                this.m_impulse.z = 0
            }
        } else this.m_limitState = I.e_inactiveLimit; if (d.warmStarting) {
            this.m_impulse.x *= d.dtRatio;
            this.m_impulse.y *= d.dtRatio;
            this.m_motorImpulse *= d.dtRatio;
            d = this.m_impulse.x;
            e = this.m_impulse.y;
            h.m_linearVelocity.x -= j * d;
            h.m_linearVelocity.y -= j * e;
            h.m_angularVelocity -= g * (q * e - n * d + this.m_motorImpulse + this.m_impulse.z);
            l.m_linearVelocity.x += o * d;
            l.m_linearVelocity.y += o * e;
            l.m_angularVelocity += b * (a * e - c * d + this.m_motorImpulse + this.m_impulse.z)
        } else {
            this.m_impulse.SetZero();
            this.m_motorImpulse = 0
        }
    };
    N.prototype.SolveVelocityConstraints = function (d) {
        var h = this.m_bodyA,
            l = this.m_bodyB,
            j = 0,
            o = j = 0,
            q = 0,
            n = 0,
            a = 0,
            c = h.m_linearVelocity,
            g = h.m_angularVelocity,
            b = l.m_linearVelocity,
            e = l.m_angularVelocity,
            f = h.m_invMass,
            m = l.m_invMass,
            r = h.m_invI,
            s = l.m_invI;
        if (this.m_enableMotor && this.m_limitState != I.e_equalLimits) {
            o = this.m_motorMass * -(e - g - this.m_motorSpeed);
            q = this.m_motorImpulse;
            n = d.dt * this.m_maxMotorTorque;
            this.m_motorImpulse = y.Clamp(this.m_motorImpulse + o, -n, n);
            o = this.m_motorImpulse - q;
            g -= r * o;
            e += s * o
        }
        if (this.m_enableLimit && this.m_limitState != I.e_inactiveLimit) {
            d = h.m_xf.R;
            o = this.m_localAnchor1.x - h.m_sweep.localCenter.x;
            q = this.m_localAnchor1.y - h.m_sweep.localCenter.y;
            j = d.col1.x * o + d.col2.x * q;
            q = d.col1.y * o + d.col2.y * q;
            o = j;
            d = l.m_xf.R;
            n = this.m_localAnchor2.x - l.m_sweep.localCenter.x;
            a = this.m_localAnchor2.y - l.m_sweep.localCenter.y;
            j = d.col1.x * n + d.col2.x * a;
            a = d.col1.y * n + d.col2.y * a;
            n = j;
            d = b.x + -e * a - c.x - -g * q;
            var v = b.y + e * n - c.y - g * o;
            this.m_mass.Solve33(this.impulse3, -d, -v, -(e - g));
            if (this.m_limitState == I.e_equalLimits) this.m_impulse.Add(this.impulse3);
            else if (this.m_limitState == I.e_atLowerLimit) {
                j = this.m_impulse.z + this.impulse3.z;
                if (j < 0) {
                    this.m_mass.Solve22(this.reduced, -d, -v);
                    this.impulse3.x = this.reduced.x;
                    this.impulse3.y = this.reduced.y;
                    this.impulse3.z = -this.m_impulse.z;
                    this.m_impulse.x += this.reduced.x;
                    this.m_impulse.y += this.reduced.y;
                    this.m_impulse.z = 0
                }
            } else if (this.m_limitState == I.e_atUpperLimit) {
                j = this.m_impulse.z + this.impulse3.z;
                if (j > 0) {
                    this.m_mass.Solve22(this.reduced, -d, -v);
                    this.impulse3.x = this.reduced.x;
                    this.impulse3.y = this.reduced.y;
                    this.impulse3.z = -this.m_impulse.z;
                    this.m_impulse.x += this.reduced.x;
                    this.m_impulse.y += this.reduced.y;
                    this.m_impulse.z = 0
                }
            }
            c.x -= f * this.impulse3.x;
            c.y -= f * this.impulse3.y;
            g -= r * (o * this.impulse3.y - q * this.impulse3.x + this.impulse3.z);
            b.x += m * this.impulse3.x;
            b.y += m * this.impulse3.y;
            e += s * (n * this.impulse3.y - a * this.impulse3.x + this.impulse3.z)
        } else {
            d = h.m_xf.R;
            o = this.m_localAnchor1.x - h.m_sweep.localCenter.x;
            q = this.m_localAnchor1.y - h.m_sweep.localCenter.y;
            j = d.col1.x * o + d.col2.x * q;
            q = d.col1.y * o + d.col2.y * q;
            o = j;
            d = l.m_xf.R;
            n = this.m_localAnchor2.x - l.m_sweep.localCenter.x;
            a = this.m_localAnchor2.y - l.m_sweep.localCenter.y;
            j = d.col1.x * n + d.col2.x * a;
            a = d.col1.y * n + d.col2.y * a;
            n = j;
            this.m_mass.Solve22(this.impulse2, -(b.x + -e * a - c.x - -g * q), -(b.y + e * n - c.y - g * o));
            this.m_impulse.x += this.impulse2.x;
            this.m_impulse.y += this.impulse2.y;
            c.x -= f * this.impulse2.x;
            c.y -= f * this.impulse2.y;
            g -= r * (o * this.impulse2.y - q * this.impulse2.x);
            b.x += m * this.impulse2.x;
            b.y += m * this.impulse2.y;
            e += s * (n * this.impulse2.y - a * this.impulse2.x)
        }
        h.m_linearVelocity.SetV(c);
        h.m_angularVelocity = g;
        l.m_linearVelocity.SetV(b);
        l.m_angularVelocity = e
    };
    N.prototype.SolvePositionConstraints = function () {
        var d = 0,
            h, l = this.m_bodyA,
            j = this.m_bodyB,
            o = 0,
            q = h = 0,
            n = 0,
            a = 0;
        if (this.m_enableLimit && this.m_limitState != I.e_inactiveLimit) {
            d = j.m_sweep.a - l.m_sweep.a - this.m_referenceAngle;
            var c = 0;
            if (this.m_limitState == I.e_equalLimits) {
                d = y.Clamp(d - this.m_lowerAngle, -F.b2_maxAngularCorrection, F.b2_maxAngularCorrection);
                c = -this.m_motorMass * d;
                o = y.Abs(d)
            } else if (this.m_limitState == I.e_atLowerLimit) {
                d = d - this.m_lowerAngle;
                o = -d;
                d = y.Clamp(d + F.b2_angularSlop, -F.b2_maxAngularCorrection, 0);
                c = -this.m_motorMass * d
            } else if (this.m_limitState == I.e_atUpperLimit) {
                o = d = d - this.m_upperAngle;
                d = y.Clamp(d - F.b2_angularSlop, 0, F.b2_maxAngularCorrection);
                c = -this.m_motorMass * d
            }
            l.m_sweep.a -= l.m_invI * c;
            j.m_sweep.a += j.m_invI * c;
            l.SynchronizeTransform();
            j.SynchronizeTransform()
        }
        h = l.m_xf.R;
        c = this.m_localAnchor1.x - l.m_sweep.localCenter.x;
        d = this.m_localAnchor1.y - l.m_sweep.localCenter.y;
        q = h.col1.x * c + h.col2.x * d;
        d = h.col1.y * c + h.col2.y * d;
        c = q;
        h = j.m_xf.R;
        var g = this.m_localAnchor2.x - j.m_sweep.localCenter.x,
            b = this.m_localAnchor2.y - j.m_sweep.localCenter.y;
        q = h.col1.x * g + h.col2.x * b;
        b = h.col1.y * g + h.col2.y * b;
        g = q;
        n = j.m_sweep.c.x + g - l.m_sweep.c.x - c;
        a = j.m_sweep.c.y + b - l.m_sweep.c.y - d;
        var e = n * n + a * a;
        h = Math.sqrt(e);
        q = l.m_invMass;
        var f = j.m_invMass,
            m = l.m_invI,
            r = j.m_invI,
            s = 10 * F.b2_linearSlop;
        if (e > s * s) {
            e = 1 / (q + f);
            n = e * -n;
            a = e * -a;
            l.m_sweep.c.x -= 0.5 * q * n;
            l.m_sweep.c.y -= 0.5 * q * a;
            j.m_sweep.c.x += 0.5 * f * n;
            j.m_sweep.c.y += 0.5 * f * a;
            n = j.m_sweep.c.x + g - l.m_sweep.c.x - c;
            a = j.m_sweep.c.y + b - l.m_sweep.c.y - d
        }
        this.K1.col1.x = q + f;
        this.K1.col2.x = 0;
        this.K1.col1.y = 0;
        this.K1.col2.y = q + f;
        this.K2.col1.x = m * d * d;
        this.K2.col2.x = -m * c * d;
        this.K2.col1.y = -m * c * d;
        this.K2.col2.y = m * c * c;
        this.K3.col1.x = r * b * b;
        this.K3.col2.x = -r * g * b;
        this.K3.col1.y = -r * g * b;
        this.K3.col2.y = r * g * g;
        this.K.SetM(this.K1);
        this.K.AddM(this.K2);
        this.K.AddM(this.K3);
        this.K.Solve(N.tImpulse, -n, -a);
        n = N.tImpulse.x;
        a = N.tImpulse.y;
        l.m_sweep.c.x -= l.m_invMass * n;
        l.m_sweep.c.y -= l.m_invMass * a;
        l.m_sweep.a -= l.m_invI * (c * a - d * n);
        j.m_sweep.c.x += j.m_invMass * n;
        j.m_sweep.c.y += j.m_invMass * a;
        j.m_sweep.a += j.m_invI * (g * a - b * n);
        l.SynchronizeTransform();
        j.SynchronizeTransform();
        return h <= F.b2_linearSlop && o <= F.b2_angularSlop
    };
    Box2D.postDefs.push(function () {
        Box2D.Dynamics.Joints.b2RevoluteJoint.tImpulse = new w
    });
    Box2D.inherit(S, Box2D.Dynamics.Joints.b2JointDef);
    S.prototype.__super = Box2D.Dynamics.Joints.b2JointDef.prototype;
    S.b2RevoluteJointDef = function () {
        Box2D.Dynamics.Joints.b2JointDef.b2JointDef.apply(this, arguments);
        this.localAnchorA = new w;
        this.localAnchorB = new w
    };
    S.prototype.b2RevoluteJointDef = function () {
        this.__super.b2JointDef.call(this);
        this.type = I.e_revoluteJoint;
        this.localAnchorA.Set(0, 0);
        this.localAnchorB.Set(0, 0);
        this.motorSpeed = this.maxMotorTorque = this.upperAngle = this.lowerAngle = this.referenceAngle = 0;
        this.enableMotor = this.enableLimit = false
    };
    S.prototype.Initialize = function (d, h, l) {
        this.bodyA = d;
        this.bodyB = h;
        this.localAnchorA = this.bodyA.GetLocalPoint(l);
        this.localAnchorB = this.bodyB.GetLocalPoint(l);
        this.referenceAngle = this.bodyB.GetAngle() - this.bodyA.GetAngle()
    };
    Box2D.inherit(aa, Box2D.Dynamics.Joints.b2Joint);
    aa.prototype.__super = Box2D.Dynamics.Joints.b2Joint.prototype;
    aa.b2WeldJoint = function () {
        Box2D.Dynamics.Joints.b2Joint.b2Joint.apply(this, arguments);
        this.m_localAnchorA = new w;
        this.m_localAnchorB = new w;
        this.m_impulse = new A;
        this.m_mass = new K
    };
    aa.prototype.GetAnchorA = function () {
        return this.m_bodyA.GetWorldPoint(this.m_localAnchorA)
    };
    aa.prototype.GetAnchorB = function () {
        return this.m_bodyB.GetWorldPoint(this.m_localAnchorB)
    };
    aa.prototype.GetReactionForce = function (d) {
            if (d === undefined) d = 0;
            return new w(d * this.m_impulse.x, d * this.m_impulse.y)
    };
    aa.prototype.GetReactionTorque = function (d) {
        if (d === undefined) d = 0;
        return d * this.m_impulse.z
    };
    aa.prototype.b2WeldJoint = function (d) {
        this.__super.b2Joint.call(this, d);
        this.m_localAnchorA.SetV(d.localAnchorA);
        this.m_localAnchorB.SetV(d.localAnchorB);
        this.m_referenceAngle = d.referenceAngle;
        this.m_impulse.SetZero();
        this.m_mass = new K
    };
    aa.prototype.InitVelocityConstraints = function (d) {
        var h, l = 0,
            j = this.m_bodyA,
            o = this.m_bodyB;
        h = j.m_xf.R;
        var q = this.m_localAnchorA.x - j.m_sweep.localCenter.x,
            n = this.m_localAnchorA.y - j.m_sweep.localCenter.y;
        l = h.col1.x * q + h.col2.x * n;
        n = h.col1.y * q + h.col2.y * n;
        q = l;
        h = o.m_xf.R;
        var a = this.m_localAnchorB.x - o.m_sweep.localCenter.x,
            c = this.m_localAnchorB.y - o.m_sweep.localCenter.y;
        l = h.col1.x * a + h.col2.x * c;
        c = h.col1.y * a + h.col2.y * c;
        a = l;
        h = j.m_invMass;
        l = o.m_invMass;
        var g = j.m_invI,
            b = o.m_invI;
        this.m_mass.col1.x = h + l + n * n * g + c * c * b;
        this.m_mass.col2.x = -n * q * g - c * a * b;
        this.m_mass.col3.x = -n * g - c * b;
        this.m_mass.col1.y = this.m_mass.col2.x;
        this.m_mass.col2.y = h + l + q * q * g + a * a * b;
        this.m_mass.col3.y = q * g + a * b;
        this.m_mass.col1.z = this.m_mass.col3.x;
        this.m_mass.col2.z = this.m_mass.col3.y;
        this.m_mass.col3.z = g + b;
        if (d.warmStarting) {
            this.m_impulse.x *= d.dtRatio;
            this.m_impulse.y *= d.dtRatio;
            this.m_impulse.z *= d.dtRatio;
            j.m_linearVelocity.x -= h * this.m_impulse.x;
            j.m_linearVelocity.y -= h * this.m_impulse.y;
            j.m_angularVelocity -= g * (q * this.m_impulse.y - n * this.m_impulse.x + this.m_impulse.z);
            o.m_linearVelocity.x += l * this.m_impulse.x;
            o.m_linearVelocity.y += l * this.m_impulse.y;
            o.m_angularVelocity += b * (a * this.m_impulse.y - c * this.m_impulse.x + this.m_impulse.z)
        } else this.m_impulse.SetZero()
    };
    aa.prototype.SolveVelocityConstraints = function () {
        var d, h = 0,
            l = this.m_bodyA,
            j = this.m_bodyB,
            o = l.m_linearVelocity,
            q = l.m_angularVelocity,
            n = j.m_linearVelocity,
            a = j.m_angularVelocity,
            c = l.m_invMass,
            g = j.m_invMass,
            b = l.m_invI,
            e = j.m_invI;
        d = l.m_xf.R;
        var f = this.m_localAnchorA.x - l.m_sweep.localCenter.x,
            m = this.m_localAnchorA.y - l.m_sweep.localCenter.y;
        h = d.col1.x * f + d.col2.x * m;
        m = d.col1.y * f + d.col2.y * m;
        f = h;
        d = j.m_xf.R;
        var r = this.m_localAnchorB.x - j.m_sweep.localCenter.x,
            s = this.m_localAnchorB.y - j.m_sweep.localCenter.y;
        h = d.col1.x * r + d.col2.x * s;
        s = d.col1.y * r + d.col2.y * s;
        r = h;
        d = n.x - a * s - o.x + q * m;
        h = n.y + a * r - o.y - q * f;
        var v = a - q,
            t = new A;
        this.m_mass.Solve33(t, -d, -h, -v);
        this.m_impulse.Add(t);
        o.x -= c * t.x;
        o.y -= c * t.y;
        q -= b * (f * t.y - m * t.x + t.z);
        n.x += g * t.x;
        n.y += g * t.y;
        a += e * (r * t.y - s * t.x + t.z);
        l.m_angularVelocity = q;
        j.m_angularVelocity = a
    };
    aa.prototype.SolvePositionConstraints = function () {
        var d, h = 0,
            l = this.m_bodyA,
            j = this.m_bodyB;
        d = l.m_xf.R;
        var o = this.m_localAnchorA.x - l.m_sweep.localCenter.x,
            q = this.m_localAnchorA.y - l.m_sweep.localCenter.y;
        h = d.col1.x * o + d.col2.x * q;
        q = d.col1.y * o + d.col2.y * q;
        o = h;
        d = j.m_xf.R;
        var n = this.m_localAnchorB.x - j.m_sweep.localCenter.x,
            a = this.m_localAnchorB.y - j.m_sweep.localCenter.y;
        h = d.col1.x * n + d.col2.x * a;
        a = d.col1.y * n + d.col2.y * a;
        n = h;
        d = l.m_invMass;
        h = j.m_invMass;
        var c = l.m_invI,
            g = j.m_invI,
            b = j.m_sweep.c.x + n - l.m_sweep.c.x - o,
            e = j.m_sweep.c.y + a - l.m_sweep.c.y - q,
            f = j.m_sweep.a - l.m_sweep.a - this.m_referenceAngle,
            m = 10 * F.b2_linearSlop,
            r = Math.sqrt(b * b + e * e),
            s = y.Abs(f);
        if (r > m) {
            c *= 1;
            g *= 1
        }
        this.m_mass.col1.x = d + h + q * q * c + a * a * g;
        this.m_mass.col2.x = -q * o * c - a * n * g;
        this.m_mass.col3.x = -q * c - a * g;
        this.m_mass.col1.y = this.m_mass.col2.x;
        this.m_mass.col2.y = d + h + o * o * c + n * n * g;
        this.m_mass.col3.y = o * c + n * g;
        this.m_mass.col1.z = this.m_mass.col3.x;
        this.m_mass.col2.z = this.m_mass.col3.y;
        this.m_mass.col3.z = c + g;
        m = new A;
        this.m_mass.Solve33(m, -b, -e, -f);
        l.m_sweep.c.x -= d * m.x;
        l.m_sweep.c.y -= d * m.y;
        l.m_sweep.a -= c * (o * m.y - q * m.x + m.z);
        j.m_sweep.c.x += h * m.x;
        j.m_sweep.c.y += h * m.y;
        j.m_sweep.a += g * (n * m.y - a * m.x + m.z);
        l.SynchronizeTransform();
        j.SynchronizeTransform();
        return r <= F.b2_linearSlop && s <= F.b2_angularSlop
    };
    Box2D.inherit(Z, Box2D.Dynamics.Joints.b2JointDef);
    Z.prototype.__super = Box2D.Dynamics.Joints.b2JointDef.prototype;
    Z.b2WeldJointDef = function () {
        Box2D.Dynamics.Joints.b2JointDef.b2JointDef.apply(this, arguments);
        this.localAnchorA = new w;
        this.localAnchorB = new w
    };
    Z.prototype.b2WeldJointDef = function () {
        this.__super.b2JointDef.call(this);
        this.type = I.e_weldJoint;
        this.referenceAngle = 0
    };
    Z.prototype.Initialize = function (d, h, l) {
        this.bodyA = d;
        this.bodyB = h;
        this.localAnchorA.SetV(this.bodyA.GetLocalPoint(l));
        this.localAnchorB.SetV(this.bodyB.GetLocalPoint(l));
        this.referenceAngle = this.bodyB.GetAngle() - this.bodyA.GetAngle()
    }
})();
(function () {
    var F = Box2D.Dynamics.b2DebugDraw;
    F.b2DebugDraw = function () {
        this.m_xformScale = this.m_fillAlpha = this.m_alpha = this.m_lineThickness = this.m_drawScale = 1;
        var G = this;
        this.m_sprite = {
            graphics: {
                clear: function () {
                    G.m_ctx.clearRect(0, 0, G.m_ctx.canvas.width, G.m_ctx.canvas.height)
                }
            }
        }
    };
    F.prototype._color = function (G, K) {
        return "rgba(" + ((G & 16711680) >> 16) + "," + ((G & 65280) >> 8) + "," + (G & 255) + "," + K + ")"
    };
    F.prototype.b2DebugDraw = function () {
        this.m_drawFlags = 0
    };
    F.prototype.SetFlags = function (G) {
        if (G === undefined) G = 0;
        this.m_drawFlags = G
    };
    F.prototype.GetFlags = function () {
        return this.m_drawFlags
    };
    F.prototype.AppendFlags = function (G) {
        if (G === undefined) G = 0;
        this.m_drawFlags |= G
    };
    F.prototype.ClearFlags = function (G) {
        if (G === undefined) G = 0;
        this.m_drawFlags &= ~G
    };
    F.prototype.SetSprite = function (G) {
        this.m_ctx = G
    };
    F.prototype.GetSprite = function () {
        return this.m_ctx
    };
    F.prototype.SetDrawScale = function (G) {
        if (G === undefined) G = 0;
        this.m_drawScale = G
    };
    F.prototype.GetDrawScale = function () {
        return this.m_drawScale
    };
    F.prototype.SetLineThickness = function (G) {
        if (G === undefined) G = 0;
        this.m_lineThickness = G;
        this.m_ctx.strokeWidth = G
    };
    F.prototype.GetLineThickness = function () {
        return this.m_lineThickness
    };
    F.prototype.SetAlpha = function (G) {
        if (G === undefined) G = 0;
        this.m_alpha = G
    };
    F.prototype.GetAlpha = function () {
        return this.m_alpha
    };
    F.prototype.SetFillAlpha = function (G) {
        if (G === undefined) G = 0;
        this.m_fillAlpha = G
    };
    F.prototype.GetFillAlpha = function () {
        return this.m_fillAlpha
    };
    F.prototype.SetXFormScale = function (G) {
        if (G === undefined) G = 0;
        this.m_xformScale = G
    };
    F.prototype.GetXFormScale = function () {
        return this.m_xformScale
    };
    F.prototype.DrawPolygon = function (G, K, y) {
        if (K) {
            var w = this.m_ctx,
                A = this.m_drawScale;
            w.beginPath();
            w.strokeStyle = this._color(y.color, this.m_alpha);
            w.moveTo(G[0].x * A, G[0].y * A);
            for (y = 1; y < K; y++) w.lineTo(G[y].x * A, G[y].y * A);
            w.lineTo(G[0].x * A, G[0].y * A);
            w.closePath();
            w.stroke()
        }
    };
    F.prototype.DrawSolidPolygon = function (G, K, y) {
        if (K) {
            var w = this.m_ctx,
                A = this.m_drawScale;
            w.beginPath();
            w.strokeStyle = this._color(y.color, this.m_alpha);
            w.fillStyle = this._color(y.color, this.m_fillAlpha);
            w.moveTo(G[0].x * A, G[0].y * A);
            for (y = 1; y < K; y++) w.lineTo(G[y].x * A, G[y].y * A);
            w.lineTo(G[0].x * A, G[0].y * A);
            w.closePath();
            w.fill();
            w.stroke()
        }
    };
    F.prototype.DrawCircle = function (G, K, y) {
        if (K) {
            var w = this.m_ctx,
                A = this.m_drawScale;
            w.beginPath();
            w.strokeStyle = this._color(y.color, this.m_alpha);
            w.arc(G.x * A, G.y * A, K * A, 0, Math.PI * 2, true);
            w.closePath();
            w.stroke()
        }
    };
    F.prototype.DrawSolidCircle = function (G, K, y, w) {
        if (K) {
            var A = this.m_ctx,
                U = this.m_drawScale,
                p = G.x * U,
                B = G.y * U;
            A.moveTo(0, 0);
            A.beginPath();
            A.strokeStyle = this._color(w.color, this.m_alpha);
            A.fillStyle = this._color(w.color, this.m_fillAlpha);
            A.arc(p, B, K * U, 0, Math.PI * 2, true);
            A.moveTo(p, B);
            A.lineTo((G.x + y.x * K) * U, (G.y + y.y * K) * U);
            A.closePath();
            A.fill();
            A.stroke()
        }
    };
    F.prototype.DrawSegment = function (G, K, y) {
        var w = this.m_ctx,
            A = this.m_drawScale;
        w.strokeStyle = this._color(y.color, this.m_alpha);
        w.beginPath();
        w.moveTo(G.x * A, G.y * A);
        w.lineTo(K.x * A, K.y * A);
        w.closePath();
        w.stroke()
    };
    F.prototype.DrawTransform = function (G) {
        var K = this.m_ctx,
            y = this.m_drawScale;
        K.beginPath();
        K.strokeStyle = this._color(16711680, this.m_alpha);
        K.moveTo(G.position.x * y, G.position.y * y);
        K.lineTo((G.position.x + this.m_xformScale * G.R.col1.x) * y, (G.position.y + this.m_xformScale * G.R.col1.y) * y);
        K.strokeStyle = this._color(65280, this.m_alpha);
        K.moveTo(G.position.x * y, G.position.y * y);
        K.lineTo((G.position.x + this.m_xformScale * G.R.col2.x) * y, (G.position.y + this.m_xformScale * G.R.col2.y) * y);
        K.closePath();
        K.stroke()
    }
})();
var i;
for (i = 0; i < Box2D.postDefs.length; ++i) Box2D.postDefs[i]();
delete Box2D.postDefs;

// Copyright 2013 the V8 project authors. All rights reserved.

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

function MakeNewWorld () {
  var Vec2 = Box2D.Common.Math.b2Vec2,
      BodyDef = Box2D.Dynamics.b2BodyDef,
      Body = Box2D.Dynamics.b2Body,
      FixtureDef = Box2D.Dynamics.b2FixtureDef,
      Fixture = Box2D.Dynamics.b2Fixture,
      World = Box2D.Dynamics.b2World,
      MassData = Box2D.Collision.Shapes.b2MassData,
      PolygonShape = Box2D.Collision.Shapes.b2PolygonShape,
      CircleShape = Box2D.Collision.Shapes.b2CircleShape;

  var gravity = new Vec2(0, -10);
  var world = new World(gravity, true);

  var shape = new PolygonShape();
  shape.SetAsEdge(new Vec2(-40.0, 0), new Vec2(40.0, 0));

  var fd = new FixtureDef();
  fd.density = 0.0;
  fd.shape = shape;
  var bd = new BodyDef();
  var ground = world.CreateBody(bd);
  ground.CreateFixture(fd);

  var a = .5;
  var shape = new PolygonShape();
  shape.SetAsBox(a, a);

  var x = new Vec2(-7.0, 0.75);
  var y = new Vec2();
  var deltaX = new Vec2(0.5625, 1);
  var deltaY = new Vec2(1.125, 0.0);

  for (var i = 0; i < 10; ++i) {
    y.Set(x.x, x.y);

    for (var j = 0; j < 5; ++j) {
      var fd = new FixtureDef();
      fd.density = 5.0;
      fd.shape = shape;

      var bd = new BodyDef();
      bd.type = Body.b2_dynamicBody;
      bd.position.Set(y.x, y.y);
      var body = world.CreateBody(bd);
      body.CreateFixture(fd);
      y.Add(deltaY);
    }

    x.Add(deltaX);
  }

  return world;
}

var world = null;

var Box2DBenchmark = new BenchmarkSuite('Box2D', [5432788],
                                        [new Benchmark('Box2D',
													   false, 
													   false,
                                                       runBox2D,
                                                       setupBox2D,
                                                       tearDownBox2D,
                                                       null,
                                                       8)]);

function runBox2D() {
  var world = MakeNewWorld();
  for (var i = 0; i < 20; i++) {
    world.Step(1 / 60, 10, 3);
  }
}

function setupBox2D() {
}

function tearDownBox2D() {
  world = null;
  Box2D = null;
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

