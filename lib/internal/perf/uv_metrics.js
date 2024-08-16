'use strict';

const binding = internalBinding('performance');

function uvMetricsInfo() {
  return binding.uvMetricsInfo();
}

module.exports = {
  uvMetricsInfo,
};
