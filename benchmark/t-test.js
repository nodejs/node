'use strict';

// Class for doing statistic test following a Student's t-distribution,
// with default configuration: mu = 0, alternative = two.sided
function TTest(leftData, rightData) {
  this.left = this.createData(leftData);
  this.right = this.createData(rightData);
  this.df = this.left.size + this.right.size - 2;
  this.dfhalf = this.df / 2;
  this.mean = this.left.mean - this.right.mean;
  this.commonVariance = ((this.left.size - 1) * this.left.variance +
                         (this.right.size - 1) * this.right.variance) / this.df;
  this.se = Math.sqrt(this.commonVariance * (1 / this.left.size + 1 /
                      this.right.size));
}

TTest.prototype.createData = function(data) {
  // Calculate the sum
  var sum = 0;
  var i;
  for (i = 0; i < data.length; i++) {
    sum += data[i];
  }
  // Calculate the mean.
  var mean = sum / data.length;
  // Calculate the sqsum.
  var sqsum = 0;
  for (i = 0; i < data.length; i++) {
    sqsum += (data[i] - mean) * (data[i] - mean);
  }
  // Calculate the variance
  var variance = sqsum / (data.length - 1);
  // Return data.
  return {
    sum: sum,
    mean: mean,
    sqsum: sqsum,
    variance: variance,
    data: data,
    size: data.length
  };
};

// Calculates a fast aproximation of the Gamma Function.
TTest.prototype.gamma = function(x) {
  var p = [0.99999999999980993, 676.5203681218851, -1259.1392167224028,
           771.32342877765313, -176.61502916214059, 12.507343278686905,
           -0.13857109526572012, 9.9843695780195716e-6, 1.5056327351493116e-7];

  var g = 7;
  if (x < 0.5) {
    return Math.PI / (Math.sin(Math.PI * x) * this.gamma(1 - x));
  }

  x -= 1;
  var a = p[0];
  var t = x + g + 0.5;
  for (var i = 1; i < p.length; i++) {
    a += p[i] / (x + i);
  }

  return Math.sqrt(2 * Math.PI) * Math.pow(t, x + 0.5) * Math.exp(-t) * a;
};

TTest.prototype.logGamma = function(n) {
  return Math.log(Math.abs(this.gamma(n)));
};

TTest.prototype.log1p = function(n) {
  return Math.log(n + 1);
};

TTest.prototype.betacf = function(x, a, b) {
  var fpmin = 1e-30;
  var c = 1;
  var d = 1 - (a + b) * x / (a + 1);
  var m2, aa, del, h;
  if (Math.abs(d) < fpmin) d = fpmin;
  d = 1 / d;
  h = d;
  for (var m = 1; m <= 100; m++) {
    m2 = 2 * m;
    aa = m * (b - m) * x / ((a - 1 + m2) * (a + m2));
    d = 1 + aa * d;
    if (Math.abs(d) < fpmin) d = fpmin;
    c = 1 + aa / c;
    if (Math.abs(c) < fpmin) c = fpmin;
    d = 1 / d;
    h *= d * c;
    aa = -(a + m) * (a + b + m) * x / ((a + m2) * (a + 1 + m2));
    d = 1 + aa * d;
    if (Math.abs(d) < fpmin) d = fpmin;
    c = 1 + aa / c;
    if (Math.abs(c) < fpmin) c = fpmin;
    d = 1 / d;
    del = d * c;
    h *= del;
    if (Math.abs(del - 1.0) < 3e-7) break;
  }
  return h;
};

TTest.prototype.incBeta = function(x, a, b) {
  if ((a === 1 && b === 1) || (x === 0) || (x === 1))
    return x;
  if (a === 0)
    return 1;
  if (b === 0)
    return 0;
  var bt = Math.exp(this.logGamma(a + b) - this.logGamma(a) - this.logGamma(b) +
    a * Math.log(x) + b * this.log1p(-x));
  if (x < (a + 1) / (a + b + 2))
    return bt * this.betacf(x, a, b) / a;
  return 1 - bt * this.betacf(1 - x, b, a) / b;
};

TTest.prototype.cdf = function(x) {
  var fac = Math.sqrt(x * x + this.df);
  return this.incBeta((x + fac) / (2 * fac), this.dfhalf, this.dfhalf);
};

TTest.prototype.testValue = function() {
  return this.mean / this.se;
};

TTest.prototype.pValue = function() {
  return 2 * (1 - this.cdf(Math.abs(this.testValue())));
};

module.exports = TTest;
