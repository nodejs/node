'use strict';

const common = require('../common');

// TODO(@anonrig): Remove this check when Ada removes ICU requirement.
if (!common.hasIntl) {
  // A handful of the benchmarks fail when ICU is not included.
  // ICU is responsible for ignoring certain inputs from the hostname
  // and without it, it is not possible to validate the correctness of the input.
  // DomainToASCII method in Unicode specification states which characters are
  // ignored and/or remapped. Doing this outside of the scope of DomainToASCII,
  // would be a violation of the WHATWG URL specification.
  // Please look into: https://unicode.org/reports/tr46/#ProcessingStepMap
  common.skip('missing Intl');
}

const runBenchmark = require('../common/benchmark');

runBenchmark('url', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
