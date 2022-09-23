'use strict';

const Transform = require('internal/streams/transform');
const { TapLexer, TokenKind } = require('internal/test_runner/tap_lexer');
const { TapChecker } = require('internal/test_runner/tap_checker');
const {
  codes: { ERR_TAP_PARSER_ERROR },
} = require('internal/errors');

/**
 *
 * TAP14 specifications
 *
 * See https://testanything.org/tap-version-14-specification.html
 *
 * Note that the following grammar is intended as a rough "pseudocode" guidance.
 * It is not strict EBNF:
 *
 * TAPDocument := Version Plan Body | Version Body Plan
 * Version     := "TAP version 14\n"
 * Plan        := "1.." (Number) (" # " Reason)? "\n"
 * Body        := (TestPoint | BailOut | Pragma | Comment | Anything | Empty | Subtest)*
 * TestPoint   := ("not ")? "ok" (" " Number)? ((" -")? (" " Description) )? (" " Directive)? "\n" (YAMLBlock)?
 * Directive   := " # " ("todo" | "skip") (" " Reason)?
 * YAMLBlock   := "  ---\n" (YAMLLine)* "  ...\n"
 * YAMLLine    := "  " (YAML)* "\n"
 * BailOut     := "Bail out!" (" " Reason)? "\n"
 * Reason      := [^\n]+
 * Pragma      := "pragma " [+-] PragmaKey "\n"
 * PragmaKey   := ([a-zA-Z0-9_-])+
 * Subtest     := ("# Subtest" (": " SubtestName)?)? "\n" SubtestDocument TestPoint
 * Comment     := ^ (" ")* "#" [^\n]* "\n"
 * Empty       := [\s\t]* "\n"
 * Anything    := [^\n]+ "\n"
 *
 */

/**
 * An LL(1) parser for TAP14/TAP13.
 */
class TapParser extends Transform {
  #checker = null;
  #lexer = null;
  #tokens = [[]];
  #currentTokenIndex = 0;
  #currentTokenChunk = 0;
  #currentToken = null;
  #output = {};
  #documents = [];
  #subTestLevel = 0;
  #yamlBlockBuffer = [];
  #isYAMLBlock = false;
  #kSubtestBlockIndentationFactor = 4;
  #kYamlIndentationFactor = 2;
  #isAsyncParsingEnabled = false;

  // Use this stack to keep track of the beginning of each subtest
  // the top of the stack is the current subtest.
  // Every time we enter a subtest, we push a new subtest onto the stack
  // when the stack is empty, we assume all subtests have been terminated.
  #subtestsStack = [];

  constructor({ specs = TapChecker.TAP13 } = {}) {
    super({ __proto__: null, readableObjectMode: true });

    this.#checker = new TapChecker({ specs });
  }

  // ----------------------------------------------------------------------//
  // ----------------------------- Public API -----------------------------//
  // ----------------------------------------------------------------------//

  async parseAsync(chunk, callback = null) {
    this.#isAsyncParsingEnabled = true;
    this.#currentTokenChunk = 0;
    this.#currentTokenIndex = 0;
    this.input = chunk;
    this.#lexer = new TapLexer(chunk);

    try {
      this.#tokens = this.#makeChunks(this.#lexer.scan());
      this.#parseTokens(callback);
    } catch (error) {
      callback(null, error);
    }
  }

  parse(input, callback = null) {
    this.#isAsyncParsingEnabled = false;
    this.input = input;
    this.#lexer = new TapLexer(input);
    this.#tokens = this.#makeChunks(this.#lexer.scan());
    this.#parseTokens(callback);

    if (this.#isYAMLBlock) {
      // Looks like we have a non-ending YAML block
      this.#error('Expected end of YAML block', this.#tokens.at(-1).at(-1));
    }

    this.#output = {
      root: { ...this.#documents },
    };

    return this.#output;
  }

  // Check if the TAP content is semantically valid
  check() {
    return this.#checker.check(this.#output);
  }

  // ----------------------------------------------------------------------//
  // --------------------------- Transform API ----------------------------//
  // ----------------------------------------------------------------------//

  _transform(chunk, _encoding, next) {
    const line = chunk.toString('utf-8');
    this.parseAsync(line, (ast, error) => {
      if (error) {
        this.emit('error', error);
      } else {
        this.push({
          __proto__: null,
          ...ast,
        });
      }
    });
    next();
  }

  // ----------------------------------------------------------------------//
  // ----------------------------- Private API ----------------------------//
  // ----------------------------------------------------------------------//

  #parseTokens(callback = null) {
    this.#tokens.forEach((chunk) => this.#parseChunk(chunk, callback));
  }

  #parseChunk(chunk, callback = null) {
    this.#subTestLevel = this.#getCurrentIndentationLevel();
    // If the chunk starts with a multiple of 4 whitespace tokens,
    // then it's a subtest
    if (this.#subTestLevel > 0) {
      // Remove the indentation block from the current chunk
      // but only if the chunk is not a YAML block

      if (!this.#isYAMLBlock) {
        chunk = chunk.slice(
          this.#subTestLevel * this.#kSubtestBlockIndentationFactor
        );
      }
    }

    const astNode = this.#TAPDocument(chunk);

    callback?.({
      ...astNode,
      nesting: this.#subTestLevel,
      lexeme: chunk.map((token) => token.value).join(''),
      tokens: chunk,
    });

    // Move pointers to the next chunk and reset the current token index
    this.#currentTokenChunk++;
    this.#currentTokenIndex = 0;
  }

  #error(message, token, received = null) {
    if (this.#isAsyncParsingEnabled) {
      // when async parsing is enabled, don't throw.
      // Unrecognized tokens would be ignored.
      return;
    }

    throw new ERR_TAP_PARSER_ERROR(
      message,
      ', received ' + (received || `"${token.value}" (${token.kind})`),
      token,
      this.input
    );
  }

  #peek(shouldSkipBlankTokens = true) {
    if (shouldSkipBlankTokens) {
      this.#skip(TokenKind.WHITESPACE);
    }

    return this.#tokens[this.#currentTokenChunk][this.#currentTokenIndex];
  }

  #next(shouldSkipBlankTokens = true) {
    if (shouldSkipBlankTokens) {
      this.#skip(TokenKind.WHITESPACE);
    }

    if (this.#tokens[this.#currentTokenChunk]) {
      this.#currentToken =
        this.#tokens[this.#currentTokenChunk][this.#currentTokenIndex++];
    } else {
      this.#currentToken = null;
    }

    return this.#currentToken;
  }

  // Skip the provided tokens in the current chunk
  #skip(...tokensToSkip) {
    let token = this.#tokens[this.#currentTokenChunk][this.#currentTokenIndex];
    while (token && tokensToSkip.includes(token.kind)) {
      // pre-increment to skip current tokens but make sure we don't advance index on the last iteration
      token = this.#tokens[this.#currentTokenChunk][++this.#currentTokenIndex];
    }
  }

  #readNextLiterals() {
    const literals = [];
    let nextToken = this.#peek(false);

    // Read all literal, numeric, whitespace and escape tokens until we hit a different token
    // or reach end of current chunk
    while (
      nextToken &&
      [
        TokenKind.LITERAL,
        TokenKind.NUMERIC,
        TokenKind.DASH,
        TokenKind.PLUS,
        TokenKind.WHITESPACE,
        TokenKind.ESCAPE,
      ].includes(nextToken.kind)
    ) {
      const word = this.#next(false).value;

      // Don't output escaped characters
      if (nextToken.kind !== TokenKind.ESCAPE) {
        literals.push(word);
      }

      nextToken = this.#peek(false);
    }

    return literals.join('');
  }

  // Split all tokens by EOL: [[token1, token2, ...], [token1, token2, ...]]
  // this would simplify the parsing logic
  #makeChunks(tokens) {
    return [...tokens]
      .reduce(
        (acc, token) => {
          if (token.kind === TokenKind.EOL) {
            acc.push([]);
          } else {
            acc[acc.length - 1].push(token);
          }
          return acc;
        },
        [[]]
      )
      .filter((chunk) => chunk.length > 0 && chunk[0].kind !== TokenKind.EOF);
  }

  #countNextSpaces() {
    const chunk = this.#tokens[this.#currentTokenChunk];

    // Count the number of whitespace tokens in the chunk, starting from the first token
    let whitespaceCount = 0;
    while (chunk?.[whitespaceCount]?.kind === TokenKind.WHITESPACE) {
      whitespaceCount++;
    }
    return whitespaceCount;
  }

  #getCurrentIndentationLevel() {
    const whitespaceCount = this.#countNextSpaces();
    const indentationFactor = this.#kSubtestBlockIndentationFactor;

    // If the number of whitespace tokens is an exact multiple of "indentationFactor"
    // then return the level of the indentation
    if (whitespaceCount % indentationFactor === 0) {
      return whitespaceCount / indentationFactor;
    }

    // Take into account any extra 2 whitespace related to a YAML block
    if (
      (whitespaceCount - this.#kYamlIndentationFactor) % indentationFactor ===
      0
    ) {
      return (
        (whitespaceCount - this.#kYamlIndentationFactor) / indentationFactor
      );
    }

    // 0 is the default root indentation level
    return 0;
  }

  // ----------------------------------------------------------------------//
  // ------------------------------ Visitors ------------------------------//
  // ----------------------------------------------------------------------//

  // Run a depth-first traversal of each node in the current AST
  // until we visit a certain indentation level.
  // we also create a new "documents" entry for each level we visit
  // this will be used to host the coming subtest entries
  #visit(node, currentLevel, fn, stopLevel = 0) {
    node.documents ||= [{}];

    if (currentLevel === stopLevel) {
      return fn(node);
    }

    for (const document of node.documents) {
      this.#visit(document, currentLevel - 1, fn, stopLevel);
    }
  }

  #visitTestPoint(value) {
    this.#visit(this.#documents, this.#subTestLevel, (node) => {
      // If we are at the parent level, check if the current test is terminating any
      // subtests that are still open
      if (this.#subtestsStack.length > 0) {
        // Peek most recent subtest on the stack
        const { name, level } = this.#subtestsStack.at(-1);

        // If the current test is terminating a subtest, then we need to close it
        /* eslint-disable brace-style */
        if (level === this.#subTestLevel + 1 && name === value.description) {
          // Terminate the most recent subtest
          this.#subtestsStack.pop();

          // Mark the subtest as terminated in the most recent child document
          node.documents.at(-1).documents.at(-1).terminated = true;

          // Create a sub documents entry in current documents for the next subtest (if any)
          // this will allow us to make sure any new subtests are created in the new document (context)
          node.documents.at(-1).documents.push({});

          // Add the test point entry to the most recent parent document
          node.documents.at(-1).tests ||= [];
          node.documents.at(-1).tests.push(value);
        }

        // If no subtest is terminating, then we need to add the test point to the most recent subtest
        else {
          node.documents.at(-1).tests ||= [];
          node.documents.at(-1).tests.push(value);
        }
      }
      // If no subtests were parsed, then we need to add the test point to the most recent document
      // this is the case when we are parsing a test that is at the root level
      else {
        node.documents.at(-1).tests ||= [];
        node.documents.at(-1).tests.push(value);
      }
    });
  }

  #visitPlan(value) {
    this.#visit(this.#documents, this.#subTestLevel, (node) => {
      node.documents.at(-1).plan = value;
    });
  }

  #visitVersion(value) {
    this.#visit(this.#documents, this.#subTestLevel, (node) => {
      node.documents.at(-1).version = value;
    });
  }

  #visitComment(value) {
    this.#visit(this.#documents, this.#subTestLevel, (node) => {
      node.documents.at(-1).comments ||= [];
      node.documents.at(-1).comments.push(value);
    });
  }

  #visitSubtestName(value) {
    this.#visit(
      this.#documents,
      this.#subTestLevel,
      (node) => {
        node.documents.at(-1).name = value;
        node.documents.at(-1).terminated = false;

        // We store the name of the coming subtest, and its level.
        // the subtest level is the level of the current indentation level + 1
        this.#subtestsStack.push({
          name: value,
          nesting: this.#subTestLevel + 1,
        });
      },
      // Subtest name declared in comment is usually encountered before the subtest block starts.
      // we need to emit the name on the node that comes after the current node.
      // this is why we set the stop level to -1.
      // This will allow us to create a new document node for the coming subtest.
      -1
    );
  }

  #visitPragma(value) {
    this.#visit(this.#documents, this.#subTestLevel, (node) => {
      node.documents.at(-1).pragmas ||= {};
      node.documents.at(-1).pragmas = {
        ...node.documents.at(-1).pragmas,
        ...value,
      };
    });
  }

  #visitYAMLBlock(value) {
    this.#visit(this.#documents, this.#subTestLevel, (node) => {
      node.documents.at(-1).tests ||= [{}];
      node.documents.at(-1).tests.at(-1).diagnostics = value;
    });
  }

  #visitBailout(value) {
    this.#visit(this.#documents, this.#subTestLevel, (node) => {
      node.documents.at(-1).bailout = value;
    });
  }

  // --------------------------------------------------------------------------//
  // ------------------------------ Parser rules ------------------------------//
  // --------------------------------------------------------------------------//

  // TAPDocument := Version Plan Body | Version Body Plan
  #TAPDocument(tokenChunks) {
    const tokenChunksAsString = tokenChunks
      .map((token) => token.value)
      .join('');
    const firstToken = tokenChunks[0];
    const { kind } = firstToken;

    switch (kind) {
      case TokenKind.TAP:
        return this.#Version();
      case TokenKind.NUMERIC:
        return this.#Plan();
      case TokenKind.TAP_TEST_OK:
      case TokenKind.TAP_TEST_NOTOK:
        return this.#TestPoint();
      case TokenKind.COMMENT:
      case TokenKind.HASH:
        return this.#Comment();
      case TokenKind.TAP_PRAGMA:
        return this.#Pragma();
      case TokenKind.WHITESPACE:
        return this.#YAMLBlock();
      case TokenKind.LITERAL:
        // Check for "Bail out!" literal (case insensitive)
        if (/^Bail\s+out!/i.test(tokenChunksAsString)) {
          return this.#Bailout();
        }

        this.#error('Expected a valid token', firstToken);

        break;
      default:
      this.#error('Expected a valid token', firstToken);
    }

    return {
      kind: TokenKind.UNKNOWN,
      node: {
        value: tokenChunksAsString,
      },
    };
  }

  // ----------------Version----------------
  // Version := "TAP version Number\n"
  #Version() {
    const tapToken = this.#next();

    if (tapToken.kind !== TokenKind.TAP) {
      this.#error('Expected "TAP" keyword', tapToken);
    }

    const versionToken = this.#next();
    if (versionToken.kind !== TokenKind.TAP_VERSION) {
      this.#error('Expected "version" keyword', versionToken);
    }

    const numberToken = this.#next();
    if (numberToken.kind !== TokenKind.NUMERIC) {
      this.#error('Expected a version number', numberToken);
    }

    const version = numberToken.value;
    this.#visitVersion(version);

    return { kind: TokenKind.TAP_VERSION, node: { version } };
  }

  // ----------------Plan----------------
  // Plan := "1.." (Number) (" # " Reason)? "\n"
  #Plan() {
    // Even if specs mention plan starts at 1, we need to make sure we read the plan start value
    // in case of a missing or invalid plan start value
    const planStart = this.#next();

    if (planStart.kind !== TokenKind.NUMERIC) {
      this.#error('Expected a plan start count', planStart);
    }

    const planToken = this.#next();
    if (planToken.kind !== TokenKind.TAP_PLAN) {
      this.#error('Expected ".." symbol', planToken);
    }

    const planEnd = this.#next();
    if (planEnd.kind !== TokenKind.NUMERIC) {
      this.#error('Expected a plan end count', planEnd);
    }

    const plan = {
      start: planStart.value,
      end: planEnd.value,
    };

    // Read optional reason
    const hashToken = this.#peek();
    if (hashToken) {
      if (hashToken.kind === TokenKind.HASH) {
        this.#next(); // skip hash
        plan.reason = this.#readNextLiterals().trim();
      } else if (hashToken.kind === TokenKind.LITERAL) {
        this.#error('Expected "#" symbol before a reason', hashToken);
      }
    }

    this.#visitPlan(plan);

    return {
      kind: TokenKind.TAP_PLAN,
      node: {
        plan,
      },
    };
  }

  // ----------------TestPoint----------------
  // TestPoint := ("not ")? "ok" (" " Number)? ((" -")? (" " Description) )? (" " Directive)? "\n" (YAMLBlock)?
  // Directive   := " # " ("todo" | "skip") (" " Reason)?
  // YAMLBlock   := "  ---\n" (YAMLLine)* "  ...\n"
  // YAMLLine    := "  " (YAML)* "\n"

  // Test Status: ok/not ok (required)
  // Test number (recommended)
  // Description (recommended, prefixed by " - ")
  // Directive (only when necessary)
  #TestPoint() {
    const notToken = this.#peek();
    let isTestFailed = false;

    if (notToken.kind === TokenKind.TAP_TEST_NOTOK) {
      this.#next(); // skip "not" token
      isTestFailed = true;
    }

    const okToken = this.#next();
    if (okToken.kind !== TokenKind.TAP_TEST_OK) {
      this.#error('Expected "ok" or "not ok" keyword', okToken);
    }

    // Read optional test number
    let numberToken = this.#peek();
    if (numberToken && numberToken.kind === TokenKind.NUMERIC) {
      numberToken = this.#next().value;
    } else {
      numberToken = ''; // Set an empty ID to indicate that the test hasn't provider an ID
    }

    const test = {
      // Output both failed and passed properties to make it easier for the checker to detect the test status
      status: {
        fail: isTestFailed,
        pass: !isTestFailed,
        todo: false,
        skip: false,
      },
      id: numberToken,
      description: '',
      reason: '',
    };

    // Read optional description prefix " - "
    const descriptionDashToken = this.#peek();
    if (descriptionDashToken && descriptionDashToken.kind === TokenKind.DASH) {
      this.#next(); // skip dash
    }

    // Read optional description
    if (this.#peek()) {
      const description = this.#readNextLiterals().trim();
      if (description) {
        test.description = description;
      }
    }

    // Read optional directive and reason
    const hashToken = this.#peek();
    if (hashToken && hashToken.kind === TokenKind.HASH) {
      this.#next(); // skip hash
    }

    let todoOrSkipToken = this.#peek();
    if (todoOrSkipToken && todoOrSkipToken.kind === TokenKind.LITERAL) {
      if (/todo/i.test(todoOrSkipToken.value)) {
        todoOrSkipToken = 'todo';
        this.#next(); // skip token
      } else if (/skip/i.test(todoOrSkipToken.value)) {
        todoOrSkipToken = 'skip';
        this.#next(); // skip token
      }
    }

    const reason = this.#readNextLiterals().trim();
    if (todoOrSkipToken) {
      if (reason) {
        test.reason = reason;
      }

      test.status.todo = todoOrSkipToken === 'todo';
      test.status.skip = todoOrSkipToken === 'skip';
    }

    this.#visitTestPoint(test);

    return {
      kind: isTestFailed ? TokenKind.TAP_TEST_NOTOK : TokenKind.TAP_TEST_OK,
      node: {
        test,
      },
    };
  }

  // ----------------Bailout----------------
  // BailOut := "Bail out!" (" " Reason)? "\n"
  #Bailout() {
    this.#next(); // skip "Bail"
    this.#next(); // skip "out!"

    // Read optional reason
    const hashToken = this.#peek();
    if (hashToken && hashToken.kind === TokenKind.HASH) {
      this.#next(); // skip hash
    }

    const reason = this.#readNextLiterals().trim();
    this.#visitBailout(reason);

    return {
      kind: TokenKind.TAP_BAILOUT,
      node: { bailout: true, reason },
    };
  }

  // ----------------Comment----------------
  // Comment := ^ (" ")* "#" [^\n]* "\n"
  #Comment() {
    const commentToken = this.#next();
    if (
      commentToken.kind !== TokenKind.COMMENT &&
      commentToken.kind !== TokenKind.HASH
    ) {
      this.#error('Expected "#" symbol', commentToken);
    }

    const subtestKeyword = this.#peek();
    if (subtestKeyword) {
      if (/^Subtest:/i.test(subtestKeyword.value)) {
        this.#next(); // skip subtest keyword
        const subtestNameLiteral = this.#readNextLiterals().trim();
        this.#visitSubtestName(subtestNameLiteral);
        return {
          kind: TokenKind.TAP_SUBTEST_POINT,
          node: {
            name: subtestNameLiteral,
          },
        };
      } else {
        const commentLiteral = this.#readNextLiterals().trim();
        this.#visitComment(commentLiteral);
        return {
          kind: TokenKind.COMMENT,
          node: { comment: commentLiteral },
        };
      }
    }

    return;
  }

  // ----------------YAMLBlock----------------
  // YAMLBlock   := "  ---\n" (YAMLLine)* "  ...\n"
  // YAMLLine    := "  " (YAML)* "\n"
  #YAMLBlock() {
    const space1 = this.#peek(false);
    if (space1 && space1.kind === TokenKind.WHITESPACE) {
      this.#next(false); // skip 1st space
    }

    const space2 = this.#peek(false);
    if (space2 && space2.kind === TokenKind.WHITESPACE) {
      this.#next(false); // skip 2nd space
    }

    const yamlBlockToken = this.#peek(); // peek and skip blank token
    if (yamlBlockToken.kind === TokenKind.TAP_YAML_START) {
      if (this.#isYAMLBlock) {
        // Looks like we have another YAML start block, but we didn't close the previous one
        this.#error('Unexpected YAML start marker', yamlBlockToken);
      }

      this.#isYAMLBlock = true;
      this.#next(false); // skip "---"

      return {
        kind: TokenKind.TAP_YAML_START,
        node: {
          diagnostics: this.#yamlBlockBuffer,
        },
      };
    } else if (yamlBlockToken.kind === TokenKind.TAP_YAML_END) {
      if (!this.#isYAMLBlock) {
        // Looks like we have an YAML end block, but we didn't encounter any YAML start marker
        this.#error('Unexpected YAML end marker', yamlBlockToken);
      }

      this.#isYAMLBlock = false;
      this.#next(false); // skip "..."

      const diagnostics = this.#yamlBlockBuffer;
      this.#yamlBlockBuffer = []; // Free the buffer for the next YAML block

      this.#visitYAMLBlock(diagnostics);

      return {
        kind: TokenKind.TAP_YAML_END,
        node: {
          diagnostics,
        },
      };
    } else if (this.#isYAMLBlock) {
      // We are in a YAML block, read content as is and store it in the buffer
      // TODO(@manekinekko): should we use a YAML parser here?
      const yamlLiteral = this.#readNextLiterals();
      this.#yamlBlockBuffer.push(yamlLiteral);
    } else {
      // Check if this is a valid indentation level
      const spacesFound = this.#countNextSpaces();

      // valid indentation level should be a multiple of 6 (2 yaml spaces + 4 subtest spaces)
      if (
        spacesFound %
          (this.#kYamlIndentationFactor +
            this.#kSubtestBlockIndentationFactor) !==
        0
      ) {
        this.#error(
          `Expected valid YAML indentation (${
            this.#kYamlIndentationFactor
          } spaces)`,
          this.#tokens[this.#currentTokenChunk][spacesFound - 1],
          `${spacesFound} space` + (spacesFound > 1 ? 's' : '')
        );
      }

      return {
        kind: TokenKind.UNKNOWN,
        node: {
          value: this.#yamlBlockBuffer.join('\n'),
        },
      };
    }
  }

  // ----------------PRAGMA----------------
  // Pragma := "pragma " [+-] PragmaKey "\n"
  // PragmaKey := ([a-zA-Z0-9_-])+
  // TODO(@manekinekko): pragmas are parsed but not used yet! TapChecker() should take care of that.
  #Pragma() {
    const pragmaToken = this.#next();
    if (pragmaToken.kind !== TokenKind.TAP_PRAGMA) {
      this.#error('Expected "pragma" keyword', pragmaToken);
    }

    const pragmas = {};

    let nextToken = this.#peek();
    while (
      nextToken &&
      [TokenKind.EOL, TokenKind.EOF].includes(nextToken.kind) === false
    ) {
      let isEnabled = true;
      const pragmaKeySign = this.#next();
      if (pragmaKeySign.kind === TokenKind.PLUS) {
        isEnabled = true;
      } else if (pragmaKeySign.kind === TokenKind.DASH) {
        isEnabled = false;
      } else {
        this.#error('Expected "+" or "-" before pragma keys', pragmaKeySign);
      }

      const pragmaKeyToken = this.#peek();
      if (pragmaKeyToken.kind !== TokenKind.LITERAL) {
        this.#error('Expected pragma key', pragmaKeyToken);
      }

      let pragmaKey = this.#next().value;

      // In some cases, pragma key can be followed by a comma separator,
      // so we need to remove it
      pragmaKey = pragmaKey.replace(/,/g, '');

      pragmas[pragmaKey] = isEnabled;
      nextToken = this.#peek();
    }

    this.#visitPragma(pragmas);
  }
}

module.exports = { TapParser };
