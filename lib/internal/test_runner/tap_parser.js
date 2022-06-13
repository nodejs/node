/** TAP14 specifications

See https://testanything.org/tap-version-14-specification.html

Grammar:

TAPDocument := Version Plan Body | Version Body Plan
Version     := "TAP version 14\n"
Plan        := "1.." (Number) (" # " Reason)? "\n"
Body        := (TestPoint | BailOut | Pragma | Comment | Anything | Empty | Subtest)*
TestPoint   := ("not ")? "ok" (" " Number)? ((" -")? (" " Description) )? (" " Directive)? "\n" (YAMLBlock)?
Directive   := " # " ("todo" | "skip") (" " Reason)?
YAMLBlock   := "  ---\n" (YAMLLine)* "  ...\n"
YAMLLine    := "  " (YAML)* "\n"
BailOut     := "Bail out!" (" " Reason)? "\n"
Reason      := [^\n]+
Pragma      := "pragma " [+-] PragmaKey "\n"
PragmaKey   := ([a-zA-Z0-9_-])+
Subtest     := ("# Subtest" (": " SubtestName)?)? "\n" SubtestDocument TestPoint
Comment     := ^ (" ")* "#" [^\n]* "\n"
Empty       := [\s\t]* "\n"
Anything    := [^\n]+ "\n"

*/

const console = require('console');
const { TapLexer } = require('./tap_lexer');

class TapParser {
  constructor(input) {
    this.lexer = new TapLexer(input);
    this.tokens = [...this.lexer.scanAll()];

    console.log(this.tokens);

    this.currentTokenIndex = 0;
    this.currentToken = null;
  }

  next() {
    this.currentToken = this.tokens[this.currentTokenIndex++];
    console.log({token: this.currentToken})
    return this.currentToken;
  }

  peek() {
    return this.tokens[this.currentTokenIndex];
  }

  // TAPDocument := Version Plan Body | Version Body Plan
  parse() {
    return [this.Version(), this.Plan(), this.Body()];
  }

  // Version := "TAP version 14\n"
  Version() {
    console.log('----------------Version----------------');

    const versionToken = this.next();
    if (versionToken.type !== TapLexer.Tokens.TAP_VERSION) {
      this.lexer.error(`Expected "TAP version"`);
    }

    const numberToken = this.next();
    if (numberToken.type !== TapLexer.Tokens.NUMERIC) {
      this.lexer.error(`Expected Numeric`);
    }

    return {
      type: 'Version',
      body: [versionToken, numberToken],
    };
  }

  // Plan := "1.." (Number) (" # " Reason)? "\n"
  Plan() {
    console.log('----------------Plan----------------');

    const planStart = this.next();
    if (planStart.type !== TapLexer.Tokens.NUMERIC) {
      this.lexer.error(`Expected a Numeric`);
    }

    const planToken = this.next();
    if (planToken.type !== TapLexer.Tokens.TAP_PLAN) {
      this.lexer.error(`Expected ".."`);
    }

    const planEnd = this.next();
    if (planEnd.type !== TapLexer.Tokens.NUMERIC) {
      this.lexer.error(`Expected a Numeric`);
    }

    const body = [planStart, planEnd];

    // Read optional reason
    const reason = this.peek();
    if (reason.type === TapLexer.Tokens.COMMENT) {
      reason.value = reason.value;
      body.push(reason);
      this.next();
    }

    return {
      type: 'Plan',
      body,
    };
  }

  // Body := (TestPoint | BailOut | Pragma | Comment | Anything | Empty | Subtest)*
  Body() {
    console.log('----------------Body----------------');

    const body = [];

    while (this.peek()) {
      const testPoint = this.TestPoint();
      if (testPoint) {
        body.push(testPoint);
      }
    }

    return {
      type: 'Body',
      body
    };
  }

  // TestPoint := ("not ")? "ok" (" " Number)? ((" -")? (" " Description) )? (" " Directive)? "\n" (YAMLBlock)?

  // Test Status: ok/not ok (required)
  // Test number (recommended)
  // Description (recommended, prefixed by " - ")
  // Directive (only when necessary)
  TestPoint() {
    console.log('----------------TestPoint----------------');

    const notOrOkToken = this.next();
    if (notOrOkToken.type !== TapLexer.Tokens.TAP_TEST_OK && notOrOkToken.type !== TapLexer.Tokens.TAP_TEST_NOTOK) {
      this.lexer.error(`Expected "ok" or "not ok"`);
    }

    const numberToken = this.next();
    if (numberToken.type !== TapLexer.Tokens.NUMERIC) {
      this.lexer.error(`Expected Numeric`);
    }

    const body = [notOrOkToken, numberToken];

    // Read optional description
    const descriptionToken = this.peek();
    if (descriptionToken.type === TapLexer.Tokens.COMMENT) {
      descriptionToken.value = descriptionToken.value
      body.push(descriptionToken);
      this.next();
    }

    // Read optional directive
    const reasonToken = this.peek();
    if (reasonToken.type === TapLexer.Tokens.COMMENT) {
      reasonToken.value = reasonToken.value;
      body.push(reasonToken);
      this.next();
    }

    return {
      type: 'TestPoint',
      body,
    };
  }
}

module.exports = { TapParser };
