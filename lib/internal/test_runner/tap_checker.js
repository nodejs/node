'use strict';

const { ArrayPrototypeFilter, ArrayPrototypeFind, NumberParseInt } =
  primordials;
const {
  codes: { ERR_TAP_VALIDATOR_ERROR },
} = require('internal/errors');
const { TokenKind } = require('internal/test_runner/tap_lexer');

// TODO(@manekinekko): add more validation rules based on the TAP14 spec.
// See https://testanything.org/tap-version-14-specification.html
class TAPValidationStrategy {
  validate(ast) {
    this.#validateVersion(ast);
    this.#validatePlan(ast);
    this.#validateTestPoints(ast);

    return true;
  }

  #validateVersion(ast) {
    const entry = ArrayPrototypeFind(
      ast,
      (node) => node.kind === TokenKind.TAP_VERSION
    );

    if (!entry) {
      throw new ERR_TAP_VALIDATOR_ERROR('missing TAP version');
    }

    const { version } = entry.node;

    // TAP14 specification is compatible with observed behavior of existing TAP13 consumers and producers
    if (version !== '14' && version !== '13') {
      throw new ERR_TAP_VALIDATOR_ERROR('TAP version should be 13 or 14');
    }
  }

  #validatePlan(ast) {
    const entry = ArrayPrototypeFind(
      ast,
      (node) => node.kind === TokenKind.TAP_PLAN
    );

    if (!entry) {
      throw new ERR_TAP_VALIDATOR_ERROR('missing TAP plan');
    }

    const { plan } = entry.node;

    if (!plan.start) {
      throw new ERR_TAP_VALIDATOR_ERROR('missing plan start');
    }

    if (!plan.end) {
      throw new ERR_TAP_VALIDATOR_ERROR('missing plan end');
    }

    const planStart = NumberParseInt(plan.start, 10);
    const planEnd = NumberParseInt(plan.end, 10);

    if (planEnd !== 0 && planStart > planEnd) {
      throw new ERR_TAP_VALIDATOR_ERROR(
        `plan start ${planStart} is greater than plan end ${planEnd}`
      );
    }
  }

  // TODO(@manekinekko): since we are dealing with a flat AST, we need to
  // validate test points grouped by their "nesting" level. This is because a set of
  // Test points belongs to a TAP document. Each new subtest block creates a new TAP document.
  // https://testanything.org/tap-version-14-specification.html#subtests
  #validateTestPoints(ast) {
    const bailoutEntry = ArrayPrototypeFind(
      ast,
      (node) => node.kind === TokenKind.TAP_BAIL_OUT
    );
    const planEntry = ArrayPrototypeFind(
      ast,
      (node) => node.kind === TokenKind.TAP_PLAN
    );
    const testPointEntries = ArrayPrototypeFilter(
      ast,
      (node) =>
        node.kind === TokenKind.TAP_TEST_OK ||
        node.kind === TokenKind.TAP_TEST_NOTOK
    );

    const { plan } = planEntry.node;

    const planStart = NumberParseInt(plan.start, 10);
    const planEnd = NumberParseInt(plan.end, 10);

    if (planEnd === 0 && testPointEntries.length > 0) {
      throw new ERR_TAP_VALIDATOR_ERROR(
        `found ${testPointEntries.length} Test Point${
          testPointEntries.length > 1 ? 's' : ''
        } but plan is ${planStart}..0`
      );
    }

    if (planEnd > 0) {
      if (testPointEntries.length === 0) {
        throw new ERR_TAP_VALIDATOR_ERROR('missing Test Points');
      }

      if (!bailoutEntry && testPointEntries.length !== planEnd) {
        throw new ERR_TAP_VALIDATOR_ERROR(
          `test Points count ${testPointEntries.length} does not match plan count ${planEnd}`
        );
      }

      for (let i = 0; i < testPointEntries.length; i++) {
        const { test } = testPointEntries[i].node;
        const testId = NumberParseInt(test.id, 10);

        if (testId < planStart || testId > planEnd) {
          throw new ERR_TAP_VALIDATOR_ERROR(
            `test ${testId} is out of plan range ${planStart}..${planEnd}`
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
