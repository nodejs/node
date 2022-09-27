'use strict';

const { NumberParseInt } = primordials;
const {
  codes: { ERR_TAP_VALIDATOR_ERROR },
} = require('internal/errors');

class TAPValidationStrategy {
  validate(ast) {
    this.#validateDocument(ast);
    this.#validateVersion(ast);
    this.#validatePlan(ast);
    this.#validateTestPoints(ast);

    return true;
  }

  #validateDocument(ast) {
    const { documents } = ast.root;

    if (documents.length > 1) {
      throw new ERR_TAP_VALIDATOR_ERROR('found more than one TAP documents');
    }
  }

  #validateVersion(ast) {
    const { version } = ast.root.documents[0];

    // TAP14 specification is compatible with observed behavior of existing TAP13 consumers and producers
    if (version !== '14' && version !== '13') {
      throw new ERR_TAP_VALIDATOR_ERROR('TAP version should be 13 or 14');
    }
  }

  #validatePlan(ast) {
    const { plan } = ast.root.documents[0];

    if (!plan) {
      throw new ERR_TAP_VALIDATOR_ERROR('missing Plan');
    }

    if (!plan.start) {
      throw new ERR_TAP_VALIDATOR_ERROR('missing Plan start');
    }

    if (!plan.end) {
      throw new ERR_TAP_VALIDATOR_ERROR('missing Plan end');
    }

    const planStart = NumberParseInt(plan.start, 10);
    const planEnd = NumberParseInt(plan.end, 10);

    if (planEnd !== 0 && planStart > planEnd) {
      throw new ERR_TAP_VALIDATOR_ERROR(
        `plan start ${planStart} is greater than Plan end ${planEnd}`
      );
    }
  }

  #validateTestPoints(ast) {
    const { tests, plan, bailout } = ast.root.documents[0];
    const planStart = NumberParseInt(plan.start, 10);
    const planEnd = NumberParseInt(plan.end, 10);

    if (planEnd === 0 && tests && tests.length > 0) {
      throw new ERR_TAP_VALIDATOR_ERROR(
        `found ${tests.length} Test Point${
          tests.length > 1 ? 's' : ''
        } but Plan is ${planStart}..0`
      );
    }

    if (planEnd > 0) {
      if (!tests || tests.length === 0) {
        throw new ERR_TAP_VALIDATOR_ERROR('missing Test Points');
      }

      if (!bailout && tests.length !== planEnd) {
        throw new ERR_TAP_VALIDATOR_ERROR(
          `test Points count ${tests.length} does not match Plan count ${planEnd}`
        );
      }

      for (let i = 0; i < tests.length; i++) {
        const test = tests.at(i);
        const testId = NumberParseInt(test.id, 10);

        if (testId < planStart || testId > planEnd) {
          throw new ERR_TAP_VALIDATOR_ERROR(
            `test ${testId} is out of Plan range ${planStart}..${planEnd}`
          );
        }
      }
    }
  }
}

// TAP14 and TAP13 are compatible with each other
class TAP13ValidationStrategy extends TAPValidationStrategy {}
class TAP14ValidationStrategy extends TAPValidationStrategy {}

class TapChecker {
  static TAP13 = '13';
  static TAP14 = '14';

  constructor({ specs }) {
    switch (specs) {
      case TapChecker.TAP13:
        this.strategy = new TAP13ValidationStrategy();
        break;
      case TapChecker.TAP14:
        this.strategy = new TAP14ValidationStrategy();
        break;
      default:
        this.strategy = new TAP14ValidationStrategy();
    }
  }

  check(ast) {
    return this.strategy.validate(ast);
  }
}

module.exports = {
  TapChecker,
  TAP14ValidationStrategy,
  TAP13ValidationStrategy,
};
