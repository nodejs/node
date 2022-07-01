'use strict';

const { TapParser } = require('internal/test_runner/tap_parser');

class TAPValidationError extends Error {
  constructor(message) {
    super(message);
    this.name = 'TAPValidationError';
  }
}

class TAPValidationStrategy {
  validate(ast) {
    console.log(JSON.stringify(ast, null, 2));

    this.#validateDocument(ast);
    this.#validateVersion(ast);
    this.#validatePlan(ast);
    this.#validateTestPoints(ast);

    return true;
  }

  #validateDocument(ast) {
    const { documents } = ast.root;

    if (documents.length > 1) {
      throw new TAPValidationError('Found more than one TAP documents');
    }
  }

  #validateVersion(ast) {
    const { version } = ast.root.documents[0];

    // TAP14 specification is compatible with observed behavior of existing TAP13 consumers and producers
    if (version !== '14' && version !== '13') {
      throw new TAPValidationError('TAP version should be 14');
    }
  }

  #validatePlan(ast) {
    const { plan } = ast.root.documents[0];

    if (!plan) {
      throw new TAPValidationError('Missing Plan');
    }

    if (!plan.start) {
      throw new TAPValidationError('Missing Plan start');
    }

    if (!plan.end) {
      throw new TAPValidationError('Missing Plan end');
    }

    const planStart = parseInt(plan.start, 10);
    const planEnd = parseInt(plan.end, 10);

    if (planEnd !== 0 && planStart > planEnd) {
      throw new TAPValidationError(
        `Plan start ${planStart} is greater than Plan end ${planEnd}`
      );
    }
  }

  #validateTestPoints(ast) {
    const { tests, plan, bailout } = ast.root.documents[0];
    const planStart = parseInt(plan.start, 10);
    const planEnd = parseInt(plan.end, 10);

    if (planEnd === 0 && tests && tests.length > 0) {
      throw new TAPValidationError(
        `Found ${tests.length} Test Point${
          tests.length > 1 ? 's' : ''
        } but Plan is ${planStart}..0`
      );
    }

    if (planEnd > 0) {
      if (!tests || tests.length === 0) {
        throw new TAPValidationError('Missing Test Points');
      }

      if (!bailout && tests.length !== planEnd) {
        throw new TAPValidationError(
          `Test Points count ${tests.length} does not match Plan count ${planEnd}`
        );
      }

      for (let i = 0; i < tests.length; i++) {
        const test = tests.at(i);
        const testId = parseInt(test.id, 10);

        if (testId < planStart || testId > planEnd) {
          throw new TAPValidationError(
            `Test ${testId} is out of Plan range ${planStart}..${planEnd}`
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
  constructor({ specs }) {
    switch (specs) {
      case '13':
        this.strategy = new TAP13ValidationStrategy();
        break;
      case '14':
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
