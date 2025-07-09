'use strict';

const runtimeConditionHandlers = new Set();

function registerCondition(handler) {
  runtimeConditionHandlers.add(handler);
}

function applyRuntimeConditions(nmPath, request, conditions) {
  for (const handler of runtimeConditionHandlers) {
    handler({ nmPath, request, conditions });
  }
}

module.exports = {
  registerCondition,
  applyRuntimeConditions,
};
