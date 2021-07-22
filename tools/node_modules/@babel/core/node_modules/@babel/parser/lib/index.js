'use strict';

Object.defineProperty(exports, '__esModule', { value: true });

const beforeExpr = true;
const startsExpr = true;
const isLoop = true;
const isAssign = true;
const prefix = true;
const postfix = true;
class TokenType {
  constructor(label, conf = {}) {
    this.label = void 0;
    this.keyword = void 0;
    this.beforeExpr = void 0;
    this.startsExpr = void 0;
    this.rightAssociative = void 0;
    this.isLoop = void 0;
    this.isAssign = void 0;
    this.prefix = void 0;
    this.postfix = void 0;
    this.binop = void 0;
    this.updateContext = void 0;
    this.label = label;
    this.keyword = conf.keyword;
    this.beforeExpr = !!conf.beforeExpr;
    this.startsExpr = !!conf.startsExpr;
    this.rightAssociative = !!conf.rightAssociative;
    this.isLoop = !!conf.isLoop;
    this.isAssign = !!conf.isAssign;
    this.prefix = !!conf.prefix;
    this.postfix = !!conf.postfix;
    this.binop = conf.binop != null ? conf.binop : null;
    this.updateContext = null;
  }

}
const keywords$1 = new Map();

function createKeyword(name, options = {}) {
  options.keyword = name;
  const token = new TokenType(name, options);
  keywords$1.set(name, token);
  return token;
}

function createBinop(name, binop) {
  return new TokenType(name, {
    beforeExpr,
    binop
  });
}

const types$1 = {
  num: new TokenType("num", {
    startsExpr
  }),
  bigint: new TokenType("bigint", {
    startsExpr
  }),
  decimal: new TokenType("decimal", {
    startsExpr
  }),
  regexp: new TokenType("regexp", {
    startsExpr
  }),
  string: new TokenType("string", {
    startsExpr
  }),
  name: new TokenType("name", {
    startsExpr
  }),
  privateName: new TokenType("#name", {
    startsExpr
  }),
  eof: new TokenType("eof"),
  bracketL: new TokenType("[", {
    beforeExpr,
    startsExpr
  }),
  bracketHashL: new TokenType("#[", {
    beforeExpr,
    startsExpr
  }),
  bracketBarL: new TokenType("[|", {
    beforeExpr,
    startsExpr
  }),
  bracketR: new TokenType("]"),
  bracketBarR: new TokenType("|]"),
  braceL: new TokenType("{", {
    beforeExpr,
    startsExpr
  }),
  braceBarL: new TokenType("{|", {
    beforeExpr,
    startsExpr
  }),
  braceHashL: new TokenType("#{", {
    beforeExpr,
    startsExpr
  }),
  braceR: new TokenType("}", {
    beforeExpr
  }),
  braceBarR: new TokenType("|}"),
  parenL: new TokenType("(", {
    beforeExpr,
    startsExpr
  }),
  parenR: new TokenType(")"),
  comma: new TokenType(",", {
    beforeExpr
  }),
  semi: new TokenType(";", {
    beforeExpr
  }),
  colon: new TokenType(":", {
    beforeExpr
  }),
  doubleColon: new TokenType("::", {
    beforeExpr
  }),
  dot: new TokenType("."),
  question: new TokenType("?", {
    beforeExpr
  }),
  questionDot: new TokenType("?."),
  arrow: new TokenType("=>", {
    beforeExpr
  }),
  template: new TokenType("template"),
  ellipsis: new TokenType("...", {
    beforeExpr
  }),
  backQuote: new TokenType("`", {
    startsExpr
  }),
  dollarBraceL: new TokenType("${", {
    beforeExpr,
    startsExpr
  }),
  at: new TokenType("@"),
  hash: new TokenType("#", {
    startsExpr
  }),
  interpreterDirective: new TokenType("#!..."),
  eq: new TokenType("=", {
    beforeExpr,
    isAssign
  }),
  assign: new TokenType("_=", {
    beforeExpr,
    isAssign
  }),
  slashAssign: new TokenType("_=", {
    beforeExpr,
    isAssign
  }),
  incDec: new TokenType("++/--", {
    prefix,
    postfix,
    startsExpr
  }),
  bang: new TokenType("!", {
    beforeExpr,
    prefix,
    startsExpr
  }),
  tilde: new TokenType("~", {
    beforeExpr,
    prefix,
    startsExpr
  }),
  pipeline: createBinop("|>", 0),
  nullishCoalescing: createBinop("??", 1),
  logicalOR: createBinop("||", 1),
  logicalAND: createBinop("&&", 2),
  bitwiseOR: createBinop("|", 3),
  bitwiseXOR: createBinop("^", 4),
  bitwiseAND: createBinop("&", 5),
  equality: createBinop("==/!=/===/!==", 6),
  relational: createBinop("</>/<=/>=", 7),
  bitShift: createBinop("<</>>/>>>", 8),
  plusMin: new TokenType("+/-", {
    beforeExpr,
    binop: 9,
    prefix,
    startsExpr
  }),
  modulo: new TokenType("%", {
    beforeExpr,
    binop: 10,
    startsExpr
  }),
  star: new TokenType("*", {
    binop: 10
  }),
  slash: createBinop("/", 10),
  exponent: new TokenType("**", {
    beforeExpr,
    binop: 11,
    rightAssociative: true
  }),
  _break: createKeyword("break"),
  _case: createKeyword("case", {
    beforeExpr
  }),
  _catch: createKeyword("catch"),
  _continue: createKeyword("continue"),
  _debugger: createKeyword("debugger"),
  _default: createKeyword("default", {
    beforeExpr
  }),
  _do: createKeyword("do", {
    isLoop,
    beforeExpr
  }),
  _else: createKeyword("else", {
    beforeExpr
  }),
  _finally: createKeyword("finally"),
  _for: createKeyword("for", {
    isLoop
  }),
  _function: createKeyword("function", {
    startsExpr
  }),
  _if: createKeyword("if"),
  _return: createKeyword("return", {
    beforeExpr
  }),
  _switch: createKeyword("switch"),
  _throw: createKeyword("throw", {
    beforeExpr,
    prefix,
    startsExpr
  }),
  _try: createKeyword("try"),
  _var: createKeyword("var"),
  _const: createKeyword("const"),
  _while: createKeyword("while", {
    isLoop
  }),
  _with: createKeyword("with"),
  _new: createKeyword("new", {
    beforeExpr,
    startsExpr
  }),
  _this: createKeyword("this", {
    startsExpr
  }),
  _super: createKeyword("super", {
    startsExpr
  }),
  _class: createKeyword("class", {
    startsExpr
  }),
  _extends: createKeyword("extends", {
    beforeExpr
  }),
  _export: createKeyword("export"),
  _import: createKeyword("import", {
    startsExpr
  }),
  _null: createKeyword("null", {
    startsExpr
  }),
  _true: createKeyword("true", {
    startsExpr
  }),
  _false: createKeyword("false", {
    startsExpr
  }),
  _in: createKeyword("in", {
    beforeExpr,
    binop: 7
  }),
  _instanceof: createKeyword("instanceof", {
    beforeExpr,
    binop: 7
  }),
  _typeof: createKeyword("typeof", {
    beforeExpr,
    prefix,
    startsExpr
  }),
  _void: createKeyword("void", {
    beforeExpr,
    prefix,
    startsExpr
  }),
  _delete: createKeyword("delete", {
    beforeExpr,
    prefix,
    startsExpr
  })
};

const lineBreak = /\r\n?|[\n\u2028\u2029]/;
const lineBreakG = new RegExp(lineBreak.source, "g");
function isNewLine(code) {
  switch (code) {
    case 10:
    case 13:
    case 8232:
    case 8233:
      return true;

    default:
      return false;
  }
}
const skipWhiteSpace = /(?:\s|\/\/.*|\/\*[^]*?\*\/)*/g;
function isWhitespace(code) {
  switch (code) {
    case 0x0009:
    case 0x000b:
    case 0x000c:
    case 32:
    case 160:
    case 5760:
    case 0x2000:
    case 0x2001:
    case 0x2002:
    case 0x2003:
    case 0x2004:
    case 0x2005:
    case 0x2006:
    case 0x2007:
    case 0x2008:
    case 0x2009:
    case 0x200a:
    case 0x202f:
    case 0x205f:
    case 0x3000:
    case 0xfeff:
      return true;

    default:
      return false;
  }
}

class Position {
  constructor(line, col) {
    this.line = void 0;
    this.column = void 0;
    this.line = line;
    this.column = col;
  }

}
class SourceLocation {
  constructor(start, end) {
    this.start = void 0;
    this.end = void 0;
    this.filename = void 0;
    this.identifierName = void 0;
    this.start = start;
    this.end = end;
  }

}
function getLineInfo(input, offset) {
  let line = 1;
  let lineStart = 0;
  let match;
  lineBreakG.lastIndex = 0;

  while ((match = lineBreakG.exec(input)) && match.index < offset) {
    line++;
    lineStart = lineBreakG.lastIndex;
  }

  return new Position(line, offset - lineStart);
}

class BaseParser {
  constructor() {
    this.sawUnambiguousESM = false;
    this.ambiguousScriptDifferentAst = false;
  }

  hasPlugin(name) {
    return this.plugins.has(name);
  }

  getPluginOption(plugin, name) {
    if (this.hasPlugin(plugin)) return this.plugins.get(plugin)[name];
  }

}

function last(stack) {
  return stack[stack.length - 1];
}

class CommentsParser extends BaseParser {
  addComment(comment) {
    if (this.filename) comment.loc.filename = this.filename;
    this.state.trailingComments.push(comment);
    this.state.leadingComments.push(comment);
  }

  adjustCommentsAfterTrailingComma(node, elements, takeAllComments) {
    if (this.state.leadingComments.length === 0) {
      return;
    }

    let lastElement = null;
    let i = elements.length;

    while (lastElement === null && i > 0) {
      lastElement = elements[--i];
    }

    if (lastElement === null) {
      return;
    }

    for (let j = 0; j < this.state.leadingComments.length; j++) {
      if (this.state.leadingComments[j].end < this.state.commentPreviousNode.end) {
        this.state.leadingComments.splice(j, 1);
        j--;
      }
    }

    const newTrailingComments = [];

    for (let i = 0; i < this.state.leadingComments.length; i++) {
      const leadingComment = this.state.leadingComments[i];

      if (leadingComment.end < node.end) {
        newTrailingComments.push(leadingComment);

        if (!takeAllComments) {
          this.state.leadingComments.splice(i, 1);
          i--;
        }
      } else {
        if (node.trailingComments === undefined) {
          node.trailingComments = [];
        }

        node.trailingComments.push(leadingComment);
      }
    }

    if (takeAllComments) this.state.leadingComments = [];

    if (newTrailingComments.length > 0) {
      lastElement.trailingComments = newTrailingComments;
    } else if (lastElement.trailingComments !== undefined) {
      lastElement.trailingComments = [];
    }
  }

  processComment(node) {
    if (node.type === "Program" && node.body.length > 0) return;
    const stack = this.state.commentStack;
    let firstChild, lastChild, trailingComments, i, j;

    if (this.state.trailingComments.length > 0) {
      if (this.state.trailingComments[0].start >= node.end) {
        trailingComments = this.state.trailingComments;
        this.state.trailingComments = [];
      } else {
        this.state.trailingComments.length = 0;
      }
    } else if (stack.length > 0) {
      const lastInStack = last(stack);

      if (lastInStack.trailingComments && lastInStack.trailingComments[0].start >= node.end) {
        trailingComments = lastInStack.trailingComments;
        delete lastInStack.trailingComments;
      }
    }

    if (stack.length > 0 && last(stack).start >= node.start) {
      firstChild = stack.pop();
    }

    while (stack.length > 0 && last(stack).start >= node.start) {
      lastChild = stack.pop();
    }

    if (!lastChild && firstChild) lastChild = firstChild;

    if (firstChild) {
      switch (node.type) {
        case "ObjectExpression":
          this.adjustCommentsAfterTrailingComma(node, node.properties);
          break;

        case "ObjectPattern":
          this.adjustCommentsAfterTrailingComma(node, node.properties, true);
          break;

        case "CallExpression":
          this.adjustCommentsAfterTrailingComma(node, node.arguments);
          break;

        case "ArrayExpression":
          this.adjustCommentsAfterTrailingComma(node, node.elements);
          break;

        case "ArrayPattern":
          this.adjustCommentsAfterTrailingComma(node, node.elements, true);
          break;
      }
    } else if (this.state.commentPreviousNode && (this.state.commentPreviousNode.type === "ImportSpecifier" && node.type !== "ImportSpecifier" || this.state.commentPreviousNode.type === "ExportSpecifier" && node.type !== "ExportSpecifier")) {
      this.adjustCommentsAfterTrailingComma(node, [this.state.commentPreviousNode]);
    }

    if (lastChild) {
      if (lastChild.leadingComments) {
        if (lastChild !== node && lastChild.leadingComments.length > 0 && last(lastChild.leadingComments).end <= node.start) {
          node.leadingComments = lastChild.leadingComments;
          delete lastChild.leadingComments;
        } else {
          for (i = lastChild.leadingComments.length - 2; i >= 0; --i) {
            if (lastChild.leadingComments[i].end <= node.start) {
              node.leadingComments = lastChild.leadingComments.splice(0, i + 1);
              break;
            }
          }
        }
      }
    } else if (this.state.leadingComments.length > 0) {
      if (last(this.state.leadingComments).end <= node.start) {
        if (this.state.commentPreviousNode) {
          for (j = 0; j < this.state.leadingComments.length; j++) {
            if (this.state.leadingComments[j].end < this.state.commentPreviousNode.end) {
              this.state.leadingComments.splice(j, 1);
              j--;
            }
          }
        }

        if (this.state.leadingComments.length > 0) {
          node.leadingComments = this.state.leadingComments;
          this.state.leadingComments = [];
        }
      } else {
        for (i = 0; i < this.state.leadingComments.length; i++) {
          if (this.state.leadingComments[i].end > node.start) {
            break;
          }
        }

        const leadingComments = this.state.leadingComments.slice(0, i);

        if (leadingComments.length) {
          node.leadingComments = leadingComments;
        }

        trailingComments = this.state.leadingComments.slice(i);

        if (trailingComments.length === 0) {
          trailingComments = null;
        }
      }
    }

    this.state.commentPreviousNode = node;

    if (trailingComments) {
      if (trailingComments.length && trailingComments[0].start >= node.start && last(trailingComments).end <= node.end) {
        node.innerComments = trailingComments;
      } else {
        const firstTrailingCommentIndex = trailingComments.findIndex(comment => comment.end >= node.end);

        if (firstTrailingCommentIndex > 0) {
          node.innerComments = trailingComments.slice(0, firstTrailingCommentIndex);
          node.trailingComments = trailingComments.slice(firstTrailingCommentIndex);
        } else {
          node.trailingComments = trailingComments;
        }
      }
    }

    stack.push(node);
  }

}

const ErrorCodes = Object.freeze({
  SyntaxError: "BABEL_PARSER_SYNTAX_ERROR",
  SourceTypeModuleError: "BABEL_PARSER_SOURCETYPE_MODULE_REQUIRED"
});

const ErrorMessages = makeErrorTemplates({
  AccessorIsGenerator: "A %0ter cannot be a generator.",
  ArgumentsInClass: "'arguments' is only allowed in functions and class methods.",
  AsyncFunctionInSingleStatementContext: "Async functions can only be declared at the top level or inside a block.",
  AwaitBindingIdentifier: "Can not use 'await' as identifier inside an async function.",
  AwaitBindingIdentifierInStaticBlock: "Can not use 'await' as identifier inside a static block.",
  AwaitExpressionFormalParameter: "'await' is not allowed in async function parameters.",
  AwaitNotInAsyncContext: "'await' is only allowed within async functions and at the top levels of modules.",
  AwaitNotInAsyncFunction: "'await' is only allowed within async functions.",
  BadGetterArity: "A 'get' accesor must not have any formal parameters.",
  BadSetterArity: "A 'set' accesor must have exactly one formal parameter.",
  BadSetterRestParameter: "A 'set' accesor function argument must not be a rest parameter.",
  ConstructorClassField: "Classes may not have a field named 'constructor'.",
  ConstructorClassPrivateField: "Classes may not have a private field named '#constructor'.",
  ConstructorIsAccessor: "Class constructor may not be an accessor.",
  ConstructorIsAsync: "Constructor can't be an async function.",
  ConstructorIsGenerator: "Constructor can't be a generator.",
  DeclarationMissingInitializer: "'%0' require an initialization value.",
  DecoratorBeforeExport: "Decorators must be placed *before* the 'export' keyword. You can set the 'decoratorsBeforeExport' option to false to use the 'export @decorator class {}' syntax.",
  DecoratorConstructor: "Decorators can't be used with a constructor. Did you mean '@dec class { ... }'?",
  DecoratorExportClass: "Using the export keyword between a decorator and a class is not allowed. Please use `export @dec class` instead.",
  DecoratorSemicolon: "Decorators must not be followed by a semicolon.",
  DecoratorStaticBlock: "Decorators can't be used with a static block.",
  DeletePrivateField: "Deleting a private field is not allowed.",
  DestructureNamedImport: "ES2015 named imports do not destructure. Use another statement for destructuring after the import.",
  DuplicateConstructor: "Duplicate constructor in the same class.",
  DuplicateDefaultExport: "Only one default export allowed per module.",
  DuplicateExport: "`%0` has already been exported. Exported identifiers must be unique.",
  DuplicateProto: "Redefinition of __proto__ property.",
  DuplicateRegExpFlags: "Duplicate regular expression flag.",
  ElementAfterRest: "Rest element must be last element.",
  EscapedCharNotAnIdentifier: "Invalid Unicode escape.",
  ExportBindingIsString: "A string literal cannot be used as an exported binding without `from`.\n- Did you mean `export { '%0' as '%1' } from 'some-module'`?",
  ExportDefaultFromAsIdentifier: "'from' is not allowed as an identifier after 'export default'.",
  ForInOfLoopInitializer: "'%0' loop variable declaration may not have an initializer.",
  ForOfAsync: "The left-hand side of a for-of loop may not be 'async'.",
  ForOfLet: "The left-hand side of a for-of loop may not start with 'let'.",
  GeneratorInSingleStatementContext: "Generators can only be declared at the top level or inside a block.",
  IllegalBreakContinue: "Unsyntactic %0.",
  IllegalLanguageModeDirective: "Illegal 'use strict' directive in function with non-simple parameter list.",
  IllegalReturn: "'return' outside of function.",
  ImportBindingIsString: 'A string literal cannot be used as an imported binding.\n- Did you mean `import { "%0" as foo }`?',
  ImportCallArgumentTrailingComma: "Trailing comma is disallowed inside import(...) arguments.",
  ImportCallArity: "`import()` requires exactly %0.",
  ImportCallNotNewExpression: "Cannot use new with import(...).",
  ImportCallSpreadArgument: "`...` is not allowed in `import()`.",
  InvalidBigIntLiteral: "Invalid BigIntLiteral.",
  InvalidCodePoint: "Code point out of bounds.",
  InvalidDecimal: "Invalid decimal.",
  InvalidDigit: "Expected number in radix %0.",
  InvalidEscapeSequence: "Bad character escape sequence.",
  InvalidEscapeSequenceTemplate: "Invalid escape sequence in template.",
  InvalidEscapedReservedWord: "Escape sequence in keyword %0.",
  InvalidIdentifier: "Invalid identifier %0.",
  InvalidLhs: "Invalid left-hand side in %0.",
  InvalidLhsBinding: "Binding invalid left-hand side in %0.",
  InvalidNumber: "Invalid number.",
  InvalidOrMissingExponent: "Floating-point numbers require a valid exponent after the 'e'.",
  InvalidOrUnexpectedToken: "Unexpected character '%0'.",
  InvalidParenthesizedAssignment: "Invalid parenthesized assignment pattern.",
  InvalidPrivateFieldResolution: "Private name #%0 is not defined.",
  InvalidPropertyBindingPattern: "Binding member expression.",
  InvalidRecordProperty: "Only properties and spread elements are allowed in record definitions.",
  InvalidRestAssignmentPattern: "Invalid rest operator's argument.",
  LabelRedeclaration: "Label '%0' is already declared.",
  LetInLexicalBinding: "'let' is not allowed to be used as a name in 'let' or 'const' declarations.",
  LineTerminatorBeforeArrow: "No line break is allowed before '=>'.",
  MalformedRegExpFlags: "Invalid regular expression flag.",
  MissingClassName: "A class name is required.",
  MissingEqInAssignment: "Only '=' operator can be used for specifying default value.",
  MissingSemicolon: "Missing semicolon.",
  MissingUnicodeEscape: "Expecting Unicode escape sequence \\uXXXX.",
  MixingCoalesceWithLogical: "Nullish coalescing operator(??) requires parens when mixing with logical operators.",
  ModuleAttributeDifferentFromType: "The only accepted module attribute is `type`.",
  ModuleAttributeInvalidValue: "Only string literals are allowed as module attribute values.",
  ModuleAttributesWithDuplicateKeys: 'Duplicate key "%0" is not allowed in module attributes.',
  ModuleExportNameHasLoneSurrogate: "An export name cannot include a lone surrogate, found '\\u%0'.",
  ModuleExportUndefined: "Export '%0' is not defined.",
  MultipleDefaultsInSwitch: "Multiple default clauses.",
  NewlineAfterThrow: "Illegal newline after throw.",
  NoCatchOrFinally: "Missing catch or finally clause.",
  NumberIdentifier: "Identifier directly after number.",
  NumericSeparatorInEscapeSequence: "Numeric separators are not allowed inside unicode escape sequences or hex escape sequences.",
  ObsoleteAwaitStar: "'await*' has been removed from the async functions proposal. Use Promise.all() instead.",
  OptionalChainingNoNew: "Constructors in/after an Optional Chain are not allowed.",
  OptionalChainingNoTemplate: "Tagged Template Literals are not allowed in optionalChain.",
  OverrideOnConstructor: "'override' modifier cannot appear on a constructor declaration.",
  ParamDupe: "Argument name clash.",
  PatternHasAccessor: "Object pattern can't contain getter or setter.",
  PatternHasMethod: "Object pattern can't contain methods.",
  PipelineBodyNoArrow: 'Unexpected arrow "=>" after pipeline body; arrow function in pipeline body must be parenthesized.',
  PipelineBodySequenceExpression: "Pipeline body may not be a comma-separated sequence expression.",
  PipelineHeadSequenceExpression: "Pipeline head should not be a comma-separated sequence expression.",
  PipelineTopicUnused: "Pipeline is in topic style but does not use topic reference.",
  PrimaryTopicNotAllowed: "Topic reference was used in a lexical context without topic binding.",
  PrimaryTopicRequiresSmartPipeline: "Primary Topic Reference found but pipelineOperator not passed 'smart' for 'proposal' option.",
  PrivateInExpectedIn: "Private names are only allowed in property accesses (`obj.#%0`) or in `in` expressions (`#%0 in obj`).",
  PrivateNameRedeclaration: "Duplicate private name #%0.",
  RecordExpressionBarIncorrectEndSyntaxType: "Record expressions ending with '|}' are only allowed when the 'syntaxType' option of the 'recordAndTuple' plugin is set to 'bar'.",
  RecordExpressionBarIncorrectStartSyntaxType: "Record expressions starting with '{|' are only allowed when the 'syntaxType' option of the 'recordAndTuple' plugin is set to 'bar'.",
  RecordExpressionHashIncorrectStartSyntaxType: "Record expressions starting with '#{' are only allowed when the 'syntaxType' option of the 'recordAndTuple' plugin is set to 'hash'.",
  RecordNoProto: "'__proto__' is not allowed in Record expressions.",
  RestTrailingComma: "Unexpected trailing comma after rest element.",
  SloppyFunction: "In non-strict mode code, functions can only be declared at top level, inside a block, or as the body of an if statement.",
  StaticPrototype: "Classes may not have static property named prototype.",
  StrictDelete: "Deleting local variable in strict mode.",
  StrictEvalArguments: "Assigning to '%0' in strict mode.",
  StrictEvalArgumentsBinding: "Binding '%0' in strict mode.",
  StrictFunction: "In strict mode code, functions can only be declared at top level or inside a block.",
  StrictNumericEscape: "The only valid numeric escape in strict mode is '\\0'.",
  StrictOctalLiteral: "Legacy octal literals are not allowed in strict mode.",
  StrictWith: "'with' in strict mode.",
  SuperNotAllowed: "`super()` is only valid inside a class constructor of a subclass. Maybe a typo in the method name ('constructor') or not extending another class?",
  SuperPrivateField: "Private fields can't be accessed on super.",
  TrailingDecorator: "Decorators must be attached to a class element.",
  TupleExpressionBarIncorrectEndSyntaxType: "Tuple expressions ending with '|]' are only allowed when the 'syntaxType' option of the 'recordAndTuple' plugin is set to 'bar'.",
  TupleExpressionBarIncorrectStartSyntaxType: "Tuple expressions starting with '[|' are only allowed when the 'syntaxType' option of the 'recordAndTuple' plugin is set to 'bar'.",
  TupleExpressionHashIncorrectStartSyntaxType: "Tuple expressions starting with '#[' are only allowed when the 'syntaxType' option of the 'recordAndTuple' plugin is set to 'hash'.",
  UnexpectedArgumentPlaceholder: "Unexpected argument placeholder.",
  UnexpectedAwaitAfterPipelineBody: 'Unexpected "await" after pipeline body; await must have parentheses in minimal proposal.',
  UnexpectedDigitAfterHash: "Unexpected digit after hash token.",
  UnexpectedImportExport: "'import' and 'export' may only appear at the top level.",
  UnexpectedKeyword: "Unexpected keyword '%0'.",
  UnexpectedLeadingDecorator: "Leading decorators must be attached to a class declaration.",
  UnexpectedLexicalDeclaration: "Lexical declaration cannot appear in a single-statement context.",
  UnexpectedNewTarget: "`new.target` can only be used in functions or class properties.",
  UnexpectedNumericSeparator: "A numeric separator is only allowed between two digits.",
  UnexpectedPrivateField: "Private names can only be used as the name of a class element (i.e. class C { #p = 42; #m() {} } )\n or a property of member expression (i.e. this.#p).",
  UnexpectedReservedWord: "Unexpected reserved word '%0'.",
  UnexpectedSuper: "'super' is only allowed in object methods and classes.",
  UnexpectedToken: "Unexpected token '%0'.",
  UnexpectedTokenUnaryExponentiation: "Illegal expression. Wrap left hand side or entire exponentiation in parentheses.",
  UnsupportedBind: "Binding should be performed on object property.",
  UnsupportedDecoratorExport: "A decorated export must export a class declaration.",
  UnsupportedDefaultExport: "Only expressions, functions or classes are allowed as the `default` export.",
  UnsupportedImport: "`import` can only be used in `import()` or `import.meta`.",
  UnsupportedMetaProperty: "The only valid meta property for %0 is %0.%1.",
  UnsupportedParameterDecorator: "Decorators cannot be used to decorate parameters.",
  UnsupportedPropertyDecorator: "Decorators cannot be used to decorate object literal properties.",
  UnsupportedSuper: "'super' can only be used with function calls (i.e. super()) or in property accesses (i.e. super.prop or super[prop]).",
  UnterminatedComment: "Unterminated comment.",
  UnterminatedRegExp: "Unterminated regular expression.",
  UnterminatedString: "Unterminated string constant.",
  UnterminatedTemplate: "Unterminated template.",
  VarRedeclaration: "Identifier '%0' has already been declared.",
  YieldBindingIdentifier: "Can not use 'yield' as identifier inside a generator.",
  YieldInParameter: "Yield expression is not allowed in formal parameters.",
  ZeroDigitNumericSeparator: "Numeric separator can not be used after leading 0."
}, ErrorCodes.SyntaxError);
const SourceTypeModuleErrorMessages = makeErrorTemplates({
  ImportMetaOutsideModule: `import.meta may appear only with 'sourceType: "module"'`,
  ImportOutsideModule: `'import' and 'export' may appear only with 'sourceType: "module"'`
}, ErrorCodes.SourceTypeModuleError);

function makeErrorTemplates(messages, code) {
  const templates = {};
  Object.keys(messages).forEach(reasonCode => {
    templates[reasonCode] = Object.freeze({
      code,
      reasonCode,
      template: messages[reasonCode]
    });
  });
  return Object.freeze(templates);
}
class ParserError extends CommentsParser {
  getLocationForPosition(pos) {
    let loc;
    if (pos === this.state.start) loc = this.state.startLoc;else if (pos === this.state.lastTokStart) loc = this.state.lastTokStartLoc;else if (pos === this.state.end) loc = this.state.endLoc;else if (pos === this.state.lastTokEnd) loc = this.state.lastTokEndLoc;else loc = getLineInfo(this.input, pos);
    return loc;
  }

  raise(pos, {
    code,
    reasonCode,
    template
  }, ...params) {
    return this.raiseWithData(pos, {
      code,
      reasonCode
    }, template, ...params);
  }

  raiseOverwrite(pos, {
    code,
    template
  }, ...params) {
    const loc = this.getLocationForPosition(pos);
    const message = template.replace(/%(\d+)/g, (_, i) => params[i]) + ` (${loc.line}:${loc.column})`;

    if (this.options.errorRecovery) {
      const errors = this.state.errors;

      for (let i = errors.length - 1; i >= 0; i--) {
        const error = errors[i];

        if (error.pos === pos) {
          return Object.assign(error, {
            message
          });
        } else if (error.pos < pos) {
          break;
        }
      }
    }

    return this._raise({
      code,
      loc,
      pos
    }, message);
  }

  raiseWithData(pos, data, errorTemplate, ...params) {
    const loc = this.getLocationForPosition(pos);
    const message = errorTemplate.replace(/%(\d+)/g, (_, i) => params[i]) + ` (${loc.line}:${loc.column})`;
    return this._raise(Object.assign({
      loc,
      pos
    }, data), message);
  }

  _raise(errorContext, message) {
    const err = new SyntaxError(message);
    Object.assign(err, errorContext);

    if (this.options.errorRecovery) {
      if (!this.isLookahead) this.state.errors.push(err);
      return err;
    } else {
      throw err;
    }
  }

}

var estree = (superClass => class extends superClass {
  parseRegExpLiteral({
    pattern,
    flags
  }) {
    let regex = null;

    try {
      regex = new RegExp(pattern, flags);
    } catch (e) {}

    const node = this.estreeParseLiteral(regex);
    node.regex = {
      pattern,
      flags
    };
    return node;
  }

  parseBigIntLiteral(value) {
    let bigInt;

    try {
      bigInt = BigInt(value);
    } catch (_unused) {
      bigInt = null;
    }

    const node = this.estreeParseLiteral(bigInt);
    node.bigint = String(node.value || value);
    return node;
  }

  parseDecimalLiteral(value) {
    const decimal = null;
    const node = this.estreeParseLiteral(decimal);
    node.decimal = String(node.value || value);
    return node;
  }

  estreeParseLiteral(value) {
    return this.parseLiteral(value, "Literal");
  }

  parseStringLiteral(value) {
    return this.estreeParseLiteral(value);
  }

  parseNumericLiteral(value) {
    return this.estreeParseLiteral(value);
  }

  parseNullLiteral() {
    return this.estreeParseLiteral(null);
  }

  parseBooleanLiteral(value) {
    return this.estreeParseLiteral(value);
  }

  directiveToStmt(directive) {
    const directiveLiteral = directive.value;
    const stmt = this.startNodeAt(directive.start, directive.loc.start);
    const expression = this.startNodeAt(directiveLiteral.start, directiveLiteral.loc.start);
    expression.value = directiveLiteral.extra.expressionValue;
    expression.raw = directiveLiteral.extra.raw;
    stmt.expression = this.finishNodeAt(expression, "Literal", directiveLiteral.end, directiveLiteral.loc.end);
    stmt.directive = directiveLiteral.extra.raw.slice(1, -1);
    return this.finishNodeAt(stmt, "ExpressionStatement", directive.end, directive.loc.end);
  }

  initFunction(node, isAsync) {
    super.initFunction(node, isAsync);
    node.expression = false;
  }

  checkDeclaration(node) {
    if (node != null && this.isObjectProperty(node)) {
      this.checkDeclaration(node.value);
    } else {
      super.checkDeclaration(node);
    }
  }

  getObjectOrClassMethodParams(method) {
    return method.value.params;
  }

  isValidDirective(stmt) {
    var _stmt$expression$extr;

    return stmt.type === "ExpressionStatement" && stmt.expression.type === "Literal" && typeof stmt.expression.value === "string" && !((_stmt$expression$extr = stmt.expression.extra) != null && _stmt$expression$extr.parenthesized);
  }

  stmtToDirective(stmt) {
    const directive = super.stmtToDirective(stmt);
    const value = stmt.expression.value;
    this.addExtra(directive.value, "expressionValue", value);
    return directive;
  }

  parseBlockBody(node, ...args) {
    super.parseBlockBody(node, ...args);
    const directiveStatements = node.directives.map(d => this.directiveToStmt(d));
    node.body = directiveStatements.concat(node.body);
    delete node.directives;
  }

  pushClassMethod(classBody, method, isGenerator, isAsync, isConstructor, allowsDirectSuper) {
    this.parseMethod(method, isGenerator, isAsync, isConstructor, allowsDirectSuper, "ClassMethod", true);

    if (method.typeParameters) {
      method.value.typeParameters = method.typeParameters;
      delete method.typeParameters;
    }

    classBody.body.push(method);
  }

  parseMaybePrivateName(...args) {
    const node = super.parseMaybePrivateName(...args);

    if (node.type === "PrivateName" && this.getPluginOption("estree", "classFeatures")) {
      return this.convertPrivateNameToPrivateIdentifier(node);
    }

    return node;
  }

  convertPrivateNameToPrivateIdentifier(node) {
    const name = super.getPrivateNameSV(node);
    node = node;
    delete node.id;
    node.name = name;
    node.type = "PrivateIdentifier";
    return node;
  }

  isPrivateName(node) {
    if (!this.getPluginOption("estree", "classFeatures")) {
      return super.isPrivateName(node);
    }

    return node.type === "PrivateIdentifier";
  }

  getPrivateNameSV(node) {
    if (!this.getPluginOption("estree", "classFeatures")) {
      return super.getPrivateNameSV(node);
    }

    return node.name;
  }

  parseLiteral(value, type) {
    const node = super.parseLiteral(value, type);
    node.raw = node.extra.raw;
    delete node.extra;
    return node;
  }

  parseFunctionBody(node, allowExpression, isMethod = false) {
    super.parseFunctionBody(node, allowExpression, isMethod);
    node.expression = node.body.type !== "BlockStatement";
  }

  parseMethod(node, isGenerator, isAsync, isConstructor, allowDirectSuper, type, inClassScope = false) {
    let funcNode = this.startNode();
    funcNode.kind = node.kind;
    funcNode = super.parseMethod(funcNode, isGenerator, isAsync, isConstructor, allowDirectSuper, type, inClassScope);
    funcNode.type = "FunctionExpression";
    delete funcNode.kind;
    node.value = funcNode;

    if (type === "ClassPrivateMethod") {
      node.computed = false;
    }

    type = "MethodDefinition";
    return this.finishNode(node, type);
  }

  parseClassProperty(...args) {
    const propertyNode = super.parseClassProperty(...args);

    if (this.getPluginOption("estree", "classFeatures")) {
      propertyNode.type = "PropertyDefinition";
    }

    return propertyNode;
  }

  parseClassPrivateProperty(...args) {
    const propertyNode = super.parseClassPrivateProperty(...args);

    if (this.getPluginOption("estree", "classFeatures")) {
      propertyNode.type = "PropertyDefinition";
      propertyNode.computed = false;
    }

    return propertyNode;
  }

  parseObjectMethod(prop, isGenerator, isAsync, isPattern, isAccessor) {
    const node = super.parseObjectMethod(prop, isGenerator, isAsync, isPattern, isAccessor);

    if (node) {
      node.type = "Property";
      if (node.kind === "method") node.kind = "init";
      node.shorthand = false;
    }

    return node;
  }

  parseObjectProperty(prop, startPos, startLoc, isPattern, refExpressionErrors) {
    const node = super.parseObjectProperty(prop, startPos, startLoc, isPattern, refExpressionErrors);

    if (node) {
      node.kind = "init";
      node.type = "Property";
    }

    return node;
  }

  toAssignable(node, isLHS = false) {
    if (node != null && this.isObjectProperty(node)) {
      this.toAssignable(node.value, isLHS);
      return node;
    }

    return super.toAssignable(node, isLHS);
  }

  toAssignableObjectExpressionProp(prop, ...args) {
    if (prop.kind === "get" || prop.kind === "set") {
      this.raise(prop.key.start, ErrorMessages.PatternHasAccessor);
    } else if (prop.method) {
      this.raise(prop.key.start, ErrorMessages.PatternHasMethod);
    } else {
      super.toAssignableObjectExpressionProp(prop, ...args);
    }
  }

  finishCallExpression(node, optional) {
    super.finishCallExpression(node, optional);

    if (node.callee.type === "Import") {
      node.type = "ImportExpression";
      node.source = node.arguments[0];

      if (this.hasPlugin("importAssertions")) {
        var _node$arguments$;

        node.attributes = (_node$arguments$ = node.arguments[1]) != null ? _node$arguments$ : null;
      }

      delete node.arguments;
      delete node.callee;
    }

    return node;
  }

  toReferencedArguments(node) {
    if (node.type === "ImportExpression") {
      return;
    }

    super.toReferencedArguments(node);
  }

  parseExport(node) {
    super.parseExport(node);

    switch (node.type) {
      case "ExportAllDeclaration":
        node.exported = null;
        break;

      case "ExportNamedDeclaration":
        if (node.specifiers.length === 1 && node.specifiers[0].type === "ExportNamespaceSpecifier") {
          node.type = "ExportAllDeclaration";
          node.exported = node.specifiers[0].exported;
          delete node.specifiers;
        }

        break;
    }

    return node;
  }

  parseSubscript(base, startPos, startLoc, noCalls, state) {
    const node = super.parseSubscript(base, startPos, startLoc, noCalls, state);

    if (state.optionalChainMember) {
      if (node.type === "OptionalMemberExpression" || node.type === "OptionalCallExpression") {
        node.type = node.type.substring(8);
      }

      if (state.stop) {
        const chain = this.startNodeAtNode(node);
        chain.expression = node;
        return this.finishNode(chain, "ChainExpression");
      }
    } else if (node.type === "MemberExpression" || node.type === "CallExpression") {
      node.optional = false;
    }

    return node;
  }

  hasPropertyAsPrivateName(node) {
    if (node.type === "ChainExpression") {
      node = node.expression;
    }

    return super.hasPropertyAsPrivateName(node);
  }

  isOptionalChain(node) {
    return node.type === "ChainExpression";
  }

  isObjectProperty(node) {
    return node.type === "Property" && node.kind === "init" && !node.method;
  }

  isObjectMethod(node) {
    return node.method || node.kind === "get" || node.kind === "set";
  }

});

class TokContext {
  constructor(token, preserveSpace) {
    this.token = void 0;
    this.preserveSpace = void 0;
    this.token = token;
    this.preserveSpace = !!preserveSpace;
  }

}
const types = {
  brace: new TokContext("{"),
  template: new TokContext("`", true)
};

types$1.braceR.updateContext = context => {
  context.pop();
};

types$1.braceL.updateContext = types$1.braceHashL.updateContext = types$1.dollarBraceL.updateContext = context => {
  context.push(types.brace);
};

types$1.backQuote.updateContext = context => {
  if (context[context.length - 1] === types.template) {
    context.pop();
  } else {
    context.push(types.template);
  }
};

let nonASCIIidentifierStartChars = "\xaa\xb5\xba\xc0-\xd6\xd8-\xf6\xf8-\u02c1\u02c6-\u02d1\u02e0-\u02e4\u02ec\u02ee\u0370-\u0374\u0376\u0377\u037a-\u037d\u037f\u0386\u0388-\u038a\u038c\u038e-\u03a1\u03a3-\u03f5\u03f7-\u0481\u048a-\u052f\u0531-\u0556\u0559\u0560-\u0588\u05d0-\u05ea\u05ef-\u05f2\u0620-\u064a\u066e\u066f\u0671-\u06d3\u06d5\u06e5\u06e6\u06ee\u06ef\u06fa-\u06fc\u06ff\u0710\u0712-\u072f\u074d-\u07a5\u07b1\u07ca-\u07ea\u07f4\u07f5\u07fa\u0800-\u0815\u081a\u0824\u0828\u0840-\u0858\u0860-\u086a\u08a0-\u08b4\u08b6-\u08c7\u0904-\u0939\u093d\u0950\u0958-\u0961\u0971-\u0980\u0985-\u098c\u098f\u0990\u0993-\u09a8\u09aa-\u09b0\u09b2\u09b6-\u09b9\u09bd\u09ce\u09dc\u09dd\u09df-\u09e1\u09f0\u09f1\u09fc\u0a05-\u0a0a\u0a0f\u0a10\u0a13-\u0a28\u0a2a-\u0a30\u0a32\u0a33\u0a35\u0a36\u0a38\u0a39\u0a59-\u0a5c\u0a5e\u0a72-\u0a74\u0a85-\u0a8d\u0a8f-\u0a91\u0a93-\u0aa8\u0aaa-\u0ab0\u0ab2\u0ab3\u0ab5-\u0ab9\u0abd\u0ad0\u0ae0\u0ae1\u0af9\u0b05-\u0b0c\u0b0f\u0b10\u0b13-\u0b28\u0b2a-\u0b30\u0b32\u0b33\u0b35-\u0b39\u0b3d\u0b5c\u0b5d\u0b5f-\u0b61\u0b71\u0b83\u0b85-\u0b8a\u0b8e-\u0b90\u0b92-\u0b95\u0b99\u0b9a\u0b9c\u0b9e\u0b9f\u0ba3\u0ba4\u0ba8-\u0baa\u0bae-\u0bb9\u0bd0\u0c05-\u0c0c\u0c0e-\u0c10\u0c12-\u0c28\u0c2a-\u0c39\u0c3d\u0c58-\u0c5a\u0c60\u0c61\u0c80\u0c85-\u0c8c\u0c8e-\u0c90\u0c92-\u0ca8\u0caa-\u0cb3\u0cb5-\u0cb9\u0cbd\u0cde\u0ce0\u0ce1\u0cf1\u0cf2\u0d04-\u0d0c\u0d0e-\u0d10\u0d12-\u0d3a\u0d3d\u0d4e\u0d54-\u0d56\u0d5f-\u0d61\u0d7a-\u0d7f\u0d85-\u0d96\u0d9a-\u0db1\u0db3-\u0dbb\u0dbd\u0dc0-\u0dc6\u0e01-\u0e30\u0e32\u0e33\u0e40-\u0e46\u0e81\u0e82\u0e84\u0e86-\u0e8a\u0e8c-\u0ea3\u0ea5\u0ea7-\u0eb0\u0eb2\u0eb3\u0ebd\u0ec0-\u0ec4\u0ec6\u0edc-\u0edf\u0f00\u0f40-\u0f47\u0f49-\u0f6c\u0f88-\u0f8c\u1000-\u102a\u103f\u1050-\u1055\u105a-\u105d\u1061\u1065\u1066\u106e-\u1070\u1075-\u1081\u108e\u10a0-\u10c5\u10c7\u10cd\u10d0-\u10fa\u10fc-\u1248\u124a-\u124d\u1250-\u1256\u1258\u125a-\u125d\u1260-\u1288\u128a-\u128d\u1290-\u12b0\u12b2-\u12b5\u12b8-\u12be\u12c0\u12c2-\u12c5\u12c8-\u12d6\u12d8-\u1310\u1312-\u1315\u1318-\u135a\u1380-\u138f\u13a0-\u13f5\u13f8-\u13fd\u1401-\u166c\u166f-\u167f\u1681-\u169a\u16a0-\u16ea\u16ee-\u16f8\u1700-\u170c\u170e-\u1711\u1720-\u1731\u1740-\u1751\u1760-\u176c\u176e-\u1770\u1780-\u17b3\u17d7\u17dc\u1820-\u1878\u1880-\u18a8\u18aa\u18b0-\u18f5\u1900-\u191e\u1950-\u196d\u1970-\u1974\u1980-\u19ab\u19b0-\u19c9\u1a00-\u1a16\u1a20-\u1a54\u1aa7\u1b05-\u1b33\u1b45-\u1b4b\u1b83-\u1ba0\u1bae\u1baf\u1bba-\u1be5\u1c00-\u1c23\u1c4d-\u1c4f\u1c5a-\u1c7d\u1c80-\u1c88\u1c90-\u1cba\u1cbd-\u1cbf\u1ce9-\u1cec\u1cee-\u1cf3\u1cf5\u1cf6\u1cfa\u1d00-\u1dbf\u1e00-\u1f15\u1f18-\u1f1d\u1f20-\u1f45\u1f48-\u1f4d\u1f50-\u1f57\u1f59\u1f5b\u1f5d\u1f5f-\u1f7d\u1f80-\u1fb4\u1fb6-\u1fbc\u1fbe\u1fc2-\u1fc4\u1fc6-\u1fcc\u1fd0-\u1fd3\u1fd6-\u1fdb\u1fe0-\u1fec\u1ff2-\u1ff4\u1ff6-\u1ffc\u2071\u207f\u2090-\u209c\u2102\u2107\u210a-\u2113\u2115\u2118-\u211d\u2124\u2126\u2128\u212a-\u2139\u213c-\u213f\u2145-\u2149\u214e\u2160-\u2188\u2c00-\u2c2e\u2c30-\u2c5e\u2c60-\u2ce4\u2ceb-\u2cee\u2cf2\u2cf3\u2d00-\u2d25\u2d27\u2d2d\u2d30-\u2d67\u2d6f\u2d80-\u2d96\u2da0-\u2da6\u2da8-\u2dae\u2db0-\u2db6\u2db8-\u2dbe\u2dc0-\u2dc6\u2dc8-\u2dce\u2dd0-\u2dd6\u2dd8-\u2dde\u3005-\u3007\u3021-\u3029\u3031-\u3035\u3038-\u303c\u3041-\u3096\u309b-\u309f\u30a1-\u30fa\u30fc-\u30ff\u3105-\u312f\u3131-\u318e\u31a0-\u31bf\u31f0-\u31ff\u3400-\u4dbf\u4e00-\u9ffc\ua000-\ua48c\ua4d0-\ua4fd\ua500-\ua60c\ua610-\ua61f\ua62a\ua62b\ua640-\ua66e\ua67f-\ua69d\ua6a0-\ua6ef\ua717-\ua71f\ua722-\ua788\ua78b-\ua7bf\ua7c2-\ua7ca\ua7f5-\ua801\ua803-\ua805\ua807-\ua80a\ua80c-\ua822\ua840-\ua873\ua882-\ua8b3\ua8f2-\ua8f7\ua8fb\ua8fd\ua8fe\ua90a-\ua925\ua930-\ua946\ua960-\ua97c\ua984-\ua9b2\ua9cf\ua9e0-\ua9e4\ua9e6-\ua9ef\ua9fa-\ua9fe\uaa00-\uaa28\uaa40-\uaa42\uaa44-\uaa4b\uaa60-\uaa76\uaa7a\uaa7e-\uaaaf\uaab1\uaab5\uaab6\uaab9-\uaabd\uaac0\uaac2\uaadb-\uaadd\uaae0-\uaaea\uaaf2-\uaaf4\uab01-\uab06\uab09-\uab0e\uab11-\uab16\uab20-\uab26\uab28-\uab2e\uab30-\uab5a\uab5c-\uab69\uab70-\uabe2\uac00-\ud7a3\ud7b0-\ud7c6\ud7cb-\ud7fb\uf900-\ufa6d\ufa70-\ufad9\ufb00-\ufb06\ufb13-\ufb17\ufb1d\ufb1f-\ufb28\ufb2a-\ufb36\ufb38-\ufb3c\ufb3e\ufb40\ufb41\ufb43\ufb44\ufb46-\ufbb1\ufbd3-\ufd3d\ufd50-\ufd8f\ufd92-\ufdc7\ufdf0-\ufdfb\ufe70-\ufe74\ufe76-\ufefc\uff21-\uff3a\uff41-\uff5a\uff66-\uffbe\uffc2-\uffc7\uffca-\uffcf\uffd2-\uffd7\uffda-\uffdc";
let nonASCIIidentifierChars = "\u200c\u200d\xb7\u0300-\u036f\u0387\u0483-\u0487\u0591-\u05bd\u05bf\u05c1\u05c2\u05c4\u05c5\u05c7\u0610-\u061a\u064b-\u0669\u0670\u06d6-\u06dc\u06df-\u06e4\u06e7\u06e8\u06ea-\u06ed\u06f0-\u06f9\u0711\u0730-\u074a\u07a6-\u07b0\u07c0-\u07c9\u07eb-\u07f3\u07fd\u0816-\u0819\u081b-\u0823\u0825-\u0827\u0829-\u082d\u0859-\u085b\u08d3-\u08e1\u08e3-\u0903\u093a-\u093c\u093e-\u094f\u0951-\u0957\u0962\u0963\u0966-\u096f\u0981-\u0983\u09bc\u09be-\u09c4\u09c7\u09c8\u09cb-\u09cd\u09d7\u09e2\u09e3\u09e6-\u09ef\u09fe\u0a01-\u0a03\u0a3c\u0a3e-\u0a42\u0a47\u0a48\u0a4b-\u0a4d\u0a51\u0a66-\u0a71\u0a75\u0a81-\u0a83\u0abc\u0abe-\u0ac5\u0ac7-\u0ac9\u0acb-\u0acd\u0ae2\u0ae3\u0ae6-\u0aef\u0afa-\u0aff\u0b01-\u0b03\u0b3c\u0b3e-\u0b44\u0b47\u0b48\u0b4b-\u0b4d\u0b55-\u0b57\u0b62\u0b63\u0b66-\u0b6f\u0b82\u0bbe-\u0bc2\u0bc6-\u0bc8\u0bca-\u0bcd\u0bd7\u0be6-\u0bef\u0c00-\u0c04\u0c3e-\u0c44\u0c46-\u0c48\u0c4a-\u0c4d\u0c55\u0c56\u0c62\u0c63\u0c66-\u0c6f\u0c81-\u0c83\u0cbc\u0cbe-\u0cc4\u0cc6-\u0cc8\u0cca-\u0ccd\u0cd5\u0cd6\u0ce2\u0ce3\u0ce6-\u0cef\u0d00-\u0d03\u0d3b\u0d3c\u0d3e-\u0d44\u0d46-\u0d48\u0d4a-\u0d4d\u0d57\u0d62\u0d63\u0d66-\u0d6f\u0d81-\u0d83\u0dca\u0dcf-\u0dd4\u0dd6\u0dd8-\u0ddf\u0de6-\u0def\u0df2\u0df3\u0e31\u0e34-\u0e3a\u0e47-\u0e4e\u0e50-\u0e59\u0eb1\u0eb4-\u0ebc\u0ec8-\u0ecd\u0ed0-\u0ed9\u0f18\u0f19\u0f20-\u0f29\u0f35\u0f37\u0f39\u0f3e\u0f3f\u0f71-\u0f84\u0f86\u0f87\u0f8d-\u0f97\u0f99-\u0fbc\u0fc6\u102b-\u103e\u1040-\u1049\u1056-\u1059\u105e-\u1060\u1062-\u1064\u1067-\u106d\u1071-\u1074\u1082-\u108d\u108f-\u109d\u135d-\u135f\u1369-\u1371\u1712-\u1714\u1732-\u1734\u1752\u1753\u1772\u1773\u17b4-\u17d3\u17dd\u17e0-\u17e9\u180b-\u180d\u1810-\u1819\u18a9\u1920-\u192b\u1930-\u193b\u1946-\u194f\u19d0-\u19da\u1a17-\u1a1b\u1a55-\u1a5e\u1a60-\u1a7c\u1a7f-\u1a89\u1a90-\u1a99\u1ab0-\u1abd\u1abf\u1ac0\u1b00-\u1b04\u1b34-\u1b44\u1b50-\u1b59\u1b6b-\u1b73\u1b80-\u1b82\u1ba1-\u1bad\u1bb0-\u1bb9\u1be6-\u1bf3\u1c24-\u1c37\u1c40-\u1c49\u1c50-\u1c59\u1cd0-\u1cd2\u1cd4-\u1ce8\u1ced\u1cf4\u1cf7-\u1cf9\u1dc0-\u1df9\u1dfb-\u1dff\u203f\u2040\u2054\u20d0-\u20dc\u20e1\u20e5-\u20f0\u2cef-\u2cf1\u2d7f\u2de0-\u2dff\u302a-\u302f\u3099\u309a\ua620-\ua629\ua66f\ua674-\ua67d\ua69e\ua69f\ua6f0\ua6f1\ua802\ua806\ua80b\ua823-\ua827\ua82c\ua880\ua881\ua8b4-\ua8c5\ua8d0-\ua8d9\ua8e0-\ua8f1\ua8ff-\ua909\ua926-\ua92d\ua947-\ua953\ua980-\ua983\ua9b3-\ua9c0\ua9d0-\ua9d9\ua9e5\ua9f0-\ua9f9\uaa29-\uaa36\uaa43\uaa4c\uaa4d\uaa50-\uaa59\uaa7b-\uaa7d\uaab0\uaab2-\uaab4\uaab7\uaab8\uaabe\uaabf\uaac1\uaaeb-\uaaef\uaaf5\uaaf6\uabe3-\uabea\uabec\uabed\uabf0-\uabf9\ufb1e\ufe00-\ufe0f\ufe20-\ufe2f\ufe33\ufe34\ufe4d-\ufe4f\uff10-\uff19\uff3f";
const nonASCIIidentifierStart = new RegExp("[" + nonASCIIidentifierStartChars + "]");
const nonASCIIidentifier = new RegExp("[" + nonASCIIidentifierStartChars + nonASCIIidentifierChars + "]");
nonASCIIidentifierStartChars = nonASCIIidentifierChars = null;
const astralIdentifierStartCodes = [0, 11, 2, 25, 2, 18, 2, 1, 2, 14, 3, 13, 35, 122, 70, 52, 268, 28, 4, 48, 48, 31, 14, 29, 6, 37, 11, 29, 3, 35, 5, 7, 2, 4, 43, 157, 19, 35, 5, 35, 5, 39, 9, 51, 157, 310, 10, 21, 11, 7, 153, 5, 3, 0, 2, 43, 2, 1, 4, 0, 3, 22, 11, 22, 10, 30, 66, 18, 2, 1, 11, 21, 11, 25, 71, 55, 7, 1, 65, 0, 16, 3, 2, 2, 2, 28, 43, 28, 4, 28, 36, 7, 2, 27, 28, 53, 11, 21, 11, 18, 14, 17, 111, 72, 56, 50, 14, 50, 14, 35, 349, 41, 7, 1, 79, 28, 11, 0, 9, 21, 107, 20, 28, 22, 13, 52, 76, 44, 33, 24, 27, 35, 30, 0, 3, 0, 9, 34, 4, 0, 13, 47, 15, 3, 22, 0, 2, 0, 36, 17, 2, 24, 85, 6, 2, 0, 2, 3, 2, 14, 2, 9, 8, 46, 39, 7, 3, 1, 3, 21, 2, 6, 2, 1, 2, 4, 4, 0, 19, 0, 13, 4, 159, 52, 19, 3, 21, 2, 31, 47, 21, 1, 2, 0, 185, 46, 42, 3, 37, 47, 21, 0, 60, 42, 14, 0, 72, 26, 230, 43, 117, 63, 32, 7, 3, 0, 3, 7, 2, 1, 2, 23, 16, 0, 2, 0, 95, 7, 3, 38, 17, 0, 2, 0, 29, 0, 11, 39, 8, 0, 22, 0, 12, 45, 20, 0, 35, 56, 264, 8, 2, 36, 18, 0, 50, 29, 113, 6, 2, 1, 2, 37, 22, 0, 26, 5, 2, 1, 2, 31, 15, 0, 328, 18, 190, 0, 80, 921, 103, 110, 18, 195, 2749, 1070, 4050, 582, 8634, 568, 8, 30, 114, 29, 19, 47, 17, 3, 32, 20, 6, 18, 689, 63, 129, 74, 6, 0, 67, 12, 65, 1, 2, 0, 29, 6135, 9, 1237, 43, 8, 8952, 286, 50, 2, 18, 3, 9, 395, 2309, 106, 6, 12, 4, 8, 8, 9, 5991, 84, 2, 70, 2, 1, 3, 0, 3, 1, 3, 3, 2, 11, 2, 0, 2, 6, 2, 64, 2, 3, 3, 7, 2, 6, 2, 27, 2, 3, 2, 4, 2, 0, 4, 6, 2, 339, 3, 24, 2, 24, 2, 30, 2, 24, 2, 30, 2, 24, 2, 30, 2, 24, 2, 30, 2, 24, 2, 7, 2357, 44, 11, 6, 17, 0, 370, 43, 1301, 196, 60, 67, 8, 0, 1205, 3, 2, 26, 2, 1, 2, 0, 3, 0, 2, 9, 2, 3, 2, 0, 2, 0, 7, 0, 5, 0, 2, 0, 2, 0, 2, 2, 2, 1, 2, 0, 3, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 1, 2, 0, 3, 3, 2, 6, 2, 3, 2, 3, 2, 0, 2, 9, 2, 16, 6, 2, 2, 4, 2, 16, 4421, 42717, 35, 4148, 12, 221, 3, 5761, 15, 7472, 3104, 541, 1507, 4938];
const astralIdentifierCodes = [509, 0, 227, 0, 150, 4, 294, 9, 1368, 2, 2, 1, 6, 3, 41, 2, 5, 0, 166, 1, 574, 3, 9, 9, 370, 1, 154, 10, 176, 2, 54, 14, 32, 9, 16, 3, 46, 10, 54, 9, 7, 2, 37, 13, 2, 9, 6, 1, 45, 0, 13, 2, 49, 13, 9, 3, 2, 11, 83, 11, 7, 0, 161, 11, 6, 9, 7, 3, 56, 1, 2, 6, 3, 1, 3, 2, 10, 0, 11, 1, 3, 6, 4, 4, 193, 17, 10, 9, 5, 0, 82, 19, 13, 9, 214, 6, 3, 8, 28, 1, 83, 16, 16, 9, 82, 12, 9, 9, 84, 14, 5, 9, 243, 14, 166, 9, 71, 5, 2, 1, 3, 3, 2, 0, 2, 1, 13, 9, 120, 6, 3, 6, 4, 0, 29, 9, 41, 6, 2, 3, 9, 0, 10, 10, 47, 15, 406, 7, 2, 7, 17, 9, 57, 21, 2, 13, 123, 5, 4, 0, 2, 1, 2, 6, 2, 0, 9, 9, 49, 4, 2, 1, 2, 4, 9, 9, 330, 3, 19306, 9, 135, 4, 60, 6, 26, 9, 1014, 0, 2, 54, 8, 3, 82, 0, 12, 1, 19628, 1, 5319, 4, 4, 5, 9, 7, 3, 6, 31, 3, 149, 2, 1418, 49, 513, 54, 5, 49, 9, 0, 15, 0, 23, 4, 2, 14, 1361, 6, 2, 16, 3, 6, 2, 1, 2, 4, 262, 6, 10, 9, 419, 13, 1495, 6, 110, 6, 6, 9, 4759, 9, 787719, 239];

function isInAstralSet(code, set) {
  let pos = 0x10000;

  for (let i = 0, length = set.length; i < length; i += 2) {
    pos += set[i];
    if (pos > code) return false;
    pos += set[i + 1];
    if (pos >= code) return true;
  }

  return false;
}

function isIdentifierStart(code) {
  if (code < 65) return code === 36;
  if (code <= 90) return true;
  if (code < 97) return code === 95;
  if (code <= 122) return true;

  if (code <= 0xffff) {
    return code >= 0xaa && nonASCIIidentifierStart.test(String.fromCharCode(code));
  }

  return isInAstralSet(code, astralIdentifierStartCodes);
}
function isIdentifierChar(code) {
  if (code < 48) return code === 36;
  if (code < 58) return true;
  if (code < 65) return false;
  if (code <= 90) return true;
  if (code < 97) return code === 95;
  if (code <= 122) return true;

  if (code <= 0xffff) {
    return code >= 0xaa && nonASCIIidentifier.test(String.fromCharCode(code));
  }

  return isInAstralSet(code, astralIdentifierStartCodes) || isInAstralSet(code, astralIdentifierCodes);
}

const reservedWords = {
  keyword: ["break", "case", "catch", "continue", "debugger", "default", "do", "else", "finally", "for", "function", "if", "return", "switch", "throw", "try", "var", "const", "while", "with", "new", "this", "super", "class", "extends", "export", "import", "null", "true", "false", "in", "instanceof", "typeof", "void", "delete"],
  strict: ["implements", "interface", "let", "package", "private", "protected", "public", "static", "yield"],
  strictBind: ["eval", "arguments"]
};
const keywords = new Set(reservedWords.keyword);
const reservedWordsStrictSet = new Set(reservedWords.strict);
const reservedWordsStrictBindSet = new Set(reservedWords.strictBind);
function isReservedWord(word, inModule) {
  return inModule && word === "await" || word === "enum";
}
function isStrictReservedWord(word, inModule) {
  return isReservedWord(word, inModule) || reservedWordsStrictSet.has(word);
}
function isStrictBindOnlyReservedWord(word) {
  return reservedWordsStrictBindSet.has(word);
}
function isStrictBindReservedWord(word, inModule) {
  return isStrictReservedWord(word, inModule) || isStrictBindOnlyReservedWord(word);
}
function isKeyword(word) {
  return keywords.has(word);
}

function isIteratorStart(current, next) {
  return current === 64 && next === 64;
}
const reservedWordLikeSet = new Set(["break", "case", "catch", "continue", "debugger", "default", "do", "else", "finally", "for", "function", "if", "return", "switch", "throw", "try", "var", "const", "while", "with", "new", "this", "super", "class", "extends", "export", "import", "null", "true", "false", "in", "instanceof", "typeof", "void", "delete", "implements", "interface", "let", "package", "private", "protected", "public", "static", "yield", "eval", "arguments", "enum", "await"]);
function canBeReservedWord(word) {
  return reservedWordLikeSet.has(word);
}

const SCOPE_OTHER = 0b000000000,
      SCOPE_PROGRAM = 0b000000001,
      SCOPE_FUNCTION = 0b000000010,
      SCOPE_ARROW = 0b000000100,
      SCOPE_SIMPLE_CATCH = 0b000001000,
      SCOPE_SUPER = 0b000010000,
      SCOPE_DIRECT_SUPER = 0b000100000,
      SCOPE_CLASS = 0b001000000,
      SCOPE_STATIC_BLOCK = 0b010000000,
      SCOPE_TS_MODULE = 0b100000000,
      SCOPE_VAR = SCOPE_PROGRAM | SCOPE_FUNCTION | SCOPE_TS_MODULE;
const BIND_KIND_VALUE = 0b000000000001,
      BIND_KIND_TYPE = 0b000000000010,
      BIND_SCOPE_VAR = 0b000000000100,
      BIND_SCOPE_LEXICAL = 0b000000001000,
      BIND_SCOPE_FUNCTION = 0b000000010000,
      BIND_FLAGS_NONE = 0b000001000000,
      BIND_FLAGS_CLASS = 0b000010000000,
      BIND_FLAGS_TS_ENUM = 0b000100000000,
      BIND_FLAGS_TS_CONST_ENUM = 0b001000000000,
      BIND_FLAGS_TS_EXPORT_ONLY = 0b010000000000,
      BIND_FLAGS_FLOW_DECLARE_FN = 0b100000000000;
const BIND_CLASS = BIND_KIND_VALUE | BIND_KIND_TYPE | BIND_SCOPE_LEXICAL | BIND_FLAGS_CLASS,
      BIND_LEXICAL = BIND_KIND_VALUE | 0 | BIND_SCOPE_LEXICAL | 0,
      BIND_VAR = BIND_KIND_VALUE | 0 | BIND_SCOPE_VAR | 0,
      BIND_FUNCTION = BIND_KIND_VALUE | 0 | BIND_SCOPE_FUNCTION | 0,
      BIND_TS_INTERFACE = 0 | BIND_KIND_TYPE | 0 | BIND_FLAGS_CLASS,
      BIND_TS_TYPE = 0 | BIND_KIND_TYPE | 0 | 0,
      BIND_TS_ENUM = BIND_KIND_VALUE | BIND_KIND_TYPE | BIND_SCOPE_LEXICAL | BIND_FLAGS_TS_ENUM,
      BIND_TS_AMBIENT = 0 | 0 | 0 | BIND_FLAGS_TS_EXPORT_ONLY,
      BIND_NONE = 0 | 0 | 0 | BIND_FLAGS_NONE,
      BIND_OUTSIDE = BIND_KIND_VALUE | 0 | 0 | BIND_FLAGS_NONE,
      BIND_TS_CONST_ENUM = BIND_TS_ENUM | BIND_FLAGS_TS_CONST_ENUM,
      BIND_TS_NAMESPACE = 0 | 0 | 0 | BIND_FLAGS_TS_EXPORT_ONLY,
      BIND_FLOW_DECLARE_FN = BIND_FLAGS_FLOW_DECLARE_FN;
const CLASS_ELEMENT_FLAG_STATIC = 0b100,
      CLASS_ELEMENT_KIND_GETTER = 0b010,
      CLASS_ELEMENT_KIND_SETTER = 0b001,
      CLASS_ELEMENT_KIND_ACCESSOR = CLASS_ELEMENT_KIND_GETTER | CLASS_ELEMENT_KIND_SETTER;
const CLASS_ELEMENT_STATIC_GETTER = CLASS_ELEMENT_KIND_GETTER | CLASS_ELEMENT_FLAG_STATIC,
      CLASS_ELEMENT_STATIC_SETTER = CLASS_ELEMENT_KIND_SETTER | CLASS_ELEMENT_FLAG_STATIC,
      CLASS_ELEMENT_INSTANCE_GETTER = CLASS_ELEMENT_KIND_GETTER,
      CLASS_ELEMENT_INSTANCE_SETTER = CLASS_ELEMENT_KIND_SETTER,
      CLASS_ELEMENT_OTHER = 0;

class Scope {
  constructor(flags) {
    this.var = new Set();
    this.lexical = new Set();
    this.functions = new Set();
    this.flags = flags;
  }

}
class ScopeHandler {
  constructor(raise, inModule) {
    this.scopeStack = [];
    this.undefinedExports = new Map();
    this.undefinedPrivateNames = new Map();
    this.raise = raise;
    this.inModule = inModule;
  }

  get inFunction() {
    return (this.currentVarScopeFlags() & SCOPE_FUNCTION) > 0;
  }

  get allowSuper() {
    return (this.currentThisScopeFlags() & SCOPE_SUPER) > 0;
  }

  get allowDirectSuper() {
    return (this.currentThisScopeFlags() & SCOPE_DIRECT_SUPER) > 0;
  }

  get inClass() {
    return (this.currentThisScopeFlags() & SCOPE_CLASS) > 0;
  }

  get inClassAndNotInNonArrowFunction() {
    const flags = this.currentThisScopeFlags();
    return (flags & SCOPE_CLASS) > 0 && (flags & SCOPE_FUNCTION) === 0;
  }

  get inStaticBlock() {
    return (this.currentThisScopeFlags() & SCOPE_STATIC_BLOCK) > 0;
  }

  get inNonArrowFunction() {
    return (this.currentThisScopeFlags() & SCOPE_FUNCTION) > 0;
  }

  get treatFunctionsAsVar() {
    return this.treatFunctionsAsVarInScope(this.currentScope());
  }

  createScope(flags) {
    return new Scope(flags);
  }

  enter(flags) {
    this.scopeStack.push(this.createScope(flags));
  }

  exit() {
    this.scopeStack.pop();
  }

  treatFunctionsAsVarInScope(scope) {
    return !!(scope.flags & SCOPE_FUNCTION || !this.inModule && scope.flags & SCOPE_PROGRAM);
  }

  declareName(name, bindingType, pos) {
    let scope = this.currentScope();

    if (bindingType & BIND_SCOPE_LEXICAL || bindingType & BIND_SCOPE_FUNCTION) {
      this.checkRedeclarationInScope(scope, name, bindingType, pos);

      if (bindingType & BIND_SCOPE_FUNCTION) {
        scope.functions.add(name);
      } else {
        scope.lexical.add(name);
      }

      if (bindingType & BIND_SCOPE_LEXICAL) {
        this.maybeExportDefined(scope, name);
      }
    } else if (bindingType & BIND_SCOPE_VAR) {
      for (let i = this.scopeStack.length - 1; i >= 0; --i) {
        scope = this.scopeStack[i];
        this.checkRedeclarationInScope(scope, name, bindingType, pos);
        scope.var.add(name);
        this.maybeExportDefined(scope, name);
        if (scope.flags & SCOPE_VAR) break;
      }
    }

    if (this.inModule && scope.flags & SCOPE_PROGRAM) {
      this.undefinedExports.delete(name);
    }
  }

  maybeExportDefined(scope, name) {
    if (this.inModule && scope.flags & SCOPE_PROGRAM) {
      this.undefinedExports.delete(name);
    }
  }

  checkRedeclarationInScope(scope, name, bindingType, pos) {
    if (this.isRedeclaredInScope(scope, name, bindingType)) {
      this.raise(pos, ErrorMessages.VarRedeclaration, name);
    }
  }

  isRedeclaredInScope(scope, name, bindingType) {
    if (!(bindingType & BIND_KIND_VALUE)) return false;

    if (bindingType & BIND_SCOPE_LEXICAL) {
      return scope.lexical.has(name) || scope.functions.has(name) || scope.var.has(name);
    }

    if (bindingType & BIND_SCOPE_FUNCTION) {
      return scope.lexical.has(name) || !this.treatFunctionsAsVarInScope(scope) && scope.var.has(name);
    }

    return scope.lexical.has(name) && !(scope.flags & SCOPE_SIMPLE_CATCH && scope.lexical.values().next().value === name) || !this.treatFunctionsAsVarInScope(scope) && scope.functions.has(name);
  }

  checkLocalExport(id) {
    const {
      name
    } = id;
    const topLevelScope = this.scopeStack[0];

    if (!topLevelScope.lexical.has(name) && !topLevelScope.var.has(name) && !topLevelScope.functions.has(name)) {
      this.undefinedExports.set(name, id.start);
    }
  }

  currentScope() {
    return this.scopeStack[this.scopeStack.length - 1];
  }

  currentVarScopeFlags() {
    for (let i = this.scopeStack.length - 1;; i--) {
      const {
        flags
      } = this.scopeStack[i];

      if (flags & SCOPE_VAR) {
        return flags;
      }
    }
  }

  currentThisScopeFlags() {
    for (let i = this.scopeStack.length - 1;; i--) {
      const {
        flags
      } = this.scopeStack[i];

      if (flags & (SCOPE_VAR | SCOPE_CLASS) && !(flags & SCOPE_ARROW)) {
        return flags;
      }
    }
  }

}

class FlowScope extends Scope {
  constructor(...args) {
    super(...args);
    this.declareFunctions = new Set();
  }

}

class FlowScopeHandler extends ScopeHandler {
  createScope(flags) {
    return new FlowScope(flags);
  }

  declareName(name, bindingType, pos) {
    const scope = this.currentScope();

    if (bindingType & BIND_FLAGS_FLOW_DECLARE_FN) {
      this.checkRedeclarationInScope(scope, name, bindingType, pos);
      this.maybeExportDefined(scope, name);
      scope.declareFunctions.add(name);
      return;
    }

    super.declareName(...arguments);
  }

  isRedeclaredInScope(scope, name, bindingType) {
    if (super.isRedeclaredInScope(...arguments)) return true;

    if (bindingType & BIND_FLAGS_FLOW_DECLARE_FN) {
      return !scope.declareFunctions.has(name) && (scope.lexical.has(name) || scope.functions.has(name));
    }

    return false;
  }

  checkLocalExport(id) {
    if (!this.scopeStack[0].declareFunctions.has(id.name)) {
      super.checkLocalExport(id);
    }
  }

}

const reservedTypes = new Set(["_", "any", "bool", "boolean", "empty", "extends", "false", "interface", "mixed", "null", "number", "static", "string", "true", "typeof", "void"]);
const FlowErrors = makeErrorTemplates({
  AmbiguousConditionalArrow: "Ambiguous expression: wrap the arrow functions in parentheses to disambiguate.",
  AmbiguousDeclareModuleKind: "Found both `declare module.exports` and `declare export` in the same module. Modules can only have 1 since they are either an ES module or they are a CommonJS module.",
  AssignReservedType: "Cannot overwrite reserved type %0.",
  DeclareClassElement: "The `declare` modifier can only appear on class fields.",
  DeclareClassFieldInitializer: "Initializers are not allowed in fields with the `declare` modifier.",
  DuplicateDeclareModuleExports: "Duplicate `declare module.exports` statement.",
  EnumBooleanMemberNotInitialized: "Boolean enum members need to be initialized. Use either `%0 = true,` or `%0 = false,` in enum `%1`.",
  EnumDuplicateMemberName: "Enum member names need to be unique, but the name `%0` has already been used before in enum `%1`.",
  EnumInconsistentMemberValues: "Enum `%0` has inconsistent member initializers. Either use no initializers, or consistently use literals (either booleans, numbers, or strings) for all member initializers.",
  EnumInvalidExplicitType: "Enum type `%1` is not valid. Use one of `boolean`, `number`, `string`, or `symbol` in enum `%0`.",
  EnumInvalidExplicitTypeUnknownSupplied: "Supplied enum type is not valid. Use one of `boolean`, `number`, `string`, or `symbol` in enum `%0`.",
  EnumInvalidMemberInitializerPrimaryType: "Enum `%0` has type `%2`, so the initializer of `%1` needs to be a %2 literal.",
  EnumInvalidMemberInitializerSymbolType: "Symbol enum members cannot be initialized. Use `%1,` in enum `%0`.",
  EnumInvalidMemberInitializerUnknownType: "The enum member initializer for `%1` needs to be a literal (either a boolean, number, or string) in enum `%0`.",
  EnumInvalidMemberName: "Enum member names cannot start with lowercase 'a' through 'z'. Instead of using `%0`, consider using `%1`, in enum `%2`.",
  EnumNumberMemberNotInitialized: "Number enum members need to be initialized, e.g. `%1 = 1` in enum `%0`.",
  EnumStringMemberInconsistentlyInitailized: "String enum members need to consistently either all use initializers, or use no initializers, in enum `%0`.",
  GetterMayNotHaveThisParam: "A getter cannot have a `this` parameter.",
  ImportTypeShorthandOnlyInPureImport: "The `type` and `typeof` keywords on named imports can only be used on regular `import` statements. It cannot be used with `import type` or `import typeof` statements.",
  InexactInsideExact: "Explicit inexact syntax cannot appear inside an explicit exact object type.",
  InexactInsideNonObject: "Explicit inexact syntax cannot appear in class or interface definitions.",
  InexactVariance: "Explicit inexact syntax cannot have variance.",
  InvalidNonTypeImportInDeclareModule: "Imports within a `declare module` body must always be `import type` or `import typeof`.",
  MissingTypeParamDefault: "Type parameter declaration needs a default, since a preceding type parameter declaration has a default.",
  NestedDeclareModule: "`declare module` cannot be used inside another `declare module`.",
  NestedFlowComment: "Cannot have a flow comment inside another flow comment.",
  OptionalBindingPattern: "A binding pattern parameter cannot be optional in an implementation signature.",
  SetterMayNotHaveThisParam: "A setter cannot have a `this` parameter.",
  SpreadVariance: "Spread properties cannot have variance.",
  ThisParamAnnotationRequired: "A type annotation is required for the `this` parameter.",
  ThisParamBannedInConstructor: "Constructors cannot have a `this` parameter; constructors don't bind `this` like other functions.",
  ThisParamMayNotBeOptional: "The `this` parameter cannot be optional.",
  ThisParamMustBeFirst: "The `this` parameter must be the first function parameter.",
  ThisParamNoDefault: "The `this` parameter may not have a default value.",
  TypeBeforeInitializer: "Type annotations must come before default assignments, e.g. instead of `age = 25: number` use `age: number = 25`.",
  TypeCastInPattern: "The type cast expression is expected to be wrapped with parenthesis.",
  UnexpectedExplicitInexactInObject: "Explicit inexact syntax must appear at the end of an inexact object.",
  UnexpectedReservedType: "Unexpected reserved type %0.",
  UnexpectedReservedUnderscore: "`_` is only allowed as a type argument to call or new.",
  UnexpectedSpaceBetweenModuloChecks: "Spaces between `%` and `checks` are not allowed here.",
  UnexpectedSpreadType: "Spread operator cannot appear in class or interface definitions.",
  UnexpectedSubtractionOperand: 'Unexpected token, expected "number" or "bigint".',
  UnexpectedTokenAfterTypeParameter: "Expected an arrow function after this type parameter declaration.",
  UnexpectedTypeParameterBeforeAsyncArrowFunction: "Type parameters must come after the async keyword, e.g. instead of `<T> async () => {}`, use `async <T>() => {}`.",
  UnsupportedDeclareExportKind: "`declare export %0` is not supported. Use `%1` instead.",
  UnsupportedStatementInDeclareModule: "Only declares and type imports are allowed inside declare module.",
  UnterminatedFlowComment: "Unterminated flow-comment."
}, ErrorCodes.SyntaxError);

function isEsModuleType(bodyElement) {
  return bodyElement.type === "DeclareExportAllDeclaration" || bodyElement.type === "DeclareExportDeclaration" && (!bodyElement.declaration || bodyElement.declaration.type !== "TypeAlias" && bodyElement.declaration.type !== "InterfaceDeclaration");
}

function hasTypeImportKind(node) {
  return node.importKind === "type" || node.importKind === "typeof";
}

function isMaybeDefaultImport(state) {
  return (state.type === types$1.name || !!state.type.keyword) && state.value !== "from";
}

const exportSuggestions = {
  const: "declare export var",
  let: "declare export var",
  type: "export type",
  interface: "export interface"
};

function partition(list, test) {
  const list1 = [];
  const list2 = [];

  for (let i = 0; i < list.length; i++) {
    (test(list[i], i, list) ? list1 : list2).push(list[i]);
  }

  return [list1, list2];
}

const FLOW_PRAGMA_REGEX = /\*?\s*@((?:no)?flow)\b/;
var flow = (superClass => class extends superClass {
  constructor(...args) {
    super(...args);
    this.flowPragma = undefined;
  }

  getScopeHandler() {
    return FlowScopeHandler;
  }

  shouldParseTypes() {
    return this.getPluginOption("flow", "all") || this.flowPragma === "flow";
  }

  shouldParseEnums() {
    return !!this.getPluginOption("flow", "enums");
  }

  finishToken(type, val) {
    if (type !== types$1.string && type !== types$1.semi && type !== types$1.interpreterDirective) {
      if (this.flowPragma === undefined) {
        this.flowPragma = null;
      }
    }

    return super.finishToken(type, val);
  }

  addComment(comment) {
    if (this.flowPragma === undefined) {
      const matches = FLOW_PRAGMA_REGEX.exec(comment.value);

      if (!matches) ; else if (matches[1] === "flow") {
        this.flowPragma = "flow";
      } else if (matches[1] === "noflow") {
        this.flowPragma = "noflow";
      } else {
        throw new Error("Unexpected flow pragma");
      }
    }

    return super.addComment(comment);
  }

  flowParseTypeInitialiser(tok) {
    const oldInType = this.state.inType;
    this.state.inType = true;
    this.expect(tok || types$1.colon);
    const type = this.flowParseType();
    this.state.inType = oldInType;
    return type;
  }

  flowParsePredicate() {
    const node = this.startNode();
    const moduloPos = this.state.start;
    this.next();
    this.expectContextual("checks");

    if (this.state.lastTokStart > moduloPos + 1) {
      this.raise(moduloPos, FlowErrors.UnexpectedSpaceBetweenModuloChecks);
    }

    if (this.eat(types$1.parenL)) {
      node.value = this.parseExpression();
      this.expect(types$1.parenR);
      return this.finishNode(node, "DeclaredPredicate");
    } else {
      return this.finishNode(node, "InferredPredicate");
    }
  }

  flowParseTypeAndPredicateInitialiser() {
    const oldInType = this.state.inType;
    this.state.inType = true;
    this.expect(types$1.colon);
    let type = null;
    let predicate = null;

    if (this.match(types$1.modulo)) {
      this.state.inType = oldInType;
      predicate = this.flowParsePredicate();
    } else {
      type = this.flowParseType();
      this.state.inType = oldInType;

      if (this.match(types$1.modulo)) {
        predicate = this.flowParsePredicate();
      }
    }

    return [type, predicate];
  }

  flowParseDeclareClass(node) {
    this.next();
    this.flowParseInterfaceish(node, true);
    return this.finishNode(node, "DeclareClass");
  }

  flowParseDeclareFunction(node) {
    this.next();
    const id = node.id = this.parseIdentifier();
    const typeNode = this.startNode();
    const typeContainer = this.startNode();

    if (this.isRelational("<")) {
      typeNode.typeParameters = this.flowParseTypeParameterDeclaration();
    } else {
      typeNode.typeParameters = null;
    }

    this.expect(types$1.parenL);
    const tmp = this.flowParseFunctionTypeParams();
    typeNode.params = tmp.params;
    typeNode.rest = tmp.rest;
    typeNode.this = tmp._this;
    this.expect(types$1.parenR);
    [typeNode.returnType, node.predicate] = this.flowParseTypeAndPredicateInitialiser();
    typeContainer.typeAnnotation = this.finishNode(typeNode, "FunctionTypeAnnotation");
    id.typeAnnotation = this.finishNode(typeContainer, "TypeAnnotation");
    this.resetEndLocation(id);
    this.semicolon();
    this.scope.declareName(node.id.name, BIND_FLOW_DECLARE_FN, node.id.start);
    return this.finishNode(node, "DeclareFunction");
  }

  flowParseDeclare(node, insideModule) {
    if (this.match(types$1._class)) {
      return this.flowParseDeclareClass(node);
    } else if (this.match(types$1._function)) {
      return this.flowParseDeclareFunction(node);
    } else if (this.match(types$1._var)) {
      return this.flowParseDeclareVariable(node);
    } else if (this.eatContextual("module")) {
      if (this.match(types$1.dot)) {
        return this.flowParseDeclareModuleExports(node);
      } else {
        if (insideModule) {
          this.raise(this.state.lastTokStart, FlowErrors.NestedDeclareModule);
        }

        return this.flowParseDeclareModule(node);
      }
    } else if (this.isContextual("type")) {
      return this.flowParseDeclareTypeAlias(node);
    } else if (this.isContextual("opaque")) {
      return this.flowParseDeclareOpaqueType(node);
    } else if (this.isContextual("interface")) {
      return this.flowParseDeclareInterface(node);
    } else if (this.match(types$1._export)) {
      return this.flowParseDeclareExportDeclaration(node, insideModule);
    } else {
      throw this.unexpected();
    }
  }

  flowParseDeclareVariable(node) {
    this.next();
    node.id = this.flowParseTypeAnnotatableIdentifier(true);
    this.scope.declareName(node.id.name, BIND_VAR, node.id.start);
    this.semicolon();
    return this.finishNode(node, "DeclareVariable");
  }

  flowParseDeclareModule(node) {
    this.scope.enter(SCOPE_OTHER);

    if (this.match(types$1.string)) {
      node.id = this.parseExprAtom();
    } else {
      node.id = this.parseIdentifier();
    }

    const bodyNode = node.body = this.startNode();
    const body = bodyNode.body = [];
    this.expect(types$1.braceL);

    while (!this.match(types$1.braceR)) {
      let bodyNode = this.startNode();

      if (this.match(types$1._import)) {
        this.next();

        if (!this.isContextual("type") && !this.match(types$1._typeof)) {
          this.raise(this.state.lastTokStart, FlowErrors.InvalidNonTypeImportInDeclareModule);
        }

        this.parseImport(bodyNode);
      } else {
        this.expectContextual("declare", FlowErrors.UnsupportedStatementInDeclareModule);
        bodyNode = this.flowParseDeclare(bodyNode, true);
      }

      body.push(bodyNode);
    }

    this.scope.exit();
    this.expect(types$1.braceR);
    this.finishNode(bodyNode, "BlockStatement");
    let kind = null;
    let hasModuleExport = false;
    body.forEach(bodyElement => {
      if (isEsModuleType(bodyElement)) {
        if (kind === "CommonJS") {
          this.raise(bodyElement.start, FlowErrors.AmbiguousDeclareModuleKind);
        }

        kind = "ES";
      } else if (bodyElement.type === "DeclareModuleExports") {
        if (hasModuleExport) {
          this.raise(bodyElement.start, FlowErrors.DuplicateDeclareModuleExports);
        }

        if (kind === "ES") {
          this.raise(bodyElement.start, FlowErrors.AmbiguousDeclareModuleKind);
        }

        kind = "CommonJS";
        hasModuleExport = true;
      }
    });
    node.kind = kind || "CommonJS";
    return this.finishNode(node, "DeclareModule");
  }

  flowParseDeclareExportDeclaration(node, insideModule) {
    this.expect(types$1._export);

    if (this.eat(types$1._default)) {
      if (this.match(types$1._function) || this.match(types$1._class)) {
        node.declaration = this.flowParseDeclare(this.startNode());
      } else {
        node.declaration = this.flowParseType();
        this.semicolon();
      }

      node.default = true;
      return this.finishNode(node, "DeclareExportDeclaration");
    } else {
      if (this.match(types$1._const) || this.isLet() || (this.isContextual("type") || this.isContextual("interface")) && !insideModule) {
        const label = this.state.value;
        const suggestion = exportSuggestions[label];
        throw this.raise(this.state.start, FlowErrors.UnsupportedDeclareExportKind, label, suggestion);
      }

      if (this.match(types$1._var) || this.match(types$1._function) || this.match(types$1._class) || this.isContextual("opaque")) {
          node.declaration = this.flowParseDeclare(this.startNode());
          node.default = false;
          return this.finishNode(node, "DeclareExportDeclaration");
        } else if (this.match(types$1.star) || this.match(types$1.braceL) || this.isContextual("interface") || this.isContextual("type") || this.isContextual("opaque")) {
          node = this.parseExport(node);

          if (node.type === "ExportNamedDeclaration") {
            node.type = "ExportDeclaration";
            node.default = false;
            delete node.exportKind;
          }

          node.type = "Declare" + node.type;
          return node;
        }
    }

    throw this.unexpected();
  }

  flowParseDeclareModuleExports(node) {
    this.next();
    this.expectContextual("exports");
    node.typeAnnotation = this.flowParseTypeAnnotation();
    this.semicolon();
    return this.finishNode(node, "DeclareModuleExports");
  }

  flowParseDeclareTypeAlias(node) {
    this.next();
    this.flowParseTypeAlias(node);
    node.type = "DeclareTypeAlias";
    return node;
  }

  flowParseDeclareOpaqueType(node) {
    this.next();
    this.flowParseOpaqueType(node, true);
    node.type = "DeclareOpaqueType";
    return node;
  }

  flowParseDeclareInterface(node) {
    this.next();
    this.flowParseInterfaceish(node);
    return this.finishNode(node, "DeclareInterface");
  }

  flowParseInterfaceish(node, isClass = false) {
    node.id = this.flowParseRestrictedIdentifier(!isClass, true);
    this.scope.declareName(node.id.name, isClass ? BIND_FUNCTION : BIND_LEXICAL, node.id.start);

    if (this.isRelational("<")) {
      node.typeParameters = this.flowParseTypeParameterDeclaration();
    } else {
      node.typeParameters = null;
    }

    node.extends = [];
    node.implements = [];
    node.mixins = [];

    if (this.eat(types$1._extends)) {
      do {
        node.extends.push(this.flowParseInterfaceExtends());
      } while (!isClass && this.eat(types$1.comma));
    }

    if (this.isContextual("mixins")) {
      this.next();

      do {
        node.mixins.push(this.flowParseInterfaceExtends());
      } while (this.eat(types$1.comma));
    }

    if (this.isContextual("implements")) {
      this.next();

      do {
        node.implements.push(this.flowParseInterfaceExtends());
      } while (this.eat(types$1.comma));
    }

    node.body = this.flowParseObjectType({
      allowStatic: isClass,
      allowExact: false,
      allowSpread: false,
      allowProto: isClass,
      allowInexact: false
    });
  }

  flowParseInterfaceExtends() {
    const node = this.startNode();
    node.id = this.flowParseQualifiedTypeIdentifier();

    if (this.isRelational("<")) {
      node.typeParameters = this.flowParseTypeParameterInstantiation();
    } else {
      node.typeParameters = null;
    }

    return this.finishNode(node, "InterfaceExtends");
  }

  flowParseInterface(node) {
    this.flowParseInterfaceish(node);
    return this.finishNode(node, "InterfaceDeclaration");
  }

  checkNotUnderscore(word) {
    if (word === "_") {
      this.raise(this.state.start, FlowErrors.UnexpectedReservedUnderscore);
    }
  }

  checkReservedType(word, startLoc, declaration) {
    if (!reservedTypes.has(word)) return;
    this.raise(startLoc, declaration ? FlowErrors.AssignReservedType : FlowErrors.UnexpectedReservedType, word);
  }

  flowParseRestrictedIdentifier(liberal, declaration) {
    this.checkReservedType(this.state.value, this.state.start, declaration);
    return this.parseIdentifier(liberal);
  }

  flowParseTypeAlias(node) {
    node.id = this.flowParseRestrictedIdentifier(false, true);
    this.scope.declareName(node.id.name, BIND_LEXICAL, node.id.start);

    if (this.isRelational("<")) {
      node.typeParameters = this.flowParseTypeParameterDeclaration();
    } else {
      node.typeParameters = null;
    }

    node.right = this.flowParseTypeInitialiser(types$1.eq);
    this.semicolon();
    return this.finishNode(node, "TypeAlias");
  }

  flowParseOpaqueType(node, declare) {
    this.expectContextual("type");
    node.id = this.flowParseRestrictedIdentifier(true, true);
    this.scope.declareName(node.id.name, BIND_LEXICAL, node.id.start);

    if (this.isRelational("<")) {
      node.typeParameters = this.flowParseTypeParameterDeclaration();
    } else {
      node.typeParameters = null;
    }

    node.supertype = null;

    if (this.match(types$1.colon)) {
      node.supertype = this.flowParseTypeInitialiser(types$1.colon);
    }

    node.impltype = null;

    if (!declare) {
      node.impltype = this.flowParseTypeInitialiser(types$1.eq);
    }

    this.semicolon();
    return this.finishNode(node, "OpaqueType");
  }

  flowParseTypeParameter(requireDefault = false) {
    const nodeStart = this.state.start;
    const node = this.startNode();
    const variance = this.flowParseVariance();
    const ident = this.flowParseTypeAnnotatableIdentifier();
    node.name = ident.name;
    node.variance = variance;
    node.bound = ident.typeAnnotation;

    if (this.match(types$1.eq)) {
      this.eat(types$1.eq);
      node.default = this.flowParseType();
    } else {
      if (requireDefault) {
        this.raise(nodeStart, FlowErrors.MissingTypeParamDefault);
      }
    }

    return this.finishNode(node, "TypeParameter");
  }

  flowParseTypeParameterDeclaration() {
    const oldInType = this.state.inType;
    const node = this.startNode();
    node.params = [];
    this.state.inType = true;

    if (this.isRelational("<") || this.match(types$1.jsxTagStart)) {
      this.next();
    } else {
      this.unexpected();
    }

    let defaultRequired = false;

    do {
      const typeParameter = this.flowParseTypeParameter(defaultRequired);
      node.params.push(typeParameter);

      if (typeParameter.default) {
        defaultRequired = true;
      }

      if (!this.isRelational(">")) {
        this.expect(types$1.comma);
      }
    } while (!this.isRelational(">"));

    this.expectRelational(">");
    this.state.inType = oldInType;
    return this.finishNode(node, "TypeParameterDeclaration");
  }

  flowParseTypeParameterInstantiation() {
    const node = this.startNode();
    const oldInType = this.state.inType;
    node.params = [];
    this.state.inType = true;
    this.expectRelational("<");
    const oldNoAnonFunctionType = this.state.noAnonFunctionType;
    this.state.noAnonFunctionType = false;

    while (!this.isRelational(">")) {
      node.params.push(this.flowParseType());

      if (!this.isRelational(">")) {
        this.expect(types$1.comma);
      }
    }

    this.state.noAnonFunctionType = oldNoAnonFunctionType;
    this.expectRelational(">");
    this.state.inType = oldInType;
    return this.finishNode(node, "TypeParameterInstantiation");
  }

  flowParseTypeParameterInstantiationCallOrNew() {
    const node = this.startNode();
    const oldInType = this.state.inType;
    node.params = [];
    this.state.inType = true;
    this.expectRelational("<");

    while (!this.isRelational(">")) {
      node.params.push(this.flowParseTypeOrImplicitInstantiation());

      if (!this.isRelational(">")) {
        this.expect(types$1.comma);
      }
    }

    this.expectRelational(">");
    this.state.inType = oldInType;
    return this.finishNode(node, "TypeParameterInstantiation");
  }

  flowParseInterfaceType() {
    const node = this.startNode();
    this.expectContextual("interface");
    node.extends = [];

    if (this.eat(types$1._extends)) {
      do {
        node.extends.push(this.flowParseInterfaceExtends());
      } while (this.eat(types$1.comma));
    }

    node.body = this.flowParseObjectType({
      allowStatic: false,
      allowExact: false,
      allowSpread: false,
      allowProto: false,
      allowInexact: false
    });
    return this.finishNode(node, "InterfaceTypeAnnotation");
  }

  flowParseObjectPropertyKey() {
    return this.match(types$1.num) || this.match(types$1.string) ? this.parseExprAtom() : this.parseIdentifier(true);
  }

  flowParseObjectTypeIndexer(node, isStatic, variance) {
    node.static = isStatic;

    if (this.lookahead().type === types$1.colon) {
      node.id = this.flowParseObjectPropertyKey();
      node.key = this.flowParseTypeInitialiser();
    } else {
      node.id = null;
      node.key = this.flowParseType();
    }

    this.expect(types$1.bracketR);
    node.value = this.flowParseTypeInitialiser();
    node.variance = variance;
    return this.finishNode(node, "ObjectTypeIndexer");
  }

  flowParseObjectTypeInternalSlot(node, isStatic) {
    node.static = isStatic;
    node.id = this.flowParseObjectPropertyKey();
    this.expect(types$1.bracketR);
    this.expect(types$1.bracketR);

    if (this.isRelational("<") || this.match(types$1.parenL)) {
      node.method = true;
      node.optional = false;
      node.value = this.flowParseObjectTypeMethodish(this.startNodeAt(node.start, node.loc.start));
    } else {
      node.method = false;

      if (this.eat(types$1.question)) {
        node.optional = true;
      }

      node.value = this.flowParseTypeInitialiser();
    }

    return this.finishNode(node, "ObjectTypeInternalSlot");
  }

  flowParseObjectTypeMethodish(node) {
    node.params = [];
    node.rest = null;
    node.typeParameters = null;
    node.this = null;

    if (this.isRelational("<")) {
      node.typeParameters = this.flowParseTypeParameterDeclaration();
    }

    this.expect(types$1.parenL);

    if (this.match(types$1._this)) {
      node.this = this.flowParseFunctionTypeParam(true);
      node.this.name = null;

      if (!this.match(types$1.parenR)) {
        this.expect(types$1.comma);
      }
    }

    while (!this.match(types$1.parenR) && !this.match(types$1.ellipsis)) {
      node.params.push(this.flowParseFunctionTypeParam(false));

      if (!this.match(types$1.parenR)) {
        this.expect(types$1.comma);
      }
    }

    if (this.eat(types$1.ellipsis)) {
      node.rest = this.flowParseFunctionTypeParam(false);
    }

    this.expect(types$1.parenR);
    node.returnType = this.flowParseTypeInitialiser();
    return this.finishNode(node, "FunctionTypeAnnotation");
  }

  flowParseObjectTypeCallProperty(node, isStatic) {
    const valueNode = this.startNode();
    node.static = isStatic;
    node.value = this.flowParseObjectTypeMethodish(valueNode);
    return this.finishNode(node, "ObjectTypeCallProperty");
  }

  flowParseObjectType({
    allowStatic,
    allowExact,
    allowSpread,
    allowProto,
    allowInexact
  }) {
    const oldInType = this.state.inType;
    this.state.inType = true;
    const nodeStart = this.startNode();
    nodeStart.callProperties = [];
    nodeStart.properties = [];
    nodeStart.indexers = [];
    nodeStart.internalSlots = [];
    let endDelim;
    let exact;
    let inexact = false;

    if (allowExact && this.match(types$1.braceBarL)) {
      this.expect(types$1.braceBarL);
      endDelim = types$1.braceBarR;
      exact = true;
    } else {
      this.expect(types$1.braceL);
      endDelim = types$1.braceR;
      exact = false;
    }

    nodeStart.exact = exact;

    while (!this.match(endDelim)) {
      let isStatic = false;
      let protoStart = null;
      let inexactStart = null;
      const node = this.startNode();

      if (allowProto && this.isContextual("proto")) {
        const lookahead = this.lookahead();

        if (lookahead.type !== types$1.colon && lookahead.type !== types$1.question) {
          this.next();
          protoStart = this.state.start;
          allowStatic = false;
        }
      }

      if (allowStatic && this.isContextual("static")) {
        const lookahead = this.lookahead();

        if (lookahead.type !== types$1.colon && lookahead.type !== types$1.question) {
          this.next();
          isStatic = true;
        }
      }

      const variance = this.flowParseVariance();

      if (this.eat(types$1.bracketL)) {
        if (protoStart != null) {
          this.unexpected(protoStart);
        }

        if (this.eat(types$1.bracketL)) {
          if (variance) {
            this.unexpected(variance.start);
          }

          nodeStart.internalSlots.push(this.flowParseObjectTypeInternalSlot(node, isStatic));
        } else {
          nodeStart.indexers.push(this.flowParseObjectTypeIndexer(node, isStatic, variance));
        }
      } else if (this.match(types$1.parenL) || this.isRelational("<")) {
        if (protoStart != null) {
          this.unexpected(protoStart);
        }

        if (variance) {
          this.unexpected(variance.start);
        }

        nodeStart.callProperties.push(this.flowParseObjectTypeCallProperty(node, isStatic));
      } else {
        let kind = "init";

        if (this.isContextual("get") || this.isContextual("set")) {
          const lookahead = this.lookahead();

          if (lookahead.type === types$1.name || lookahead.type === types$1.string || lookahead.type === types$1.num) {
            kind = this.state.value;
            this.next();
          }
        }

        const propOrInexact = this.flowParseObjectTypeProperty(node, isStatic, protoStart, variance, kind, allowSpread, allowInexact != null ? allowInexact : !exact);

        if (propOrInexact === null) {
          inexact = true;
          inexactStart = this.state.lastTokStart;
        } else {
          nodeStart.properties.push(propOrInexact);
        }
      }

      this.flowObjectTypeSemicolon();

      if (inexactStart && !this.match(types$1.braceR) && !this.match(types$1.braceBarR)) {
        this.raise(inexactStart, FlowErrors.UnexpectedExplicitInexactInObject);
      }
    }

    this.expect(endDelim);

    if (allowSpread) {
      nodeStart.inexact = inexact;
    }

    const out = this.finishNode(nodeStart, "ObjectTypeAnnotation");
    this.state.inType = oldInType;
    return out;
  }

  flowParseObjectTypeProperty(node, isStatic, protoStart, variance, kind, allowSpread, allowInexact) {
    if (this.eat(types$1.ellipsis)) {
      const isInexactToken = this.match(types$1.comma) || this.match(types$1.semi) || this.match(types$1.braceR) || this.match(types$1.braceBarR);

      if (isInexactToken) {
        if (!allowSpread) {
          this.raise(this.state.lastTokStart, FlowErrors.InexactInsideNonObject);
        } else if (!allowInexact) {
          this.raise(this.state.lastTokStart, FlowErrors.InexactInsideExact);
        }

        if (variance) {
          this.raise(variance.start, FlowErrors.InexactVariance);
        }

        return null;
      }

      if (!allowSpread) {
        this.raise(this.state.lastTokStart, FlowErrors.UnexpectedSpreadType);
      }

      if (protoStart != null) {
        this.unexpected(protoStart);
      }

      if (variance) {
        this.raise(variance.start, FlowErrors.SpreadVariance);
      }

      node.argument = this.flowParseType();
      return this.finishNode(node, "ObjectTypeSpreadProperty");
    } else {
      node.key = this.flowParseObjectPropertyKey();
      node.static = isStatic;
      node.proto = protoStart != null;
      node.kind = kind;
      let optional = false;

      if (this.isRelational("<") || this.match(types$1.parenL)) {
        node.method = true;

        if (protoStart != null) {
          this.unexpected(protoStart);
        }

        if (variance) {
          this.unexpected(variance.start);
        }

        node.value = this.flowParseObjectTypeMethodish(this.startNodeAt(node.start, node.loc.start));

        if (kind === "get" || kind === "set") {
          this.flowCheckGetterSetterParams(node);
        }

        if (!allowSpread && node.key.name === "constructor" && node.value.this) {
          this.raise(node.value.this.start, FlowErrors.ThisParamBannedInConstructor);
        }
      } else {
        if (kind !== "init") this.unexpected();
        node.method = false;

        if (this.eat(types$1.question)) {
          optional = true;
        }

        node.value = this.flowParseTypeInitialiser();
        node.variance = variance;
      }

      node.optional = optional;
      return this.finishNode(node, "ObjectTypeProperty");
    }
  }

  flowCheckGetterSetterParams(property) {
    const paramCount = property.kind === "get" ? 0 : 1;
    const start = property.start;
    const length = property.value.params.length + (property.value.rest ? 1 : 0);

    if (property.value.this) {
      this.raise(property.value.this.start, property.kind === "get" ? FlowErrors.GetterMayNotHaveThisParam : FlowErrors.SetterMayNotHaveThisParam);
    }

    if (length !== paramCount) {
      if (property.kind === "get") {
        this.raise(start, ErrorMessages.BadGetterArity);
      } else {
        this.raise(start, ErrorMessages.BadSetterArity);
      }
    }

    if (property.kind === "set" && property.value.rest) {
      this.raise(start, ErrorMessages.BadSetterRestParameter);
    }
  }

  flowObjectTypeSemicolon() {
    if (!this.eat(types$1.semi) && !this.eat(types$1.comma) && !this.match(types$1.braceR) && !this.match(types$1.braceBarR)) {
      this.unexpected();
    }
  }

  flowParseQualifiedTypeIdentifier(startPos, startLoc, id) {
    startPos = startPos || this.state.start;
    startLoc = startLoc || this.state.startLoc;
    let node = id || this.flowParseRestrictedIdentifier(true);

    while (this.eat(types$1.dot)) {
      const node2 = this.startNodeAt(startPos, startLoc);
      node2.qualification = node;
      node2.id = this.flowParseRestrictedIdentifier(true);
      node = this.finishNode(node2, "QualifiedTypeIdentifier");
    }

    return node;
  }

  flowParseGenericType(startPos, startLoc, id) {
    const node = this.startNodeAt(startPos, startLoc);
    node.typeParameters = null;
    node.id = this.flowParseQualifiedTypeIdentifier(startPos, startLoc, id);

    if (this.isRelational("<")) {
      node.typeParameters = this.flowParseTypeParameterInstantiation();
    }

    return this.finishNode(node, "GenericTypeAnnotation");
  }

  flowParseTypeofType() {
    const node = this.startNode();
    this.expect(types$1._typeof);
    node.argument = this.flowParsePrimaryType();
    return this.finishNode(node, "TypeofTypeAnnotation");
  }

  flowParseTupleType() {
    const node = this.startNode();
    node.types = [];
    this.expect(types$1.bracketL);

    while (this.state.pos < this.length && !this.match(types$1.bracketR)) {
      node.types.push(this.flowParseType());
      if (this.match(types$1.bracketR)) break;
      this.expect(types$1.comma);
    }

    this.expect(types$1.bracketR);
    return this.finishNode(node, "TupleTypeAnnotation");
  }

  flowParseFunctionTypeParam(first) {
    let name = null;
    let optional = false;
    let typeAnnotation = null;
    const node = this.startNode();
    const lh = this.lookahead();
    const isThis = this.state.type === types$1._this;

    if (lh.type === types$1.colon || lh.type === types$1.question) {
      if (isThis && !first) {
        this.raise(node.start, FlowErrors.ThisParamMustBeFirst);
      }

      name = this.parseIdentifier(isThis);

      if (this.eat(types$1.question)) {
        optional = true;

        if (isThis) {
          this.raise(node.start, FlowErrors.ThisParamMayNotBeOptional);
        }
      }

      typeAnnotation = this.flowParseTypeInitialiser();
    } else {
      typeAnnotation = this.flowParseType();
    }

    node.name = name;
    node.optional = optional;
    node.typeAnnotation = typeAnnotation;
    return this.finishNode(node, "FunctionTypeParam");
  }

  reinterpretTypeAsFunctionTypeParam(type) {
    const node = this.startNodeAt(type.start, type.loc.start);
    node.name = null;
    node.optional = false;
    node.typeAnnotation = type;
    return this.finishNode(node, "FunctionTypeParam");
  }

  flowParseFunctionTypeParams(params = []) {
    let rest = null;
    let _this = null;

    if (this.match(types$1._this)) {
      _this = this.flowParseFunctionTypeParam(true);
      _this.name = null;

      if (!this.match(types$1.parenR)) {
        this.expect(types$1.comma);
      }
    }

    while (!this.match(types$1.parenR) && !this.match(types$1.ellipsis)) {
      params.push(this.flowParseFunctionTypeParam(false));

      if (!this.match(types$1.parenR)) {
        this.expect(types$1.comma);
      }
    }

    if (this.eat(types$1.ellipsis)) {
      rest = this.flowParseFunctionTypeParam(false);
    }

    return {
      params,
      rest,
      _this
    };
  }

  flowIdentToTypeAnnotation(startPos, startLoc, node, id) {
    switch (id.name) {
      case "any":
        return this.finishNode(node, "AnyTypeAnnotation");

      case "bool":
      case "boolean":
        return this.finishNode(node, "BooleanTypeAnnotation");

      case "mixed":
        return this.finishNode(node, "MixedTypeAnnotation");

      case "empty":
        return this.finishNode(node, "EmptyTypeAnnotation");

      case "number":
        return this.finishNode(node, "NumberTypeAnnotation");

      case "string":
        return this.finishNode(node, "StringTypeAnnotation");

      case "symbol":
        return this.finishNode(node, "SymbolTypeAnnotation");

      default:
        this.checkNotUnderscore(id.name);
        return this.flowParseGenericType(startPos, startLoc, id);
    }
  }

  flowParsePrimaryType() {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    const node = this.startNode();
    let tmp;
    let type;
    let isGroupedType = false;
    const oldNoAnonFunctionType = this.state.noAnonFunctionType;

    switch (this.state.type) {
      case types$1.name:
        if (this.isContextual("interface")) {
          return this.flowParseInterfaceType();
        }

        return this.flowIdentToTypeAnnotation(startPos, startLoc, node, this.parseIdentifier());

      case types$1.braceL:
        return this.flowParseObjectType({
          allowStatic: false,
          allowExact: false,
          allowSpread: true,
          allowProto: false,
          allowInexact: true
        });

      case types$1.braceBarL:
        return this.flowParseObjectType({
          allowStatic: false,
          allowExact: true,
          allowSpread: true,
          allowProto: false,
          allowInexact: false
        });

      case types$1.bracketL:
        this.state.noAnonFunctionType = false;
        type = this.flowParseTupleType();
        this.state.noAnonFunctionType = oldNoAnonFunctionType;
        return type;

      case types$1.relational:
        if (this.state.value === "<") {
          node.typeParameters = this.flowParseTypeParameterDeclaration();
          this.expect(types$1.parenL);
          tmp = this.flowParseFunctionTypeParams();
          node.params = tmp.params;
          node.rest = tmp.rest;
          node.this = tmp._this;
          this.expect(types$1.parenR);
          this.expect(types$1.arrow);
          node.returnType = this.flowParseType();
          return this.finishNode(node, "FunctionTypeAnnotation");
        }

        break;

      case types$1.parenL:
        this.next();

        if (!this.match(types$1.parenR) && !this.match(types$1.ellipsis)) {
          if (this.match(types$1.name) || this.match(types$1._this)) {
            const token = this.lookahead().type;
            isGroupedType = token !== types$1.question && token !== types$1.colon;
          } else {
            isGroupedType = true;
          }
        }

        if (isGroupedType) {
          this.state.noAnonFunctionType = false;
          type = this.flowParseType();
          this.state.noAnonFunctionType = oldNoAnonFunctionType;

          if (this.state.noAnonFunctionType || !(this.match(types$1.comma) || this.match(types$1.parenR) && this.lookahead().type === types$1.arrow)) {
            this.expect(types$1.parenR);
            return type;
          } else {
            this.eat(types$1.comma);
          }
        }

        if (type) {
          tmp = this.flowParseFunctionTypeParams([this.reinterpretTypeAsFunctionTypeParam(type)]);
        } else {
          tmp = this.flowParseFunctionTypeParams();
        }

        node.params = tmp.params;
        node.rest = tmp.rest;
        node.this = tmp._this;
        this.expect(types$1.parenR);
        this.expect(types$1.arrow);
        node.returnType = this.flowParseType();
        node.typeParameters = null;
        return this.finishNode(node, "FunctionTypeAnnotation");

      case types$1.string:
        return this.parseLiteral(this.state.value, "StringLiteralTypeAnnotation");

      case types$1._true:
      case types$1._false:
        node.value = this.match(types$1._true);
        this.next();
        return this.finishNode(node, "BooleanLiteralTypeAnnotation");

      case types$1.plusMin:
        if (this.state.value === "-") {
          this.next();

          if (this.match(types$1.num)) {
            return this.parseLiteralAtNode(-this.state.value, "NumberLiteralTypeAnnotation", node);
          }

          if (this.match(types$1.bigint)) {
            return this.parseLiteralAtNode(-this.state.value, "BigIntLiteralTypeAnnotation", node);
          }

          throw this.raise(this.state.start, FlowErrors.UnexpectedSubtractionOperand);
        }

        throw this.unexpected();

      case types$1.num:
        return this.parseLiteral(this.state.value, "NumberLiteralTypeAnnotation");

      case types$1.bigint:
        return this.parseLiteral(this.state.value, "BigIntLiteralTypeAnnotation");

      case types$1._void:
        this.next();
        return this.finishNode(node, "VoidTypeAnnotation");

      case types$1._null:
        this.next();
        return this.finishNode(node, "NullLiteralTypeAnnotation");

      case types$1._this:
        this.next();
        return this.finishNode(node, "ThisTypeAnnotation");

      case types$1.star:
        this.next();
        return this.finishNode(node, "ExistsTypeAnnotation");

      default:
        if (this.state.type.keyword === "typeof") {
          return this.flowParseTypeofType();
        } else if (this.state.type.keyword) {
          const label = this.state.type.label;
          this.next();
          return super.createIdentifier(node, label);
        }

    }

    throw this.unexpected();
  }

  flowParsePostfixType() {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    let type = this.flowParsePrimaryType();
    let seenOptionalIndexedAccess = false;

    while ((this.match(types$1.bracketL) || this.match(types$1.questionDot)) && !this.canInsertSemicolon()) {
      const node = this.startNodeAt(startPos, startLoc);
      const optional = this.eat(types$1.questionDot);
      seenOptionalIndexedAccess = seenOptionalIndexedAccess || optional;
      this.expect(types$1.bracketL);

      if (!optional && this.match(types$1.bracketR)) {
        node.elementType = type;
        this.next();
        type = this.finishNode(node, "ArrayTypeAnnotation");
      } else {
        node.objectType = type;
        node.indexType = this.flowParseType();
        this.expect(types$1.bracketR);

        if (seenOptionalIndexedAccess) {
          node.optional = optional;
          type = this.finishNode(node, "OptionalIndexedAccessType");
        } else {
          type = this.finishNode(node, "IndexedAccessType");
        }
      }
    }

    return type;
  }

  flowParsePrefixType() {
    const node = this.startNode();

    if (this.eat(types$1.question)) {
      node.typeAnnotation = this.flowParsePrefixType();
      return this.finishNode(node, "NullableTypeAnnotation");
    } else {
      return this.flowParsePostfixType();
    }
  }

  flowParseAnonFunctionWithoutParens() {
    const param = this.flowParsePrefixType();

    if (!this.state.noAnonFunctionType && this.eat(types$1.arrow)) {
      const node = this.startNodeAt(param.start, param.loc.start);
      node.params = [this.reinterpretTypeAsFunctionTypeParam(param)];
      node.rest = null;
      node.this = null;
      node.returnType = this.flowParseType();
      node.typeParameters = null;
      return this.finishNode(node, "FunctionTypeAnnotation");
    }

    return param;
  }

  flowParseIntersectionType() {
    const node = this.startNode();
    this.eat(types$1.bitwiseAND);
    const type = this.flowParseAnonFunctionWithoutParens();
    node.types = [type];

    while (this.eat(types$1.bitwiseAND)) {
      node.types.push(this.flowParseAnonFunctionWithoutParens());
    }

    return node.types.length === 1 ? type : this.finishNode(node, "IntersectionTypeAnnotation");
  }

  flowParseUnionType() {
    const node = this.startNode();
    this.eat(types$1.bitwiseOR);
    const type = this.flowParseIntersectionType();
    node.types = [type];

    while (this.eat(types$1.bitwiseOR)) {
      node.types.push(this.flowParseIntersectionType());
    }

    return node.types.length === 1 ? type : this.finishNode(node, "UnionTypeAnnotation");
  }

  flowParseType() {
    const oldInType = this.state.inType;
    this.state.inType = true;
    const type = this.flowParseUnionType();
    this.state.inType = oldInType;
    return type;
  }

  flowParseTypeOrImplicitInstantiation() {
    if (this.state.type === types$1.name && this.state.value === "_") {
      const startPos = this.state.start;
      const startLoc = this.state.startLoc;
      const node = this.parseIdentifier();
      return this.flowParseGenericType(startPos, startLoc, node);
    } else {
      return this.flowParseType();
    }
  }

  flowParseTypeAnnotation() {
    const node = this.startNode();
    node.typeAnnotation = this.flowParseTypeInitialiser();
    return this.finishNode(node, "TypeAnnotation");
  }

  flowParseTypeAnnotatableIdentifier(allowPrimitiveOverride) {
    const ident = allowPrimitiveOverride ? this.parseIdentifier() : this.flowParseRestrictedIdentifier();

    if (this.match(types$1.colon)) {
      ident.typeAnnotation = this.flowParseTypeAnnotation();
      this.resetEndLocation(ident);
    }

    return ident;
  }

  typeCastToParameter(node) {
    node.expression.typeAnnotation = node.typeAnnotation;
    this.resetEndLocation(node.expression, node.typeAnnotation.end, node.typeAnnotation.loc.end);
    return node.expression;
  }

  flowParseVariance() {
    let variance = null;

    if (this.match(types$1.plusMin)) {
      variance = this.startNode();

      if (this.state.value === "+") {
        variance.kind = "plus";
      } else {
        variance.kind = "minus";
      }

      this.next();
      this.finishNode(variance, "Variance");
    }

    return variance;
  }

  parseFunctionBody(node, allowExpressionBody, isMethod = false) {
    if (allowExpressionBody) {
      return this.forwardNoArrowParamsConversionAt(node, () => super.parseFunctionBody(node, true, isMethod));
    }

    return super.parseFunctionBody(node, false, isMethod);
  }

  parseFunctionBodyAndFinish(node, type, isMethod = false) {
    if (this.match(types$1.colon)) {
      const typeNode = this.startNode();
      [typeNode.typeAnnotation, node.predicate] = this.flowParseTypeAndPredicateInitialiser();
      node.returnType = typeNode.typeAnnotation ? this.finishNode(typeNode, "TypeAnnotation") : null;
    }

    super.parseFunctionBodyAndFinish(node, type, isMethod);
  }

  parseStatement(context, topLevel) {
    if (this.state.strict && this.match(types$1.name) && this.state.value === "interface") {
      const lookahead = this.lookahead();

      if (lookahead.type === types$1.name || isKeyword(lookahead.value)) {
        const node = this.startNode();
        this.next();
        return this.flowParseInterface(node);
      }
    } else if (this.shouldParseEnums() && this.isContextual("enum")) {
      const node = this.startNode();
      this.next();
      return this.flowParseEnumDeclaration(node);
    }

    const stmt = super.parseStatement(context, topLevel);

    if (this.flowPragma === undefined && !this.isValidDirective(stmt)) {
      this.flowPragma = null;
    }

    return stmt;
  }

  parseExpressionStatement(node, expr) {
    if (expr.type === "Identifier") {
      if (expr.name === "declare") {
        if (this.match(types$1._class) || this.match(types$1.name) || this.match(types$1._function) || this.match(types$1._var) || this.match(types$1._export)) {
          return this.flowParseDeclare(node);
        }
      } else if (this.match(types$1.name)) {
        if (expr.name === "interface") {
          return this.flowParseInterface(node);
        } else if (expr.name === "type") {
          return this.flowParseTypeAlias(node);
        } else if (expr.name === "opaque") {
          return this.flowParseOpaqueType(node, false);
        }
      }
    }

    return super.parseExpressionStatement(node, expr);
  }

  shouldParseExportDeclaration() {
    return this.isContextual("type") || this.isContextual("interface") || this.isContextual("opaque") || this.shouldParseEnums() && this.isContextual("enum") || super.shouldParseExportDeclaration();
  }

  isExportDefaultSpecifier() {
    if (this.match(types$1.name) && (this.state.value === "type" || this.state.value === "interface" || this.state.value === "opaque" || this.shouldParseEnums() && this.state.value === "enum")) {
      return false;
    }

    return super.isExportDefaultSpecifier();
  }

  parseExportDefaultExpression() {
    if (this.shouldParseEnums() && this.isContextual("enum")) {
      const node = this.startNode();
      this.next();
      return this.flowParseEnumDeclaration(node);
    }

    return super.parseExportDefaultExpression();
  }

  parseConditional(expr, startPos, startLoc, refExpressionErrors) {
    if (!this.match(types$1.question)) return expr;

    if (this.state.maybeInArrowParameters) {
      const result = this.tryParse(() => super.parseConditional(expr, startPos, startLoc));

      if (!result.node) {
        if (result.error) {
          super.setOptionalParametersError(refExpressionErrors, result.error);
        }

        return expr;
      }

      if (result.error) this.state = result.failState;
      return result.node;
    }

    this.expect(types$1.question);
    const state = this.state.clone();
    const originalNoArrowAt = this.state.noArrowAt;
    const node = this.startNodeAt(startPos, startLoc);
    let {
      consequent,
      failed
    } = this.tryParseConditionalConsequent();
    let [valid, invalid] = this.getArrowLikeExpressions(consequent);

    if (failed || invalid.length > 0) {
      const noArrowAt = [...originalNoArrowAt];

      if (invalid.length > 0) {
        this.state = state;
        this.state.noArrowAt = noArrowAt;

        for (let i = 0; i < invalid.length; i++) {
          noArrowAt.push(invalid[i].start);
        }

        ({
          consequent,
          failed
        } = this.tryParseConditionalConsequent());
        [valid, invalid] = this.getArrowLikeExpressions(consequent);
      }

      if (failed && valid.length > 1) {
        this.raise(state.start, FlowErrors.AmbiguousConditionalArrow);
      }

      if (failed && valid.length === 1) {
        this.state = state;
        this.state.noArrowAt = noArrowAt.concat(valid[0].start);
        ({
          consequent,
          failed
        } = this.tryParseConditionalConsequent());
      }
    }

    this.getArrowLikeExpressions(consequent, true);
    this.state.noArrowAt = originalNoArrowAt;
    this.expect(types$1.colon);
    node.test = expr;
    node.consequent = consequent;
    node.alternate = this.forwardNoArrowParamsConversionAt(node, () => this.parseMaybeAssign(undefined, undefined));
    return this.finishNode(node, "ConditionalExpression");
  }

  tryParseConditionalConsequent() {
    this.state.noArrowParamsConversionAt.push(this.state.start);
    const consequent = this.parseMaybeAssignAllowIn();
    const failed = !this.match(types$1.colon);
    this.state.noArrowParamsConversionAt.pop();
    return {
      consequent,
      failed
    };
  }

  getArrowLikeExpressions(node, disallowInvalid) {
    const stack = [node];
    const arrows = [];

    while (stack.length !== 0) {
      const node = stack.pop();

      if (node.type === "ArrowFunctionExpression") {
        if (node.typeParameters || !node.returnType) {
          this.finishArrowValidation(node);
        } else {
          arrows.push(node);
        }

        stack.push(node.body);
      } else if (node.type === "ConditionalExpression") {
        stack.push(node.consequent);
        stack.push(node.alternate);
      }
    }

    if (disallowInvalid) {
      arrows.forEach(node => this.finishArrowValidation(node));
      return [arrows, []];
    }

    return partition(arrows, node => node.params.every(param => this.isAssignable(param, true)));
  }

  finishArrowValidation(node) {
    var _node$extra;

    this.toAssignableList(node.params, (_node$extra = node.extra) == null ? void 0 : _node$extra.trailingComma, false);
    this.scope.enter(SCOPE_FUNCTION | SCOPE_ARROW);
    super.checkParams(node, false, true);
    this.scope.exit();
  }

  forwardNoArrowParamsConversionAt(node, parse) {
    let result;

    if (this.state.noArrowParamsConversionAt.indexOf(node.start) !== -1) {
      this.state.noArrowParamsConversionAt.push(this.state.start);
      result = parse();
      this.state.noArrowParamsConversionAt.pop();
    } else {
      result = parse();
    }

    return result;
  }

  parseParenItem(node, startPos, startLoc) {
    node = super.parseParenItem(node, startPos, startLoc);

    if (this.eat(types$1.question)) {
      node.optional = true;
      this.resetEndLocation(node);
    }

    if (this.match(types$1.colon)) {
      const typeCastNode = this.startNodeAt(startPos, startLoc);
      typeCastNode.expression = node;
      typeCastNode.typeAnnotation = this.flowParseTypeAnnotation();
      return this.finishNode(typeCastNode, "TypeCastExpression");
    }

    return node;
  }

  assertModuleNodeAllowed(node) {
    if (node.type === "ImportDeclaration" && (node.importKind === "type" || node.importKind === "typeof") || node.type === "ExportNamedDeclaration" && node.exportKind === "type" || node.type === "ExportAllDeclaration" && node.exportKind === "type") {
      return;
    }

    super.assertModuleNodeAllowed(node);
  }

  parseExport(node) {
    const decl = super.parseExport(node);

    if (decl.type === "ExportNamedDeclaration" || decl.type === "ExportAllDeclaration") {
      decl.exportKind = decl.exportKind || "value";
    }

    return decl;
  }

  parseExportDeclaration(node) {
    if (this.isContextual("type")) {
      node.exportKind = "type";
      const declarationNode = this.startNode();
      this.next();

      if (this.match(types$1.braceL)) {
        node.specifiers = this.parseExportSpecifiers();
        this.parseExportFrom(node);
        return null;
      } else {
        return this.flowParseTypeAlias(declarationNode);
      }
    } else if (this.isContextual("opaque")) {
      node.exportKind = "type";
      const declarationNode = this.startNode();
      this.next();
      return this.flowParseOpaqueType(declarationNode, false);
    } else if (this.isContextual("interface")) {
      node.exportKind = "type";
      const declarationNode = this.startNode();
      this.next();
      return this.flowParseInterface(declarationNode);
    } else if (this.shouldParseEnums() && this.isContextual("enum")) {
      node.exportKind = "value";
      const declarationNode = this.startNode();
      this.next();
      return this.flowParseEnumDeclaration(declarationNode);
    } else {
      return super.parseExportDeclaration(node);
    }
  }

  eatExportStar(node) {
    if (super.eatExportStar(...arguments)) return true;

    if (this.isContextual("type") && this.lookahead().type === types$1.star) {
      node.exportKind = "type";
      this.next();
      this.next();
      return true;
    }

    return false;
  }

  maybeParseExportNamespaceSpecifier(node) {
    const pos = this.state.start;
    const hasNamespace = super.maybeParseExportNamespaceSpecifier(node);

    if (hasNamespace && node.exportKind === "type") {
      this.unexpected(pos);
    }

    return hasNamespace;
  }

  parseClassId(node, isStatement, optionalId) {
    super.parseClassId(node, isStatement, optionalId);

    if (this.isRelational("<")) {
      node.typeParameters = this.flowParseTypeParameterDeclaration();
    }
  }

  parseClassMember(classBody, member, state) {
    const pos = this.state.start;

    if (this.isContextual("declare")) {
      if (this.parseClassMemberFromModifier(classBody, member)) {
        return;
      }

      member.declare = true;
    }

    super.parseClassMember(classBody, member, state);

    if (member.declare) {
      if (member.type !== "ClassProperty" && member.type !== "ClassPrivateProperty" && member.type !== "PropertyDefinition") {
          this.raise(pos, FlowErrors.DeclareClassElement);
        } else if (member.value) {
        this.raise(member.value.start, FlowErrors.DeclareClassFieldInitializer);
      }
    }
  }

  isIterator(word) {
    return word === "iterator" || word === "asyncIterator";
  }

  readIterator() {
    const word = super.readWord1();
    const fullWord = "@@" + word;

    if (!this.isIterator(word) || !this.state.inType) {
      this.raise(this.state.pos, ErrorMessages.InvalidIdentifier, fullWord);
    }

    this.finishToken(types$1.name, fullWord);
  }

  getTokenFromCode(code) {
    const next = this.input.charCodeAt(this.state.pos + 1);

    if (code === 123 && next === 124) {
      return this.finishOp(types$1.braceBarL, 2);
    } else if (this.state.inType && (code === 62 || code === 60)) {
      return this.finishOp(types$1.relational, 1);
    } else if (this.state.inType && code === 63) {
      if (next === 46) {
        return this.finishOp(types$1.questionDot, 2);
      }

      return this.finishOp(types$1.question, 1);
    } else if (isIteratorStart(code, next)) {
      this.state.pos += 2;
      return this.readIterator();
    } else {
      return super.getTokenFromCode(code);
    }
  }

  isAssignable(node, isBinding) {
    switch (node.type) {
      case "Identifier":
      case "ObjectPattern":
      case "ArrayPattern":
      case "AssignmentPattern":
        return true;

      case "ObjectExpression":
        {
          const last = node.properties.length - 1;
          return node.properties.every((prop, i) => {
            return prop.type !== "ObjectMethod" && (i === last || prop.type === "SpreadElement") && this.isAssignable(prop);
          });
        }

      case "ObjectProperty":
        return this.isAssignable(node.value);

      case "SpreadElement":
        return this.isAssignable(node.argument);

      case "ArrayExpression":
        return node.elements.every(element => this.isAssignable(element));

      case "AssignmentExpression":
        return node.operator === "=";

      case "ParenthesizedExpression":
      case "TypeCastExpression":
        return this.isAssignable(node.expression);

      case "MemberExpression":
      case "OptionalMemberExpression":
        return !isBinding;

      default:
        return false;
    }
  }

  toAssignable(node, isLHS = false) {
    if (node.type === "TypeCastExpression") {
      return super.toAssignable(this.typeCastToParameter(node), isLHS);
    } else {
      return super.toAssignable(node, isLHS);
    }
  }

  toAssignableList(exprList, trailingCommaPos, isLHS) {
    for (let i = 0; i < exprList.length; i++) {
      const expr = exprList[i];

      if ((expr == null ? void 0 : expr.type) === "TypeCastExpression") {
        exprList[i] = this.typeCastToParameter(expr);
      }
    }

    return super.toAssignableList(exprList, trailingCommaPos, isLHS);
  }

  toReferencedList(exprList, isParenthesizedExpr) {
    for (let i = 0; i < exprList.length; i++) {
      var _expr$extra;

      const expr = exprList[i];

      if (expr && expr.type === "TypeCastExpression" && !((_expr$extra = expr.extra) != null && _expr$extra.parenthesized) && (exprList.length > 1 || !isParenthesizedExpr)) {
        this.raise(expr.typeAnnotation.start, FlowErrors.TypeCastInPattern);
      }
    }

    return exprList;
  }

  parseArrayLike(close, canBePattern, isTuple, refExpressionErrors) {
    const node = super.parseArrayLike(close, canBePattern, isTuple, refExpressionErrors);

    if (canBePattern && !this.state.maybeInArrowParameters) {
      this.toReferencedList(node.elements);
    }

    return node;
  }

  checkLVal(expr, ...args) {
    if (expr.type !== "TypeCastExpression") {
      return super.checkLVal(expr, ...args);
    }
  }

  parseClassProperty(node) {
    if (this.match(types$1.colon)) {
      node.typeAnnotation = this.flowParseTypeAnnotation();
    }

    return super.parseClassProperty(node);
  }

  parseClassPrivateProperty(node) {
    if (this.match(types$1.colon)) {
      node.typeAnnotation = this.flowParseTypeAnnotation();
    }

    return super.parseClassPrivateProperty(node);
  }

  isClassMethod() {
    return this.isRelational("<") || super.isClassMethod();
  }

  isClassProperty() {
    return this.match(types$1.colon) || super.isClassProperty();
  }

  isNonstaticConstructor(method) {
    return !this.match(types$1.colon) && super.isNonstaticConstructor(method);
  }

  pushClassMethod(classBody, method, isGenerator, isAsync, isConstructor, allowsDirectSuper) {
    if (method.variance) {
      this.unexpected(method.variance.start);
    }

    delete method.variance;

    if (this.isRelational("<")) {
      method.typeParameters = this.flowParseTypeParameterDeclaration();
    }

    super.pushClassMethod(classBody, method, isGenerator, isAsync, isConstructor, allowsDirectSuper);

    if (method.params && isConstructor) {
      const params = method.params;

      if (params.length > 0 && this.isThisParam(params[0])) {
        this.raise(method.start, FlowErrors.ThisParamBannedInConstructor);
      }
    } else if (method.type === "MethodDefinition" && isConstructor && method.value.params) {
      const params = method.value.params;

      if (params.length > 0 && this.isThisParam(params[0])) {
        this.raise(method.start, FlowErrors.ThisParamBannedInConstructor);
      }
    }
  }

  pushClassPrivateMethod(classBody, method, isGenerator, isAsync) {
    if (method.variance) {
      this.unexpected(method.variance.start);
    }

    delete method.variance;

    if (this.isRelational("<")) {
      method.typeParameters = this.flowParseTypeParameterDeclaration();
    }

    super.pushClassPrivateMethod(classBody, method, isGenerator, isAsync);
  }

  parseClassSuper(node) {
    super.parseClassSuper(node);

    if (node.superClass && this.isRelational("<")) {
      node.superTypeParameters = this.flowParseTypeParameterInstantiation();
    }

    if (this.isContextual("implements")) {
      this.next();
      const implemented = node.implements = [];

      do {
        const node = this.startNode();
        node.id = this.flowParseRestrictedIdentifier(true);

        if (this.isRelational("<")) {
          node.typeParameters = this.flowParseTypeParameterInstantiation();
        } else {
          node.typeParameters = null;
        }

        implemented.push(this.finishNode(node, "ClassImplements"));
      } while (this.eat(types$1.comma));
    }
  }

  checkGetterSetterParams(method) {
    super.checkGetterSetterParams(method);
    const params = this.getObjectOrClassMethodParams(method);

    if (params.length > 0) {
      const param = params[0];

      if (this.isThisParam(param) && method.kind === "get") {
        this.raise(param.start, FlowErrors.GetterMayNotHaveThisParam);
      } else if (this.isThisParam(param)) {
        this.raise(param.start, FlowErrors.SetterMayNotHaveThisParam);
      }
    }
  }

  parsePropertyName(node, isPrivateNameAllowed) {
    const variance = this.flowParseVariance();
    const key = super.parsePropertyName(node, isPrivateNameAllowed);
    node.variance = variance;
    return key;
  }

  parseObjPropValue(prop, startPos, startLoc, isGenerator, isAsync, isPattern, isAccessor, refExpressionErrors) {
    if (prop.variance) {
      this.unexpected(prop.variance.start);
    }

    delete prop.variance;
    let typeParameters;

    if (this.isRelational("<") && !isAccessor) {
      typeParameters = this.flowParseTypeParameterDeclaration();
      if (!this.match(types$1.parenL)) this.unexpected();
    }

    super.parseObjPropValue(prop, startPos, startLoc, isGenerator, isAsync, isPattern, isAccessor, refExpressionErrors);

    if (typeParameters) {
      (prop.value || prop).typeParameters = typeParameters;
    }
  }

  parseAssignableListItemTypes(param) {
    if (this.eat(types$1.question)) {
      if (param.type !== "Identifier") {
        this.raise(param.start, FlowErrors.OptionalBindingPattern);
      }

      if (this.isThisParam(param)) {
        this.raise(param.start, FlowErrors.ThisParamMayNotBeOptional);
      }

      param.optional = true;
    }

    if (this.match(types$1.colon)) {
      param.typeAnnotation = this.flowParseTypeAnnotation();
    } else if (this.isThisParam(param)) {
      this.raise(param.start, FlowErrors.ThisParamAnnotationRequired);
    }

    if (this.match(types$1.eq) && this.isThisParam(param)) {
      this.raise(param.start, FlowErrors.ThisParamNoDefault);
    }

    this.resetEndLocation(param);
    return param;
  }

  parseMaybeDefault(startPos, startLoc, left) {
    const node = super.parseMaybeDefault(startPos, startLoc, left);

    if (node.type === "AssignmentPattern" && node.typeAnnotation && node.right.start < node.typeAnnotation.start) {
      this.raise(node.typeAnnotation.start, FlowErrors.TypeBeforeInitializer);
    }

    return node;
  }

  shouldParseDefaultImport(node) {
    if (!hasTypeImportKind(node)) {
      return super.shouldParseDefaultImport(node);
    }

    return isMaybeDefaultImport(this.state);
  }

  parseImportSpecifierLocal(node, specifier, type, contextDescription) {
    specifier.local = hasTypeImportKind(node) ? this.flowParseRestrictedIdentifier(true, true) : this.parseIdentifier();
    this.checkLVal(specifier.local, contextDescription, BIND_LEXICAL);
    node.specifiers.push(this.finishNode(specifier, type));
  }

  maybeParseDefaultImportSpecifier(node) {
    node.importKind = "value";
    let kind = null;

    if (this.match(types$1._typeof)) {
      kind = "typeof";
    } else if (this.isContextual("type")) {
      kind = "type";
    }

    if (kind) {
      const lh = this.lookahead();

      if (kind === "type" && lh.type === types$1.star) {
        this.unexpected(lh.start);
      }

      if (isMaybeDefaultImport(lh) || lh.type === types$1.braceL || lh.type === types$1.star) {
        this.next();
        node.importKind = kind;
      }
    }

    return super.maybeParseDefaultImportSpecifier(node);
  }

  parseImportSpecifier(node) {
    const specifier = this.startNode();
    const firstIdentIsString = this.match(types$1.string);
    const firstIdent = this.parseModuleExportName();
    let specifierTypeKind = null;

    if (firstIdent.type === "Identifier") {
      if (firstIdent.name === "type") {
        specifierTypeKind = "type";
      } else if (firstIdent.name === "typeof") {
        specifierTypeKind = "typeof";
      }
    }

    let isBinding = false;

    if (this.isContextual("as") && !this.isLookaheadContextual("as")) {
      const as_ident = this.parseIdentifier(true);

      if (specifierTypeKind !== null && !this.match(types$1.name) && !this.state.type.keyword) {
        specifier.imported = as_ident;
        specifier.importKind = specifierTypeKind;
        specifier.local = as_ident.__clone();
      } else {
        specifier.imported = firstIdent;
        specifier.importKind = null;
        specifier.local = this.parseIdentifier();
      }
    } else if (specifierTypeKind !== null && (this.match(types$1.name) || this.state.type.keyword)) {
      specifier.imported = this.parseIdentifier(true);
      specifier.importKind = specifierTypeKind;

      if (this.eatContextual("as")) {
        specifier.local = this.parseIdentifier();
      } else {
        isBinding = true;
        specifier.local = specifier.imported.__clone();
      }
    } else {
      if (firstIdentIsString) {
        throw this.raise(specifier.start, ErrorMessages.ImportBindingIsString, firstIdent.value);
      }

      isBinding = true;
      specifier.imported = firstIdent;
      specifier.importKind = null;
      specifier.local = specifier.imported.__clone();
    }

    const nodeIsTypeImport = hasTypeImportKind(node);
    const specifierIsTypeImport = hasTypeImportKind(specifier);

    if (nodeIsTypeImport && specifierIsTypeImport) {
      this.raise(specifier.start, FlowErrors.ImportTypeShorthandOnlyInPureImport);
    }

    if (nodeIsTypeImport || specifierIsTypeImport) {
      this.checkReservedType(specifier.local.name, specifier.local.start, true);
    }

    if (isBinding && !nodeIsTypeImport && !specifierIsTypeImport) {
      this.checkReservedWord(specifier.local.name, specifier.start, true, true);
    }

    this.checkLVal(specifier.local, "import specifier", BIND_LEXICAL);
    node.specifiers.push(this.finishNode(specifier, "ImportSpecifier"));
  }

  parseBindingAtom() {
    switch (this.state.type) {
      case types$1._this:
        return this.parseIdentifier(true);

      default:
        return super.parseBindingAtom();
    }
  }

  parseFunctionParams(node, allowModifiers) {
    const kind = node.kind;

    if (kind !== "get" && kind !== "set" && this.isRelational("<")) {
      node.typeParameters = this.flowParseTypeParameterDeclaration();
    }

    super.parseFunctionParams(node, allowModifiers);
  }

  parseVarId(decl, kind) {
    super.parseVarId(decl, kind);

    if (this.match(types$1.colon)) {
      decl.id.typeAnnotation = this.flowParseTypeAnnotation();
      this.resetEndLocation(decl.id);
    }
  }

  parseAsyncArrowFromCallExpression(node, call) {
    if (this.match(types$1.colon)) {
      const oldNoAnonFunctionType = this.state.noAnonFunctionType;
      this.state.noAnonFunctionType = true;
      node.returnType = this.flowParseTypeAnnotation();
      this.state.noAnonFunctionType = oldNoAnonFunctionType;
    }

    return super.parseAsyncArrowFromCallExpression(node, call);
  }

  shouldParseAsyncArrow() {
    return this.match(types$1.colon) || super.shouldParseAsyncArrow();
  }

  parseMaybeAssign(refExpressionErrors, afterLeftParse) {
    var _jsx;

    let state = null;
    let jsx;

    if (this.hasPlugin("jsx") && (this.match(types$1.jsxTagStart) || this.isRelational("<"))) {
      state = this.state.clone();
      jsx = this.tryParse(() => super.parseMaybeAssign(refExpressionErrors, afterLeftParse), state);
      if (!jsx.error) return jsx.node;
      const {
        context
      } = this.state;
      const curContext = context[context.length - 1];

      if (curContext === types.j_oTag) {
        context.length -= 2;
      } else if (curContext === types.j_expr) {
        context.length -= 1;
      }
    }

    if ((_jsx = jsx) != null && _jsx.error || this.isRelational("<")) {
      var _jsx2, _jsx3;

      state = state || this.state.clone();
      let typeParameters;
      const arrow = this.tryParse(abort => {
        var _arrowExpression$extr;

        typeParameters = this.flowParseTypeParameterDeclaration();
        const arrowExpression = this.forwardNoArrowParamsConversionAt(typeParameters, () => {
          const result = super.parseMaybeAssign(refExpressionErrors, afterLeftParse);
          this.resetStartLocationFromNode(result, typeParameters);
          return result;
        });

        if (arrowExpression.type !== "ArrowFunctionExpression" && (_arrowExpression$extr = arrowExpression.extra) != null && _arrowExpression$extr.parenthesized) {
          abort();
        }

        const expr = this.maybeUnwrapTypeCastExpression(arrowExpression);
        expr.typeParameters = typeParameters;
        this.resetStartLocationFromNode(expr, typeParameters);
        return arrowExpression;
      }, state);
      let arrowExpression = null;

      if (arrow.node && this.maybeUnwrapTypeCastExpression(arrow.node).type === "ArrowFunctionExpression") {
        if (!arrow.error && !arrow.aborted) {
          if (arrow.node.async) {
            this.raise(typeParameters.start, FlowErrors.UnexpectedTypeParameterBeforeAsyncArrowFunction);
          }

          return arrow.node;
        }

        arrowExpression = arrow.node;
      }

      if ((_jsx2 = jsx) != null && _jsx2.node) {
        this.state = jsx.failState;
        return jsx.node;
      }

      if (arrowExpression) {
        this.state = arrow.failState;
        return arrowExpression;
      }

      if ((_jsx3 = jsx) != null && _jsx3.thrown) throw jsx.error;
      if (arrow.thrown) throw arrow.error;
      throw this.raise(typeParameters.start, FlowErrors.UnexpectedTokenAfterTypeParameter);
    }

    return super.parseMaybeAssign(refExpressionErrors, afterLeftParse);
  }

  parseArrow(node) {
    if (this.match(types$1.colon)) {
      const result = this.tryParse(() => {
        const oldNoAnonFunctionType = this.state.noAnonFunctionType;
        this.state.noAnonFunctionType = true;
        const typeNode = this.startNode();
        [typeNode.typeAnnotation, node.predicate] = this.flowParseTypeAndPredicateInitialiser();
        this.state.noAnonFunctionType = oldNoAnonFunctionType;
        if (this.canInsertSemicolon()) this.unexpected();
        if (!this.match(types$1.arrow)) this.unexpected();
        return typeNode;
      });
      if (result.thrown) return null;
      if (result.error) this.state = result.failState;
      node.returnType = result.node.typeAnnotation ? this.finishNode(result.node, "TypeAnnotation") : null;
    }

    return super.parseArrow(node);
  }

  shouldParseArrow() {
    return this.match(types$1.colon) || super.shouldParseArrow();
  }

  setArrowFunctionParameters(node, params) {
    if (this.state.noArrowParamsConversionAt.indexOf(node.start) !== -1) {
      node.params = params;
    } else {
      super.setArrowFunctionParameters(node, params);
    }
  }

  checkParams(node, allowDuplicates, isArrowFunction) {
    if (isArrowFunction && this.state.noArrowParamsConversionAt.indexOf(node.start) !== -1) {
      return;
    }

    for (let i = 0; i < node.params.length; i++) {
      if (this.isThisParam(node.params[i]) && i > 0) {
        this.raise(node.params[i].start, FlowErrors.ThisParamMustBeFirst);
      }
    }

    return super.checkParams(...arguments);
  }

  parseParenAndDistinguishExpression(canBeArrow) {
    return super.parseParenAndDistinguishExpression(canBeArrow && this.state.noArrowAt.indexOf(this.state.start) === -1);
  }

  parseSubscripts(base, startPos, startLoc, noCalls) {
    if (base.type === "Identifier" && base.name === "async" && this.state.noArrowAt.indexOf(startPos) !== -1) {
      this.next();
      const node = this.startNodeAt(startPos, startLoc);
      node.callee = base;
      node.arguments = this.parseCallExpressionArguments(types$1.parenR, false);
      base = this.finishNode(node, "CallExpression");
    } else if (base.type === "Identifier" && base.name === "async" && this.isRelational("<")) {
      const state = this.state.clone();
      const arrow = this.tryParse(abort => this.parseAsyncArrowWithTypeParameters(startPos, startLoc) || abort(), state);
      if (!arrow.error && !arrow.aborted) return arrow.node;
      const result = this.tryParse(() => super.parseSubscripts(base, startPos, startLoc, noCalls), state);
      if (result.node && !result.error) return result.node;

      if (arrow.node) {
        this.state = arrow.failState;
        return arrow.node;
      }

      if (result.node) {
        this.state = result.failState;
        return result.node;
      }

      throw arrow.error || result.error;
    }

    return super.parseSubscripts(base, startPos, startLoc, noCalls);
  }

  parseSubscript(base, startPos, startLoc, noCalls, subscriptState) {
    if (this.match(types$1.questionDot) && this.isLookaheadToken_lt()) {
      subscriptState.optionalChainMember = true;

      if (noCalls) {
        subscriptState.stop = true;
        return base;
      }

      this.next();
      const node = this.startNodeAt(startPos, startLoc);
      node.callee = base;
      node.typeArguments = this.flowParseTypeParameterInstantiation();
      this.expect(types$1.parenL);
      node.arguments = this.parseCallExpressionArguments(types$1.parenR, false);
      node.optional = true;
      return this.finishCallExpression(node, true);
    } else if (!noCalls && this.shouldParseTypes() && this.isRelational("<")) {
      const node = this.startNodeAt(startPos, startLoc);
      node.callee = base;
      const result = this.tryParse(() => {
        node.typeArguments = this.flowParseTypeParameterInstantiationCallOrNew();
        this.expect(types$1.parenL);
        node.arguments = this.parseCallExpressionArguments(types$1.parenR, false);
        if (subscriptState.optionalChainMember) node.optional = false;
        return this.finishCallExpression(node, subscriptState.optionalChainMember);
      });

      if (result.node) {
        if (result.error) this.state = result.failState;
        return result.node;
      }
    }

    return super.parseSubscript(base, startPos, startLoc, noCalls, subscriptState);
  }

  parseNewArguments(node) {
    let targs = null;

    if (this.shouldParseTypes() && this.isRelational("<")) {
      targs = this.tryParse(() => this.flowParseTypeParameterInstantiationCallOrNew()).node;
    }

    node.typeArguments = targs;
    super.parseNewArguments(node);
  }

  parseAsyncArrowWithTypeParameters(startPos, startLoc) {
    const node = this.startNodeAt(startPos, startLoc);
    this.parseFunctionParams(node);
    if (!this.parseArrow(node)) return;
    return this.parseArrowExpression(node, undefined, true);
  }

  readToken_mult_modulo(code) {
    const next = this.input.charCodeAt(this.state.pos + 1);

    if (code === 42 && next === 47 && this.state.hasFlowComment) {
      this.state.hasFlowComment = false;
      this.state.pos += 2;
      this.nextToken();
      return;
    }

    super.readToken_mult_modulo(code);
  }

  readToken_pipe_amp(code) {
    const next = this.input.charCodeAt(this.state.pos + 1);

    if (code === 124 && next === 125) {
      this.finishOp(types$1.braceBarR, 2);
      return;
    }

    super.readToken_pipe_amp(code);
  }

  parseTopLevel(file, program) {
    const fileNode = super.parseTopLevel(file, program);

    if (this.state.hasFlowComment) {
      this.raise(this.state.pos, FlowErrors.UnterminatedFlowComment);
    }

    return fileNode;
  }

  skipBlockComment() {
    if (this.hasPlugin("flowComments") && this.skipFlowComment()) {
      if (this.state.hasFlowComment) {
        this.unexpected(null, FlowErrors.NestedFlowComment);
      }

      this.hasFlowCommentCompletion();
      this.state.pos += this.skipFlowComment();
      this.state.hasFlowComment = true;
      return;
    }

    if (this.state.hasFlowComment) {
      const end = this.input.indexOf("*-/", this.state.pos += 2);

      if (end === -1) {
        throw this.raise(this.state.pos - 2, ErrorMessages.UnterminatedComment);
      }

      this.state.pos = end + 3;
      return;
    }

    super.skipBlockComment();
  }

  skipFlowComment() {
    const {
      pos
    } = this.state;
    let shiftToFirstNonWhiteSpace = 2;

    while ([32, 9].includes(this.input.charCodeAt(pos + shiftToFirstNonWhiteSpace))) {
      shiftToFirstNonWhiteSpace++;
    }

    const ch2 = this.input.charCodeAt(shiftToFirstNonWhiteSpace + pos);
    const ch3 = this.input.charCodeAt(shiftToFirstNonWhiteSpace + pos + 1);

    if (ch2 === 58 && ch3 === 58) {
      return shiftToFirstNonWhiteSpace + 2;
    }

    if (this.input.slice(shiftToFirstNonWhiteSpace + pos, shiftToFirstNonWhiteSpace + pos + 12) === "flow-include") {
      return shiftToFirstNonWhiteSpace + 12;
    }

    if (ch2 === 58 && ch3 !== 58) {
      return shiftToFirstNonWhiteSpace;
    }

    return false;
  }

  hasFlowCommentCompletion() {
    const end = this.input.indexOf("*/", this.state.pos);

    if (end === -1) {
      throw this.raise(this.state.pos, ErrorMessages.UnterminatedComment);
    }
  }

  flowEnumErrorBooleanMemberNotInitialized(pos, {
    enumName,
    memberName
  }) {
    this.raise(pos, FlowErrors.EnumBooleanMemberNotInitialized, memberName, enumName);
  }

  flowEnumErrorInvalidMemberName(pos, {
    enumName,
    memberName
  }) {
    const suggestion = memberName[0].toUpperCase() + memberName.slice(1);
    this.raise(pos, FlowErrors.EnumInvalidMemberName, memberName, suggestion, enumName);
  }

  flowEnumErrorDuplicateMemberName(pos, {
    enumName,
    memberName
  }) {
    this.raise(pos, FlowErrors.EnumDuplicateMemberName, memberName, enumName);
  }

  flowEnumErrorInconsistentMemberValues(pos, {
    enumName
  }) {
    this.raise(pos, FlowErrors.EnumInconsistentMemberValues, enumName);
  }

  flowEnumErrorInvalidExplicitType(pos, {
    enumName,
    suppliedType
  }) {
    return this.raise(pos, suppliedType === null ? FlowErrors.EnumInvalidExplicitTypeUnknownSupplied : FlowErrors.EnumInvalidExplicitType, enumName, suppliedType);
  }

  flowEnumErrorInvalidMemberInitializer(pos, {
    enumName,
    explicitType,
    memberName
  }) {
    let message = null;

    switch (explicitType) {
      case "boolean":
      case "number":
      case "string":
        message = FlowErrors.EnumInvalidMemberInitializerPrimaryType;
        break;

      case "symbol":
        message = FlowErrors.EnumInvalidMemberInitializerSymbolType;
        break;

      default:
        message = FlowErrors.EnumInvalidMemberInitializerUnknownType;
    }

    return this.raise(pos, message, enumName, memberName, explicitType);
  }

  flowEnumErrorNumberMemberNotInitialized(pos, {
    enumName,
    memberName
  }) {
    this.raise(pos, FlowErrors.EnumNumberMemberNotInitialized, enumName, memberName);
  }

  flowEnumErrorStringMemberInconsistentlyInitailized(pos, {
    enumName
  }) {
    this.raise(pos, FlowErrors.EnumStringMemberInconsistentlyInitailized, enumName);
  }

  flowEnumMemberInit() {
    const startPos = this.state.start;

    const endOfInit = () => this.match(types$1.comma) || this.match(types$1.braceR);

    switch (this.state.type) {
      case types$1.num:
        {
          const literal = this.parseNumericLiteral(this.state.value);

          if (endOfInit()) {
            return {
              type: "number",
              pos: literal.start,
              value: literal
            };
          }

          return {
            type: "invalid",
            pos: startPos
          };
        }

      case types$1.string:
        {
          const literal = this.parseStringLiteral(this.state.value);

          if (endOfInit()) {
            return {
              type: "string",
              pos: literal.start,
              value: literal
            };
          }

          return {
            type: "invalid",
            pos: startPos
          };
        }

      case types$1._true:
      case types$1._false:
        {
          const literal = this.parseBooleanLiteral(this.match(types$1._true));

          if (endOfInit()) {
            return {
              type: "boolean",
              pos: literal.start,
              value: literal
            };
          }

          return {
            type: "invalid",
            pos: startPos
          };
        }

      default:
        return {
          type: "invalid",
          pos: startPos
        };
    }
  }

  flowEnumMemberRaw() {
    const pos = this.state.start;
    const id = this.parseIdentifier(true);
    const init = this.eat(types$1.eq) ? this.flowEnumMemberInit() : {
      type: "none",
      pos
    };
    return {
      id,
      init
    };
  }

  flowEnumCheckExplicitTypeMismatch(pos, context, expectedType) {
    const {
      explicitType
    } = context;

    if (explicitType === null) {
      return;
    }

    if (explicitType !== expectedType) {
      this.flowEnumErrorInvalidMemberInitializer(pos, context);
    }
  }

  flowEnumMembers({
    enumName,
    explicitType
  }) {
    const seenNames = new Set();
    const members = {
      booleanMembers: [],
      numberMembers: [],
      stringMembers: [],
      defaultedMembers: []
    };
    let hasUnknownMembers = false;

    while (!this.match(types$1.braceR)) {
      if (this.eat(types$1.ellipsis)) {
        hasUnknownMembers = true;
        break;
      }

      const memberNode = this.startNode();
      const {
        id,
        init
      } = this.flowEnumMemberRaw();
      const memberName = id.name;

      if (memberName === "") {
        continue;
      }

      if (/^[a-z]/.test(memberName)) {
        this.flowEnumErrorInvalidMemberName(id.start, {
          enumName,
          memberName
        });
      }

      if (seenNames.has(memberName)) {
        this.flowEnumErrorDuplicateMemberName(id.start, {
          enumName,
          memberName
        });
      }

      seenNames.add(memberName);
      const context = {
        enumName,
        explicitType,
        memberName
      };
      memberNode.id = id;

      switch (init.type) {
        case "boolean":
          {
            this.flowEnumCheckExplicitTypeMismatch(init.pos, context, "boolean");
            memberNode.init = init.value;
            members.booleanMembers.push(this.finishNode(memberNode, "EnumBooleanMember"));
            break;
          }

        case "number":
          {
            this.flowEnumCheckExplicitTypeMismatch(init.pos, context, "number");
            memberNode.init = init.value;
            members.numberMembers.push(this.finishNode(memberNode, "EnumNumberMember"));
            break;
          }

        case "string":
          {
            this.flowEnumCheckExplicitTypeMismatch(init.pos, context, "string");
            memberNode.init = init.value;
            members.stringMembers.push(this.finishNode(memberNode, "EnumStringMember"));
            break;
          }

        case "invalid":
          {
            throw this.flowEnumErrorInvalidMemberInitializer(init.pos, context);
          }

        case "none":
          {
            switch (explicitType) {
              case "boolean":
                this.flowEnumErrorBooleanMemberNotInitialized(init.pos, context);
                break;

              case "number":
                this.flowEnumErrorNumberMemberNotInitialized(init.pos, context);
                break;

              default:
                members.defaultedMembers.push(this.finishNode(memberNode, "EnumDefaultedMember"));
            }
          }
      }

      if (!this.match(types$1.braceR)) {
        this.expect(types$1.comma);
      }
    }

    return {
      members,
      hasUnknownMembers
    };
  }

  flowEnumStringMembers(initializedMembers, defaultedMembers, {
    enumName
  }) {
    if (initializedMembers.length === 0) {
      return defaultedMembers;
    } else if (defaultedMembers.length === 0) {
      return initializedMembers;
    } else if (defaultedMembers.length > initializedMembers.length) {
      for (const member of initializedMembers) {
        this.flowEnumErrorStringMemberInconsistentlyInitailized(member.start, {
          enumName
        });
      }

      return defaultedMembers;
    } else {
      for (const member of defaultedMembers) {
        this.flowEnumErrorStringMemberInconsistentlyInitailized(member.start, {
          enumName
        });
      }

      return initializedMembers;
    }
  }

  flowEnumParseExplicitType({
    enumName
  }) {
    if (this.eatContextual("of")) {
      if (!this.match(types$1.name)) {
        throw this.flowEnumErrorInvalidExplicitType(this.state.start, {
          enumName,
          suppliedType: null
        });
      }

      const {
        value
      } = this.state;
      this.next();

      if (value !== "boolean" && value !== "number" && value !== "string" && value !== "symbol") {
        this.flowEnumErrorInvalidExplicitType(this.state.start, {
          enumName,
          suppliedType: value
        });
      }

      return value;
    }

    return null;
  }

  flowEnumBody(node, {
    enumName,
    nameLoc
  }) {
    const explicitType = this.flowEnumParseExplicitType({
      enumName
    });
    this.expect(types$1.braceL);
    const {
      members,
      hasUnknownMembers
    } = this.flowEnumMembers({
      enumName,
      explicitType
    });
    node.hasUnknownMembers = hasUnknownMembers;

    switch (explicitType) {
      case "boolean":
        node.explicitType = true;
        node.members = members.booleanMembers;
        this.expect(types$1.braceR);
        return this.finishNode(node, "EnumBooleanBody");

      case "number":
        node.explicitType = true;
        node.members = members.numberMembers;
        this.expect(types$1.braceR);
        return this.finishNode(node, "EnumNumberBody");

      case "string":
        node.explicitType = true;
        node.members = this.flowEnumStringMembers(members.stringMembers, members.defaultedMembers, {
          enumName
        });
        this.expect(types$1.braceR);
        return this.finishNode(node, "EnumStringBody");

      case "symbol":
        node.members = members.defaultedMembers;
        this.expect(types$1.braceR);
        return this.finishNode(node, "EnumSymbolBody");

      default:
        {
          const empty = () => {
            node.members = [];
            this.expect(types$1.braceR);
            return this.finishNode(node, "EnumStringBody");
          };

          node.explicitType = false;
          const boolsLen = members.booleanMembers.length;
          const numsLen = members.numberMembers.length;
          const strsLen = members.stringMembers.length;
          const defaultedLen = members.defaultedMembers.length;

          if (!boolsLen && !numsLen && !strsLen && !defaultedLen) {
            return empty();
          } else if (!boolsLen && !numsLen) {
            node.members = this.flowEnumStringMembers(members.stringMembers, members.defaultedMembers, {
              enumName
            });
            this.expect(types$1.braceR);
            return this.finishNode(node, "EnumStringBody");
          } else if (!numsLen && !strsLen && boolsLen >= defaultedLen) {
            for (const member of members.defaultedMembers) {
              this.flowEnumErrorBooleanMemberNotInitialized(member.start, {
                enumName,
                memberName: member.id.name
              });
            }

            node.members = members.booleanMembers;
            this.expect(types$1.braceR);
            return this.finishNode(node, "EnumBooleanBody");
          } else if (!boolsLen && !strsLen && numsLen >= defaultedLen) {
            for (const member of members.defaultedMembers) {
              this.flowEnumErrorNumberMemberNotInitialized(member.start, {
                enumName,
                memberName: member.id.name
              });
            }

            node.members = members.numberMembers;
            this.expect(types$1.braceR);
            return this.finishNode(node, "EnumNumberBody");
          } else {
            this.flowEnumErrorInconsistentMemberValues(nameLoc, {
              enumName
            });
            return empty();
          }
        }
    }
  }

  flowParseEnumDeclaration(node) {
    const id = this.parseIdentifier();
    node.id = id;
    node.body = this.flowEnumBody(this.startNode(), {
      enumName: id.name,
      nameLoc: id.start
    });
    return this.finishNode(node, "EnumDeclaration");
  }

  isLookaheadToken_lt() {
    const next = this.nextTokenStart();

    if (this.input.charCodeAt(next) === 60) {
      const afterNext = this.input.charCodeAt(next + 1);
      return afterNext !== 60 && afterNext !== 61;
    }

    return false;
  }

  maybeUnwrapTypeCastExpression(node) {
    return node.type === "TypeCastExpression" ? node.expression : node;
  }

});

const entities = {
  quot: "\u0022",
  amp: "&",
  apos: "\u0027",
  lt: "<",
  gt: ">",
  nbsp: "\u00A0",
  iexcl: "\u00A1",
  cent: "\u00A2",
  pound: "\u00A3",
  curren: "\u00A4",
  yen: "\u00A5",
  brvbar: "\u00A6",
  sect: "\u00A7",
  uml: "\u00A8",
  copy: "\u00A9",
  ordf: "\u00AA",
  laquo: "\u00AB",
  not: "\u00AC",
  shy: "\u00AD",
  reg: "\u00AE",
  macr: "\u00AF",
  deg: "\u00B0",
  plusmn: "\u00B1",
  sup2: "\u00B2",
  sup3: "\u00B3",
  acute: "\u00B4",
  micro: "\u00B5",
  para: "\u00B6",
  middot: "\u00B7",
  cedil: "\u00B8",
  sup1: "\u00B9",
  ordm: "\u00BA",
  raquo: "\u00BB",
  frac14: "\u00BC",
  frac12: "\u00BD",
  frac34: "\u00BE",
  iquest: "\u00BF",
  Agrave: "\u00C0",
  Aacute: "\u00C1",
  Acirc: "\u00C2",
  Atilde: "\u00C3",
  Auml: "\u00C4",
  Aring: "\u00C5",
  AElig: "\u00C6",
  Ccedil: "\u00C7",
  Egrave: "\u00C8",
  Eacute: "\u00C9",
  Ecirc: "\u00CA",
  Euml: "\u00CB",
  Igrave: "\u00CC",
  Iacute: "\u00CD",
  Icirc: "\u00CE",
  Iuml: "\u00CF",
  ETH: "\u00D0",
  Ntilde: "\u00D1",
  Ograve: "\u00D2",
  Oacute: "\u00D3",
  Ocirc: "\u00D4",
  Otilde: "\u00D5",
  Ouml: "\u00D6",
  times: "\u00D7",
  Oslash: "\u00D8",
  Ugrave: "\u00D9",
  Uacute: "\u00DA",
  Ucirc: "\u00DB",
  Uuml: "\u00DC",
  Yacute: "\u00DD",
  THORN: "\u00DE",
  szlig: "\u00DF",
  agrave: "\u00E0",
  aacute: "\u00E1",
  acirc: "\u00E2",
  atilde: "\u00E3",
  auml: "\u00E4",
  aring: "\u00E5",
  aelig: "\u00E6",
  ccedil: "\u00E7",
  egrave: "\u00E8",
  eacute: "\u00E9",
  ecirc: "\u00EA",
  euml: "\u00EB",
  igrave: "\u00EC",
  iacute: "\u00ED",
  icirc: "\u00EE",
  iuml: "\u00EF",
  eth: "\u00F0",
  ntilde: "\u00F1",
  ograve: "\u00F2",
  oacute: "\u00F3",
  ocirc: "\u00F4",
  otilde: "\u00F5",
  ouml: "\u00F6",
  divide: "\u00F7",
  oslash: "\u00F8",
  ugrave: "\u00F9",
  uacute: "\u00FA",
  ucirc: "\u00FB",
  uuml: "\u00FC",
  yacute: "\u00FD",
  thorn: "\u00FE",
  yuml: "\u00FF",
  OElig: "\u0152",
  oelig: "\u0153",
  Scaron: "\u0160",
  scaron: "\u0161",
  Yuml: "\u0178",
  fnof: "\u0192",
  circ: "\u02C6",
  tilde: "\u02DC",
  Alpha: "\u0391",
  Beta: "\u0392",
  Gamma: "\u0393",
  Delta: "\u0394",
  Epsilon: "\u0395",
  Zeta: "\u0396",
  Eta: "\u0397",
  Theta: "\u0398",
  Iota: "\u0399",
  Kappa: "\u039A",
  Lambda: "\u039B",
  Mu: "\u039C",
  Nu: "\u039D",
  Xi: "\u039E",
  Omicron: "\u039F",
  Pi: "\u03A0",
  Rho: "\u03A1",
  Sigma: "\u03A3",
  Tau: "\u03A4",
  Upsilon: "\u03A5",
  Phi: "\u03A6",
  Chi: "\u03A7",
  Psi: "\u03A8",
  Omega: "\u03A9",
  alpha: "\u03B1",
  beta: "\u03B2",
  gamma: "\u03B3",
  delta: "\u03B4",
  epsilon: "\u03B5",
  zeta: "\u03B6",
  eta: "\u03B7",
  theta: "\u03B8",
  iota: "\u03B9",
  kappa: "\u03BA",
  lambda: "\u03BB",
  mu: "\u03BC",
  nu: "\u03BD",
  xi: "\u03BE",
  omicron: "\u03BF",
  pi: "\u03C0",
  rho: "\u03C1",
  sigmaf: "\u03C2",
  sigma: "\u03C3",
  tau: "\u03C4",
  upsilon: "\u03C5",
  phi: "\u03C6",
  chi: "\u03C7",
  psi: "\u03C8",
  omega: "\u03C9",
  thetasym: "\u03D1",
  upsih: "\u03D2",
  piv: "\u03D6",
  ensp: "\u2002",
  emsp: "\u2003",
  thinsp: "\u2009",
  zwnj: "\u200C",
  zwj: "\u200D",
  lrm: "\u200E",
  rlm: "\u200F",
  ndash: "\u2013",
  mdash: "\u2014",
  lsquo: "\u2018",
  rsquo: "\u2019",
  sbquo: "\u201A",
  ldquo: "\u201C",
  rdquo: "\u201D",
  bdquo: "\u201E",
  dagger: "\u2020",
  Dagger: "\u2021",
  bull: "\u2022",
  hellip: "\u2026",
  permil: "\u2030",
  prime: "\u2032",
  Prime: "\u2033",
  lsaquo: "\u2039",
  rsaquo: "\u203A",
  oline: "\u203E",
  frasl: "\u2044",
  euro: "\u20AC",
  image: "\u2111",
  weierp: "\u2118",
  real: "\u211C",
  trade: "\u2122",
  alefsym: "\u2135",
  larr: "\u2190",
  uarr: "\u2191",
  rarr: "\u2192",
  darr: "\u2193",
  harr: "\u2194",
  crarr: "\u21B5",
  lArr: "\u21D0",
  uArr: "\u21D1",
  rArr: "\u21D2",
  dArr: "\u21D3",
  hArr: "\u21D4",
  forall: "\u2200",
  part: "\u2202",
  exist: "\u2203",
  empty: "\u2205",
  nabla: "\u2207",
  isin: "\u2208",
  notin: "\u2209",
  ni: "\u220B",
  prod: "\u220F",
  sum: "\u2211",
  minus: "\u2212",
  lowast: "\u2217",
  radic: "\u221A",
  prop: "\u221D",
  infin: "\u221E",
  ang: "\u2220",
  and: "\u2227",
  or: "\u2228",
  cap: "\u2229",
  cup: "\u222A",
  int: "\u222B",
  there4: "\u2234",
  sim: "\u223C",
  cong: "\u2245",
  asymp: "\u2248",
  ne: "\u2260",
  equiv: "\u2261",
  le: "\u2264",
  ge: "\u2265",
  sub: "\u2282",
  sup: "\u2283",
  nsub: "\u2284",
  sube: "\u2286",
  supe: "\u2287",
  oplus: "\u2295",
  otimes: "\u2297",
  perp: "\u22A5",
  sdot: "\u22C5",
  lceil: "\u2308",
  rceil: "\u2309",
  lfloor: "\u230A",
  rfloor: "\u230B",
  lang: "\u2329",
  rang: "\u232A",
  loz: "\u25CA",
  spades: "\u2660",
  clubs: "\u2663",
  hearts: "\u2665",
  diams: "\u2666"
};

class State {
  constructor() {
    this.strict = void 0;
    this.curLine = void 0;
    this.startLoc = void 0;
    this.endLoc = void 0;
    this.errors = [];
    this.potentialArrowAt = -1;
    this.noArrowAt = [];
    this.noArrowParamsConversionAt = [];
    this.maybeInArrowParameters = false;
    this.inPipeline = false;
    this.inType = false;
    this.noAnonFunctionType = false;
    this.inPropertyName = false;
    this.hasFlowComment = false;
    this.isAmbientContext = false;
    this.inAbstractClass = false;
    this.topicContext = {
      maxNumOfResolvableTopics: 0,
      maxTopicIndex: null
    };
    this.soloAwait = false;
    this.inFSharpPipelineDirectBody = false;
    this.labels = [];
    this.decoratorStack = [[]];
    this.comments = [];
    this.trailingComments = [];
    this.leadingComments = [];
    this.commentStack = [];
    this.commentPreviousNode = null;
    this.pos = 0;
    this.lineStart = 0;
    this.type = types$1.eof;
    this.value = null;
    this.start = 0;
    this.end = 0;
    this.lastTokEndLoc = null;
    this.lastTokStartLoc = null;
    this.lastTokStart = 0;
    this.lastTokEnd = 0;
    this.context = [types.brace];
    this.exprAllowed = true;
    this.containsEsc = false;
    this.strictErrors = new Map();
    this.tokensLength = 0;
  }

  init(options) {
    this.strict = options.strictMode === false ? false : options.sourceType === "module";
    this.curLine = options.startLine;
    this.startLoc = this.endLoc = this.curPosition();
  }

  curPosition() {
    return new Position(this.curLine, this.pos - this.lineStart);
  }

  clone(skipArrays) {
    const state = new State();
    const keys = Object.keys(this);

    for (let i = 0, length = keys.length; i < length; i++) {
      const key = keys[i];
      let val = this[key];

      if (!skipArrays && Array.isArray(val)) {
        val = val.slice();
      }

      state[key] = val;
    }

    return state;
  }

}

const HEX_NUMBER = /^[\da-fA-F]+$/;
const DECIMAL_NUMBER = /^\d+$/;
const JsxErrors = makeErrorTemplates({
  AttributeIsEmpty: "JSX attributes must only be assigned a non-empty expression.",
  MissingClosingTagElement: "Expected corresponding JSX closing tag for <%0>.",
  MissingClosingTagFragment: "Expected corresponding JSX closing tag for <>.",
  UnexpectedSequenceExpression: "Sequence expressions cannot be directly nested inside JSX. Did you mean to wrap it in parentheses (...)?",
  UnsupportedJsxValue: "JSX value should be either an expression or a quoted JSX text.",
  UnterminatedJsxContent: "Unterminated JSX contents.",
  UnwrappedAdjacentJSXElements: "Adjacent JSX elements must be wrapped in an enclosing tag. Did you want a JSX fragment <>...</>?"
}, ErrorCodes.SyntaxError);
types.j_oTag = new TokContext("<tag");
types.j_cTag = new TokContext("</tag");
types.j_expr = new TokContext("<tag>...</tag>", true);
types$1.jsxName = new TokenType("jsxName");
types$1.jsxText = new TokenType("jsxText", {
  beforeExpr: true
});
types$1.jsxTagStart = new TokenType("jsxTagStart", {
  startsExpr: true
});
types$1.jsxTagEnd = new TokenType("jsxTagEnd");

types$1.jsxTagStart.updateContext = context => {
  context.push(types.j_expr, types.j_oTag);
};

function isFragment(object) {
  return object ? object.type === "JSXOpeningFragment" || object.type === "JSXClosingFragment" : false;
}

function getQualifiedJSXName(object) {
  if (object.type === "JSXIdentifier") {
    return object.name;
  }

  if (object.type === "JSXNamespacedName") {
    return object.namespace.name + ":" + object.name.name;
  }

  if (object.type === "JSXMemberExpression") {
    return getQualifiedJSXName(object.object) + "." + getQualifiedJSXName(object.property);
  }

  throw new Error("Node had unexpected type: " + object.type);
}

var jsx = (superClass => class extends superClass {
  jsxReadToken() {
    let out = "";
    let chunkStart = this.state.pos;

    for (;;) {
      if (this.state.pos >= this.length) {
        throw this.raise(this.state.start, JsxErrors.UnterminatedJsxContent);
      }

      const ch = this.input.charCodeAt(this.state.pos);

      switch (ch) {
        case 60:
        case 123:
          if (this.state.pos === this.state.start) {
            if (ch === 60 && this.state.exprAllowed) {
              ++this.state.pos;
              return this.finishToken(types$1.jsxTagStart);
            }

            return super.getTokenFromCode(ch);
          }

          out += this.input.slice(chunkStart, this.state.pos);
          return this.finishToken(types$1.jsxText, out);

        case 38:
          out += this.input.slice(chunkStart, this.state.pos);
          out += this.jsxReadEntity();
          chunkStart = this.state.pos;
          break;

        case 62:
        case 125:

        default:
          if (isNewLine(ch)) {
            out += this.input.slice(chunkStart, this.state.pos);
            out += this.jsxReadNewLine(true);
            chunkStart = this.state.pos;
          } else {
            ++this.state.pos;
          }

      }
    }
  }

  jsxReadNewLine(normalizeCRLF) {
    const ch = this.input.charCodeAt(this.state.pos);
    let out;
    ++this.state.pos;

    if (ch === 13 && this.input.charCodeAt(this.state.pos) === 10) {
      ++this.state.pos;
      out = normalizeCRLF ? "\n" : "\r\n";
    } else {
      out = String.fromCharCode(ch);
    }

    ++this.state.curLine;
    this.state.lineStart = this.state.pos;
    return out;
  }

  jsxReadString(quote) {
    let out = "";
    let chunkStart = ++this.state.pos;

    for (;;) {
      if (this.state.pos >= this.length) {
        throw this.raise(this.state.start, ErrorMessages.UnterminatedString);
      }

      const ch = this.input.charCodeAt(this.state.pos);
      if (ch === quote) break;

      if (ch === 38) {
        out += this.input.slice(chunkStart, this.state.pos);
        out += this.jsxReadEntity();
        chunkStart = this.state.pos;
      } else if (isNewLine(ch)) {
        out += this.input.slice(chunkStart, this.state.pos);
        out += this.jsxReadNewLine(false);
        chunkStart = this.state.pos;
      } else {
        ++this.state.pos;
      }
    }

    out += this.input.slice(chunkStart, this.state.pos++);
    return this.finishToken(types$1.string, out);
  }

  jsxReadEntity() {
    let str = "";
    let count = 0;
    let entity;
    let ch = this.input[this.state.pos];
    const startPos = ++this.state.pos;

    while (this.state.pos < this.length && count++ < 10) {
      ch = this.input[this.state.pos++];

      if (ch === ";") {
        if (str[0] === "#") {
          if (str[1] === "x") {
            str = str.substr(2);

            if (HEX_NUMBER.test(str)) {
              entity = String.fromCodePoint(parseInt(str, 16));
            }
          } else {
            str = str.substr(1);

            if (DECIMAL_NUMBER.test(str)) {
              entity = String.fromCodePoint(parseInt(str, 10));
            }
          }
        } else {
          entity = entities[str];
        }

        break;
      }

      str += ch;
    }

    if (!entity) {
      this.state.pos = startPos;
      return "&";
    }

    return entity;
  }

  jsxReadWord() {
    let ch;
    const start = this.state.pos;

    do {
      ch = this.input.charCodeAt(++this.state.pos);
    } while (isIdentifierChar(ch) || ch === 45);

    return this.finishToken(types$1.jsxName, this.input.slice(start, this.state.pos));
  }

  jsxParseIdentifier() {
    const node = this.startNode();

    if (this.match(types$1.jsxName)) {
      node.name = this.state.value;
    } else if (this.state.type.keyword) {
      node.name = this.state.type.keyword;
    } else {
      this.unexpected();
    }

    this.next();
    return this.finishNode(node, "JSXIdentifier");
  }

  jsxParseNamespacedName() {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    const name = this.jsxParseIdentifier();
    if (!this.eat(types$1.colon)) return name;
    const node = this.startNodeAt(startPos, startLoc);
    node.namespace = name;
    node.name = this.jsxParseIdentifier();
    return this.finishNode(node, "JSXNamespacedName");
  }

  jsxParseElementName() {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    let node = this.jsxParseNamespacedName();

    if (node.type === "JSXNamespacedName") {
      return node;
    }

    while (this.eat(types$1.dot)) {
      const newNode = this.startNodeAt(startPos, startLoc);
      newNode.object = node;
      newNode.property = this.jsxParseIdentifier();
      node = this.finishNode(newNode, "JSXMemberExpression");
    }

    return node;
  }

  jsxParseAttributeValue() {
    let node;

    switch (this.state.type) {
      case types$1.braceL:
        node = this.startNode();
        this.next();
        node = this.jsxParseExpressionContainer(node);

        if (node.expression.type === "JSXEmptyExpression") {
          this.raise(node.start, JsxErrors.AttributeIsEmpty);
        }

        return node;

      case types$1.jsxTagStart:
      case types$1.string:
        return this.parseExprAtom();

      default:
        throw this.raise(this.state.start, JsxErrors.UnsupportedJsxValue);
    }
  }

  jsxParseEmptyExpression() {
    const node = this.startNodeAt(this.state.lastTokEnd, this.state.lastTokEndLoc);
    return this.finishNodeAt(node, "JSXEmptyExpression", this.state.start, this.state.startLoc);
  }

  jsxParseSpreadChild(node) {
    this.next();
    node.expression = this.parseExpression();
    this.expect(types$1.braceR);
    return this.finishNode(node, "JSXSpreadChild");
  }

  jsxParseExpressionContainer(node) {
    if (this.match(types$1.braceR)) {
      node.expression = this.jsxParseEmptyExpression();
    } else {
      const expression = this.parseExpression();
      node.expression = expression;
    }

    this.expect(types$1.braceR);
    return this.finishNode(node, "JSXExpressionContainer");
  }

  jsxParseAttribute() {
    const node = this.startNode();

    if (this.eat(types$1.braceL)) {
      this.expect(types$1.ellipsis);
      node.argument = this.parseMaybeAssignAllowIn();
      this.expect(types$1.braceR);
      return this.finishNode(node, "JSXSpreadAttribute");
    }

    node.name = this.jsxParseNamespacedName();
    node.value = this.eat(types$1.eq) ? this.jsxParseAttributeValue() : null;
    return this.finishNode(node, "JSXAttribute");
  }

  jsxParseOpeningElementAt(startPos, startLoc) {
    const node = this.startNodeAt(startPos, startLoc);

    if (this.match(types$1.jsxTagEnd)) {
      this.expect(types$1.jsxTagEnd);
      return this.finishNode(node, "JSXOpeningFragment");
    }

    node.name = this.jsxParseElementName();
    return this.jsxParseOpeningElementAfterName(node);
  }

  jsxParseOpeningElementAfterName(node) {
    const attributes = [];

    while (!this.match(types$1.slash) && !this.match(types$1.jsxTagEnd)) {
      attributes.push(this.jsxParseAttribute());
    }

    node.attributes = attributes;
    node.selfClosing = this.eat(types$1.slash);
    this.expect(types$1.jsxTagEnd);
    return this.finishNode(node, "JSXOpeningElement");
  }

  jsxParseClosingElementAt(startPos, startLoc) {
    const node = this.startNodeAt(startPos, startLoc);

    if (this.match(types$1.jsxTagEnd)) {
      this.expect(types$1.jsxTagEnd);
      return this.finishNode(node, "JSXClosingFragment");
    }

    node.name = this.jsxParseElementName();
    this.expect(types$1.jsxTagEnd);
    return this.finishNode(node, "JSXClosingElement");
  }

  jsxParseElementAt(startPos, startLoc) {
    const node = this.startNodeAt(startPos, startLoc);
    const children = [];
    const openingElement = this.jsxParseOpeningElementAt(startPos, startLoc);
    let closingElement = null;

    if (!openingElement.selfClosing) {
      contents: for (;;) {
        switch (this.state.type) {
          case types$1.jsxTagStart:
            startPos = this.state.start;
            startLoc = this.state.startLoc;
            this.next();

            if (this.eat(types$1.slash)) {
              closingElement = this.jsxParseClosingElementAt(startPos, startLoc);
              break contents;
            }

            children.push(this.jsxParseElementAt(startPos, startLoc));
            break;

          case types$1.jsxText:
            children.push(this.parseExprAtom());
            break;

          case types$1.braceL:
            {
              const node = this.startNode();
              this.next();

              if (this.match(types$1.ellipsis)) {
                children.push(this.jsxParseSpreadChild(node));
              } else {
                children.push(this.jsxParseExpressionContainer(node));
              }

              break;
            }

          default:
            throw this.unexpected();
        }
      }

      if (isFragment(openingElement) && !isFragment(closingElement)) {
        this.raise(closingElement.start, JsxErrors.MissingClosingTagFragment);
      } else if (!isFragment(openingElement) && isFragment(closingElement)) {
        this.raise(closingElement.start, JsxErrors.MissingClosingTagElement, getQualifiedJSXName(openingElement.name));
      } else if (!isFragment(openingElement) && !isFragment(closingElement)) {
        if (getQualifiedJSXName(closingElement.name) !== getQualifiedJSXName(openingElement.name)) {
          this.raise(closingElement.start, JsxErrors.MissingClosingTagElement, getQualifiedJSXName(openingElement.name));
        }
      }
    }

    if (isFragment(openingElement)) {
      node.openingFragment = openingElement;
      node.closingFragment = closingElement;
    } else {
      node.openingElement = openingElement;
      node.closingElement = closingElement;
    }

    node.children = children;

    if (this.isRelational("<")) {
      throw this.raise(this.state.start, JsxErrors.UnwrappedAdjacentJSXElements);
    }

    return isFragment(openingElement) ? this.finishNode(node, "JSXFragment") : this.finishNode(node, "JSXElement");
  }

  jsxParseElement() {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    this.next();
    return this.jsxParseElementAt(startPos, startLoc);
  }

  parseExprAtom(refExpressionErrors) {
    if (this.match(types$1.jsxText)) {
      return this.parseLiteral(this.state.value, "JSXText");
    } else if (this.match(types$1.jsxTagStart)) {
      return this.jsxParseElement();
    } else if (this.isRelational("<") && this.input.charCodeAt(this.state.pos) !== 33) {
      this.finishToken(types$1.jsxTagStart);
      return this.jsxParseElement();
    } else {
      return super.parseExprAtom(refExpressionErrors);
    }
  }

  createLookaheadState(state) {
    const lookaheadState = super.createLookaheadState(state);
    lookaheadState.inPropertyName = state.inPropertyName;
    return lookaheadState;
  }

  getTokenFromCode(code) {
    if (this.state.inPropertyName) return super.getTokenFromCode(code);
    const context = this.curContext();

    if (context === types.j_expr) {
      return this.jsxReadToken();
    }

    if (context === types.j_oTag || context === types.j_cTag) {
      if (isIdentifierStart(code)) {
        return this.jsxReadWord();
      }

      if (code === 62) {
        ++this.state.pos;
        return this.finishToken(types$1.jsxTagEnd);
      }

      if ((code === 34 || code === 39) && context === types.j_oTag) {
        return this.jsxReadString(code);
      }
    }

    if (code === 60 && this.state.exprAllowed && this.input.charCodeAt(this.state.pos + 1) !== 33) {
      ++this.state.pos;
      return this.finishToken(types$1.jsxTagStart);
    }

    return super.getTokenFromCode(code);
  }

  updateContext(prevType) {
    super.updateContext(prevType);
    const {
      context,
      type
    } = this.state;

    if (type === types$1.slash && prevType === types$1.jsxTagStart) {
      context.splice(-2, 2, types.j_cTag);
      this.state.exprAllowed = false;
    } else if (type === types$1.jsxTagEnd) {
      const out = context.pop();

      if (out === types.j_oTag && prevType === types$1.slash || out === types.j_cTag) {
        context.pop();
        this.state.exprAllowed = context[context.length - 1] === types.j_expr;
      } else {
        this.state.exprAllowed = true;
      }
    } else if (type.keyword && (prevType === types$1.dot || prevType === types$1.questionDot)) {
      this.state.exprAllowed = false;
    } else {
      this.state.exprAllowed = type.beforeExpr;
    }
  }

});

class TypeScriptScope extends Scope {
  constructor(...args) {
    super(...args);
    this.types = new Set();
    this.enums = new Set();
    this.constEnums = new Set();
    this.classes = new Set();
    this.exportOnlyBindings = new Set();
  }

}

class TypeScriptScopeHandler extends ScopeHandler {
  createScope(flags) {
    return new TypeScriptScope(flags);
  }

  declareName(name, bindingType, pos) {
    const scope = this.currentScope();

    if (bindingType & BIND_FLAGS_TS_EXPORT_ONLY) {
      this.maybeExportDefined(scope, name);
      scope.exportOnlyBindings.add(name);
      return;
    }

    super.declareName(...arguments);

    if (bindingType & BIND_KIND_TYPE) {
      if (!(bindingType & BIND_KIND_VALUE)) {
        this.checkRedeclarationInScope(scope, name, bindingType, pos);
        this.maybeExportDefined(scope, name);
      }

      scope.types.add(name);
    }

    if (bindingType & BIND_FLAGS_TS_ENUM) scope.enums.add(name);
    if (bindingType & BIND_FLAGS_TS_CONST_ENUM) scope.constEnums.add(name);
    if (bindingType & BIND_FLAGS_CLASS) scope.classes.add(name);
  }

  isRedeclaredInScope(scope, name, bindingType) {
    if (scope.enums.has(name)) {
      if (bindingType & BIND_FLAGS_TS_ENUM) {
        const isConst = !!(bindingType & BIND_FLAGS_TS_CONST_ENUM);
        const wasConst = scope.constEnums.has(name);
        return isConst !== wasConst;
      }

      return true;
    }

    if (bindingType & BIND_FLAGS_CLASS && scope.classes.has(name)) {
      if (scope.lexical.has(name)) {
        return !!(bindingType & BIND_KIND_VALUE);
      } else {
        return false;
      }
    }

    if (bindingType & BIND_KIND_TYPE && scope.types.has(name)) {
      return true;
    }

    return super.isRedeclaredInScope(...arguments);
  }

  checkLocalExport(id) {
    const topLevelScope = this.scopeStack[0];
    const {
      name
    } = id;

    if (!topLevelScope.types.has(name) && !topLevelScope.exportOnlyBindings.has(name)) {
      super.checkLocalExport(id);
    }
  }

}

const PARAM = 0b0000,
      PARAM_YIELD = 0b0001,
      PARAM_AWAIT = 0b0010,
      PARAM_RETURN = 0b0100,
      PARAM_IN = 0b1000;
class ProductionParameterHandler {
  constructor() {
    this.stacks = [];
  }

  enter(flags) {
    this.stacks.push(flags);
  }

  exit() {
    this.stacks.pop();
  }

  currentFlags() {
    return this.stacks[this.stacks.length - 1];
  }

  get hasAwait() {
    return (this.currentFlags() & PARAM_AWAIT) > 0;
  }

  get hasYield() {
    return (this.currentFlags() & PARAM_YIELD) > 0;
  }

  get hasReturn() {
    return (this.currentFlags() & PARAM_RETURN) > 0;
  }

  get hasIn() {
    return (this.currentFlags() & PARAM_IN) > 0;
  }

}
function functionFlags(isAsync, isGenerator) {
  return (isAsync ? PARAM_AWAIT : 0) | (isGenerator ? PARAM_YIELD : 0);
}

function nonNull(x) {
  if (x == null) {
    throw new Error(`Unexpected ${x} value.`);
  }

  return x;
}

function assert(x) {
  if (!x) {
    throw new Error("Assert fail");
  }
}

const TSErrors = makeErrorTemplates({
  AbstractMethodHasImplementation: "Method '%0' cannot have an implementation because it is marked abstract.",
  AccesorCannotDeclareThisParameter: "'get' and 'set' accessors cannot declare 'this' parameters.",
  AccesorCannotHaveTypeParameters: "An accessor cannot have type parameters.",
  ClassMethodHasDeclare: "Class methods cannot have the 'declare' modifier.",
  ClassMethodHasReadonly: "Class methods cannot have the 'readonly' modifier.",
  ConstructorHasTypeParameters: "Type parameters cannot appear on a constructor declaration.",
  DeclareAccessor: "'declare' is not allowed in %0ters.",
  DeclareClassFieldHasInitializer: "Initializers are not allowed in ambient contexts.",
  DeclareFunctionHasImplementation: "An implementation cannot be declared in ambient contexts.",
  DuplicateAccessibilityModifier: "Accessibility modifier already seen.",
  DuplicateModifier: "Duplicate modifier: '%0'.",
  EmptyHeritageClauseType: "'%0' list cannot be empty.",
  EmptyTypeArguments: "Type argument list cannot be empty.",
  EmptyTypeParameters: "Type parameter list cannot be empty.",
  ExpectedAmbientAfterExportDeclare: "'export declare' must be followed by an ambient declaration.",
  ImportAliasHasImportType: "An import alias can not use 'import type'.",
  IncompatibleModifiers: "'%0' modifier cannot be used with '%1' modifier.",
  IndexSignatureHasAbstract: "Index signatures cannot have the 'abstract' modifier.",
  IndexSignatureHasAccessibility: "Index signatures cannot have an accessibility modifier ('%0').",
  IndexSignatureHasDeclare: "Index signatures cannot have the 'declare' modifier.",
  IndexSignatureHasOverride: "'override' modifier cannot appear on an index signature.",
  IndexSignatureHasStatic: "Index signatures cannot have the 'static' modifier.",
  InvalidModifierOnTypeMember: "'%0' modifier cannot appear on a type member.",
  InvalidModifiersOrder: "'%0' modifier must precede '%1' modifier.",
  InvalidTupleMemberLabel: "Tuple members must be labeled with a simple identifier.",
  MixedLabeledAndUnlabeledElements: "Tuple members must all have names or all not have names.",
  NonAbstractClassHasAbstractMethod: "Abstract methods can only appear within an abstract class.",
  NonClassMethodPropertyHasAbstractModifer: "'abstract' modifier can only appear on a class, method, or property declaration.",
  OptionalTypeBeforeRequired: "A required element cannot follow an optional element.",
  OverrideNotInSubClass: "This member cannot have an 'override' modifier because its containing class does not extend another class.",
  PatternIsOptional: "A binding pattern parameter cannot be optional in an implementation signature.",
  PrivateElementHasAbstract: "Private elements cannot have the 'abstract' modifier.",
  PrivateElementHasAccessibility: "Private elements cannot have an accessibility modifier ('%0').",
  ReadonlyForMethodSignature: "'readonly' modifier can only appear on a property declaration or index signature.",
  SetAccesorCannotHaveOptionalParameter: "A 'set' accessor cannot have an optional parameter.",
  SetAccesorCannotHaveRestParameter: "A 'set' accessor cannot have rest parameter.",
  SetAccesorCannotHaveReturnType: "A 'set' accessor cannot have a return type annotation.",
  StaticBlockCannotHaveModifier: "Static class blocks cannot have any modifier.",
  TypeAnnotationAfterAssign: "Type annotations must come before default assignments, e.g. instead of `age = 25: number` use `age: number = 25`.",
  TypeImportCannotSpecifyDefaultAndNamed: "A type-only import can specify a default import or named bindings, but not both.",
  UnexpectedParameterModifier: "A parameter property is only allowed in a constructor implementation.",
  UnexpectedReadonly: "'readonly' type modifier is only permitted on array and tuple literal types.",
  UnexpectedTypeAnnotation: "Did not expect a type annotation here.",
  UnexpectedTypeCastInParameter: "Unexpected type cast in parameter position.",
  UnsupportedImportTypeArgument: "Argument in a type import must be a string literal.",
  UnsupportedParameterPropertyKind: "A parameter property may not be declared using a binding pattern.",
  UnsupportedSignatureParameterKind: "Name in a signature must be an Identifier, ObjectPattern or ArrayPattern, instead got %0."
}, ErrorCodes.SyntaxError);

function keywordTypeFromName(value) {
  switch (value) {
    case "any":
      return "TSAnyKeyword";

    case "boolean":
      return "TSBooleanKeyword";

    case "bigint":
      return "TSBigIntKeyword";

    case "never":
      return "TSNeverKeyword";

    case "number":
      return "TSNumberKeyword";

    case "object":
      return "TSObjectKeyword";

    case "string":
      return "TSStringKeyword";

    case "symbol":
      return "TSSymbolKeyword";

    case "undefined":
      return "TSUndefinedKeyword";

    case "unknown":
      return "TSUnknownKeyword";

    default:
      return undefined;
  }
}

function tsIsAccessModifier(modifier) {
  return modifier === "private" || modifier === "public" || modifier === "protected";
}

var typescript = (superClass => class extends superClass {
  getScopeHandler() {
    return TypeScriptScopeHandler;
  }

  tsIsIdentifier() {
    return this.match(types$1.name);
  }

  tsTokenCanFollowModifier() {
    return (this.match(types$1.bracketL) || this.match(types$1.braceL) || this.match(types$1.star) || this.match(types$1.ellipsis) || this.match(types$1.privateName) || this.isLiteralPropertyName()) && !this.hasPrecedingLineBreak();
  }

  tsNextTokenCanFollowModifier() {
    this.next();
    return this.tsTokenCanFollowModifier();
  }

  tsParseModifier(allowedModifiers) {
    if (!this.match(types$1.name)) {
      return undefined;
    }

    const modifier = this.state.value;

    if (allowedModifiers.indexOf(modifier) !== -1 && this.tsTryParse(this.tsNextTokenCanFollowModifier.bind(this))) {
      return modifier;
    }

    return undefined;
  }

  tsParseModifiers(modified, allowedModifiers, disallowedModifiers, errorTemplate) {
    const enforceOrder = (pos, modifier, before, after) => {
      if (modifier === before && modified[after]) {
        this.raise(pos, TSErrors.InvalidModifiersOrder, before, after);
      }
    };

    const incompatible = (pos, modifier, mod1, mod2) => {
      if (modified[mod1] && modifier === mod2 || modified[mod2] && modifier === mod1) {
        this.raise(pos, TSErrors.IncompatibleModifiers, mod1, mod2);
      }
    };

    for (;;) {
      const startPos = this.state.start;
      const modifier = this.tsParseModifier(allowedModifiers.concat(disallowedModifiers != null ? disallowedModifiers : []));
      if (!modifier) break;

      if (tsIsAccessModifier(modifier)) {
        if (modified.accessibility) {
          this.raise(startPos, TSErrors.DuplicateAccessibilityModifier);
        } else {
          enforceOrder(startPos, modifier, modifier, "override");
          enforceOrder(startPos, modifier, modifier, "static");
          enforceOrder(startPos, modifier, modifier, "readonly");
          modified.accessibility = modifier;
        }
      } else {
        if (Object.hasOwnProperty.call(modified, modifier)) {
          this.raise(startPos, TSErrors.DuplicateModifier, modifier);
        } else {
          enforceOrder(startPos, modifier, "static", "readonly");
          enforceOrder(startPos, modifier, "static", "override");
          enforceOrder(startPos, modifier, "override", "readonly");
          enforceOrder(startPos, modifier, "abstract", "override");
          incompatible(startPos, modifier, "declare", "override");
          incompatible(startPos, modifier, "static", "abstract");
        }

        modified[modifier] = true;
      }

      if (disallowedModifiers != null && disallowedModifiers.includes(modifier)) {
        this.raise(startPos, errorTemplate, modifier);
      }
    }
  }

  tsIsListTerminator(kind) {
    switch (kind) {
      case "EnumMembers":
      case "TypeMembers":
        return this.match(types$1.braceR);

      case "HeritageClauseElement":
        return this.match(types$1.braceL);

      case "TupleElementTypes":
        return this.match(types$1.bracketR);

      case "TypeParametersOrArguments":
        return this.isRelational(">");
    }

    throw new Error("Unreachable");
  }

  tsParseList(kind, parseElement) {
    const result = [];

    while (!this.tsIsListTerminator(kind)) {
      result.push(parseElement());
    }

    return result;
  }

  tsParseDelimitedList(kind, parseElement) {
    return nonNull(this.tsParseDelimitedListWorker(kind, parseElement, true));
  }

  tsParseDelimitedListWorker(kind, parseElement, expectSuccess) {
    const result = [];

    for (;;) {
      if (this.tsIsListTerminator(kind)) {
        break;
      }

      const element = parseElement();

      if (element == null) {
        return undefined;
      }

      result.push(element);

      if (this.eat(types$1.comma)) {
        continue;
      }

      if (this.tsIsListTerminator(kind)) {
        break;
      }

      if (expectSuccess) {
        this.expect(types$1.comma);
      }

      return undefined;
    }

    return result;
  }

  tsParseBracketedList(kind, parseElement, bracket, skipFirstToken) {
    if (!skipFirstToken) {
      if (bracket) {
        this.expect(types$1.bracketL);
      } else {
        this.expectRelational("<");
      }
    }

    const result = this.tsParseDelimitedList(kind, parseElement);

    if (bracket) {
      this.expect(types$1.bracketR);
    } else {
      this.expectRelational(">");
    }

    return result;
  }

  tsParseImportType() {
    const node = this.startNode();
    this.expect(types$1._import);
    this.expect(types$1.parenL);

    if (!this.match(types$1.string)) {
      this.raise(this.state.start, TSErrors.UnsupportedImportTypeArgument);
    }

    node.argument = this.parseExprAtom();
    this.expect(types$1.parenR);

    if (this.eat(types$1.dot)) {
      node.qualifier = this.tsParseEntityName(true);
    }

    if (this.isRelational("<")) {
      node.typeParameters = this.tsParseTypeArguments();
    }

    return this.finishNode(node, "TSImportType");
  }

  tsParseEntityName(allowReservedWords) {
    let entity = this.parseIdentifier();

    while (this.eat(types$1.dot)) {
      const node = this.startNodeAtNode(entity);
      node.left = entity;
      node.right = this.parseIdentifier(allowReservedWords);
      entity = this.finishNode(node, "TSQualifiedName");
    }

    return entity;
  }

  tsParseTypeReference() {
    const node = this.startNode();
    node.typeName = this.tsParseEntityName(false);

    if (!this.hasPrecedingLineBreak() && this.isRelational("<")) {
      node.typeParameters = this.tsParseTypeArguments();
    }

    return this.finishNode(node, "TSTypeReference");
  }

  tsParseThisTypePredicate(lhs) {
    this.next();
    const node = this.startNodeAtNode(lhs);
    node.parameterName = lhs;
    node.typeAnnotation = this.tsParseTypeAnnotation(false);
    node.asserts = false;
    return this.finishNode(node, "TSTypePredicate");
  }

  tsParseThisTypeNode() {
    const node = this.startNode();
    this.next();
    return this.finishNode(node, "TSThisType");
  }

  tsParseTypeQuery() {
    const node = this.startNode();
    this.expect(types$1._typeof);

    if (this.match(types$1._import)) {
      node.exprName = this.tsParseImportType();
    } else {
      node.exprName = this.tsParseEntityName(true);
    }

    return this.finishNode(node, "TSTypeQuery");
  }

  tsParseTypeParameter() {
    const node = this.startNode();
    node.name = this.parseIdentifierName(node.start);
    node.constraint = this.tsEatThenParseType(types$1._extends);
    node.default = this.tsEatThenParseType(types$1.eq);
    return this.finishNode(node, "TSTypeParameter");
  }

  tsTryParseTypeParameters() {
    if (this.isRelational("<")) {
      return this.tsParseTypeParameters();
    }
  }

  tsParseTypeParameters() {
    const node = this.startNode();

    if (this.isRelational("<") || this.match(types$1.jsxTagStart)) {
      this.next();
    } else {
      this.unexpected();
    }

    node.params = this.tsParseBracketedList("TypeParametersOrArguments", this.tsParseTypeParameter.bind(this), false, true);

    if (node.params.length === 0) {
      this.raise(node.start, TSErrors.EmptyTypeParameters);
    }

    return this.finishNode(node, "TSTypeParameterDeclaration");
  }

  tsTryNextParseConstantContext() {
    if (this.lookahead().type === types$1._const) {
      this.next();
      return this.tsParseTypeReference();
    }

    return null;
  }

  tsFillSignature(returnToken, signature) {
    const returnTokenRequired = returnToken === types$1.arrow;
    signature.typeParameters = this.tsTryParseTypeParameters();
    this.expect(types$1.parenL);
    signature.parameters = this.tsParseBindingListForSignature();

    if (returnTokenRequired) {
      signature.typeAnnotation = this.tsParseTypeOrTypePredicateAnnotation(returnToken);
    } else if (this.match(returnToken)) {
      signature.typeAnnotation = this.tsParseTypeOrTypePredicateAnnotation(returnToken);
    }
  }

  tsParseBindingListForSignature() {
    return this.parseBindingList(types$1.parenR, 41).map(pattern => {
      if (pattern.type !== "Identifier" && pattern.type !== "RestElement" && pattern.type !== "ObjectPattern" && pattern.type !== "ArrayPattern") {
        this.raise(pattern.start, TSErrors.UnsupportedSignatureParameterKind, pattern.type);
      }

      return pattern;
    });
  }

  tsParseTypeMemberSemicolon() {
    if (!this.eat(types$1.comma) && !this.isLineTerminator()) {
      this.expect(types$1.semi);
    }
  }

  tsParseSignatureMember(kind, node) {
    this.tsFillSignature(types$1.colon, node);
    this.tsParseTypeMemberSemicolon();
    return this.finishNode(node, kind);
  }

  tsIsUnambiguouslyIndexSignature() {
    this.next();
    return this.eat(types$1.name) && this.match(types$1.colon);
  }

  tsTryParseIndexSignature(node) {
    if (!(this.match(types$1.bracketL) && this.tsLookAhead(this.tsIsUnambiguouslyIndexSignature.bind(this)))) {
      return undefined;
    }

    this.expect(types$1.bracketL);
    const id = this.parseIdentifier();
    id.typeAnnotation = this.tsParseTypeAnnotation();
    this.resetEndLocation(id);
    this.expect(types$1.bracketR);
    node.parameters = [id];
    const type = this.tsTryParseTypeAnnotation();
    if (type) node.typeAnnotation = type;
    this.tsParseTypeMemberSemicolon();
    return this.finishNode(node, "TSIndexSignature");
  }

  tsParsePropertyOrMethodSignature(node, readonly) {
    if (this.eat(types$1.question)) node.optional = true;
    const nodeAny = node;

    if (this.match(types$1.parenL) || this.isRelational("<")) {
      if (readonly) {
        this.raise(node.start, TSErrors.ReadonlyForMethodSignature);
      }

      const method = nodeAny;

      if (method.kind && this.isRelational("<")) {
        this.raise(this.state.pos, TSErrors.AccesorCannotHaveTypeParameters);
      }

      this.tsFillSignature(types$1.colon, method);
      this.tsParseTypeMemberSemicolon();

      if (method.kind === "get") {
        if (method.parameters.length > 0) {
          this.raise(this.state.pos, ErrorMessages.BadGetterArity);

          if (this.isThisParam(method.parameters[0])) {
            this.raise(this.state.pos, TSErrors.AccesorCannotDeclareThisParameter);
          }
        }
      } else if (method.kind === "set") {
        if (method.parameters.length !== 1) {
          this.raise(this.state.pos, ErrorMessages.BadSetterArity);
        } else {
          const firstParameter = method.parameters[0];

          if (this.isThisParam(firstParameter)) {
            this.raise(this.state.pos, TSErrors.AccesorCannotDeclareThisParameter);
          }

          if (firstParameter.type === "Identifier" && firstParameter.optional) {
            this.raise(this.state.pos, TSErrors.SetAccesorCannotHaveOptionalParameter);
          }

          if (firstParameter.type === "RestElement") {
            this.raise(this.state.pos, TSErrors.SetAccesorCannotHaveRestParameter);
          }
        }

        if (method.typeAnnotation) {
          this.raise(method.typeAnnotation.start, TSErrors.SetAccesorCannotHaveReturnType);
        }
      } else {
        method.kind = "method";
      }

      return this.finishNode(method, "TSMethodSignature");
    } else {
      const property = nodeAny;
      if (readonly) property.readonly = true;
      const type = this.tsTryParseTypeAnnotation();
      if (type) property.typeAnnotation = type;
      this.tsParseTypeMemberSemicolon();
      return this.finishNode(property, "TSPropertySignature");
    }
  }

  tsParseTypeMember() {
    const node = this.startNode();

    if (this.match(types$1.parenL) || this.isRelational("<")) {
      return this.tsParseSignatureMember("TSCallSignatureDeclaration", node);
    }

    if (this.match(types$1._new)) {
      const id = this.startNode();
      this.next();

      if (this.match(types$1.parenL) || this.isRelational("<")) {
        return this.tsParseSignatureMember("TSConstructSignatureDeclaration", node);
      } else {
        node.key = this.createIdentifier(id, "new");
        return this.tsParsePropertyOrMethodSignature(node, false);
      }
    }

    this.tsParseModifiers(node, ["readonly"], ["declare", "abstract", "private", "protected", "public", "static", "override"], TSErrors.InvalidModifierOnTypeMember);
    const idx = this.tsTryParseIndexSignature(node);

    if (idx) {
      return idx;
    }

    this.parsePropertyName(node, false);

    if (!node.computed && node.key.type === "Identifier" && (node.key.name === "get" || node.key.name === "set") && this.tsTokenCanFollowModifier()) {
      node.kind = node.key.name;
      this.parsePropertyName(node, false);
    }

    return this.tsParsePropertyOrMethodSignature(node, !!node.readonly);
  }

  tsParseTypeLiteral() {
    const node = this.startNode();
    node.members = this.tsParseObjectTypeMembers();
    return this.finishNode(node, "TSTypeLiteral");
  }

  tsParseObjectTypeMembers() {
    this.expect(types$1.braceL);
    const members = this.tsParseList("TypeMembers", this.tsParseTypeMember.bind(this));
    this.expect(types$1.braceR);
    return members;
  }

  tsIsStartOfMappedType() {
    this.next();

    if (this.eat(types$1.plusMin)) {
      return this.isContextual("readonly");
    }

    if (this.isContextual("readonly")) {
      this.next();
    }

    if (!this.match(types$1.bracketL)) {
      return false;
    }

    this.next();

    if (!this.tsIsIdentifier()) {
      return false;
    }

    this.next();
    return this.match(types$1._in);
  }

  tsParseMappedTypeParameter() {
    const node = this.startNode();
    node.name = this.parseIdentifierName(node.start);
    node.constraint = this.tsExpectThenParseType(types$1._in);
    return this.finishNode(node, "TSTypeParameter");
  }

  tsParseMappedType() {
    const node = this.startNode();
    this.expect(types$1.braceL);

    if (this.match(types$1.plusMin)) {
      node.readonly = this.state.value;
      this.next();
      this.expectContextual("readonly");
    } else if (this.eatContextual("readonly")) {
      node.readonly = true;
    }

    this.expect(types$1.bracketL);
    node.typeParameter = this.tsParseMappedTypeParameter();
    node.nameType = this.eatContextual("as") ? this.tsParseType() : null;
    this.expect(types$1.bracketR);

    if (this.match(types$1.plusMin)) {
      node.optional = this.state.value;
      this.next();
      this.expect(types$1.question);
    } else if (this.eat(types$1.question)) {
      node.optional = true;
    }

    node.typeAnnotation = this.tsTryParseType();
    this.semicolon();
    this.expect(types$1.braceR);
    return this.finishNode(node, "TSMappedType");
  }

  tsParseTupleType() {
    const node = this.startNode();
    node.elementTypes = this.tsParseBracketedList("TupleElementTypes", this.tsParseTupleElementType.bind(this), true, false);
    let seenOptionalElement = false;
    let labeledElements = null;
    node.elementTypes.forEach(elementNode => {
      var _labeledElements;

      let {
        type
      } = elementNode;

      if (seenOptionalElement && type !== "TSRestType" && type !== "TSOptionalType" && !(type === "TSNamedTupleMember" && elementNode.optional)) {
        this.raise(elementNode.start, TSErrors.OptionalTypeBeforeRequired);
      }

      seenOptionalElement = seenOptionalElement || type === "TSNamedTupleMember" && elementNode.optional || type === "TSOptionalType";

      if (type === "TSRestType") {
        elementNode = elementNode.typeAnnotation;
        type = elementNode.type;
      }

      const isLabeled = type === "TSNamedTupleMember";
      labeledElements = (_labeledElements = labeledElements) != null ? _labeledElements : isLabeled;

      if (labeledElements !== isLabeled) {
        this.raise(elementNode.start, TSErrors.MixedLabeledAndUnlabeledElements);
      }
    });
    return this.finishNode(node, "TSTupleType");
  }

  tsParseTupleElementType() {
    const {
      start: startPos,
      startLoc
    } = this.state;
    const rest = this.eat(types$1.ellipsis);
    let type = this.tsParseType();
    const optional = this.eat(types$1.question);
    const labeled = this.eat(types$1.colon);

    if (labeled) {
      const labeledNode = this.startNodeAtNode(type);
      labeledNode.optional = optional;

      if (type.type === "TSTypeReference" && !type.typeParameters && type.typeName.type === "Identifier") {
        labeledNode.label = type.typeName;
      } else {
        this.raise(type.start, TSErrors.InvalidTupleMemberLabel);
        labeledNode.label = type;
      }

      labeledNode.elementType = this.tsParseType();
      type = this.finishNode(labeledNode, "TSNamedTupleMember");
    } else if (optional) {
      const optionalTypeNode = this.startNodeAtNode(type);
      optionalTypeNode.typeAnnotation = type;
      type = this.finishNode(optionalTypeNode, "TSOptionalType");
    }

    if (rest) {
      const restNode = this.startNodeAt(startPos, startLoc);
      restNode.typeAnnotation = type;
      type = this.finishNode(restNode, "TSRestType");
    }

    return type;
  }

  tsParseParenthesizedType() {
    const node = this.startNode();
    this.expect(types$1.parenL);
    node.typeAnnotation = this.tsParseType();
    this.expect(types$1.parenR);
    return this.finishNode(node, "TSParenthesizedType");
  }

  tsParseFunctionOrConstructorType(type, abstract) {
    const node = this.startNode();

    if (type === "TSConstructorType") {
      node.abstract = !!abstract;
      if (abstract) this.next();
      this.next();
    }

    this.tsFillSignature(types$1.arrow, node);
    return this.finishNode(node, type);
  }

  tsParseLiteralTypeNode() {
    const node = this.startNode();

    node.literal = (() => {
      switch (this.state.type) {
        case types$1.num:
        case types$1.bigint:
        case types$1.string:
        case types$1._true:
        case types$1._false:
          return this.parseExprAtom();

        default:
          throw this.unexpected();
      }
    })();

    return this.finishNode(node, "TSLiteralType");
  }

  tsParseTemplateLiteralType() {
    const node = this.startNode();
    node.literal = this.parseTemplate(false);
    return this.finishNode(node, "TSLiteralType");
  }

  parseTemplateSubstitution() {
    if (this.state.inType) return this.tsParseType();
    return super.parseTemplateSubstitution();
  }

  tsParseThisTypeOrThisTypePredicate() {
    const thisKeyword = this.tsParseThisTypeNode();

    if (this.isContextual("is") && !this.hasPrecedingLineBreak()) {
      return this.tsParseThisTypePredicate(thisKeyword);
    } else {
      return thisKeyword;
    }
  }

  tsParseNonArrayType() {
    switch (this.state.type) {
      case types$1.name:
      case types$1._void:
      case types$1._null:
        {
          const type = this.match(types$1._void) ? "TSVoidKeyword" : this.match(types$1._null) ? "TSNullKeyword" : keywordTypeFromName(this.state.value);

          if (type !== undefined && this.lookaheadCharCode() !== 46) {
            const node = this.startNode();
            this.next();
            return this.finishNode(node, type);
          }

          return this.tsParseTypeReference();
        }

      case types$1.string:
      case types$1.num:
      case types$1.bigint:
      case types$1._true:
      case types$1._false:
        return this.tsParseLiteralTypeNode();

      case types$1.plusMin:
        if (this.state.value === "-") {
          const node = this.startNode();
          const nextToken = this.lookahead();

          if (nextToken.type !== types$1.num && nextToken.type !== types$1.bigint) {
            throw this.unexpected();
          }

          node.literal = this.parseMaybeUnary();
          return this.finishNode(node, "TSLiteralType");
        }

        break;

      case types$1._this:
        return this.tsParseThisTypeOrThisTypePredicate();

      case types$1._typeof:
        return this.tsParseTypeQuery();

      case types$1._import:
        return this.tsParseImportType();

      case types$1.braceL:
        return this.tsLookAhead(this.tsIsStartOfMappedType.bind(this)) ? this.tsParseMappedType() : this.tsParseTypeLiteral();

      case types$1.bracketL:
        return this.tsParseTupleType();

      case types$1.parenL:
        return this.tsParseParenthesizedType();

      case types$1.backQuote:
        return this.tsParseTemplateLiteralType();
    }

    throw this.unexpected();
  }

  tsParseArrayTypeOrHigher() {
    let type = this.tsParseNonArrayType();

    while (!this.hasPrecedingLineBreak() && this.eat(types$1.bracketL)) {
      if (this.match(types$1.bracketR)) {
        const node = this.startNodeAtNode(type);
        node.elementType = type;
        this.expect(types$1.bracketR);
        type = this.finishNode(node, "TSArrayType");
      } else {
        const node = this.startNodeAtNode(type);
        node.objectType = type;
        node.indexType = this.tsParseType();
        this.expect(types$1.bracketR);
        type = this.finishNode(node, "TSIndexedAccessType");
      }
    }

    return type;
  }

  tsParseTypeOperator(operator) {
    const node = this.startNode();
    this.expectContextual(operator);
    node.operator = operator;
    node.typeAnnotation = this.tsParseTypeOperatorOrHigher();

    if (operator === "readonly") {
      this.tsCheckTypeAnnotationForReadOnly(node);
    }

    return this.finishNode(node, "TSTypeOperator");
  }

  tsCheckTypeAnnotationForReadOnly(node) {
    switch (node.typeAnnotation.type) {
      case "TSTupleType":
      case "TSArrayType":
        return;

      default:
        this.raise(node.start, TSErrors.UnexpectedReadonly);
    }
  }

  tsParseInferType() {
    const node = this.startNode();
    this.expectContextual("infer");
    const typeParameter = this.startNode();
    typeParameter.name = this.parseIdentifierName(typeParameter.start);
    node.typeParameter = this.finishNode(typeParameter, "TSTypeParameter");
    return this.finishNode(node, "TSInferType");
  }

  tsParseTypeOperatorOrHigher() {
    const operator = ["keyof", "unique", "readonly"].find(kw => this.isContextual(kw));
    return operator ? this.tsParseTypeOperator(operator) : this.isContextual("infer") ? this.tsParseInferType() : this.tsParseArrayTypeOrHigher();
  }

  tsParseUnionOrIntersectionType(kind, parseConstituentType, operator) {
    const node = this.startNode();
    const hasLeadingOperator = this.eat(operator);
    const types = [];

    do {
      types.push(parseConstituentType());
    } while (this.eat(operator));

    if (types.length === 1 && !hasLeadingOperator) {
      return types[0];
    }

    node.types = types;
    return this.finishNode(node, kind);
  }

  tsParseIntersectionTypeOrHigher() {
    return this.tsParseUnionOrIntersectionType("TSIntersectionType", this.tsParseTypeOperatorOrHigher.bind(this), types$1.bitwiseAND);
  }

  tsParseUnionTypeOrHigher() {
    return this.tsParseUnionOrIntersectionType("TSUnionType", this.tsParseIntersectionTypeOrHigher.bind(this), types$1.bitwiseOR);
  }

  tsIsStartOfFunctionType() {
    if (this.isRelational("<")) {
      return true;
    }

    return this.match(types$1.parenL) && this.tsLookAhead(this.tsIsUnambiguouslyStartOfFunctionType.bind(this));
  }

  tsSkipParameterStart() {
    if (this.match(types$1.name) || this.match(types$1._this)) {
      this.next();
      return true;
    }

    if (this.match(types$1.braceL)) {
      let braceStackCounter = 1;
      this.next();

      while (braceStackCounter > 0) {
        if (this.match(types$1.braceL)) {
          ++braceStackCounter;
        } else if (this.match(types$1.braceR)) {
          --braceStackCounter;
        }

        this.next();
      }

      return true;
    }

    if (this.match(types$1.bracketL)) {
      let braceStackCounter = 1;
      this.next();

      while (braceStackCounter > 0) {
        if (this.match(types$1.bracketL)) {
          ++braceStackCounter;
        } else if (this.match(types$1.bracketR)) {
          --braceStackCounter;
        }

        this.next();
      }

      return true;
    }

    return false;
  }

  tsIsUnambiguouslyStartOfFunctionType() {
    this.next();

    if (this.match(types$1.parenR) || this.match(types$1.ellipsis)) {
      return true;
    }

    if (this.tsSkipParameterStart()) {
      if (this.match(types$1.colon) || this.match(types$1.comma) || this.match(types$1.question) || this.match(types$1.eq)) {
        return true;
      }

      if (this.match(types$1.parenR)) {
        this.next();

        if (this.match(types$1.arrow)) {
          return true;
        }
      }
    }

    return false;
  }

  tsParseTypeOrTypePredicateAnnotation(returnToken) {
    return this.tsInType(() => {
      const t = this.startNode();
      this.expect(returnToken);
      const node = this.startNode();
      const asserts = !!this.tsTryParse(this.tsParseTypePredicateAsserts.bind(this));

      if (asserts && this.match(types$1._this)) {
        let thisTypePredicate = this.tsParseThisTypeOrThisTypePredicate();

        if (thisTypePredicate.type === "TSThisType") {
          node.parameterName = thisTypePredicate;
          node.asserts = true;
          node.typeAnnotation = null;
          thisTypePredicate = this.finishNode(node, "TSTypePredicate");
        } else {
          this.resetStartLocationFromNode(thisTypePredicate, node);
          thisTypePredicate.asserts = true;
        }

        t.typeAnnotation = thisTypePredicate;
        return this.finishNode(t, "TSTypeAnnotation");
      }

      const typePredicateVariable = this.tsIsIdentifier() && this.tsTryParse(this.tsParseTypePredicatePrefix.bind(this));

      if (!typePredicateVariable) {
        if (!asserts) {
          return this.tsParseTypeAnnotation(false, t);
        }

        node.parameterName = this.parseIdentifier();
        node.asserts = asserts;
        node.typeAnnotation = null;
        t.typeAnnotation = this.finishNode(node, "TSTypePredicate");
        return this.finishNode(t, "TSTypeAnnotation");
      }

      const type = this.tsParseTypeAnnotation(false);
      node.parameterName = typePredicateVariable;
      node.typeAnnotation = type;
      node.asserts = asserts;
      t.typeAnnotation = this.finishNode(node, "TSTypePredicate");
      return this.finishNode(t, "TSTypeAnnotation");
    });
  }

  tsTryParseTypeOrTypePredicateAnnotation() {
    return this.match(types$1.colon) ? this.tsParseTypeOrTypePredicateAnnotation(types$1.colon) : undefined;
  }

  tsTryParseTypeAnnotation() {
    return this.match(types$1.colon) ? this.tsParseTypeAnnotation() : undefined;
  }

  tsTryParseType() {
    return this.tsEatThenParseType(types$1.colon);
  }

  tsParseTypePredicatePrefix() {
    const id = this.parseIdentifier();

    if (this.isContextual("is") && !this.hasPrecedingLineBreak()) {
      this.next();
      return id;
    }
  }

  tsParseTypePredicateAsserts() {
    if (!this.match(types$1.name) || this.state.value !== "asserts" || this.hasPrecedingLineBreak()) {
      return false;
    }

    const containsEsc = this.state.containsEsc;
    this.next();

    if (!this.match(types$1.name) && !this.match(types$1._this)) {
      return false;
    }

    if (containsEsc) {
      this.raise(this.state.lastTokStart, ErrorMessages.InvalidEscapedReservedWord, "asserts");
    }

    return true;
  }

  tsParseTypeAnnotation(eatColon = true, t = this.startNode()) {
    this.tsInType(() => {
      if (eatColon) this.expect(types$1.colon);
      t.typeAnnotation = this.tsParseType();
    });
    return this.finishNode(t, "TSTypeAnnotation");
  }

  tsParseType() {
    assert(this.state.inType);
    const type = this.tsParseNonConditionalType();

    if (this.hasPrecedingLineBreak() || !this.eat(types$1._extends)) {
      return type;
    }

    const node = this.startNodeAtNode(type);
    node.checkType = type;
    node.extendsType = this.tsParseNonConditionalType();
    this.expect(types$1.question);
    node.trueType = this.tsParseType();
    this.expect(types$1.colon);
    node.falseType = this.tsParseType();
    return this.finishNode(node, "TSConditionalType");
  }

  isAbstractConstructorSignature() {
    return this.isContextual("abstract") && this.lookahead().type === types$1._new;
  }

  tsParseNonConditionalType() {
    if (this.tsIsStartOfFunctionType()) {
      return this.tsParseFunctionOrConstructorType("TSFunctionType");
    }

    if (this.match(types$1._new)) {
      return this.tsParseFunctionOrConstructorType("TSConstructorType");
    } else if (this.isAbstractConstructorSignature()) {
      return this.tsParseFunctionOrConstructorType("TSConstructorType", true);
    }

    return this.tsParseUnionTypeOrHigher();
  }

  tsParseTypeAssertion() {
    const node = this.startNode();

    const _const = this.tsTryNextParseConstantContext();

    node.typeAnnotation = _const || this.tsNextThenParseType();
    this.expectRelational(">");
    node.expression = this.parseMaybeUnary();
    return this.finishNode(node, "TSTypeAssertion");
  }

  tsParseHeritageClause(descriptor) {
    const originalStart = this.state.start;
    const delimitedList = this.tsParseDelimitedList("HeritageClauseElement", this.tsParseExpressionWithTypeArguments.bind(this));

    if (!delimitedList.length) {
      this.raise(originalStart, TSErrors.EmptyHeritageClauseType, descriptor);
    }

    return delimitedList;
  }

  tsParseExpressionWithTypeArguments() {
    const node = this.startNode();
    node.expression = this.tsParseEntityName(false);

    if (this.isRelational("<")) {
      node.typeParameters = this.tsParseTypeArguments();
    }

    return this.finishNode(node, "TSExpressionWithTypeArguments");
  }

  tsParseInterfaceDeclaration(node) {
    node.id = this.parseIdentifier();
    this.checkLVal(node.id, "typescript interface declaration", BIND_TS_INTERFACE);
    node.typeParameters = this.tsTryParseTypeParameters();

    if (this.eat(types$1._extends)) {
      node.extends = this.tsParseHeritageClause("extends");
    }

    const body = this.startNode();
    body.body = this.tsInType(this.tsParseObjectTypeMembers.bind(this));
    node.body = this.finishNode(body, "TSInterfaceBody");
    return this.finishNode(node, "TSInterfaceDeclaration");
  }

  tsParseTypeAliasDeclaration(node) {
    node.id = this.parseIdentifier();
    this.checkLVal(node.id, "typescript type alias", BIND_TS_TYPE);
    node.typeParameters = this.tsTryParseTypeParameters();
    node.typeAnnotation = this.tsInType(() => {
      this.expect(types$1.eq);

      if (this.isContextual("intrinsic") && this.lookahead().type !== types$1.dot) {
        const node = this.startNode();
        this.next();
        return this.finishNode(node, "TSIntrinsicKeyword");
      }

      return this.tsParseType();
    });
    this.semicolon();
    return this.finishNode(node, "TSTypeAliasDeclaration");
  }

  tsInNoContext(cb) {
    const oldContext = this.state.context;
    this.state.context = [oldContext[0]];

    try {
      return cb();
    } finally {
      this.state.context = oldContext;
    }
  }

  tsInType(cb) {
    const oldInType = this.state.inType;
    this.state.inType = true;

    try {
      return cb();
    } finally {
      this.state.inType = oldInType;
    }
  }

  tsEatThenParseType(token) {
    return !this.match(token) ? undefined : this.tsNextThenParseType();
  }

  tsExpectThenParseType(token) {
    return this.tsDoThenParseType(() => this.expect(token));
  }

  tsNextThenParseType() {
    return this.tsDoThenParseType(() => this.next());
  }

  tsDoThenParseType(cb) {
    return this.tsInType(() => {
      cb();
      return this.tsParseType();
    });
  }

  tsParseEnumMember() {
    const node = this.startNode();
    node.id = this.match(types$1.string) ? this.parseExprAtom() : this.parseIdentifier(true);

    if (this.eat(types$1.eq)) {
      node.initializer = this.parseMaybeAssignAllowIn();
    }

    return this.finishNode(node, "TSEnumMember");
  }

  tsParseEnumDeclaration(node, isConst) {
    if (isConst) node.const = true;
    node.id = this.parseIdentifier();
    this.checkLVal(node.id, "typescript enum declaration", isConst ? BIND_TS_CONST_ENUM : BIND_TS_ENUM);
    this.expect(types$1.braceL);
    node.members = this.tsParseDelimitedList("EnumMembers", this.tsParseEnumMember.bind(this));
    this.expect(types$1.braceR);
    return this.finishNode(node, "TSEnumDeclaration");
  }

  tsParseModuleBlock() {
    const node = this.startNode();
    this.scope.enter(SCOPE_OTHER);
    this.expect(types$1.braceL);
    this.parseBlockOrModuleBlockBody(node.body = [], undefined, true, types$1.braceR);
    this.scope.exit();
    return this.finishNode(node, "TSModuleBlock");
  }

  tsParseModuleOrNamespaceDeclaration(node, nested = false) {
    node.id = this.parseIdentifier();

    if (!nested) {
      this.checkLVal(node.id, "module or namespace declaration", BIND_TS_NAMESPACE);
    }

    if (this.eat(types$1.dot)) {
      const inner = this.startNode();
      this.tsParseModuleOrNamespaceDeclaration(inner, true);
      node.body = inner;
    } else {
      this.scope.enter(SCOPE_TS_MODULE);
      this.prodParam.enter(PARAM);
      node.body = this.tsParseModuleBlock();
      this.prodParam.exit();
      this.scope.exit();
    }

    return this.finishNode(node, "TSModuleDeclaration");
  }

  tsParseAmbientExternalModuleDeclaration(node) {
    if (this.isContextual("global")) {
      node.global = true;
      node.id = this.parseIdentifier();
    } else if (this.match(types$1.string)) {
      node.id = this.parseExprAtom();
    } else {
      this.unexpected();
    }

    if (this.match(types$1.braceL)) {
      this.scope.enter(SCOPE_TS_MODULE);
      this.prodParam.enter(PARAM);
      node.body = this.tsParseModuleBlock();
      this.prodParam.exit();
      this.scope.exit();
    } else {
      this.semicolon();
    }

    return this.finishNode(node, "TSModuleDeclaration");
  }

  tsParseImportEqualsDeclaration(node, isExport) {
    node.isExport = isExport || false;
    node.id = this.parseIdentifier();
    this.checkLVal(node.id, "import equals declaration", BIND_LEXICAL);
    this.expect(types$1.eq);
    const moduleReference = this.tsParseModuleReference();

    if (node.importKind === "type" && moduleReference.type !== "TSExternalModuleReference") {
      this.raise(moduleReference.start, TSErrors.ImportAliasHasImportType);
    }

    node.moduleReference = moduleReference;
    this.semicolon();
    return this.finishNode(node, "TSImportEqualsDeclaration");
  }

  tsIsExternalModuleReference() {
    return this.isContextual("require") && this.lookaheadCharCode() === 40;
  }

  tsParseModuleReference() {
    return this.tsIsExternalModuleReference() ? this.tsParseExternalModuleReference() : this.tsParseEntityName(false);
  }

  tsParseExternalModuleReference() {
    const node = this.startNode();
    this.expectContextual("require");
    this.expect(types$1.parenL);

    if (!this.match(types$1.string)) {
      throw this.unexpected();
    }

    node.expression = this.parseExprAtom();
    this.expect(types$1.parenR);
    return this.finishNode(node, "TSExternalModuleReference");
  }

  tsLookAhead(f) {
    const state = this.state.clone();
    const res = f();
    this.state = state;
    return res;
  }

  tsTryParseAndCatch(f) {
    const result = this.tryParse(abort => f() || abort());
    if (result.aborted || !result.node) return undefined;
    if (result.error) this.state = result.failState;
    return result.node;
  }

  tsTryParse(f) {
    const state = this.state.clone();
    const result = f();

    if (result !== undefined && result !== false) {
      return result;
    } else {
      this.state = state;
      return undefined;
    }
  }

  tsTryParseDeclare(nany) {
    if (this.isLineTerminator()) {
      return;
    }

    let starttype = this.state.type;
    let kind;

    if (this.isContextual("let")) {
      starttype = types$1._var;
      kind = "let";
    }

    return this.tsInAmbientContext(() => {
      switch (starttype) {
        case types$1._function:
          nany.declare = true;
          return this.parseFunctionStatement(nany, false, true);

        case types$1._class:
          nany.declare = true;
          return this.parseClass(nany, true, false);

        case types$1._const:
          if (this.match(types$1._const) && this.isLookaheadContextual("enum")) {
            this.expect(types$1._const);
            this.expectContextual("enum");
            return this.tsParseEnumDeclaration(nany, true);
          }

        case types$1._var:
          kind = kind || this.state.value;
          return this.parseVarStatement(nany, kind);

        case types$1.name:
          {
            const value = this.state.value;

            if (value === "global") {
              return this.tsParseAmbientExternalModuleDeclaration(nany);
            } else {
              return this.tsParseDeclaration(nany, value, true);
            }
          }
      }
    });
  }

  tsTryParseExportDeclaration() {
    return this.tsParseDeclaration(this.startNode(), this.state.value, true);
  }

  tsParseExpressionStatement(node, expr) {
    switch (expr.name) {
      case "declare":
        {
          const declaration = this.tsTryParseDeclare(node);

          if (declaration) {
            declaration.declare = true;
            return declaration;
          }

          break;
        }

      case "global":
        if (this.match(types$1.braceL)) {
          this.scope.enter(SCOPE_TS_MODULE);
          this.prodParam.enter(PARAM);
          const mod = node;
          mod.global = true;
          mod.id = expr;
          mod.body = this.tsParseModuleBlock();
          this.scope.exit();
          this.prodParam.exit();
          return this.finishNode(mod, "TSModuleDeclaration");
        }

        break;

      default:
        return this.tsParseDeclaration(node, expr.name, false);
    }
  }

  tsParseDeclaration(node, value, next) {
    switch (value) {
      case "abstract":
        if (this.tsCheckLineTerminator(next) && (this.match(types$1._class) || this.match(types$1.name))) {
          return this.tsParseAbstractDeclaration(node);
        }

        break;

      case "enum":
        if (next || this.match(types$1.name)) {
          if (next) this.next();
          return this.tsParseEnumDeclaration(node, false);
        }

        break;

      case "interface":
        if (this.tsCheckLineTerminator(next) && this.match(types$1.name)) {
          return this.tsParseInterfaceDeclaration(node);
        }

        break;

      case "module":
        if (this.tsCheckLineTerminator(next)) {
          if (this.match(types$1.string)) {
            return this.tsParseAmbientExternalModuleDeclaration(node);
          } else if (this.match(types$1.name)) {
            return this.tsParseModuleOrNamespaceDeclaration(node);
          }
        }

        break;

      case "namespace":
        if (this.tsCheckLineTerminator(next) && this.match(types$1.name)) {
          return this.tsParseModuleOrNamespaceDeclaration(node);
        }

        break;

      case "type":
        if (this.tsCheckLineTerminator(next) && this.match(types$1.name)) {
          return this.tsParseTypeAliasDeclaration(node);
        }

        break;
    }
  }

  tsCheckLineTerminator(next) {
    if (next) {
      if (this.hasFollowingLineBreak()) return false;
      this.next();
      return true;
    }

    return !this.isLineTerminator();
  }

  tsTryParseGenericAsyncArrowFunction(startPos, startLoc) {
    if (!this.isRelational("<")) {
      return undefined;
    }

    const oldMaybeInArrowParameters = this.state.maybeInArrowParameters;
    this.state.maybeInArrowParameters = true;
    const res = this.tsTryParseAndCatch(() => {
      const node = this.startNodeAt(startPos, startLoc);
      node.typeParameters = this.tsParseTypeParameters();
      super.parseFunctionParams(node);
      node.returnType = this.tsTryParseTypeOrTypePredicateAnnotation();
      this.expect(types$1.arrow);
      return node;
    });
    this.state.maybeInArrowParameters = oldMaybeInArrowParameters;

    if (!res) {
      return undefined;
    }

    return this.parseArrowExpression(res, null, true);
  }

  tsParseTypeArguments() {
    const node = this.startNode();
    node.params = this.tsInType(() => this.tsInNoContext(() => {
      this.expectRelational("<");
      return this.tsParseDelimitedList("TypeParametersOrArguments", this.tsParseType.bind(this));
    }));

    if (node.params.length === 0) {
      this.raise(node.start, TSErrors.EmptyTypeArguments);
    }

    this.expectRelational(">");
    return this.finishNode(node, "TSTypeParameterInstantiation");
  }

  tsIsDeclarationStart() {
    if (this.match(types$1.name)) {
      switch (this.state.value) {
        case "abstract":
        case "declare":
        case "enum":
        case "interface":
        case "module":
        case "namespace":
        case "type":
          return true;
      }
    }

    return false;
  }

  isExportDefaultSpecifier() {
    if (this.tsIsDeclarationStart()) return false;
    return super.isExportDefaultSpecifier();
  }

  parseAssignableListItem(allowModifiers, decorators) {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    let accessibility;
    let readonly = false;
    let override = false;

    if (allowModifiers !== undefined) {
      const modified = {};
      this.tsParseModifiers(modified, ["public", "private", "protected", "override", "readonly"]);
      accessibility = modified.accessibility;
      override = modified.override;
      readonly = modified.readonly;

      if (allowModifiers === false && (accessibility || readonly || override)) {
        this.raise(startPos, TSErrors.UnexpectedParameterModifier);
      }
    }

    const left = this.parseMaybeDefault();
    this.parseAssignableListItemTypes(left);
    const elt = this.parseMaybeDefault(left.start, left.loc.start, left);

    if (accessibility || readonly || override) {
      const pp = this.startNodeAt(startPos, startLoc);

      if (decorators.length) {
        pp.decorators = decorators;
      }

      if (accessibility) pp.accessibility = accessibility;
      if (readonly) pp.readonly = readonly;
      if (override) pp.override = override;

      if (elt.type !== "Identifier" && elt.type !== "AssignmentPattern") {
        this.raise(pp.start, TSErrors.UnsupportedParameterPropertyKind);
      }

      pp.parameter = elt;
      return this.finishNode(pp, "TSParameterProperty");
    }

    if (decorators.length) {
      left.decorators = decorators;
    }

    return elt;
  }

  parseFunctionBodyAndFinish(node, type, isMethod = false) {
    if (this.match(types$1.colon)) {
      node.returnType = this.tsParseTypeOrTypePredicateAnnotation(types$1.colon);
    }

    const bodilessType = type === "FunctionDeclaration" ? "TSDeclareFunction" : type === "ClassMethod" ? "TSDeclareMethod" : undefined;

    if (bodilessType && !this.match(types$1.braceL) && this.isLineTerminator()) {
      this.finishNode(node, bodilessType);
      return;
    }

    if (bodilessType === "TSDeclareFunction" && this.state.isAmbientContext) {
      this.raise(node.start, TSErrors.DeclareFunctionHasImplementation);

      if (node.declare) {
        super.parseFunctionBodyAndFinish(node, bodilessType, isMethod);
        return;
      }
    }

    super.parseFunctionBodyAndFinish(node, type, isMethod);
  }

  registerFunctionStatementId(node) {
    if (!node.body && node.id) {
      this.checkLVal(node.id, "function name", BIND_TS_AMBIENT);
    } else {
      super.registerFunctionStatementId(...arguments);
    }
  }

  tsCheckForInvalidTypeCasts(items) {
    items.forEach(node => {
      if ((node == null ? void 0 : node.type) === "TSTypeCastExpression") {
        this.raise(node.typeAnnotation.start, TSErrors.UnexpectedTypeAnnotation);
      }
    });
  }

  toReferencedList(exprList, isInParens) {
    this.tsCheckForInvalidTypeCasts(exprList);
    return exprList;
  }

  parseArrayLike(...args) {
    const node = super.parseArrayLike(...args);

    if (node.type === "ArrayExpression") {
      this.tsCheckForInvalidTypeCasts(node.elements);
    }

    return node;
  }

  parseSubscript(base, startPos, startLoc, noCalls, state) {
    if (!this.hasPrecedingLineBreak() && this.match(types$1.bang)) {
      this.state.exprAllowed = false;
      this.next();
      const nonNullExpression = this.startNodeAt(startPos, startLoc);
      nonNullExpression.expression = base;
      return this.finishNode(nonNullExpression, "TSNonNullExpression");
    }

    if (this.isRelational("<")) {
      const result = this.tsTryParseAndCatch(() => {
        if (!noCalls && this.atPossibleAsyncArrow(base)) {
          const asyncArrowFn = this.tsTryParseGenericAsyncArrowFunction(startPos, startLoc);

          if (asyncArrowFn) {
            return asyncArrowFn;
          }
        }

        const node = this.startNodeAt(startPos, startLoc);
        node.callee = base;
        const typeArguments = this.tsParseTypeArguments();

        if (typeArguments) {
          if (!noCalls && this.eat(types$1.parenL)) {
            node.arguments = this.parseCallExpressionArguments(types$1.parenR, false);
            this.tsCheckForInvalidTypeCasts(node.arguments);
            node.typeParameters = typeArguments;

            if (state.optionalChainMember) {
              node.optional = false;
            }

            return this.finishCallExpression(node, state.optionalChainMember);
          } else if (this.match(types$1.backQuote)) {
            const result = this.parseTaggedTemplateExpression(base, startPos, startLoc, state);
            result.typeParameters = typeArguments;
            return result;
          }
        }

        this.unexpected();
      });
      if (result) return result;
    }

    return super.parseSubscript(base, startPos, startLoc, noCalls, state);
  }

  parseNewArguments(node) {
    if (this.isRelational("<")) {
      const typeParameters = this.tsTryParseAndCatch(() => {
        const args = this.tsParseTypeArguments();
        if (!this.match(types$1.parenL)) this.unexpected();
        return args;
      });

      if (typeParameters) {
        node.typeParameters = typeParameters;
      }
    }

    super.parseNewArguments(node);
  }

  parseExprOp(left, leftStartPos, leftStartLoc, minPrec) {
    if (nonNull(types$1._in.binop) > minPrec && !this.hasPrecedingLineBreak() && this.isContextual("as")) {
      const node = this.startNodeAt(leftStartPos, leftStartLoc);
      node.expression = left;

      const _const = this.tsTryNextParseConstantContext();

      if (_const) {
        node.typeAnnotation = _const;
      } else {
        node.typeAnnotation = this.tsNextThenParseType();
      }

      this.finishNode(node, "TSAsExpression");
      this.reScan_lt_gt();
      return this.parseExprOp(node, leftStartPos, leftStartLoc, minPrec);
    }

    return super.parseExprOp(left, leftStartPos, leftStartLoc, minPrec);
  }

  checkReservedWord(word, startLoc, checkKeywords, isBinding) {}

  checkDuplicateExports() {}

  parseImport(node) {
    node.importKind = "value";

    if (this.match(types$1.name) || this.match(types$1.star) || this.match(types$1.braceL)) {
      let ahead = this.lookahead();

      if (this.isContextual("type") && ahead.type !== types$1.comma && !(ahead.type === types$1.name && ahead.value === "from") && ahead.type !== types$1.eq) {
        node.importKind = "type";
        this.next();
        ahead = this.lookahead();
      }

      if (this.match(types$1.name) && ahead.type === types$1.eq) {
        return this.tsParseImportEqualsDeclaration(node);
      }
    }

    const importNode = super.parseImport(node);

    if (importNode.importKind === "type" && importNode.specifiers.length > 1 && importNode.specifiers[0].type === "ImportDefaultSpecifier") {
      this.raise(importNode.start, TSErrors.TypeImportCannotSpecifyDefaultAndNamed);
    }

    return importNode;
  }

  parseExport(node) {
    if (this.match(types$1._import)) {
      this.next();

      if (this.isContextual("type") && this.lookaheadCharCode() !== 61) {
        node.importKind = "type";
        this.next();
      } else {
        node.importKind = "value";
      }

      return this.tsParseImportEqualsDeclaration(node, true);
    } else if (this.eat(types$1.eq)) {
      const assign = node;
      assign.expression = this.parseExpression();
      this.semicolon();
      return this.finishNode(assign, "TSExportAssignment");
    } else if (this.eatContextual("as")) {
      const decl = node;
      this.expectContextual("namespace");
      decl.id = this.parseIdentifier();
      this.semicolon();
      return this.finishNode(decl, "TSNamespaceExportDeclaration");
    } else {
      if (this.isContextual("type") && this.lookahead().type === types$1.braceL) {
        this.next();
        node.exportKind = "type";
      } else {
        node.exportKind = "value";
      }

      return super.parseExport(node);
    }
  }

  isAbstractClass() {
    return this.isContextual("abstract") && this.lookahead().type === types$1._class;
  }

  parseExportDefaultExpression() {
    if (this.isAbstractClass()) {
      const cls = this.startNode();
      this.next();
      cls.abstract = true;
      this.parseClass(cls, true, true);
      return cls;
    }

    if (this.state.value === "interface") {
      const result = this.tsParseDeclaration(this.startNode(), this.state.value, true);
      if (result) return result;
    }

    return super.parseExportDefaultExpression();
  }

  parseStatementContent(context, topLevel) {
    if (this.state.type === types$1._const) {
      const ahead = this.lookahead();

      if (ahead.type === types$1.name && ahead.value === "enum") {
        const node = this.startNode();
        this.expect(types$1._const);
        this.expectContextual("enum");
        return this.tsParseEnumDeclaration(node, true);
      }
    }

    return super.parseStatementContent(context, topLevel);
  }

  parseAccessModifier() {
    return this.tsParseModifier(["public", "protected", "private"]);
  }

  tsHasSomeModifiers(member, modifiers) {
    return modifiers.some(modifier => {
      if (tsIsAccessModifier(modifier)) {
        return member.accessibility === modifier;
      }

      return !!member[modifier];
    });
  }

  parseClassMember(classBody, member, state) {
    const invalidModifersForStaticBlocks = ["declare", "private", "public", "protected", "override", "abstract", "readonly"];
    this.tsParseModifiers(member, invalidModifersForStaticBlocks.concat(["static"]));

    const callParseClassMemberWithIsStatic = () => {
      const isStatic = !!member.static;

      if (isStatic && this.eat(types$1.braceL)) {
        if (this.tsHasSomeModifiers(member, invalidModifersForStaticBlocks)) {
          this.raise(this.state.pos, TSErrors.StaticBlockCannotHaveModifier);
        }

        this.parseClassStaticBlock(classBody, member);
      } else {
        this.parseClassMemberWithIsStatic(classBody, member, state, isStatic);
      }
    };

    if (member.declare) {
      this.tsInAmbientContext(callParseClassMemberWithIsStatic);
    } else {
      callParseClassMemberWithIsStatic();
    }
  }

  parseClassMemberWithIsStatic(classBody, member, state, isStatic) {
    const idx = this.tsTryParseIndexSignature(member);

    if (idx) {
      classBody.body.push(idx);

      if (member.abstract) {
        this.raise(member.start, TSErrors.IndexSignatureHasAbstract);
      }

      if (member.accessibility) {
        this.raise(member.start, TSErrors.IndexSignatureHasAccessibility, member.accessibility);
      }

      if (member.declare) {
        this.raise(member.start, TSErrors.IndexSignatureHasDeclare);
      }

      if (member.override) {
        this.raise(member.start, TSErrors.IndexSignatureHasOverride);
      }

      return;
    }

    if (!this.state.inAbstractClass && member.abstract) {
      this.raise(member.start, TSErrors.NonAbstractClassHasAbstractMethod);
    }

    if (member.override) {
      if (!state.hadSuperClass) {
        this.raise(member.start, TSErrors.OverrideNotInSubClass);
      }
    }

    super.parseClassMemberWithIsStatic(classBody, member, state, isStatic);
  }

  parsePostMemberNameModifiers(methodOrProp) {
    const optional = this.eat(types$1.question);
    if (optional) methodOrProp.optional = true;

    if (methodOrProp.readonly && this.match(types$1.parenL)) {
      this.raise(methodOrProp.start, TSErrors.ClassMethodHasReadonly);
    }

    if (methodOrProp.declare && this.match(types$1.parenL)) {
      this.raise(methodOrProp.start, TSErrors.ClassMethodHasDeclare);
    }
  }

  parseExpressionStatement(node, expr) {
    const decl = expr.type === "Identifier" ? this.tsParseExpressionStatement(node, expr) : undefined;
    return decl || super.parseExpressionStatement(node, expr);
  }

  shouldParseExportDeclaration() {
    if (this.tsIsDeclarationStart()) return true;
    return super.shouldParseExportDeclaration();
  }

  parseConditional(expr, startPos, startLoc, refExpressionErrors) {
    if (!this.state.maybeInArrowParameters || !this.match(types$1.question)) {
      return super.parseConditional(expr, startPos, startLoc, refExpressionErrors);
    }

    const result = this.tryParse(() => super.parseConditional(expr, startPos, startLoc));

    if (!result.node) {
      if (result.error) {
        super.setOptionalParametersError(refExpressionErrors, result.error);
      }

      return expr;
    }

    if (result.error) this.state = result.failState;
    return result.node;
  }

  parseParenItem(node, startPos, startLoc) {
    node = super.parseParenItem(node, startPos, startLoc);

    if (this.eat(types$1.question)) {
      node.optional = true;
      this.resetEndLocation(node);
    }

    if (this.match(types$1.colon)) {
      const typeCastNode = this.startNodeAt(startPos, startLoc);
      typeCastNode.expression = node;
      typeCastNode.typeAnnotation = this.tsParseTypeAnnotation();
      return this.finishNode(typeCastNode, "TSTypeCastExpression");
    }

    return node;
  }

  parseExportDeclaration(node) {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    const isDeclare = this.eatContextual("declare");

    if (isDeclare && (this.isContextual("declare") || !this.shouldParseExportDeclaration())) {
      throw this.raise(this.state.start, TSErrors.ExpectedAmbientAfterExportDeclare);
    }

    let declaration;

    if (this.match(types$1.name)) {
      declaration = this.tsTryParseExportDeclaration();
    }

    if (!declaration) {
      declaration = super.parseExportDeclaration(node);
    }

    if (declaration && (declaration.type === "TSInterfaceDeclaration" || declaration.type === "TSTypeAliasDeclaration" || isDeclare)) {
      node.exportKind = "type";
    }

    if (declaration && isDeclare) {
      this.resetStartLocation(declaration, startPos, startLoc);
      declaration.declare = true;
    }

    return declaration;
  }

  parseClassId(node, isStatement, optionalId) {
    if ((!isStatement || optionalId) && this.isContextual("implements")) {
      return;
    }

    super.parseClassId(node, isStatement, optionalId, node.declare ? BIND_TS_AMBIENT : BIND_CLASS);
    const typeParameters = this.tsTryParseTypeParameters();
    if (typeParameters) node.typeParameters = typeParameters;
  }

  parseClassPropertyAnnotation(node) {
    if (!node.optional && this.eat(types$1.bang)) {
      node.definite = true;
    }

    const type = this.tsTryParseTypeAnnotation();
    if (type) node.typeAnnotation = type;
  }

  parseClassProperty(node) {
    this.parseClassPropertyAnnotation(node);

    if (this.state.isAmbientContext && this.match(types$1.eq)) {
      this.raise(this.state.start, TSErrors.DeclareClassFieldHasInitializer);
    }

    return super.parseClassProperty(node);
  }

  parseClassPrivateProperty(node) {
    if (node.abstract) {
      this.raise(node.start, TSErrors.PrivateElementHasAbstract);
    }

    if (node.accessibility) {
      this.raise(node.start, TSErrors.PrivateElementHasAccessibility, node.accessibility);
    }

    this.parseClassPropertyAnnotation(node);
    return super.parseClassPrivateProperty(node);
  }

  pushClassMethod(classBody, method, isGenerator, isAsync, isConstructor, allowsDirectSuper) {
    const typeParameters = this.tsTryParseTypeParameters();

    if (typeParameters && isConstructor) {
      this.raise(typeParameters.start, TSErrors.ConstructorHasTypeParameters);
    }

    if (method.declare && (method.kind === "get" || method.kind === "set")) {
      this.raise(method.start, TSErrors.DeclareAccessor, method.kind);
    }

    if (typeParameters) method.typeParameters = typeParameters;
    super.pushClassMethod(classBody, method, isGenerator, isAsync, isConstructor, allowsDirectSuper);
  }

  pushClassPrivateMethod(classBody, method, isGenerator, isAsync) {
    const typeParameters = this.tsTryParseTypeParameters();
    if (typeParameters) method.typeParameters = typeParameters;
    super.pushClassPrivateMethod(classBody, method, isGenerator, isAsync);
  }

  parseClassSuper(node) {
    super.parseClassSuper(node);

    if (node.superClass && this.isRelational("<")) {
      node.superTypeParameters = this.tsParseTypeArguments();
    }

    if (this.eatContextual("implements")) {
      node.implements = this.tsParseHeritageClause("implements");
    }
  }

  parseObjPropValue(prop, ...args) {
    const typeParameters = this.tsTryParseTypeParameters();
    if (typeParameters) prop.typeParameters = typeParameters;
    super.parseObjPropValue(prop, ...args);
  }

  parseFunctionParams(node, allowModifiers) {
    const typeParameters = this.tsTryParseTypeParameters();
    if (typeParameters) node.typeParameters = typeParameters;
    super.parseFunctionParams(node, allowModifiers);
  }

  parseVarId(decl, kind) {
    super.parseVarId(decl, kind);

    if (decl.id.type === "Identifier" && this.eat(types$1.bang)) {
      decl.definite = true;
    }

    const type = this.tsTryParseTypeAnnotation();

    if (type) {
      decl.id.typeAnnotation = type;
      this.resetEndLocation(decl.id);
    }
  }

  parseAsyncArrowFromCallExpression(node, call) {
    if (this.match(types$1.colon)) {
      node.returnType = this.tsParseTypeAnnotation();
    }

    return super.parseAsyncArrowFromCallExpression(node, call);
  }

  parseMaybeAssign(...args) {
    var _jsx, _jsx2, _typeCast, _jsx3, _typeCast2, _jsx4, _typeCast3;

    let state;
    let jsx;
    let typeCast;

    if (this.hasPlugin("jsx") && (this.match(types$1.jsxTagStart) || this.isRelational("<"))) {
      state = this.state.clone();
      jsx = this.tryParse(() => super.parseMaybeAssign(...args), state);
      if (!jsx.error) return jsx.node;
      const {
        context
      } = this.state;

      if (context[context.length - 1] === types.j_oTag) {
        context.length -= 2;
      } else if (context[context.length - 1] === types.j_expr) {
        context.length -= 1;
      }
    }

    if (!((_jsx = jsx) != null && _jsx.error) && !this.isRelational("<")) {
      return super.parseMaybeAssign(...args);
    }

    let typeParameters;
    state = state || this.state.clone();
    const arrow = this.tryParse(abort => {
      var _expr$extra, _typeParameters;

      typeParameters = this.tsParseTypeParameters();
      const expr = super.parseMaybeAssign(...args);

      if (expr.type !== "ArrowFunctionExpression" || (_expr$extra = expr.extra) != null && _expr$extra.parenthesized) {
        abort();
      }

      if (((_typeParameters = typeParameters) == null ? void 0 : _typeParameters.params.length) !== 0) {
        this.resetStartLocationFromNode(expr, typeParameters);
      }

      expr.typeParameters = typeParameters;
      return expr;
    }, state);
    if (!arrow.error && !arrow.aborted) return arrow.node;

    if (!jsx) {
      assert(!this.hasPlugin("jsx"));
      typeCast = this.tryParse(() => super.parseMaybeAssign(...args), state);
      if (!typeCast.error) return typeCast.node;
    }

    if ((_jsx2 = jsx) != null && _jsx2.node) {
      this.state = jsx.failState;
      return jsx.node;
    }

    if (arrow.node) {
      this.state = arrow.failState;
      return arrow.node;
    }

    if ((_typeCast = typeCast) != null && _typeCast.node) {
      this.state = typeCast.failState;
      return typeCast.node;
    }

    if ((_jsx3 = jsx) != null && _jsx3.thrown) throw jsx.error;
    if (arrow.thrown) throw arrow.error;
    if ((_typeCast2 = typeCast) != null && _typeCast2.thrown) throw typeCast.error;
    throw ((_jsx4 = jsx) == null ? void 0 : _jsx4.error) || arrow.error || ((_typeCast3 = typeCast) == null ? void 0 : _typeCast3.error);
  }

  parseMaybeUnary(refExpressionErrors) {
    if (!this.hasPlugin("jsx") && this.isRelational("<")) {
      return this.tsParseTypeAssertion();
    } else {
      return super.parseMaybeUnary(refExpressionErrors);
    }
  }

  parseArrow(node) {
    if (this.match(types$1.colon)) {
      const result = this.tryParse(abort => {
        const returnType = this.tsParseTypeOrTypePredicateAnnotation(types$1.colon);
        if (this.canInsertSemicolon() || !this.match(types$1.arrow)) abort();
        return returnType;
      });
      if (result.aborted) return;

      if (!result.thrown) {
        if (result.error) this.state = result.failState;
        node.returnType = result.node;
      }
    }

    return super.parseArrow(node);
  }

  parseAssignableListItemTypes(param) {
    if (this.eat(types$1.question)) {
      if (param.type !== "Identifier" && !this.state.isAmbientContext && !this.state.inType) {
        this.raise(param.start, TSErrors.PatternIsOptional);
      }

      param.optional = true;
    }

    const type = this.tsTryParseTypeAnnotation();
    if (type) param.typeAnnotation = type;
    this.resetEndLocation(param);
    return param;
  }

  toAssignable(node, isLHS = false) {
    switch (node.type) {
      case "TSTypeCastExpression":
        return super.toAssignable(this.typeCastToParameter(node), isLHS);

      case "TSParameterProperty":
        return super.toAssignable(node, isLHS);

      case "ParenthesizedExpression":
        return this.toAssignableParenthesizedExpression(node, isLHS);

      case "TSAsExpression":
      case "TSNonNullExpression":
      case "TSTypeAssertion":
        node.expression = this.toAssignable(node.expression, isLHS);
        return node;

      default:
        return super.toAssignable(node, isLHS);
    }
  }

  toAssignableParenthesizedExpression(node, isLHS) {
    switch (node.expression.type) {
      case "TSAsExpression":
      case "TSNonNullExpression":
      case "TSTypeAssertion":
      case "ParenthesizedExpression":
        node.expression = this.toAssignable(node.expression, isLHS);
        return node;

      default:
        return super.toAssignable(node, isLHS);
    }
  }

  checkLVal(expr, contextDescription, ...args) {
    var _expr$extra2;

    switch (expr.type) {
      case "TSTypeCastExpression":
        return;

      case "TSParameterProperty":
        this.checkLVal(expr.parameter, "parameter property", ...args);
        return;

      case "TSAsExpression":
      case "TSTypeAssertion":
        if (!args[0] && contextDescription !== "parenthesized expression" && !((_expr$extra2 = expr.extra) != null && _expr$extra2.parenthesized)) {
          this.raise(expr.start, ErrorMessages.InvalidLhs, contextDescription);
          break;
        }

        this.checkLVal(expr.expression, "parenthesized expression", ...args);
        return;

      case "TSNonNullExpression":
        this.checkLVal(expr.expression, contextDescription, ...args);
        return;

      default:
        super.checkLVal(expr, contextDescription, ...args);
        return;
    }
  }

  parseBindingAtom() {
    switch (this.state.type) {
      case types$1._this:
        return this.parseIdentifier(true);

      default:
        return super.parseBindingAtom();
    }
  }

  parseMaybeDecoratorArguments(expr) {
    if (this.isRelational("<")) {
      const typeArguments = this.tsParseTypeArguments();

      if (this.match(types$1.parenL)) {
        const call = super.parseMaybeDecoratorArguments(expr);
        call.typeParameters = typeArguments;
        return call;
      }

      this.unexpected(this.state.start, types$1.parenL);
    }

    return super.parseMaybeDecoratorArguments(expr);
  }

  checkCommaAfterRest(close) {
    if (this.state.isAmbientContext && this.match(types$1.comma) && this.lookaheadCharCode() === close) {
      this.next();
    } else {
      super.checkCommaAfterRest(close);
    }
  }

  isClassMethod() {
    return this.isRelational("<") || super.isClassMethod();
  }

  isClassProperty() {
    return this.match(types$1.bang) || this.match(types$1.colon) || super.isClassProperty();
  }

  parseMaybeDefault(...args) {
    const node = super.parseMaybeDefault(...args);

    if (node.type === "AssignmentPattern" && node.typeAnnotation && node.right.start < node.typeAnnotation.start) {
      this.raise(node.typeAnnotation.start, TSErrors.TypeAnnotationAfterAssign);
    }

    return node;
  }

  getTokenFromCode(code) {
    if (this.state.inType && (code === 62 || code === 60)) {
      return this.finishOp(types$1.relational, 1);
    } else {
      return super.getTokenFromCode(code);
    }
  }

  reScan_lt_gt() {
    if (this.match(types$1.relational)) {
      const code = this.input.charCodeAt(this.state.start);

      if (code === 60 || code === 62) {
        this.state.pos -= 1;
        this.readToken_lt_gt(code);
      }
    }
  }

  toAssignableList(exprList) {
    for (let i = 0; i < exprList.length; i++) {
      const expr = exprList[i];
      if (!expr) continue;

      switch (expr.type) {
        case "TSTypeCastExpression":
          exprList[i] = this.typeCastToParameter(expr);
          break;

        case "TSAsExpression":
        case "TSTypeAssertion":
          if (!this.state.maybeInArrowParameters) {
            exprList[i] = this.typeCastToParameter(expr);
          } else {
            this.raise(expr.start, TSErrors.UnexpectedTypeCastInParameter);
          }

          break;
      }
    }

    return super.toAssignableList(...arguments);
  }

  typeCastToParameter(node) {
    node.expression.typeAnnotation = node.typeAnnotation;
    this.resetEndLocation(node.expression, node.typeAnnotation.end, node.typeAnnotation.loc.end);
    return node.expression;
  }

  shouldParseArrow() {
    return this.match(types$1.colon) || super.shouldParseArrow();
  }

  shouldParseAsyncArrow() {
    return this.match(types$1.colon) || super.shouldParseAsyncArrow();
  }

  canHaveLeadingDecorator() {
    return super.canHaveLeadingDecorator() || this.isAbstractClass();
  }

  jsxParseOpeningElementAfterName(node) {
    if (this.isRelational("<")) {
      const typeArguments = this.tsTryParseAndCatch(() => this.tsParseTypeArguments());
      if (typeArguments) node.typeParameters = typeArguments;
    }

    return super.jsxParseOpeningElementAfterName(node);
  }

  getGetterSetterExpectedParamCount(method) {
    const baseCount = super.getGetterSetterExpectedParamCount(method);
    const params = this.getObjectOrClassMethodParams(method);
    const firstParam = params[0];
    const hasContextParam = firstParam && this.isThisParam(firstParam);
    return hasContextParam ? baseCount + 1 : baseCount;
  }

  parseCatchClauseParam() {
    const param = super.parseCatchClauseParam();
    const type = this.tsTryParseTypeAnnotation();

    if (type) {
      param.typeAnnotation = type;
      this.resetEndLocation(param);
    }

    return param;
  }

  tsInAmbientContext(cb) {
    const oldIsAmbientContext = this.state.isAmbientContext;
    this.state.isAmbientContext = true;

    try {
      return cb();
    } finally {
      this.state.isAmbientContext = oldIsAmbientContext;
    }
  }

  parseClass(node, ...args) {
    const oldInAbstractClass = this.state.inAbstractClass;
    this.state.inAbstractClass = !!node.abstract;

    try {
      return super.parseClass(node, ...args);
    } finally {
      this.state.inAbstractClass = oldInAbstractClass;
    }
  }

  tsParseAbstractDeclaration(node) {
    if (this.match(types$1._class)) {
      node.abstract = true;
      return this.parseClass(node, true, false);
    } else if (this.isContextual("interface")) {
      if (!this.hasFollowingLineBreak()) {
        node.abstract = true;
        this.raise(node.start, TSErrors.NonClassMethodPropertyHasAbstractModifer);
        this.next();
        return this.tsParseInterfaceDeclaration(node);
      }
    } else {
      this.unexpected(null, types$1._class);
    }
  }

  parseMethod(...args) {
    const method = super.parseMethod(...args);

    if (method.abstract) {
      const hasBody = this.hasPlugin("estree") ? !!method.value.body : !!method.body;

      if (hasBody) {
        const {
          key
        } = method;
        this.raise(method.start, TSErrors.AbstractMethodHasImplementation, key.type === "Identifier" ? key.name : `[${this.input.slice(key.start, key.end)}]`);
      }
    }

    return method;
  }

  shouldParseAsAmbientContext() {
    return !!this.getPluginOption("typescript", "dts");
  }

  parse() {
    if (this.shouldParseAsAmbientContext()) {
      this.state.isAmbientContext = true;
    }

    return super.parse();
  }

  getExpression() {
    if (this.shouldParseAsAmbientContext()) {
      this.state.isAmbientContext = true;
    }

    return super.getExpression();
  }

});

types$1.placeholder = new TokenType("%%", {
  startsExpr: true
});
const PlaceHolderErrors = makeErrorTemplates({
  ClassNameIsRequired: "A class name is required."
}, ErrorCodes.SyntaxError);
var placeholders = (superClass => class extends superClass {
  parsePlaceholder(expectedNode) {
    if (this.match(types$1.placeholder)) {
      const node = this.startNode();
      this.next();
      this.assertNoSpace("Unexpected space in placeholder.");
      node.name = super.parseIdentifier(true);
      this.assertNoSpace("Unexpected space in placeholder.");
      this.expect(types$1.placeholder);
      return this.finishPlaceholder(node, expectedNode);
    }
  }

  finishPlaceholder(node, expectedNode) {
    const isFinished = !!(node.expectedNode && node.type === "Placeholder");
    node.expectedNode = expectedNode;
    return isFinished ? node : this.finishNode(node, "Placeholder");
  }

  getTokenFromCode(code) {
    if (code === 37 && this.input.charCodeAt(this.state.pos + 1) === 37) {
      return this.finishOp(types$1.placeholder, 2);
    }

    return super.getTokenFromCode(...arguments);
  }

  parseExprAtom() {
    return this.parsePlaceholder("Expression") || super.parseExprAtom(...arguments);
  }

  parseIdentifier() {
    return this.parsePlaceholder("Identifier") || super.parseIdentifier(...arguments);
  }

  checkReservedWord(word) {
    if (word !== undefined) super.checkReservedWord(...arguments);
  }

  parseBindingAtom() {
    return this.parsePlaceholder("Pattern") || super.parseBindingAtom(...arguments);
  }

  checkLVal(expr) {
    if (expr.type !== "Placeholder") super.checkLVal(...arguments);
  }

  toAssignable(node) {
    if (node && node.type === "Placeholder" && node.expectedNode === "Expression") {
      node.expectedNode = "Pattern";
      return node;
    }

    return super.toAssignable(...arguments);
  }

  isLet(context) {
    if (super.isLet(context)) {
      return true;
    }

    if (!this.isContextual("let")) {
      return false;
    }

    if (context) return false;
    const nextToken = this.lookahead();

    if (nextToken.type === types$1.placeholder) {
      return true;
    }

    return false;
  }

  verifyBreakContinue(node) {
    if (node.label && node.label.type === "Placeholder") return;
    super.verifyBreakContinue(...arguments);
  }

  parseExpressionStatement(node, expr) {
    if (expr.type !== "Placeholder" || expr.extra && expr.extra.parenthesized) {
      return super.parseExpressionStatement(...arguments);
    }

    if (this.match(types$1.colon)) {
      const stmt = node;
      stmt.label = this.finishPlaceholder(expr, "Identifier");
      this.next();
      stmt.body = this.parseStatement("label");
      return this.finishNode(stmt, "LabeledStatement");
    }

    this.semicolon();
    node.name = expr.name;
    return this.finishPlaceholder(node, "Statement");
  }

  parseBlock() {
    return this.parsePlaceholder("BlockStatement") || super.parseBlock(...arguments);
  }

  parseFunctionId() {
    return this.parsePlaceholder("Identifier") || super.parseFunctionId(...arguments);
  }

  parseClass(node, isStatement, optionalId) {
    const type = isStatement ? "ClassDeclaration" : "ClassExpression";
    this.next();
    this.takeDecorators(node);
    const oldStrict = this.state.strict;
    const placeholder = this.parsePlaceholder("Identifier");

    if (placeholder) {
      if (this.match(types$1._extends) || this.match(types$1.placeholder) || this.match(types$1.braceL)) {
        node.id = placeholder;
      } else if (optionalId || !isStatement) {
        node.id = null;
        node.body = this.finishPlaceholder(placeholder, "ClassBody");
        return this.finishNode(node, type);
      } else {
        this.unexpected(null, PlaceHolderErrors.ClassNameIsRequired);
      }
    } else {
      this.parseClassId(node, isStatement, optionalId);
    }

    this.parseClassSuper(node);
    node.body = this.parsePlaceholder("ClassBody") || this.parseClassBody(!!node.superClass, oldStrict);
    return this.finishNode(node, type);
  }

  parseExport(node) {
    const placeholder = this.parsePlaceholder("Identifier");
    if (!placeholder) return super.parseExport(...arguments);

    if (!this.isContextual("from") && !this.match(types$1.comma)) {
      node.specifiers = [];
      node.source = null;
      node.declaration = this.finishPlaceholder(placeholder, "Declaration");
      return this.finishNode(node, "ExportNamedDeclaration");
    }

    this.expectPlugin("exportDefaultFrom");
    const specifier = this.startNode();
    specifier.exported = placeholder;
    node.specifiers = [this.finishNode(specifier, "ExportDefaultSpecifier")];
    return super.parseExport(node);
  }

  isExportDefaultSpecifier() {
    if (this.match(types$1._default)) {
      const next = this.nextTokenStart();

      if (this.isUnparsedContextual(next, "from")) {
        if (this.input.startsWith(types$1.placeholder.label, this.nextTokenStartSince(next + 4))) {
          return true;
        }
      }
    }

    return super.isExportDefaultSpecifier();
  }

  maybeParseExportDefaultSpecifier(node) {
    if (node.specifiers && node.specifiers.length > 0) {
      return true;
    }

    return super.maybeParseExportDefaultSpecifier(...arguments);
  }

  checkExport(node) {
    const {
      specifiers
    } = node;

    if (specifiers != null && specifiers.length) {
      node.specifiers = specifiers.filter(node => node.exported.type === "Placeholder");
    }

    super.checkExport(node);
    node.specifiers = specifiers;
  }

  parseImport(node) {
    const placeholder = this.parsePlaceholder("Identifier");
    if (!placeholder) return super.parseImport(...arguments);
    node.specifiers = [];

    if (!this.isContextual("from") && !this.match(types$1.comma)) {
      node.source = this.finishPlaceholder(placeholder, "StringLiteral");
      this.semicolon();
      return this.finishNode(node, "ImportDeclaration");
    }

    const specifier = this.startNodeAtNode(placeholder);
    specifier.local = placeholder;
    this.finishNode(specifier, "ImportDefaultSpecifier");
    node.specifiers.push(specifier);

    if (this.eat(types$1.comma)) {
      const hasStarImport = this.maybeParseStarImportSpecifier(node);
      if (!hasStarImport) this.parseNamedImportSpecifiers(node);
    }

    this.expectContextual("from");
    node.source = this.parseImportSource();
    this.semicolon();
    return this.finishNode(node, "ImportDeclaration");
  }

  parseImportSource() {
    return this.parsePlaceholder("StringLiteral") || super.parseImportSource(...arguments);
  }

});

var v8intrinsic = (superClass => class extends superClass {
  parseV8Intrinsic() {
    if (this.match(types$1.modulo)) {
      const v8IntrinsicStart = this.state.start;
      const node = this.startNode();
      this.eat(types$1.modulo);

      if (this.match(types$1.name)) {
        const name = this.parseIdentifierName(this.state.start);
        const identifier = this.createIdentifier(node, name);
        identifier.type = "V8IntrinsicIdentifier";

        if (this.match(types$1.parenL)) {
          return identifier;
        }
      }

      this.unexpected(v8IntrinsicStart);
    }
  }

  parseExprAtom() {
    return this.parseV8Intrinsic() || super.parseExprAtom(...arguments);
  }

});

function hasPlugin(plugins, name) {
  return plugins.some(plugin => {
    if (Array.isArray(plugin)) {
      return plugin[0] === name;
    } else {
      return plugin === name;
    }
  });
}
function getPluginOption(plugins, name, option) {
  const plugin = plugins.find(plugin => {
    if (Array.isArray(plugin)) {
      return plugin[0] === name;
    } else {
      return plugin === name;
    }
  });

  if (plugin && Array.isArray(plugin)) {
    return plugin[1][option];
  }

  return null;
}
const PIPELINE_PROPOSALS = ["minimal", "smart", "fsharp"];
const RECORD_AND_TUPLE_SYNTAX_TYPES = ["hash", "bar"];
function validatePlugins(plugins) {
  if (hasPlugin(plugins, "decorators")) {
    if (hasPlugin(plugins, "decorators-legacy")) {
      throw new Error("Cannot use the decorators and decorators-legacy plugin together");
    }

    const decoratorsBeforeExport = getPluginOption(plugins, "decorators", "decoratorsBeforeExport");

    if (decoratorsBeforeExport == null) {
      throw new Error("The 'decorators' plugin requires a 'decoratorsBeforeExport' option," + " whose value must be a boolean. If you are migrating from" + " Babylon/Babel 6 or want to use the old decorators proposal, you" + " should use the 'decorators-legacy' plugin instead of 'decorators'.");
    } else if (typeof decoratorsBeforeExport !== "boolean") {
      throw new Error("'decoratorsBeforeExport' must be a boolean.");
    }
  }

  if (hasPlugin(plugins, "flow") && hasPlugin(plugins, "typescript")) {
    throw new Error("Cannot combine flow and typescript plugins.");
  }

  if (hasPlugin(plugins, "placeholders") && hasPlugin(plugins, "v8intrinsic")) {
    throw new Error("Cannot combine placeholders and v8intrinsic plugins.");
  }

  if (hasPlugin(plugins, "pipelineOperator") && !PIPELINE_PROPOSALS.includes(getPluginOption(plugins, "pipelineOperator", "proposal"))) {
    throw new Error("'pipelineOperator' requires 'proposal' option whose value should be one of: " + PIPELINE_PROPOSALS.map(p => `'${p}'`).join(", "));
  }

  if (hasPlugin(plugins, "moduleAttributes")) {
    {
      if (hasPlugin(plugins, "importAssertions")) {
        throw new Error("Cannot combine importAssertions and moduleAttributes plugins.");
      }

      const moduleAttributesVerionPluginOption = getPluginOption(plugins, "moduleAttributes", "version");

      if (moduleAttributesVerionPluginOption !== "may-2020") {
        throw new Error("The 'moduleAttributes' plugin requires a 'version' option," + " representing the last proposal update. Currently, the" + " only supported value is 'may-2020'.");
      }
    }
  }

  if (hasPlugin(plugins, "recordAndTuple") && !RECORD_AND_TUPLE_SYNTAX_TYPES.includes(getPluginOption(plugins, "recordAndTuple", "syntaxType"))) {
    throw new Error("'recordAndTuple' requires 'syntaxType' option whose value should be one of: " + RECORD_AND_TUPLE_SYNTAX_TYPES.map(p => `'${p}'`).join(", "));
  }

  if (hasPlugin(plugins, "asyncDoExpressions") && !hasPlugin(plugins, "doExpressions")) {
    const error = new Error("'asyncDoExpressions' requires 'doExpressions', please add 'doExpressions' to parser plugins.");
    error.missingPlugins = "doExpressions";
    throw error;
  }
}
const mixinPlugins = {
  estree,
  jsx,
  flow,
  typescript,
  v8intrinsic,
  placeholders
};
const mixinPluginNames = Object.keys(mixinPlugins);

const defaultOptions = {
  sourceType: "script",
  sourceFilename: undefined,
  startLine: 1,
  allowAwaitOutsideFunction: false,
  allowReturnOutsideFunction: false,
  allowImportExportEverywhere: false,
  allowSuperOutsideMethod: false,
  allowUndeclaredExports: false,
  plugins: [],
  strictMode: null,
  ranges: false,
  tokens: false,
  createParenthesizedExpressions: false,
  errorRecovery: false
};
function getOptions(opts) {
  const options = {};

  for (const key of Object.keys(defaultOptions)) {
    options[key] = opts && opts[key] != null ? opts[key] : defaultOptions[key];
  }

  return options;
}

var _isDigit = function isDigit(code) {
  return code >= 48 && code <= 57;
};
const VALID_REGEX_FLAGS = new Set([103, 109, 115, 105, 121, 117, 100]);
const forbiddenNumericSeparatorSiblings = {
  decBinOct: [46, 66, 69, 79, 95, 98, 101, 111],
  hex: [46, 88, 95, 120]
};
const allowedNumericSeparatorSiblings = {};
allowedNumericSeparatorSiblings.bin = [48, 49];
allowedNumericSeparatorSiblings.oct = [...allowedNumericSeparatorSiblings.bin, 50, 51, 52, 53, 54, 55];
allowedNumericSeparatorSiblings.dec = [...allowedNumericSeparatorSiblings.oct, 56, 57];
allowedNumericSeparatorSiblings.hex = [...allowedNumericSeparatorSiblings.dec, 65, 66, 67, 68, 69, 70, 97, 98, 99, 100, 101, 102];
class Token {
  constructor(state) {
    this.type = state.type;
    this.value = state.value;
    this.start = state.start;
    this.end = state.end;
    this.loc = new SourceLocation(state.startLoc, state.endLoc);
  }

}
class Tokenizer extends ParserError {
  constructor(options, input) {
    super();
    this.isLookahead = void 0;
    this.tokens = [];
    this.state = new State();
    this.state.init(options);
    this.input = input;
    this.length = input.length;
    this.isLookahead = false;
  }

  pushToken(token) {
    this.tokens.length = this.state.tokensLength;
    this.tokens.push(token);
    ++this.state.tokensLength;
  }

  next() {
    this.checkKeywordEscapes();

    if (this.options.tokens) {
      this.pushToken(new Token(this.state));
    }

    this.state.lastTokEnd = this.state.end;
    this.state.lastTokStart = this.state.start;
    this.state.lastTokEndLoc = this.state.endLoc;
    this.state.lastTokStartLoc = this.state.startLoc;
    this.nextToken();
  }

  eat(type) {
    if (this.match(type)) {
      this.next();
      return true;
    } else {
      return false;
    }
  }

  match(type) {
    return this.state.type === type;
  }

  createLookaheadState(state) {
    return {
      pos: state.pos,
      value: null,
      type: state.type,
      start: state.start,
      end: state.end,
      lastTokEnd: state.end,
      context: [this.curContext()],
      inType: state.inType
    };
  }

  lookahead() {
    const old = this.state;
    this.state = this.createLookaheadState(old);
    this.isLookahead = true;
    this.nextToken();
    this.isLookahead = false;
    const curr = this.state;
    this.state = old;
    return curr;
  }

  nextTokenStart() {
    return this.nextTokenStartSince(this.state.pos);
  }

  nextTokenStartSince(pos) {
    skipWhiteSpace.lastIndex = pos;
    const skip = skipWhiteSpace.exec(this.input);
    return pos + skip[0].length;
  }

  lookaheadCharCode() {
    return this.input.charCodeAt(this.nextTokenStart());
  }

  codePointAtPos(pos) {
    let cp = this.input.charCodeAt(pos);

    if ((cp & 0xfc00) === 0xd800 && ++pos < this.input.length) {
      const trail = this.input.charCodeAt(pos);

      if ((trail & 0xfc00) === 0xdc00) {
        cp = 0x10000 + ((cp & 0x3ff) << 10) + (trail & 0x3ff);
      }
    }

    return cp;
  }

  setStrict(strict) {
    this.state.strict = strict;

    if (strict) {
      this.state.strictErrors.forEach((message, pos) => this.raise(pos, message));
      this.state.strictErrors.clear();
    }
  }

  curContext() {
    return this.state.context[this.state.context.length - 1];
  }

  nextToken() {
    const curContext = this.curContext();
    if (!curContext.preserveSpace) this.skipSpace();
    this.state.start = this.state.pos;
    if (!this.isLookahead) this.state.startLoc = this.state.curPosition();

    if (this.state.pos >= this.length) {
      this.finishToken(types$1.eof);
      return;
    }

    if (curContext === types.template) {
      this.readTmplToken();
    } else {
      this.getTokenFromCode(this.codePointAtPos(this.state.pos));
    }
  }

  pushComment(block, text, start, end, startLoc, endLoc) {
    const comment = {
      type: block ? "CommentBlock" : "CommentLine",
      value: text,
      start: start,
      end: end,
      loc: new SourceLocation(startLoc, endLoc)
    };
    if (this.options.tokens) this.pushToken(comment);
    this.state.comments.push(comment);
    this.addComment(comment);
  }

  skipBlockComment() {
    let startLoc;
    if (!this.isLookahead) startLoc = this.state.curPosition();
    const start = this.state.pos;
    const end = this.input.indexOf("*/", this.state.pos + 2);
    if (end === -1) throw this.raise(start, ErrorMessages.UnterminatedComment);
    this.state.pos = end + 2;
    lineBreakG.lastIndex = start;
    let match;

    while ((match = lineBreakG.exec(this.input)) && match.index < this.state.pos) {
      ++this.state.curLine;
      this.state.lineStart = match.index + match[0].length;
    }

    if (this.isLookahead) return;
    this.pushComment(true, this.input.slice(start + 2, end), start, this.state.pos, startLoc, this.state.curPosition());
  }

  skipLineComment(startSkip) {
    const start = this.state.pos;
    let startLoc;
    if (!this.isLookahead) startLoc = this.state.curPosition();
    let ch = this.input.charCodeAt(this.state.pos += startSkip);

    if (this.state.pos < this.length) {
      while (!isNewLine(ch) && ++this.state.pos < this.length) {
        ch = this.input.charCodeAt(this.state.pos);
      }
    }

    if (this.isLookahead) return;
    this.pushComment(false, this.input.slice(start + startSkip, this.state.pos), start, this.state.pos, startLoc, this.state.curPosition());
  }

  skipSpace() {
    loop: while (this.state.pos < this.length) {
      const ch = this.input.charCodeAt(this.state.pos);

      switch (ch) {
        case 32:
        case 160:
        case 9:
          ++this.state.pos;
          break;

        case 13:
          if (this.input.charCodeAt(this.state.pos + 1) === 10) {
            ++this.state.pos;
          }

        case 10:
        case 8232:
        case 8233:
          ++this.state.pos;
          ++this.state.curLine;
          this.state.lineStart = this.state.pos;
          break;

        case 47:
          switch (this.input.charCodeAt(this.state.pos + 1)) {
            case 42:
              this.skipBlockComment();
              break;

            case 47:
              this.skipLineComment(2);
              break;

            default:
              break loop;
          }

          break;

        default:
          if (isWhitespace(ch)) {
            ++this.state.pos;
          } else {
            break loop;
          }

      }
    }
  }

  finishToken(type, val) {
    this.state.end = this.state.pos;
    const prevType = this.state.type;
    this.state.type = type;
    this.state.value = val;

    if (!this.isLookahead) {
      this.state.endLoc = this.state.curPosition();
      this.updateContext(prevType);
    }
  }

  readToken_numberSign() {
    if (this.state.pos === 0 && this.readToken_interpreter()) {
      return;
    }

    const nextPos = this.state.pos + 1;
    const next = this.codePointAtPos(nextPos);

    if (next >= 48 && next <= 57) {
      throw this.raise(this.state.pos, ErrorMessages.UnexpectedDigitAfterHash);
    }

    if (next === 123 || next === 91 && this.hasPlugin("recordAndTuple")) {
      this.expectPlugin("recordAndTuple");

      if (this.getPluginOption("recordAndTuple", "syntaxType") !== "hash") {
        throw this.raise(this.state.pos, next === 123 ? ErrorMessages.RecordExpressionHashIncorrectStartSyntaxType : ErrorMessages.TupleExpressionHashIncorrectStartSyntaxType);
      }

      this.state.pos += 2;

      if (next === 123) {
        this.finishToken(types$1.braceHashL);
      } else {
        this.finishToken(types$1.bracketHashL);
      }
    } else if (isIdentifierStart(next)) {
      ++this.state.pos;
      this.finishToken(types$1.privateName, this.readWord1(next));
    } else if (next === 92) {
      ++this.state.pos;
      this.finishToken(types$1.privateName, this.readWord1());
    } else {
      this.finishOp(types$1.hash, 1);
    }
  }

  readToken_dot() {
    const next = this.input.charCodeAt(this.state.pos + 1);

    if (next >= 48 && next <= 57) {
      this.readNumber(true);
      return;
    }

    if (next === 46 && this.input.charCodeAt(this.state.pos + 2) === 46) {
      this.state.pos += 3;
      this.finishToken(types$1.ellipsis);
    } else {
      ++this.state.pos;
      this.finishToken(types$1.dot);
    }
  }

  readToken_slash() {
    const next = this.input.charCodeAt(this.state.pos + 1);

    if (next === 61) {
      this.finishOp(types$1.slashAssign, 2);
    } else {
      this.finishOp(types$1.slash, 1);
    }
  }

  readToken_interpreter() {
    if (this.state.pos !== 0 || this.length < 2) return false;
    let ch = this.input.charCodeAt(this.state.pos + 1);
    if (ch !== 33) return false;
    const start = this.state.pos;
    this.state.pos += 1;

    while (!isNewLine(ch) && ++this.state.pos < this.length) {
      ch = this.input.charCodeAt(this.state.pos);
    }

    const value = this.input.slice(start + 2, this.state.pos);
    this.finishToken(types$1.interpreterDirective, value);
    return true;
  }

  readToken_mult_modulo(code) {
    let type = code === 42 ? types$1.star : types$1.modulo;
    let width = 1;
    let next = this.input.charCodeAt(this.state.pos + 1);

    if (code === 42 && next === 42) {
      width++;
      next = this.input.charCodeAt(this.state.pos + 2);
      type = types$1.exponent;
    }

    if (next === 61 && !this.state.inType) {
      width++;
      type = types$1.assign;
    }

    this.finishOp(type, width);
  }

  readToken_pipe_amp(code) {
    const next = this.input.charCodeAt(this.state.pos + 1);

    if (next === code) {
      if (this.input.charCodeAt(this.state.pos + 2) === 61) {
        this.finishOp(types$1.assign, 3);
      } else {
        this.finishOp(code === 124 ? types$1.logicalOR : types$1.logicalAND, 2);
      }

      return;
    }

    if (code === 124) {
      if (next === 62) {
        this.finishOp(types$1.pipeline, 2);
        return;
      }

      if (this.hasPlugin("recordAndTuple") && next === 125) {
        if (this.getPluginOption("recordAndTuple", "syntaxType") !== "bar") {
          throw this.raise(this.state.pos, ErrorMessages.RecordExpressionBarIncorrectEndSyntaxType);
        }

        this.state.pos += 2;
        this.finishToken(types$1.braceBarR);
        return;
      }

      if (this.hasPlugin("recordAndTuple") && next === 93) {
        if (this.getPluginOption("recordAndTuple", "syntaxType") !== "bar") {
          throw this.raise(this.state.pos, ErrorMessages.TupleExpressionBarIncorrectEndSyntaxType);
        }

        this.state.pos += 2;
        this.finishToken(types$1.bracketBarR);
        return;
      }
    }

    if (next === 61) {
      this.finishOp(types$1.assign, 2);
      return;
    }

    this.finishOp(code === 124 ? types$1.bitwiseOR : types$1.bitwiseAND, 1);
  }

  readToken_caret() {
    const next = this.input.charCodeAt(this.state.pos + 1);

    if (next === 61) {
      this.finishOp(types$1.assign, 2);
    } else {
      this.finishOp(types$1.bitwiseXOR, 1);
    }
  }

  readToken_plus_min(code) {
    const next = this.input.charCodeAt(this.state.pos + 1);

    if (next === code) {
      if (next === 45 && !this.inModule && this.input.charCodeAt(this.state.pos + 2) === 62 && (this.state.lastTokEnd === 0 || this.hasPrecedingLineBreak())) {
        this.skipLineComment(3);
        this.skipSpace();
        this.nextToken();
        return;
      }

      this.finishOp(types$1.incDec, 2);
      return;
    }

    if (next === 61) {
      this.finishOp(types$1.assign, 2);
    } else {
      this.finishOp(types$1.plusMin, 1);
    }
  }

  readToken_lt_gt(code) {
    const next = this.input.charCodeAt(this.state.pos + 1);
    let size = 1;

    if (next === code) {
      size = code === 62 && this.input.charCodeAt(this.state.pos + 2) === 62 ? 3 : 2;

      if (this.input.charCodeAt(this.state.pos + size) === 61) {
        this.finishOp(types$1.assign, size + 1);
        return;
      }

      this.finishOp(types$1.bitShift, size);
      return;
    }

    if (next === 33 && code === 60 && !this.inModule && this.input.charCodeAt(this.state.pos + 2) === 45 && this.input.charCodeAt(this.state.pos + 3) === 45) {
      this.skipLineComment(4);
      this.skipSpace();
      this.nextToken();
      return;
    }

    if (next === 61) {
      size = 2;
    }

    this.finishOp(types$1.relational, size);
  }

  readToken_eq_excl(code) {
    const next = this.input.charCodeAt(this.state.pos + 1);

    if (next === 61) {
      this.finishOp(types$1.equality, this.input.charCodeAt(this.state.pos + 2) === 61 ? 3 : 2);
      return;
    }

    if (code === 61 && next === 62) {
      this.state.pos += 2;
      this.finishToken(types$1.arrow);
      return;
    }

    this.finishOp(code === 61 ? types$1.eq : types$1.bang, 1);
  }

  readToken_question() {
    const next = this.input.charCodeAt(this.state.pos + 1);
    const next2 = this.input.charCodeAt(this.state.pos + 2);

    if (next === 63) {
      if (next2 === 61) {
        this.finishOp(types$1.assign, 3);
      } else {
        this.finishOp(types$1.nullishCoalescing, 2);
      }
    } else if (next === 46 && !(next2 >= 48 && next2 <= 57)) {
      this.state.pos += 2;
      this.finishToken(types$1.questionDot);
    } else {
      ++this.state.pos;
      this.finishToken(types$1.question);
    }
  }

  getTokenFromCode(code) {
    switch (code) {
      case 46:
        this.readToken_dot();
        return;

      case 40:
        ++this.state.pos;
        this.finishToken(types$1.parenL);
        return;

      case 41:
        ++this.state.pos;
        this.finishToken(types$1.parenR);
        return;

      case 59:
        ++this.state.pos;
        this.finishToken(types$1.semi);
        return;

      case 44:
        ++this.state.pos;
        this.finishToken(types$1.comma);
        return;

      case 91:
        if (this.hasPlugin("recordAndTuple") && this.input.charCodeAt(this.state.pos + 1) === 124) {
          if (this.getPluginOption("recordAndTuple", "syntaxType") !== "bar") {
            throw this.raise(this.state.pos, ErrorMessages.TupleExpressionBarIncorrectStartSyntaxType);
          }

          this.state.pos += 2;
          this.finishToken(types$1.bracketBarL);
        } else {
          ++this.state.pos;
          this.finishToken(types$1.bracketL);
        }

        return;

      case 93:
        ++this.state.pos;
        this.finishToken(types$1.bracketR);
        return;

      case 123:
        if (this.hasPlugin("recordAndTuple") && this.input.charCodeAt(this.state.pos + 1) === 124) {
          if (this.getPluginOption("recordAndTuple", "syntaxType") !== "bar") {
            throw this.raise(this.state.pos, ErrorMessages.RecordExpressionBarIncorrectStartSyntaxType);
          }

          this.state.pos += 2;
          this.finishToken(types$1.braceBarL);
        } else {
          ++this.state.pos;
          this.finishToken(types$1.braceL);
        }

        return;

      case 125:
        ++this.state.pos;
        this.finishToken(types$1.braceR);
        return;

      case 58:
        if (this.hasPlugin("functionBind") && this.input.charCodeAt(this.state.pos + 1) === 58) {
          this.finishOp(types$1.doubleColon, 2);
        } else {
          ++this.state.pos;
          this.finishToken(types$1.colon);
        }

        return;

      case 63:
        this.readToken_question();
        return;

      case 96:
        ++this.state.pos;
        this.finishToken(types$1.backQuote);
        return;

      case 48:
        {
          const next = this.input.charCodeAt(this.state.pos + 1);

          if (next === 120 || next === 88) {
            this.readRadixNumber(16);
            return;
          }

          if (next === 111 || next === 79) {
            this.readRadixNumber(8);
            return;
          }

          if (next === 98 || next === 66) {
            this.readRadixNumber(2);
            return;
          }
        }

      case 49:
      case 50:
      case 51:
      case 52:
      case 53:
      case 54:
      case 55:
      case 56:
      case 57:
        this.readNumber(false);
        return;

      case 34:
      case 39:
        this.readString(code);
        return;

      case 47:
        this.readToken_slash();
        return;

      case 37:
      case 42:
        this.readToken_mult_modulo(code);
        return;

      case 124:
      case 38:
        this.readToken_pipe_amp(code);
        return;

      case 94:
        this.readToken_caret();
        return;

      case 43:
      case 45:
        this.readToken_plus_min(code);
        return;

      case 60:
      case 62:
        this.readToken_lt_gt(code);
        return;

      case 61:
      case 33:
        this.readToken_eq_excl(code);
        return;

      case 126:
        this.finishOp(types$1.tilde, 1);
        return;

      case 64:
        ++this.state.pos;
        this.finishToken(types$1.at);
        return;

      case 35:
        this.readToken_numberSign();
        return;

      case 92:
        this.readWord();
        return;

      default:
        if (isIdentifierStart(code)) {
          this.readWord(code);
          return;
        }

    }

    throw this.raise(this.state.pos, ErrorMessages.InvalidOrUnexpectedToken, String.fromCodePoint(code));
  }

  finishOp(type, size) {
    const str = this.input.slice(this.state.pos, this.state.pos + size);
    this.state.pos += size;
    this.finishToken(type, str);
  }

  readRegexp() {
    const start = this.state.start + 1;
    let escaped, inClass;
    let {
      pos
    } = this.state;

    for (;; ++pos) {
      if (pos >= this.length) {
        throw this.raise(start, ErrorMessages.UnterminatedRegExp);
      }

      const ch = this.input.charCodeAt(pos);

      if (isNewLine(ch)) {
        throw this.raise(start, ErrorMessages.UnterminatedRegExp);
      }

      if (escaped) {
        escaped = false;
      } else {
        if (ch === 91) {
          inClass = true;
        } else if (ch === 93 && inClass) {
          inClass = false;
        } else if (ch === 47 && !inClass) {
          break;
        }

        escaped = ch === 92;
      }
    }

    const content = this.input.slice(start, pos);
    ++pos;
    let mods = "";

    while (pos < this.length) {
      const cp = this.codePointAtPos(pos);
      const char = String.fromCharCode(cp);

      if (VALID_REGEX_FLAGS.has(cp)) {
        if (mods.includes(char)) {
          this.raise(pos + 1, ErrorMessages.DuplicateRegExpFlags);
        }
      } else if (isIdentifierChar(cp) || cp === 92) {
        this.raise(pos + 1, ErrorMessages.MalformedRegExpFlags);
      } else {
        break;
      }

      ++pos;
      mods += char;
    }

    this.state.pos = pos;
    this.finishToken(types$1.regexp, {
      pattern: content,
      flags: mods
    });
  }

  readInt(radix, len, forceLen, allowNumSeparator = true) {
    const start = this.state.pos;
    const forbiddenSiblings = radix === 16 ? forbiddenNumericSeparatorSiblings.hex : forbiddenNumericSeparatorSiblings.decBinOct;
    const allowedSiblings = radix === 16 ? allowedNumericSeparatorSiblings.hex : radix === 10 ? allowedNumericSeparatorSiblings.dec : radix === 8 ? allowedNumericSeparatorSiblings.oct : allowedNumericSeparatorSiblings.bin;
    let invalid = false;
    let total = 0;

    for (let i = 0, e = len == null ? Infinity : len; i < e; ++i) {
      const code = this.input.charCodeAt(this.state.pos);
      let val;

      if (code === 95) {
        const prev = this.input.charCodeAt(this.state.pos - 1);
        const next = this.input.charCodeAt(this.state.pos + 1);

        if (allowedSiblings.indexOf(next) === -1) {
          this.raise(this.state.pos, ErrorMessages.UnexpectedNumericSeparator);
        } else if (forbiddenSiblings.indexOf(prev) > -1 || forbiddenSiblings.indexOf(next) > -1 || Number.isNaN(next)) {
          this.raise(this.state.pos, ErrorMessages.UnexpectedNumericSeparator);
        }

        if (!allowNumSeparator) {
          this.raise(this.state.pos, ErrorMessages.NumericSeparatorInEscapeSequence);
        }

        ++this.state.pos;
        continue;
      }

      if (code >= 97) {
        val = code - 97 + 10;
      } else if (code >= 65) {
        val = code - 65 + 10;
      } else if (_isDigit(code)) {
        val = code - 48;
      } else {
        val = Infinity;
      }

      if (val >= radix) {
        if (this.options.errorRecovery && val <= 9) {
          val = 0;
          this.raise(this.state.start + i + 2, ErrorMessages.InvalidDigit, radix);
        } else if (forceLen) {
          val = 0;
          invalid = true;
        } else {
          break;
        }
      }

      ++this.state.pos;
      total = total * radix + val;
    }

    if (this.state.pos === start || len != null && this.state.pos - start !== len || invalid) {
      return null;
    }

    return total;
  }

  readRadixNumber(radix) {
    const start = this.state.pos;
    let isBigInt = false;
    this.state.pos += 2;
    const val = this.readInt(radix);

    if (val == null) {
      this.raise(this.state.start + 2, ErrorMessages.InvalidDigit, radix);
    }

    const next = this.input.charCodeAt(this.state.pos);

    if (next === 110) {
      ++this.state.pos;
      isBigInt = true;
    } else if (next === 109) {
      throw this.raise(start, ErrorMessages.InvalidDecimal);
    }

    if (isIdentifierStart(this.codePointAtPos(this.state.pos))) {
      throw this.raise(this.state.pos, ErrorMessages.NumberIdentifier);
    }

    if (isBigInt) {
      const str = this.input.slice(start, this.state.pos).replace(/[_n]/g, "");
      this.finishToken(types$1.bigint, str);
      return;
    }

    this.finishToken(types$1.num, val);
  }

  readNumber(startsWithDot) {
    const start = this.state.pos;
    let isFloat = false;
    let isBigInt = false;
    let isDecimal = false;
    let hasExponent = false;
    let isOctal = false;

    if (!startsWithDot && this.readInt(10) === null) {
      this.raise(start, ErrorMessages.InvalidNumber);
    }

    const hasLeadingZero = this.state.pos - start >= 2 && this.input.charCodeAt(start) === 48;

    if (hasLeadingZero) {
      const integer = this.input.slice(start, this.state.pos);
      this.recordStrictModeErrors(start, ErrorMessages.StrictOctalLiteral);

      if (!this.state.strict) {
        const underscorePos = integer.indexOf("_");

        if (underscorePos > 0) {
          this.raise(underscorePos + start, ErrorMessages.ZeroDigitNumericSeparator);
        }
      }

      isOctal = hasLeadingZero && !/[89]/.test(integer);
    }

    let next = this.input.charCodeAt(this.state.pos);

    if (next === 46 && !isOctal) {
      ++this.state.pos;
      this.readInt(10);
      isFloat = true;
      next = this.input.charCodeAt(this.state.pos);
    }

    if ((next === 69 || next === 101) && !isOctal) {
      next = this.input.charCodeAt(++this.state.pos);

      if (next === 43 || next === 45) {
        ++this.state.pos;
      }

      if (this.readInt(10) === null) {
        this.raise(start, ErrorMessages.InvalidOrMissingExponent);
      }

      isFloat = true;
      hasExponent = true;
      next = this.input.charCodeAt(this.state.pos);
    }

    if (next === 110) {
      if (isFloat || hasLeadingZero) {
        this.raise(start, ErrorMessages.InvalidBigIntLiteral);
      }

      ++this.state.pos;
      isBigInt = true;
    }

    if (next === 109) {
      this.expectPlugin("decimal", this.state.pos);

      if (hasExponent || hasLeadingZero) {
        this.raise(start, ErrorMessages.InvalidDecimal);
      }

      ++this.state.pos;
      isDecimal = true;
    }

    if (isIdentifierStart(this.codePointAtPos(this.state.pos))) {
      throw this.raise(this.state.pos, ErrorMessages.NumberIdentifier);
    }

    const str = this.input.slice(start, this.state.pos).replace(/[_mn]/g, "");

    if (isBigInt) {
      this.finishToken(types$1.bigint, str);
      return;
    }

    if (isDecimal) {
      this.finishToken(types$1.decimal, str);
      return;
    }

    const val = isOctal ? parseInt(str, 8) : parseFloat(str);
    this.finishToken(types$1.num, val);
  }

  readCodePoint(throwOnInvalid) {
    const ch = this.input.charCodeAt(this.state.pos);
    let code;

    if (ch === 123) {
      const codePos = ++this.state.pos;
      code = this.readHexChar(this.input.indexOf("}", this.state.pos) - this.state.pos, true, throwOnInvalid);
      ++this.state.pos;

      if (code !== null && code > 0x10ffff) {
        if (throwOnInvalid) {
          this.raise(codePos, ErrorMessages.InvalidCodePoint);
        } else {
          return null;
        }
      }
    } else {
      code = this.readHexChar(4, false, throwOnInvalid);
    }

    return code;
  }

  readString(quote) {
    let out = "",
        chunkStart = ++this.state.pos;

    for (;;) {
      if (this.state.pos >= this.length) {
        throw this.raise(this.state.start, ErrorMessages.UnterminatedString);
      }

      const ch = this.input.charCodeAt(this.state.pos);
      if (ch === quote) break;

      if (ch === 92) {
        out += this.input.slice(chunkStart, this.state.pos);
        out += this.readEscapedChar(false);
        chunkStart = this.state.pos;
      } else if (ch === 8232 || ch === 8233) {
        ++this.state.pos;
        ++this.state.curLine;
        this.state.lineStart = this.state.pos;
      } else if (isNewLine(ch)) {
        throw this.raise(this.state.start, ErrorMessages.UnterminatedString);
      } else {
        ++this.state.pos;
      }
    }

    out += this.input.slice(chunkStart, this.state.pos++);
    this.finishToken(types$1.string, out);
  }

  readTmplToken() {
    let out = "",
        chunkStart = this.state.pos,
        containsInvalid = false;

    for (;;) {
      if (this.state.pos >= this.length) {
        throw this.raise(this.state.start, ErrorMessages.UnterminatedTemplate);
      }

      const ch = this.input.charCodeAt(this.state.pos);

      if (ch === 96 || ch === 36 && this.input.charCodeAt(this.state.pos + 1) === 123) {
        if (this.state.pos === this.state.start && this.match(types$1.template)) {
          if (ch === 36) {
            this.state.pos += 2;
            this.finishToken(types$1.dollarBraceL);
            return;
          } else {
            ++this.state.pos;
            this.finishToken(types$1.backQuote);
            return;
          }
        }

        out += this.input.slice(chunkStart, this.state.pos);
        this.finishToken(types$1.template, containsInvalid ? null : out);
        return;
      }

      if (ch === 92) {
        out += this.input.slice(chunkStart, this.state.pos);
        const escaped = this.readEscapedChar(true);

        if (escaped === null) {
          containsInvalid = true;
        } else {
          out += escaped;
        }

        chunkStart = this.state.pos;
      } else if (isNewLine(ch)) {
        out += this.input.slice(chunkStart, this.state.pos);
        ++this.state.pos;

        switch (ch) {
          case 13:
            if (this.input.charCodeAt(this.state.pos) === 10) {
              ++this.state.pos;
            }

          case 10:
            out += "\n";
            break;

          default:
            out += String.fromCharCode(ch);
            break;
        }

        ++this.state.curLine;
        this.state.lineStart = this.state.pos;
        chunkStart = this.state.pos;
      } else {
        ++this.state.pos;
      }
    }
  }

  recordStrictModeErrors(pos, message) {
    if (this.state.strict && !this.state.strictErrors.has(pos)) {
      this.raise(pos, message);
    } else {
      this.state.strictErrors.set(pos, message);
    }
  }

  readEscapedChar(inTemplate) {
    const throwOnInvalid = !inTemplate;
    const ch = this.input.charCodeAt(++this.state.pos);
    ++this.state.pos;

    switch (ch) {
      case 110:
        return "\n";

      case 114:
        return "\r";

      case 120:
        {
          const code = this.readHexChar(2, false, throwOnInvalid);
          return code === null ? null : String.fromCharCode(code);
        }

      case 117:
        {
          const code = this.readCodePoint(throwOnInvalid);
          return code === null ? null : String.fromCodePoint(code);
        }

      case 116:
        return "\t";

      case 98:
        return "\b";

      case 118:
        return "\u000b";

      case 102:
        return "\f";

      case 13:
        if (this.input.charCodeAt(this.state.pos) === 10) {
          ++this.state.pos;
        }

      case 10:
        this.state.lineStart = this.state.pos;
        ++this.state.curLine;

      case 8232:
      case 8233:
        return "";

      case 56:
      case 57:
        if (inTemplate) {
          return null;
        } else {
          this.recordStrictModeErrors(this.state.pos - 1, ErrorMessages.StrictNumericEscape);
        }

      default:
        if (ch >= 48 && ch <= 55) {
          const codePos = this.state.pos - 1;
          const match = this.input.substr(this.state.pos - 1, 3).match(/^[0-7]+/);
          let octalStr = match[0];
          let octal = parseInt(octalStr, 8);

          if (octal > 255) {
            octalStr = octalStr.slice(0, -1);
            octal = parseInt(octalStr, 8);
          }

          this.state.pos += octalStr.length - 1;
          const next = this.input.charCodeAt(this.state.pos);

          if (octalStr !== "0" || next === 56 || next === 57) {
            if (inTemplate) {
              return null;
            } else {
              this.recordStrictModeErrors(codePos, ErrorMessages.StrictNumericEscape);
            }
          }

          return String.fromCharCode(octal);
        }

        return String.fromCharCode(ch);
    }
  }

  readHexChar(len, forceLen, throwOnInvalid) {
    const codePos = this.state.pos;
    const n = this.readInt(16, len, forceLen, false);

    if (n === null) {
      if (throwOnInvalid) {
        this.raise(codePos, ErrorMessages.InvalidEscapeSequence);
      } else {
        this.state.pos = codePos - 1;
      }
    }

    return n;
  }

  readWord1(firstCode) {
    this.state.containsEsc = false;
    let word = "";
    const start = this.state.pos;
    let chunkStart = this.state.pos;

    if (firstCode !== undefined) {
      this.state.pos += firstCode <= 0xffff ? 1 : 2;
    }

    while (this.state.pos < this.length) {
      const ch = this.codePointAtPos(this.state.pos);

      if (isIdentifierChar(ch)) {
        this.state.pos += ch <= 0xffff ? 1 : 2;
      } else if (ch === 92) {
        this.state.containsEsc = true;
        word += this.input.slice(chunkStart, this.state.pos);
        const escStart = this.state.pos;
        const identifierCheck = this.state.pos === start ? isIdentifierStart : isIdentifierChar;

        if (this.input.charCodeAt(++this.state.pos) !== 117) {
          this.raise(this.state.pos, ErrorMessages.MissingUnicodeEscape);
          chunkStart = this.state.pos - 1;
          continue;
        }

        ++this.state.pos;
        const esc = this.readCodePoint(true);

        if (esc !== null) {
          if (!identifierCheck(esc)) {
            this.raise(escStart, ErrorMessages.EscapedCharNotAnIdentifier);
          }

          word += String.fromCodePoint(esc);
        }

        chunkStart = this.state.pos;
      } else {
        break;
      }
    }

    return word + this.input.slice(chunkStart, this.state.pos);
  }

  readWord(firstCode) {
    const word = this.readWord1(firstCode);
    const type = keywords$1.get(word) || types$1.name;
    this.finishToken(type, word);
  }

  checkKeywordEscapes() {
    const kw = this.state.type.keyword;

    if (kw && this.state.containsEsc) {
      this.raise(this.state.start, ErrorMessages.InvalidEscapedReservedWord, kw);
    }
  }

  updateContext(prevType) {
    var _this$state$type$upda, _this$state$type;

    (_this$state$type$upda = (_this$state$type = this.state.type).updateContext) == null ? void 0 : _this$state$type$upda.call(_this$state$type, this.state.context);
  }

}

class ClassScope {
  constructor() {
    this.privateNames = new Set();
    this.loneAccessors = new Map();
    this.undefinedPrivateNames = new Map();
  }

}
class ClassScopeHandler {
  constructor(raise) {
    this.stack = [];
    this.undefinedPrivateNames = new Map();
    this.raise = raise;
  }

  current() {
    return this.stack[this.stack.length - 1];
  }

  enter() {
    this.stack.push(new ClassScope());
  }

  exit() {
    const oldClassScope = this.stack.pop();
    const current = this.current();

    for (const [name, pos] of Array.from(oldClassScope.undefinedPrivateNames)) {
      if (current) {
        if (!current.undefinedPrivateNames.has(name)) {
          current.undefinedPrivateNames.set(name, pos);
        }
      } else {
        this.raise(pos, ErrorMessages.InvalidPrivateFieldResolution, name);
      }
    }
  }

  declarePrivateName(name, elementType, pos) {
    const classScope = this.current();
    let redefined = classScope.privateNames.has(name);

    if (elementType & CLASS_ELEMENT_KIND_ACCESSOR) {
      const accessor = redefined && classScope.loneAccessors.get(name);

      if (accessor) {
        const oldStatic = accessor & CLASS_ELEMENT_FLAG_STATIC;
        const newStatic = elementType & CLASS_ELEMENT_FLAG_STATIC;
        const oldKind = accessor & CLASS_ELEMENT_KIND_ACCESSOR;
        const newKind = elementType & CLASS_ELEMENT_KIND_ACCESSOR;
        redefined = oldKind === newKind || oldStatic !== newStatic;
        if (!redefined) classScope.loneAccessors.delete(name);
      } else if (!redefined) {
        classScope.loneAccessors.set(name, elementType);
      }
    }

    if (redefined) {
      this.raise(pos, ErrorMessages.PrivateNameRedeclaration, name);
    }

    classScope.privateNames.add(name);
    classScope.undefinedPrivateNames.delete(name);
  }

  usePrivateName(name, pos) {
    let classScope;

    for (classScope of this.stack) {
      if (classScope.privateNames.has(name)) return;
    }

    if (classScope) {
      classScope.undefinedPrivateNames.set(name, pos);
    } else {
      this.raise(pos, ErrorMessages.InvalidPrivateFieldResolution, name);
    }
  }

}

const kExpression = 0,
      kMaybeArrowParameterDeclaration = 1,
      kMaybeAsyncArrowParameterDeclaration = 2,
      kParameterDeclaration = 3;

class ExpressionScope {
  constructor(type = kExpression) {
    this.type = void 0;
    this.type = type;
  }

  canBeArrowParameterDeclaration() {
    return this.type === kMaybeAsyncArrowParameterDeclaration || this.type === kMaybeArrowParameterDeclaration;
  }

  isCertainlyParameterDeclaration() {
    return this.type === kParameterDeclaration;
  }

}

class ArrowHeadParsingScope extends ExpressionScope {
  constructor(type) {
    super(type);
    this.errors = new Map();
  }

  recordDeclarationError(pos, template) {
    this.errors.set(pos, template);
  }

  clearDeclarationError(pos) {
    this.errors.delete(pos);
  }

  iterateErrors(iterator) {
    this.errors.forEach(iterator);
  }

}

class ExpressionScopeHandler {
  constructor(raise) {
    this.stack = [new ExpressionScope()];
    this.raise = raise;
  }

  enter(scope) {
    this.stack.push(scope);
  }

  exit() {
    this.stack.pop();
  }

  recordParameterInitializerError(pos, template) {
    const {
      stack
    } = this;
    let i = stack.length - 1;
    let scope = stack[i];

    while (!scope.isCertainlyParameterDeclaration()) {
      if (scope.canBeArrowParameterDeclaration()) {
        scope.recordDeclarationError(pos, template);
      } else {
        return;
      }

      scope = stack[--i];
    }

    this.raise(pos, template);
  }

  recordParenthesizedIdentifierError(pos, template) {
    const {
      stack
    } = this;
    const scope = stack[stack.length - 1];

    if (scope.isCertainlyParameterDeclaration()) {
      this.raise(pos, template);
    } else if (scope.canBeArrowParameterDeclaration()) {
      scope.recordDeclarationError(pos, template);
    } else {
      return;
    }
  }

  recordAsyncArrowParametersError(pos, template) {
    const {
      stack
    } = this;
    let i = stack.length - 1;
    let scope = stack[i];

    while (scope.canBeArrowParameterDeclaration()) {
      if (scope.type === kMaybeAsyncArrowParameterDeclaration) {
        scope.recordDeclarationError(pos, template);
      }

      scope = stack[--i];
    }
  }

  validateAsPattern() {
    const {
      stack
    } = this;
    const currentScope = stack[stack.length - 1];
    if (!currentScope.canBeArrowParameterDeclaration()) return;
    currentScope.iterateErrors((template, pos) => {
      this.raise(pos, template);
      let i = stack.length - 2;
      let scope = stack[i];

      while (scope.canBeArrowParameterDeclaration()) {
        scope.clearDeclarationError(pos);
        scope = stack[--i];
      }
    });
  }

}
function newParameterDeclarationScope() {
  return new ExpressionScope(kParameterDeclaration);
}
function newArrowHeadScope() {
  return new ArrowHeadParsingScope(kMaybeArrowParameterDeclaration);
}
function newAsyncArrowScope() {
  return new ArrowHeadParsingScope(kMaybeAsyncArrowParameterDeclaration);
}
function newExpressionScope() {
  return new ExpressionScope();
}

class UtilParser extends Tokenizer {
  addExtra(node, key, val) {
    if (!node) return;
    const extra = node.extra = node.extra || {};
    extra[key] = val;
  }

  isRelational(op) {
    return this.match(types$1.relational) && this.state.value === op;
  }

  expectRelational(op) {
    if (this.isRelational(op)) {
      this.next();
    } else {
      this.unexpected(null, types$1.relational);
    }
  }

  isContextual(name) {
    return this.match(types$1.name) && this.state.value === name && !this.state.containsEsc;
  }

  isUnparsedContextual(nameStart, name) {
    const nameEnd = nameStart + name.length;

    if (this.input.slice(nameStart, nameEnd) === name) {
      const nextCh = this.input.charCodeAt(nameEnd);
      return !(isIdentifierChar(nextCh) || (nextCh & 0xfc00) === 0xd800);
    }

    return false;
  }

  isLookaheadContextual(name) {
    const next = this.nextTokenStart();
    return this.isUnparsedContextual(next, name);
  }

  eatContextual(name) {
    return this.isContextual(name) && this.eat(types$1.name);
  }

  expectContextual(name, template) {
    if (!this.eatContextual(name)) this.unexpected(null, template);
  }

  canInsertSemicolon() {
    return this.match(types$1.eof) || this.match(types$1.braceR) || this.hasPrecedingLineBreak();
  }

  hasPrecedingLineBreak() {
    return lineBreak.test(this.input.slice(this.state.lastTokEnd, this.state.start));
  }

  hasFollowingLineBreak() {
    return lineBreak.test(this.input.slice(this.state.end, this.nextTokenStart()));
  }

  isLineTerminator() {
    return this.eat(types$1.semi) || this.canInsertSemicolon();
  }

  semicolon(allowAsi = true) {
    if (allowAsi ? this.isLineTerminator() : this.eat(types$1.semi)) return;
    this.raise(this.state.lastTokEnd, ErrorMessages.MissingSemicolon);
  }

  expect(type, pos) {
    this.eat(type) || this.unexpected(pos, type);
  }

  assertNoSpace(message = "Unexpected space.") {
    if (this.state.start > this.state.lastTokEnd) {
      this.raise(this.state.lastTokEnd, {
        code: ErrorCodes.SyntaxError,
        reasonCode: "UnexpectedSpace",
        template: message
      });
    }
  }

  unexpected(pos, messageOrType = {
    code: ErrorCodes.SyntaxError,
    reasonCode: "UnexpectedToken",
    template: "Unexpected token"
  }) {
    if (messageOrType instanceof TokenType) {
      messageOrType = {
        code: ErrorCodes.SyntaxError,
        reasonCode: "UnexpectedToken",
        template: `Unexpected token, expected "${messageOrType.label}"`
      };
    }

    throw this.raise(pos != null ? pos : this.state.start, messageOrType);
  }

  expectPlugin(name, pos) {
    if (!this.hasPlugin(name)) {
      throw this.raiseWithData(pos != null ? pos : this.state.start, {
        missingPlugin: [name]
      }, `This experimental syntax requires enabling the parser plugin: '${name}'`);
    }

    return true;
  }

  expectOnePlugin(names, pos) {
    if (!names.some(n => this.hasPlugin(n))) {
      throw this.raiseWithData(pos != null ? pos : this.state.start, {
        missingPlugin: names
      }, `This experimental syntax requires enabling one of the following parser plugin(s): '${names.join(", ")}'`);
    }
  }

  tryParse(fn, oldState = this.state.clone()) {
    const abortSignal = {
      node: null
    };

    try {
      const node = fn((node = null) => {
        abortSignal.node = node;
        throw abortSignal;
      });

      if (this.state.errors.length > oldState.errors.length) {
        const failState = this.state;
        this.state = oldState;
        this.state.tokensLength = failState.tokensLength;
        return {
          node,
          error: failState.errors[oldState.errors.length],
          thrown: false,
          aborted: false,
          failState
        };
      }

      return {
        node,
        error: null,
        thrown: false,
        aborted: false,
        failState: null
      };
    } catch (error) {
      const failState = this.state;
      this.state = oldState;

      if (error instanceof SyntaxError) {
        return {
          node: null,
          error,
          thrown: true,
          aborted: false,
          failState
        };
      }

      if (error === abortSignal) {
        return {
          node: abortSignal.node,
          error: null,
          thrown: false,
          aborted: true,
          failState
        };
      }

      throw error;
    }
  }

  checkExpressionErrors(refExpressionErrors, andThrow) {
    if (!refExpressionErrors) return false;
    const {
      shorthandAssign,
      doubleProto,
      optionalParameters
    } = refExpressionErrors;

    if (!andThrow) {
      return shorthandAssign >= 0 || doubleProto >= 0 || optionalParameters >= 0;
    }

    if (shorthandAssign >= 0) {
      this.unexpected(shorthandAssign);
    }

    if (doubleProto >= 0) {
      this.raise(doubleProto, ErrorMessages.DuplicateProto);
    }

    if (optionalParameters >= 0) {
      this.unexpected(optionalParameters);
    }
  }

  isLiteralPropertyName() {
    return this.match(types$1.name) || !!this.state.type.keyword || this.match(types$1.string) || this.match(types$1.num) || this.match(types$1.bigint) || this.match(types$1.decimal);
  }

  isPrivateName(node) {
    return node.type === "PrivateName";
  }

  getPrivateNameSV(node) {
    return node.id.name;
  }

  hasPropertyAsPrivateName(node) {
    return (node.type === "MemberExpression" || node.type === "OptionalMemberExpression") && this.isPrivateName(node.property);
  }

  isOptionalChain(node) {
    return node.type === "OptionalMemberExpression" || node.type === "OptionalCallExpression";
  }

  isObjectProperty(node) {
    return node.type === "ObjectProperty";
  }

  isObjectMethod(node) {
    return node.type === "ObjectMethod";
  }

  initializeScopes(inModule = this.options.sourceType === "module") {
    const oldLabels = this.state.labels;
    this.state.labels = [];
    const oldExportedIdentifiers = this.exportedIdentifiers;
    this.exportedIdentifiers = new Set();
    const oldInModule = this.inModule;
    this.inModule = inModule;
    const oldScope = this.scope;
    const ScopeHandler = this.getScopeHandler();
    this.scope = new ScopeHandler(this.raise.bind(this), this.inModule);
    const oldProdParam = this.prodParam;
    this.prodParam = new ProductionParameterHandler();
    const oldClassScope = this.classScope;
    this.classScope = new ClassScopeHandler(this.raise.bind(this));
    const oldExpressionScope = this.expressionScope;
    this.expressionScope = new ExpressionScopeHandler(this.raise.bind(this));
    return () => {
      this.state.labels = oldLabels;
      this.exportedIdentifiers = oldExportedIdentifiers;
      this.inModule = oldInModule;
      this.scope = oldScope;
      this.prodParam = oldProdParam;
      this.classScope = oldClassScope;
      this.expressionScope = oldExpressionScope;
    };
  }

  enterInitialScopes() {
    let paramFlags = PARAM;

    if (this.hasPlugin("topLevelAwait") && this.inModule) {
      paramFlags |= PARAM_AWAIT;
    }

    this.scope.enter(SCOPE_PROGRAM);
    this.prodParam.enter(paramFlags);
  }

}
class ExpressionErrors {
  constructor() {
    this.shorthandAssign = -1;
    this.doubleProto = -1;
    this.optionalParameters = -1;
  }

}

class Node {
  constructor(parser, pos, loc) {
    this.type = void 0;
    this.start = void 0;
    this.end = void 0;
    this.loc = void 0;
    this.range = void 0;
    this.leadingComments = void 0;
    this.trailingComments = void 0;
    this.innerComments = void 0;
    this.extra = void 0;
    this.type = "";
    this.start = pos;
    this.end = 0;
    this.loc = new SourceLocation(loc);
    if (parser != null && parser.options.ranges) this.range = [pos, 0];
    if (parser != null && parser.filename) this.loc.filename = parser.filename;
  }

  __clone() {
    const newNode = new Node();
    const keys = Object.keys(this);

    for (let i = 0, length = keys.length; i < length; i++) {
      const key = keys[i];

      if (key !== "leadingComments" && key !== "trailingComments" && key !== "innerComments") {
        newNode[key] = this[key];
      }
    }

    return newNode;
  }

}

class NodeUtils extends UtilParser {
  startNode() {
    return new Node(this, this.state.start, this.state.startLoc);
  }

  startNodeAt(pos, loc) {
    return new Node(this, pos, loc);
  }

  startNodeAtNode(type) {
    return this.startNodeAt(type.start, type.loc.start);
  }

  finishNode(node, type) {
    return this.finishNodeAt(node, type, this.state.lastTokEnd, this.state.lastTokEndLoc);
  }

  finishNodeAt(node, type, pos, loc) {

    node.type = type;
    node.end = pos;
    node.loc.end = loc;
    if (this.options.ranges) node.range[1] = pos;
    this.processComment(node);
    return node;
  }

  resetStartLocation(node, start, startLoc) {
    node.start = start;
    node.loc.start = startLoc;
    if (this.options.ranges) node.range[0] = start;
  }

  resetEndLocation(node, end = this.state.lastTokEnd, endLoc = this.state.lastTokEndLoc) {
    node.end = end;
    node.loc.end = endLoc;
    if (this.options.ranges) node.range[1] = end;
  }

  resetStartLocationFromNode(node, locationNode) {
    this.resetStartLocation(node, locationNode.start, locationNode.loc.start);
  }

}

const unwrapParenthesizedExpression = node => {
  return node.type === "ParenthesizedExpression" ? unwrapParenthesizedExpression(node.expression) : node;
};

class LValParser extends NodeUtils {
  toAssignable(node, isLHS = false) {
    var _node$extra, _node$extra3;

    let parenthesized = undefined;

    if (node.type === "ParenthesizedExpression" || (_node$extra = node.extra) != null && _node$extra.parenthesized) {
      parenthesized = unwrapParenthesizedExpression(node);

      if (isLHS) {
        if (parenthesized.type === "Identifier") {
          this.expressionScope.recordParenthesizedIdentifierError(node.start, ErrorMessages.InvalidParenthesizedAssignment);
        } else if (parenthesized.type !== "MemberExpression") {
          this.raise(node.start, ErrorMessages.InvalidParenthesizedAssignment);
        }
      } else {
        this.raise(node.start, ErrorMessages.InvalidParenthesizedAssignment);
      }
    }

    switch (node.type) {
      case "Identifier":
      case "ObjectPattern":
      case "ArrayPattern":
      case "AssignmentPattern":
        break;

      case "ObjectExpression":
        node.type = "ObjectPattern";

        for (let i = 0, length = node.properties.length, last = length - 1; i < length; i++) {
          var _node$extra2;

          const prop = node.properties[i];
          const isLast = i === last;
          this.toAssignableObjectExpressionProp(prop, isLast, isLHS);

          if (isLast && prop.type === "RestElement" && (_node$extra2 = node.extra) != null && _node$extra2.trailingComma) {
            this.raiseRestNotLast(node.extra.trailingComma);
          }
        }

        break;

      case "ObjectProperty":
        this.toAssignable(node.value, isLHS);
        break;

      case "SpreadElement":
        {
          this.checkToRestConversion(node);
          node.type = "RestElement";
          const arg = node.argument;
          this.toAssignable(arg, isLHS);
          break;
        }

      case "ArrayExpression":
        node.type = "ArrayPattern";
        this.toAssignableList(node.elements, (_node$extra3 = node.extra) == null ? void 0 : _node$extra3.trailingComma, isLHS);
        break;

      case "AssignmentExpression":
        if (node.operator !== "=") {
          this.raise(node.left.end, ErrorMessages.MissingEqInAssignment);
        }

        node.type = "AssignmentPattern";
        delete node.operator;
        this.toAssignable(node.left, isLHS);
        break;

      case "ParenthesizedExpression":
        this.toAssignable(parenthesized, isLHS);
        break;
    }

    return node;
  }

  toAssignableObjectExpressionProp(prop, isLast, isLHS) {
    if (prop.type === "ObjectMethod") {
      const error = prop.kind === "get" || prop.kind === "set" ? ErrorMessages.PatternHasAccessor : ErrorMessages.PatternHasMethod;
      this.raise(prop.key.start, error);
    } else if (prop.type === "SpreadElement" && !isLast) {
      this.raiseRestNotLast(prop.start);
    } else {
      this.toAssignable(prop, isLHS);
    }
  }

  toAssignableList(exprList, trailingCommaPos, isLHS) {
    let end = exprList.length;

    if (end) {
      const last = exprList[end - 1];

      if ((last == null ? void 0 : last.type) === "RestElement") {
        --end;
      } else if ((last == null ? void 0 : last.type) === "SpreadElement") {
        last.type = "RestElement";
        let arg = last.argument;
        this.toAssignable(arg, isLHS);
        arg = unwrapParenthesizedExpression(arg);

        if (arg.type !== "Identifier" && arg.type !== "MemberExpression" && arg.type !== "ArrayPattern" && arg.type !== "ObjectPattern") {
          this.unexpected(arg.start);
        }

        if (trailingCommaPos) {
          this.raiseTrailingCommaAfterRest(trailingCommaPos);
        }

        --end;
      }
    }

    for (let i = 0; i < end; i++) {
      const elt = exprList[i];

      if (elt) {
        this.toAssignable(elt, isLHS);

        if (elt.type === "RestElement") {
          this.raiseRestNotLast(elt.start);
        }
      }
    }

    return exprList;
  }

  toReferencedList(exprList, isParenthesizedExpr) {
    return exprList;
  }

  toReferencedListDeep(exprList, isParenthesizedExpr) {
    this.toReferencedList(exprList, isParenthesizedExpr);

    for (const expr of exprList) {
      if ((expr == null ? void 0 : expr.type) === "ArrayExpression") {
        this.toReferencedListDeep(expr.elements);
      }
    }
  }

  parseSpread(refExpressionErrors, refNeedsArrowPos) {
    const node = this.startNode();
    this.next();
    node.argument = this.parseMaybeAssignAllowIn(refExpressionErrors, undefined, refNeedsArrowPos);
    return this.finishNode(node, "SpreadElement");
  }

  parseRestBinding() {
    const node = this.startNode();
    this.next();
    node.argument = this.parseBindingAtom();
    return this.finishNode(node, "RestElement");
  }

  parseBindingAtom() {
    switch (this.state.type) {
      case types$1.bracketL:
        {
          const node = this.startNode();
          this.next();
          node.elements = this.parseBindingList(types$1.bracketR, 93, true);
          return this.finishNode(node, "ArrayPattern");
        }

      case types$1.braceL:
        return this.parseObjectLike(types$1.braceR, true);
    }

    return this.parseIdentifier();
  }

  parseBindingList(close, closeCharCode, allowEmpty, allowModifiers) {
    const elts = [];
    let first = true;

    while (!this.eat(close)) {
      if (first) {
        first = false;
      } else {
        this.expect(types$1.comma);
      }

      if (allowEmpty && this.match(types$1.comma)) {
        elts.push(null);
      } else if (this.eat(close)) {
        break;
      } else if (this.match(types$1.ellipsis)) {
        elts.push(this.parseAssignableListItemTypes(this.parseRestBinding()));
        this.checkCommaAfterRest(closeCharCode);
        this.expect(close);
        break;
      } else {
        const decorators = [];

        if (this.match(types$1.at) && this.hasPlugin("decorators")) {
          this.raise(this.state.start, ErrorMessages.UnsupportedParameterDecorator);
        }

        while (this.match(types$1.at)) {
          decorators.push(this.parseDecorator());
        }

        elts.push(this.parseAssignableListItem(allowModifiers, decorators));
      }
    }

    return elts;
  }

  parseAssignableListItem(allowModifiers, decorators) {
    const left = this.parseMaybeDefault();
    this.parseAssignableListItemTypes(left);
    const elt = this.parseMaybeDefault(left.start, left.loc.start, left);

    if (decorators.length) {
      left.decorators = decorators;
    }

    return elt;
  }

  parseAssignableListItemTypes(param) {
    return param;
  }

  parseMaybeDefault(startPos, startLoc, left) {
    var _startLoc, _startPos, _left;

    startLoc = (_startLoc = startLoc) != null ? _startLoc : this.state.startLoc;
    startPos = (_startPos = startPos) != null ? _startPos : this.state.start;
    left = (_left = left) != null ? _left : this.parseBindingAtom();
    if (!this.eat(types$1.eq)) return left;
    const node = this.startNodeAt(startPos, startLoc);
    node.left = left;
    node.right = this.parseMaybeAssignAllowIn();
    return this.finishNode(node, "AssignmentPattern");
  }

  checkLVal(expr, contextDescription, bindingType = BIND_NONE, checkClashes, disallowLetBinding, strictModeChanged = false) {
    switch (expr.type) {
      case "Identifier":
        {
          const {
            name
          } = expr;

          if (this.state.strict && (strictModeChanged ? isStrictBindReservedWord(name, this.inModule) : isStrictBindOnlyReservedWord(name))) {
            this.raise(expr.start, bindingType === BIND_NONE ? ErrorMessages.StrictEvalArguments : ErrorMessages.StrictEvalArgumentsBinding, name);
          }

          if (checkClashes) {
            if (checkClashes.has(name)) {
              this.raise(expr.start, ErrorMessages.ParamDupe);
            } else {
              checkClashes.add(name);
            }
          }

          if (disallowLetBinding && name === "let") {
            this.raise(expr.start, ErrorMessages.LetInLexicalBinding);
          }

          if (!(bindingType & BIND_NONE)) {
            this.scope.declareName(name, bindingType, expr.start);
          }

          break;
        }

      case "MemberExpression":
        if (bindingType !== BIND_NONE) {
          this.raise(expr.start, ErrorMessages.InvalidPropertyBindingPattern);
        }

        break;

      case "ObjectPattern":
        for (let prop of expr.properties) {
          if (this.isObjectProperty(prop)) prop = prop.value;else if (this.isObjectMethod(prop)) continue;
          this.checkLVal(prop, "object destructuring pattern", bindingType, checkClashes, disallowLetBinding);
        }

        break;

      case "ArrayPattern":
        for (const elem of expr.elements) {
          if (elem) {
            this.checkLVal(elem, "array destructuring pattern", bindingType, checkClashes, disallowLetBinding);
          }
        }

        break;

      case "AssignmentPattern":
        this.checkLVal(expr.left, "assignment pattern", bindingType, checkClashes);
        break;

      case "RestElement":
        this.checkLVal(expr.argument, "rest element", bindingType, checkClashes);
        break;

      case "ParenthesizedExpression":
        this.checkLVal(expr.expression, "parenthesized expression", bindingType, checkClashes);
        break;

      default:
        {
          this.raise(expr.start, bindingType === BIND_NONE ? ErrorMessages.InvalidLhs : ErrorMessages.InvalidLhsBinding, contextDescription);
        }
    }
  }

  checkToRestConversion(node) {
    if (node.argument.type !== "Identifier" && node.argument.type !== "MemberExpression") {
      this.raise(node.argument.start, ErrorMessages.InvalidRestAssignmentPattern);
    }
  }

  checkCommaAfterRest(close) {
    if (this.match(types$1.comma)) {
      if (this.lookaheadCharCode() === close) {
        this.raiseTrailingCommaAfterRest(this.state.start);
      } else {
        this.raiseRestNotLast(this.state.start);
      }
    }
  }

  raiseRestNotLast(pos) {
    throw this.raise(pos, ErrorMessages.ElementAfterRest);
  }

  raiseTrailingCommaAfterRest(pos) {
    this.raise(pos, ErrorMessages.RestTrailingComma);
  }

}

class ExpressionParser extends LValParser {
  checkProto(prop, isRecord, protoRef, refExpressionErrors) {
    if (prop.type === "SpreadElement" || this.isObjectMethod(prop) || prop.computed || prop.shorthand) {
      return;
    }

    const key = prop.key;
    const name = key.type === "Identifier" ? key.name : key.value;

    if (name === "__proto__") {
      if (isRecord) {
        this.raise(key.start, ErrorMessages.RecordNoProto);
        return;
      }

      if (protoRef.used) {
        if (refExpressionErrors) {
          if (refExpressionErrors.doubleProto === -1) {
            refExpressionErrors.doubleProto = key.start;
          }
        } else {
          this.raise(key.start, ErrorMessages.DuplicateProto);
        }
      }

      protoRef.used = true;
    }
  }

  shouldExitDescending(expr, potentialArrowAt) {
    return expr.type === "ArrowFunctionExpression" && expr.start === potentialArrowAt;
  }

  getExpression() {
    let paramFlags = PARAM;

    if (this.hasPlugin("topLevelAwait") && this.inModule) {
      paramFlags |= PARAM_AWAIT;
    }

    this.scope.enter(SCOPE_PROGRAM);
    this.prodParam.enter(paramFlags);
    this.nextToken();
    const expr = this.parseExpression();

    if (!this.match(types$1.eof)) {
      this.unexpected();
    }

    expr.comments = this.state.comments;
    expr.errors = this.state.errors;

    if (this.options.tokens) {
      expr.tokens = this.tokens;
    }

    return expr;
  }

  parseExpression(disallowIn, refExpressionErrors) {
    if (disallowIn) {
      return this.disallowInAnd(() => this.parseExpressionBase(refExpressionErrors));
    }

    return this.allowInAnd(() => this.parseExpressionBase(refExpressionErrors));
  }

  parseExpressionBase(refExpressionErrors) {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    const expr = this.parseMaybeAssign(refExpressionErrors);

    if (this.match(types$1.comma)) {
      const node = this.startNodeAt(startPos, startLoc);
      node.expressions = [expr];

      while (this.eat(types$1.comma)) {
        node.expressions.push(this.parseMaybeAssign(refExpressionErrors));
      }

      this.toReferencedList(node.expressions);
      return this.finishNode(node, "SequenceExpression");
    }

    return expr;
  }

  parseMaybeAssignDisallowIn(refExpressionErrors, afterLeftParse) {
    return this.disallowInAnd(() => this.parseMaybeAssign(refExpressionErrors, afterLeftParse));
  }

  parseMaybeAssignAllowIn(refExpressionErrors, afterLeftParse) {
    return this.allowInAnd(() => this.parseMaybeAssign(refExpressionErrors, afterLeftParse));
  }

  setOptionalParametersError(refExpressionErrors, resultError) {
    var _resultError$pos;

    refExpressionErrors.optionalParameters = (_resultError$pos = resultError == null ? void 0 : resultError.pos) != null ? _resultError$pos : this.state.start;
  }

  parseMaybeAssign(refExpressionErrors, afterLeftParse) {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;

    if (this.isContextual("yield")) {
      if (this.prodParam.hasYield) {
        let left = this.parseYield();

        if (afterLeftParse) {
          left = afterLeftParse.call(this, left, startPos, startLoc);
        }

        return left;
      }
    }

    let ownExpressionErrors;

    if (refExpressionErrors) {
      ownExpressionErrors = false;
    } else {
      refExpressionErrors = new ExpressionErrors();
      ownExpressionErrors = true;
    }

    if (this.match(types$1.parenL) || this.match(types$1.name)) {
      this.state.potentialArrowAt = this.state.start;
    }

    let left = this.parseMaybeConditional(refExpressionErrors);

    if (afterLeftParse) {
      left = afterLeftParse.call(this, left, startPos, startLoc);
    }

    if (this.state.type.isAssign) {
      const node = this.startNodeAt(startPos, startLoc);
      const operator = this.state.value;
      node.operator = operator;

      if (this.match(types$1.eq)) {
        node.left = this.toAssignable(left, true);
        refExpressionErrors.doubleProto = -1;
      } else {
        node.left = left;
      }

      if (refExpressionErrors.shorthandAssign >= node.left.start) {
        refExpressionErrors.shorthandAssign = -1;
      }

      this.checkLVal(left, "assignment expression");
      this.next();
      node.right = this.parseMaybeAssign();
      return this.finishNode(node, "AssignmentExpression");
    } else if (ownExpressionErrors) {
      this.checkExpressionErrors(refExpressionErrors, true);
    }

    return left;
  }

  parseMaybeConditional(refExpressionErrors) {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    const potentialArrowAt = this.state.potentialArrowAt;
    const expr = this.parseExprOps(refExpressionErrors);

    if (this.shouldExitDescending(expr, potentialArrowAt)) {
      return expr;
    }

    return this.parseConditional(expr, startPos, startLoc, refExpressionErrors);
  }

  parseConditional(expr, startPos, startLoc, refExpressionErrors) {
    if (this.eat(types$1.question)) {
      const node = this.startNodeAt(startPos, startLoc);
      node.test = expr;
      node.consequent = this.parseMaybeAssignAllowIn();
      this.expect(types$1.colon);
      node.alternate = this.parseMaybeAssign();
      return this.finishNode(node, "ConditionalExpression");
    }

    return expr;
  }

  parseExprOps(refExpressionErrors) {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    const potentialArrowAt = this.state.potentialArrowAt;
    const expr = this.parseMaybeUnary(refExpressionErrors);

    if (this.shouldExitDescending(expr, potentialArrowAt)) {
      return expr;
    }

    return this.parseExprOp(expr, startPos, startLoc, -1);
  }

  parseExprOp(left, leftStartPos, leftStartLoc, minPrec) {
    let prec = this.state.type.binop;

    if (prec != null && (this.prodParam.hasIn || !this.match(types$1._in))) {
      if (prec > minPrec) {
        const op = this.state.type;

        if (op === types$1.pipeline) {
          this.expectPlugin("pipelineOperator");

          if (this.state.inFSharpPipelineDirectBody) {
            return left;
          }

          this.state.inPipeline = true;
          this.checkPipelineAtInfixOperator(left, leftStartPos);
        }

        const node = this.startNodeAt(leftStartPos, leftStartLoc);
        node.left = left;
        node.operator = this.state.value;
        const logical = op === types$1.logicalOR || op === types$1.logicalAND;
        const coalesce = op === types$1.nullishCoalescing;

        if (coalesce) {
          prec = types$1.logicalAND.binop;
        }

        this.next();

        if (op === types$1.pipeline && this.getPluginOption("pipelineOperator", "proposal") === "minimal") {
          if (this.match(types$1.name) && this.state.value === "await" && this.prodParam.hasAwait) {
            throw this.raise(this.state.start, ErrorMessages.UnexpectedAwaitAfterPipelineBody);
          }
        }

        node.right = this.parseExprOpRightExpr(op, prec);
        this.finishNode(node, logical || coalesce ? "LogicalExpression" : "BinaryExpression");
        const nextOp = this.state.type;

        if (coalesce && (nextOp === types$1.logicalOR || nextOp === types$1.logicalAND) || logical && nextOp === types$1.nullishCoalescing) {
          throw this.raise(this.state.start, ErrorMessages.MixingCoalesceWithLogical);
        }

        return this.parseExprOp(node, leftStartPos, leftStartLoc, minPrec);
      }
    }

    return left;
  }

  parseExprOpRightExpr(op, prec) {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;

    switch (op) {
      case types$1.pipeline:
        switch (this.getPluginOption("pipelineOperator", "proposal")) {
          case "smart":
            return this.withTopicPermittingContext(() => {
              return this.parseSmartPipelineBody(this.parseExprOpBaseRightExpr(op, prec), startPos, startLoc);
            });

          case "fsharp":
            return this.withSoloAwaitPermittingContext(() => {
              return this.parseFSharpPipelineBody(prec);
            });
        }

      default:
        return this.parseExprOpBaseRightExpr(op, prec);
    }
  }

  parseExprOpBaseRightExpr(op, prec) {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    return this.parseExprOp(this.parseMaybeUnary(), startPos, startLoc, op.rightAssociative ? prec - 1 : prec);
  }

  checkExponentialAfterUnary(node) {
    if (this.match(types$1.exponent)) {
      this.raise(node.argument.start, ErrorMessages.UnexpectedTokenUnaryExponentiation);
    }
  }

  parseMaybeUnary(refExpressionErrors, sawUnary) {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    const isAwait = this.isContextual("await");

    if (isAwait && this.isAwaitAllowed()) {
      this.next();
      const expr = this.parseAwait(startPos, startLoc);
      if (!sawUnary) this.checkExponentialAfterUnary(expr);
      return expr;
    }

    if (this.isContextual("module") && this.lookaheadCharCode() === 123 && !this.hasFollowingLineBreak()) {
      return this.parseModuleExpression();
    }

    const update = this.match(types$1.incDec);
    const node = this.startNode();

    if (this.state.type.prefix) {
      node.operator = this.state.value;
      node.prefix = true;

      if (this.match(types$1._throw)) {
        this.expectPlugin("throwExpressions");
      }

      const isDelete = this.match(types$1._delete);
      this.next();
      node.argument = this.parseMaybeUnary(null, true);
      this.checkExpressionErrors(refExpressionErrors, true);

      if (this.state.strict && isDelete) {
        const arg = node.argument;

        if (arg.type === "Identifier") {
          this.raise(node.start, ErrorMessages.StrictDelete);
        } else if (this.hasPropertyAsPrivateName(arg)) {
          this.raise(node.start, ErrorMessages.DeletePrivateField);
        }
      }

      if (!update) {
        if (!sawUnary) this.checkExponentialAfterUnary(node);
        return this.finishNode(node, "UnaryExpression");
      }
    }

    const expr = this.parseUpdate(node, update, refExpressionErrors);

    if (isAwait) {
      const startsExpr = this.hasPlugin("v8intrinsic") ? this.state.type.startsExpr : this.state.type.startsExpr && !this.match(types$1.modulo);

      if (startsExpr && !this.isAmbiguousAwait()) {
        this.raiseOverwrite(startPos, this.hasPlugin("topLevelAwait") ? ErrorMessages.AwaitNotInAsyncContext : ErrorMessages.AwaitNotInAsyncFunction);
        return this.parseAwait(startPos, startLoc);
      }
    }

    return expr;
  }

  parseUpdate(node, update, refExpressionErrors) {
    if (update) {
      this.checkLVal(node.argument, "prefix operation");
      return this.finishNode(node, "UpdateExpression");
    }

    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    let expr = this.parseExprSubscripts(refExpressionErrors);
    if (this.checkExpressionErrors(refExpressionErrors, false)) return expr;

    while (this.state.type.postfix && !this.canInsertSemicolon()) {
      const node = this.startNodeAt(startPos, startLoc);
      node.operator = this.state.value;
      node.prefix = false;
      node.argument = expr;
      this.checkLVal(expr, "postfix operation");
      this.next();
      expr = this.finishNode(node, "UpdateExpression");
    }

    return expr;
  }

  parseExprSubscripts(refExpressionErrors) {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    const potentialArrowAt = this.state.potentialArrowAt;
    const expr = this.parseExprAtom(refExpressionErrors);

    if (this.shouldExitDescending(expr, potentialArrowAt)) {
      return expr;
    }

    return this.parseSubscripts(expr, startPos, startLoc);
  }

  parseSubscripts(base, startPos, startLoc, noCalls) {
    const state = {
      optionalChainMember: false,
      maybeAsyncArrow: this.atPossibleAsyncArrow(base),
      stop: false
    };

    do {
      base = this.parseSubscript(base, startPos, startLoc, noCalls, state);
      state.maybeAsyncArrow = false;
    } while (!state.stop);

    return base;
  }

  parseSubscript(base, startPos, startLoc, noCalls, state) {
    if (!noCalls && this.eat(types$1.doubleColon)) {
      return this.parseBind(base, startPos, startLoc, noCalls, state);
    } else if (this.match(types$1.backQuote)) {
      return this.parseTaggedTemplateExpression(base, startPos, startLoc, state);
    }

    let optional = false;

    if (this.match(types$1.questionDot)) {
      if (noCalls && this.lookaheadCharCode() === 40) {
        state.stop = true;
        return base;
      }

      state.optionalChainMember = optional = true;
      this.next();
    }

    if (!noCalls && this.match(types$1.parenL)) {
      return this.parseCoverCallAndAsyncArrowHead(base, startPos, startLoc, state, optional);
    } else if (optional || this.match(types$1.bracketL) || this.eat(types$1.dot)) {
      return this.parseMember(base, startPos, startLoc, state, optional);
    } else {
      state.stop = true;
      return base;
    }
  }

  parseMember(base, startPos, startLoc, state, optional) {
    const node = this.startNodeAt(startPos, startLoc);
    const computed = this.eat(types$1.bracketL);
    node.object = base;
    node.computed = computed;
    const privateName = !computed && this.match(types$1.privateName) && this.state.value;
    const property = computed ? this.parseExpression() : privateName ? this.parsePrivateName() : this.parseIdentifier(true);

    if (privateName !== false) {
      if (node.object.type === "Super") {
        this.raise(startPos, ErrorMessages.SuperPrivateField);
      }

      this.classScope.usePrivateName(privateName, property.start);
    }

    node.property = property;

    if (computed) {
      this.expect(types$1.bracketR);
    }

    if (state.optionalChainMember) {
      node.optional = optional;
      return this.finishNode(node, "OptionalMemberExpression");
    } else {
      return this.finishNode(node, "MemberExpression");
    }
  }

  parseBind(base, startPos, startLoc, noCalls, state) {
    const node = this.startNodeAt(startPos, startLoc);
    node.object = base;
    node.callee = this.parseNoCallExpr();
    state.stop = true;
    return this.parseSubscripts(this.finishNode(node, "BindExpression"), startPos, startLoc, noCalls);
  }

  parseCoverCallAndAsyncArrowHead(base, startPos, startLoc, state, optional) {
    const oldMaybeInArrowParameters = this.state.maybeInArrowParameters;
    let refExpressionErrors = null;
    this.state.maybeInArrowParameters = true;
    this.next();
    let node = this.startNodeAt(startPos, startLoc);
    node.callee = base;

    if (state.maybeAsyncArrow) {
      this.expressionScope.enter(newAsyncArrowScope());
      refExpressionErrors = new ExpressionErrors();
    }

    if (state.optionalChainMember) {
      node.optional = optional;
    }

    if (optional) {
      node.arguments = this.parseCallExpressionArguments(types$1.parenR);
    } else {
      node.arguments = this.parseCallExpressionArguments(types$1.parenR, base.type === "Import", base.type !== "Super", node, refExpressionErrors);
    }

    this.finishCallExpression(node, state.optionalChainMember);

    if (state.maybeAsyncArrow && this.shouldParseAsyncArrow() && !optional) {
      state.stop = true;
      this.expressionScope.validateAsPattern();
      this.expressionScope.exit();
      node = this.parseAsyncArrowFromCallExpression(this.startNodeAt(startPos, startLoc), node);
    } else {
      if (state.maybeAsyncArrow) {
        this.checkExpressionErrors(refExpressionErrors, true);
        this.expressionScope.exit();
      }

      this.toReferencedArguments(node);
    }

    this.state.maybeInArrowParameters = oldMaybeInArrowParameters;
    return node;
  }

  toReferencedArguments(node, isParenthesizedExpr) {
    this.toReferencedListDeep(node.arguments, isParenthesizedExpr);
  }

  parseTaggedTemplateExpression(base, startPos, startLoc, state) {
    const node = this.startNodeAt(startPos, startLoc);
    node.tag = base;
    node.quasi = this.parseTemplate(true);

    if (state.optionalChainMember) {
      this.raise(startPos, ErrorMessages.OptionalChainingNoTemplate);
    }

    return this.finishNode(node, "TaggedTemplateExpression");
  }

  atPossibleAsyncArrow(base) {
    return base.type === "Identifier" && base.name === "async" && this.state.lastTokEnd === base.end && !this.canInsertSemicolon() && base.end - base.start === 5 && base.start === this.state.potentialArrowAt;
  }

  finishCallExpression(node, optional) {
    if (node.callee.type === "Import") {
      if (node.arguments.length === 2) {
        {
          if (!this.hasPlugin("moduleAttributes")) {
            this.expectPlugin("importAssertions");
          }
        }
      }

      if (node.arguments.length === 0 || node.arguments.length > 2) {
        this.raise(node.start, ErrorMessages.ImportCallArity, this.hasPlugin("importAssertions") || this.hasPlugin("moduleAttributes") ? "one or two arguments" : "one argument");
      } else {
        for (const arg of node.arguments) {
          if (arg.type === "SpreadElement") {
            this.raise(arg.start, ErrorMessages.ImportCallSpreadArgument);
          }
        }
      }
    }

    return this.finishNode(node, optional ? "OptionalCallExpression" : "CallExpression");
  }

  parseCallExpressionArguments(close, dynamicImport, allowPlaceholder, nodeForExtra, refExpressionErrors) {
    const elts = [];
    let first = true;
    const oldInFSharpPipelineDirectBody = this.state.inFSharpPipelineDirectBody;
    this.state.inFSharpPipelineDirectBody = false;

    while (!this.eat(close)) {
      if (first) {
        first = false;
      } else {
        this.expect(types$1.comma);

        if (this.match(close)) {
          if (dynamicImport && !this.hasPlugin("importAssertions") && !this.hasPlugin("moduleAttributes")) {
            this.raise(this.state.lastTokStart, ErrorMessages.ImportCallArgumentTrailingComma);
          }

          if (nodeForExtra) {
            this.addExtra(nodeForExtra, "trailingComma", this.state.lastTokStart);
          }

          this.next();
          break;
        }
      }

      elts.push(this.parseExprListItem(false, refExpressionErrors, allowPlaceholder));
    }

    this.state.inFSharpPipelineDirectBody = oldInFSharpPipelineDirectBody;
    return elts;
  }

  shouldParseAsyncArrow() {
    return this.match(types$1.arrow) && !this.canInsertSemicolon();
  }

  parseAsyncArrowFromCallExpression(node, call) {
    var _call$extra;

    this.expect(types$1.arrow);
    this.parseArrowExpression(node, call.arguments, true, (_call$extra = call.extra) == null ? void 0 : _call$extra.trailingComma);
    return node;
  }

  parseNoCallExpr() {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    return this.parseSubscripts(this.parseExprAtom(), startPos, startLoc, true);
  }

  parseExprAtom(refExpressionErrors) {
    let node;

    switch (this.state.type) {
      case types$1._super:
        return this.parseSuper();

      case types$1._import:
        node = this.startNode();
        this.next();

        if (this.match(types$1.dot)) {
          return this.parseImportMetaProperty(node);
        }

        if (!this.match(types$1.parenL)) {
          this.raise(this.state.lastTokStart, ErrorMessages.UnsupportedImport);
        }

        return this.finishNode(node, "Import");

      case types$1._this:
        node = this.startNode();
        this.next();
        return this.finishNode(node, "ThisExpression");

      case types$1.name:
        {
          const canBeArrow = this.state.potentialArrowAt === this.state.start;
          const containsEsc = this.state.containsEsc;
          const id = this.parseIdentifier();

          if (!containsEsc && id.name === "async" && !this.canInsertSemicolon()) {
            if (this.match(types$1._function)) {
              this.next();
              return this.parseFunction(this.startNodeAtNode(id), undefined, true);
            } else if (this.match(types$1.name)) {
              if (this.lookaheadCharCode() === 61) {
                return this.parseAsyncArrowUnaryFunction(id);
              } else {
                return id;
              }
            } else if (this.match(types$1._do)) {
              return this.parseDo(true);
            }
          }

          if (canBeArrow && this.match(types$1.arrow) && !this.canInsertSemicolon()) {
            this.next();
            return this.parseArrowExpression(this.startNodeAtNode(id), [id], false);
          }

          return id;
        }

      case types$1._do:
        {
          return this.parseDo(false);
        }

      case types$1.slash:
      case types$1.slashAssign:
        {
          this.readRegexp();
          return this.parseRegExpLiteral(this.state.value);
        }

      case types$1.num:
        return this.parseNumericLiteral(this.state.value);

      case types$1.bigint:
        return this.parseBigIntLiteral(this.state.value);

      case types$1.decimal:
        return this.parseDecimalLiteral(this.state.value);

      case types$1.string:
        return this.parseStringLiteral(this.state.value);

      case types$1._null:
        return this.parseNullLiteral();

      case types$1._true:
        return this.parseBooleanLiteral(true);

      case types$1._false:
        return this.parseBooleanLiteral(false);

      case types$1.parenL:
        {
          const canBeArrow = this.state.potentialArrowAt === this.state.start;
          return this.parseParenAndDistinguishExpression(canBeArrow);
        }

      case types$1.bracketBarL:
      case types$1.bracketHashL:
        {
          return this.parseArrayLike(this.state.type === types$1.bracketBarL ? types$1.bracketBarR : types$1.bracketR, false, true, refExpressionErrors);
        }

      case types$1.bracketL:
        {
          return this.parseArrayLike(types$1.bracketR, true, false, refExpressionErrors);
        }

      case types$1.braceBarL:
      case types$1.braceHashL:
        {
          return this.parseObjectLike(this.state.type === types$1.braceBarL ? types$1.braceBarR : types$1.braceR, false, true, refExpressionErrors);
        }

      case types$1.braceL:
        {
          return this.parseObjectLike(types$1.braceR, false, false, refExpressionErrors);
        }

      case types$1._function:
        return this.parseFunctionOrFunctionSent();

      case types$1.at:
        this.parseDecorators();

      case types$1._class:
        node = this.startNode();
        this.takeDecorators(node);
        return this.parseClass(node, false);

      case types$1._new:
        return this.parseNewOrNewTarget();

      case types$1.backQuote:
        return this.parseTemplate(false);

      case types$1.doubleColon:
        {
          node = this.startNode();
          this.next();
          node.object = null;
          const callee = node.callee = this.parseNoCallExpr();

          if (callee.type === "MemberExpression") {
            return this.finishNode(node, "BindExpression");
          } else {
            throw this.raise(callee.start, ErrorMessages.UnsupportedBind);
          }
        }

      case types$1.privateName:
        {
          const start = this.state.start;
          const value = this.state.value;
          node = this.parsePrivateName();

          if (this.match(types$1._in)) {
            this.expectPlugin("privateIn");
            this.classScope.usePrivateName(value, node.start);
          } else if (this.hasPlugin("privateIn")) {
            this.raise(this.state.start, ErrorMessages.PrivateInExpectedIn, value);
          } else {
            throw this.unexpected(start);
          }

          return node;
        }

      case types$1.hash:
        {
          if (this.state.inPipeline) {
            node = this.startNode();

            if (this.getPluginOption("pipelineOperator", "proposal") !== "smart") {
              this.raise(node.start, ErrorMessages.PrimaryTopicRequiresSmartPipeline);
            }

            this.next();

            if (!this.primaryTopicReferenceIsAllowedInCurrentTopicContext()) {
              this.raise(node.start, ErrorMessages.PrimaryTopicNotAllowed);
            }

            this.registerTopicReference();
            return this.finishNode(node, "PipelinePrimaryTopicReference");
          }
        }

      case types$1.relational:
        {
          if (this.state.value === "<") {
            const lookaheadCh = this.input.codePointAt(this.nextTokenStart());

            if (isIdentifierStart(lookaheadCh) || lookaheadCh === 62) {
                this.expectOnePlugin(["jsx", "flow", "typescript"]);
              }
          }
        }

      default:
        throw this.unexpected();
    }
  }

  parseAsyncArrowUnaryFunction(id) {
    const node = this.startNodeAtNode(id);
    this.prodParam.enter(functionFlags(true, this.prodParam.hasYield));
    const params = [this.parseIdentifier()];
    this.prodParam.exit();

    if (this.hasPrecedingLineBreak()) {
      this.raise(this.state.pos, ErrorMessages.LineTerminatorBeforeArrow);
    }

    this.expect(types$1.arrow);
    this.parseArrowExpression(node, params, true);
    return node;
  }

  parseDo(isAsync) {
    this.expectPlugin("doExpressions");

    if (isAsync) {
      this.expectPlugin("asyncDoExpressions");
    }

    const node = this.startNode();
    node.async = isAsync;
    this.next();
    const oldLabels = this.state.labels;
    this.state.labels = [];

    if (isAsync) {
      this.prodParam.enter(PARAM_AWAIT);
      node.body = this.parseBlock();
      this.prodParam.exit();
    } else {
      node.body = this.parseBlock();
    }

    this.state.labels = oldLabels;
    return this.finishNode(node, "DoExpression");
  }

  parseSuper() {
    const node = this.startNode();
    this.next();

    if (this.match(types$1.parenL) && !this.scope.allowDirectSuper && !this.options.allowSuperOutsideMethod) {
      this.raise(node.start, ErrorMessages.SuperNotAllowed);
    } else if (!this.scope.allowSuper && !this.options.allowSuperOutsideMethod) {
      this.raise(node.start, ErrorMessages.UnexpectedSuper);
    }

    if (!this.match(types$1.parenL) && !this.match(types$1.bracketL) && !this.match(types$1.dot)) {
      this.raise(node.start, ErrorMessages.UnsupportedSuper);
    }

    return this.finishNode(node, "Super");
  }

  parseMaybePrivateName(isPrivateNameAllowed) {
    const isPrivate = this.match(types$1.privateName);

    if (isPrivate) {
      if (!isPrivateNameAllowed) {
        this.raise(this.state.start + 1, ErrorMessages.UnexpectedPrivateField);
      }

      return this.parsePrivateName();
    } else {
      return this.parseIdentifier(true);
    }
  }

  parsePrivateName() {
    const node = this.startNode();
    const id = this.startNodeAt(this.state.start + 1, new Position(this.state.curLine, this.state.start + 1 - this.state.lineStart));
    const name = this.state.value;
    this.next();
    node.id = this.createIdentifier(id, name);
    return this.finishNode(node, "PrivateName");
  }

  parseFunctionOrFunctionSent() {
    const node = this.startNode();
    this.next();

    if (this.prodParam.hasYield && this.match(types$1.dot)) {
      const meta = this.createIdentifier(this.startNodeAtNode(node), "function");
      this.next();
      return this.parseMetaProperty(node, meta, "sent");
    }

    return this.parseFunction(node);
  }

  parseMetaProperty(node, meta, propertyName) {
    node.meta = meta;

    if (meta.name === "function" && propertyName === "sent") {
      if (this.isContextual(propertyName)) {
        this.expectPlugin("functionSent");
      } else if (!this.hasPlugin("functionSent")) {
        this.unexpected();
      }
    }

    const containsEsc = this.state.containsEsc;
    node.property = this.parseIdentifier(true);

    if (node.property.name !== propertyName || containsEsc) {
      this.raise(node.property.start, ErrorMessages.UnsupportedMetaProperty, meta.name, propertyName);
    }

    return this.finishNode(node, "MetaProperty");
  }

  parseImportMetaProperty(node) {
    const id = this.createIdentifier(this.startNodeAtNode(node), "import");
    this.next();

    if (this.isContextual("meta")) {
      if (!this.inModule) {
        this.raise(id.start, SourceTypeModuleErrorMessages.ImportMetaOutsideModule);
      }

      this.sawUnambiguousESM = true;
    }

    return this.parseMetaProperty(node, id, "meta");
  }

  parseLiteralAtNode(value, type, node) {
    this.addExtra(node, "rawValue", value);
    this.addExtra(node, "raw", this.input.slice(node.start, this.state.end));
    node.value = value;
    this.next();
    return this.finishNode(node, type);
  }

  parseLiteral(value, type) {
    const node = this.startNode();
    return this.parseLiteralAtNode(value, type, node);
  }

  parseStringLiteral(value) {
    return this.parseLiteral(value, "StringLiteral");
  }

  parseNumericLiteral(value) {
    return this.parseLiteral(value, "NumericLiteral");
  }

  parseBigIntLiteral(value) {
    return this.parseLiteral(value, "BigIntLiteral");
  }

  parseDecimalLiteral(value) {
    return this.parseLiteral(value, "DecimalLiteral");
  }

  parseRegExpLiteral(value) {
    const node = this.parseLiteral(value.value, "RegExpLiteral");
    node.pattern = value.pattern;
    node.flags = value.flags;
    return node;
  }

  parseBooleanLiteral(value) {
    const node = this.startNode();
    node.value = value;
    this.next();
    return this.finishNode(node, "BooleanLiteral");
  }

  parseNullLiteral() {
    const node = this.startNode();
    this.next();
    return this.finishNode(node, "NullLiteral");
  }

  parseParenAndDistinguishExpression(canBeArrow) {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    let val;
    this.next();
    this.expressionScope.enter(newArrowHeadScope());
    const oldMaybeInArrowParameters = this.state.maybeInArrowParameters;
    const oldInFSharpPipelineDirectBody = this.state.inFSharpPipelineDirectBody;
    this.state.maybeInArrowParameters = true;
    this.state.inFSharpPipelineDirectBody = false;
    const innerStartPos = this.state.start;
    const innerStartLoc = this.state.startLoc;
    const exprList = [];
    const refExpressionErrors = new ExpressionErrors();
    let first = true;
    let spreadStart;
    let optionalCommaStart;

    while (!this.match(types$1.parenR)) {
      if (first) {
        first = false;
      } else {
        this.expect(types$1.comma, refExpressionErrors.optionalParameters === -1 ? null : refExpressionErrors.optionalParameters);

        if (this.match(types$1.parenR)) {
          optionalCommaStart = this.state.start;
          break;
        }
      }

      if (this.match(types$1.ellipsis)) {
        const spreadNodeStartPos = this.state.start;
        const spreadNodeStartLoc = this.state.startLoc;
        spreadStart = this.state.start;
        exprList.push(this.parseParenItem(this.parseRestBinding(), spreadNodeStartPos, spreadNodeStartLoc));
        this.checkCommaAfterRest(41);
        break;
      } else {
        exprList.push(this.parseMaybeAssignAllowIn(refExpressionErrors, this.parseParenItem));
      }
    }

    const innerEndPos = this.state.lastTokEnd;
    const innerEndLoc = this.state.lastTokEndLoc;
    this.expect(types$1.parenR);
    this.state.maybeInArrowParameters = oldMaybeInArrowParameters;
    this.state.inFSharpPipelineDirectBody = oldInFSharpPipelineDirectBody;
    let arrowNode = this.startNodeAt(startPos, startLoc);

    if (canBeArrow && this.shouldParseArrow() && (arrowNode = this.parseArrow(arrowNode))) {
      this.expressionScope.validateAsPattern();
      this.expressionScope.exit();
      this.parseArrowExpression(arrowNode, exprList, false);
      return arrowNode;
    }

    this.expressionScope.exit();

    if (!exprList.length) {
      this.unexpected(this.state.lastTokStart);
    }

    if (optionalCommaStart) this.unexpected(optionalCommaStart);
    if (spreadStart) this.unexpected(spreadStart);
    this.checkExpressionErrors(refExpressionErrors, true);
    this.toReferencedListDeep(exprList, true);

    if (exprList.length > 1) {
      val = this.startNodeAt(innerStartPos, innerStartLoc);
      val.expressions = exprList;
      this.finishNodeAt(val, "SequenceExpression", innerEndPos, innerEndLoc);
    } else {
      val = exprList[0];
    }

    if (!this.options.createParenthesizedExpressions) {
      this.addExtra(val, "parenthesized", true);
      this.addExtra(val, "parenStart", startPos);
      return val;
    }

    const parenExpression = this.startNodeAt(startPos, startLoc);
    parenExpression.expression = val;
    this.finishNode(parenExpression, "ParenthesizedExpression");
    return parenExpression;
  }

  shouldParseArrow() {
    return !this.canInsertSemicolon();
  }

  parseArrow(node) {
    if (this.eat(types$1.arrow)) {
      return node;
    }
  }

  parseParenItem(node, startPos, startLoc) {
    return node;
  }

  parseNewOrNewTarget() {
    const node = this.startNode();
    this.next();

    if (this.match(types$1.dot)) {
      const meta = this.createIdentifier(this.startNodeAtNode(node), "new");
      this.next();
      const metaProp = this.parseMetaProperty(node, meta, "target");

      if (!this.scope.inNonArrowFunction && !this.scope.inClass) {
        this.raise(metaProp.start, ErrorMessages.UnexpectedNewTarget);
      }

      return metaProp;
    }

    return this.parseNew(node);
  }

  parseNew(node) {
    node.callee = this.parseNoCallExpr();

    if (node.callee.type === "Import") {
      this.raise(node.callee.start, ErrorMessages.ImportCallNotNewExpression);
    } else if (this.isOptionalChain(node.callee)) {
      this.raise(this.state.lastTokEnd, ErrorMessages.OptionalChainingNoNew);
    } else if (this.eat(types$1.questionDot)) {
      this.raise(this.state.start, ErrorMessages.OptionalChainingNoNew);
    }

    this.parseNewArguments(node);
    return this.finishNode(node, "NewExpression");
  }

  parseNewArguments(node) {
    if (this.eat(types$1.parenL)) {
      const args = this.parseExprList(types$1.parenR);
      this.toReferencedList(args);
      node.arguments = args;
    } else {
      node.arguments = [];
    }
  }

  parseTemplateElement(isTagged) {
    const elem = this.startNode();

    if (this.state.value === null) {
      if (!isTagged) {
        this.raise(this.state.start + 1, ErrorMessages.InvalidEscapeSequenceTemplate);
      }
    }

    elem.value = {
      raw: this.input.slice(this.state.start, this.state.end).replace(/\r\n?/g, "\n"),
      cooked: this.state.value
    };
    this.next();
    elem.tail = this.match(types$1.backQuote);
    return this.finishNode(elem, "TemplateElement");
  }

  parseTemplate(isTagged) {
    const node = this.startNode();
    this.next();
    node.expressions = [];
    let curElt = this.parseTemplateElement(isTagged);
    node.quasis = [curElt];

    while (!curElt.tail) {
      this.expect(types$1.dollarBraceL);
      node.expressions.push(this.parseTemplateSubstitution());
      this.expect(types$1.braceR);
      node.quasis.push(curElt = this.parseTemplateElement(isTagged));
    }

    this.next();
    return this.finishNode(node, "TemplateLiteral");
  }

  parseTemplateSubstitution() {
    return this.parseExpression();
  }

  parseObjectLike(close, isPattern, isRecord, refExpressionErrors) {
    if (isRecord) {
      this.expectPlugin("recordAndTuple");
    }

    const oldInFSharpPipelineDirectBody = this.state.inFSharpPipelineDirectBody;
    this.state.inFSharpPipelineDirectBody = false;
    const propHash = Object.create(null);
    let first = true;
    const node = this.startNode();
    node.properties = [];
    this.next();

    while (!this.match(close)) {
      if (first) {
        first = false;
      } else {
        this.expect(types$1.comma);

        if (this.match(close)) {
          this.addExtra(node, "trailingComma", this.state.lastTokStart);
          break;
        }
      }

      const prop = this.parsePropertyDefinition(isPattern, refExpressionErrors);

      if (!isPattern) {
        this.checkProto(prop, isRecord, propHash, refExpressionErrors);
      }

      if (isRecord && !this.isObjectProperty(prop) && prop.type !== "SpreadElement") {
        this.raise(prop.start, ErrorMessages.InvalidRecordProperty);
      }

      if (prop.shorthand) {
        this.addExtra(prop, "shorthand", true);
      }

      node.properties.push(prop);
    }

    this.next();
    this.state.inFSharpPipelineDirectBody = oldInFSharpPipelineDirectBody;
    let type = "ObjectExpression";

    if (isPattern) {
      type = "ObjectPattern";
    } else if (isRecord) {
      type = "RecordExpression";
    }

    return this.finishNode(node, type);
  }

  maybeAsyncOrAccessorProp(prop) {
    return !prop.computed && prop.key.type === "Identifier" && (this.isLiteralPropertyName() || this.match(types$1.bracketL) || this.match(types$1.star));
  }

  parsePropertyDefinition(isPattern, refExpressionErrors) {
    let decorators = [];

    if (this.match(types$1.at)) {
      if (this.hasPlugin("decorators")) {
        this.raise(this.state.start, ErrorMessages.UnsupportedPropertyDecorator);
      }

      while (this.match(types$1.at)) {
        decorators.push(this.parseDecorator());
      }
    }

    const prop = this.startNode();
    let isGenerator = false;
    let isAsync = false;
    let isAccessor = false;
    let startPos;
    let startLoc;

    if (this.match(types$1.ellipsis)) {
      if (decorators.length) this.unexpected();

      if (isPattern) {
        this.next();
        prop.argument = this.parseIdentifier();
        this.checkCommaAfterRest(125);
        return this.finishNode(prop, "RestElement");
      }

      return this.parseSpread();
    }

    if (decorators.length) {
      prop.decorators = decorators;
      decorators = [];
    }

    prop.method = false;

    if (isPattern || refExpressionErrors) {
      startPos = this.state.start;
      startLoc = this.state.startLoc;
    }

    if (!isPattern) {
      isGenerator = this.eat(types$1.star);
    }

    const containsEsc = this.state.containsEsc;
    const key = this.parsePropertyName(prop, false);

    if (!isPattern && !isGenerator && !containsEsc && this.maybeAsyncOrAccessorProp(prop)) {
      const keyName = key.name;

      if (keyName === "async" && !this.hasPrecedingLineBreak()) {
        isAsync = true;
        isGenerator = this.eat(types$1.star);
        this.parsePropertyName(prop, false);
      }

      if (keyName === "get" || keyName === "set") {
        isAccessor = true;
        prop.kind = keyName;

        if (this.match(types$1.star)) {
          isGenerator = true;
          this.raise(this.state.pos, ErrorMessages.AccessorIsGenerator, keyName);
          this.next();
        }

        this.parsePropertyName(prop, false);
      }
    }

    this.parseObjPropValue(prop, startPos, startLoc, isGenerator, isAsync, isPattern, isAccessor, refExpressionErrors);
    return prop;
  }

  getGetterSetterExpectedParamCount(method) {
    return method.kind === "get" ? 0 : 1;
  }

  getObjectOrClassMethodParams(method) {
    return method.params;
  }

  checkGetterSetterParams(method) {
    var _params;

    const paramCount = this.getGetterSetterExpectedParamCount(method);
    const params = this.getObjectOrClassMethodParams(method);
    const start = method.start;

    if (params.length !== paramCount) {
      if (method.kind === "get") {
        this.raise(start, ErrorMessages.BadGetterArity);
      } else {
        this.raise(start, ErrorMessages.BadSetterArity);
      }
    }

    if (method.kind === "set" && ((_params = params[params.length - 1]) == null ? void 0 : _params.type) === "RestElement") {
      this.raise(start, ErrorMessages.BadSetterRestParameter);
    }
  }

  parseObjectMethod(prop, isGenerator, isAsync, isPattern, isAccessor) {
    if (isAccessor) {
      this.parseMethod(prop, isGenerator, false, false, false, "ObjectMethod");
      this.checkGetterSetterParams(prop);
      return prop;
    }

    if (isAsync || isGenerator || this.match(types$1.parenL)) {
      if (isPattern) this.unexpected();
      prop.kind = "method";
      prop.method = true;
      return this.parseMethod(prop, isGenerator, isAsync, false, false, "ObjectMethod");
    }
  }

  parseObjectProperty(prop, startPos, startLoc, isPattern, refExpressionErrors) {
    prop.shorthand = false;

    if (this.eat(types$1.colon)) {
      prop.value = isPattern ? this.parseMaybeDefault(this.state.start, this.state.startLoc) : this.parseMaybeAssignAllowIn(refExpressionErrors);
      return this.finishNode(prop, "ObjectProperty");
    }

    if (!prop.computed && prop.key.type === "Identifier") {
      this.checkReservedWord(prop.key.name, prop.key.start, true, false);

      if (isPattern) {
        prop.value = this.parseMaybeDefault(startPos, startLoc, prop.key.__clone());
      } else if (this.match(types$1.eq) && refExpressionErrors) {
        if (refExpressionErrors.shorthandAssign === -1) {
          refExpressionErrors.shorthandAssign = this.state.start;
        }

        prop.value = this.parseMaybeDefault(startPos, startLoc, prop.key.__clone());
      } else {
        prop.value = prop.key.__clone();
      }

      prop.shorthand = true;
      return this.finishNode(prop, "ObjectProperty");
    }
  }

  parseObjPropValue(prop, startPos, startLoc, isGenerator, isAsync, isPattern, isAccessor, refExpressionErrors) {
    const node = this.parseObjectMethod(prop, isGenerator, isAsync, isPattern, isAccessor) || this.parseObjectProperty(prop, startPos, startLoc, isPattern, refExpressionErrors);
    if (!node) this.unexpected();
    return node;
  }

  parsePropertyName(prop, isPrivateNameAllowed) {
    if (this.eat(types$1.bracketL)) {
      prop.computed = true;
      prop.key = this.parseMaybeAssignAllowIn();
      this.expect(types$1.bracketR);
    } else {
      const oldInPropertyName = this.state.inPropertyName;
      this.state.inPropertyName = true;
      const type = this.state.type;
      prop.key = type === types$1.num || type === types$1.string || type === types$1.bigint || type === types$1.decimal ? this.parseExprAtom() : this.parseMaybePrivateName(isPrivateNameAllowed);

      if (type !== types$1.privateName) {
        prop.computed = false;
      }

      this.state.inPropertyName = oldInPropertyName;
    }

    return prop.key;
  }

  initFunction(node, isAsync) {
    node.id = null;
    node.generator = false;
    node.async = !!isAsync;
  }

  parseMethod(node, isGenerator, isAsync, isConstructor, allowDirectSuper, type, inClassScope = false) {
    this.initFunction(node, isAsync);
    node.generator = !!isGenerator;
    const allowModifiers = isConstructor;
    this.scope.enter(SCOPE_FUNCTION | SCOPE_SUPER | (inClassScope ? SCOPE_CLASS : 0) | (allowDirectSuper ? SCOPE_DIRECT_SUPER : 0));
    this.prodParam.enter(functionFlags(isAsync, node.generator));
    this.parseFunctionParams(node, allowModifiers);
    this.parseFunctionBodyAndFinish(node, type, true);
    this.prodParam.exit();
    this.scope.exit();
    return node;
  }

  parseArrayLike(close, canBePattern, isTuple, refExpressionErrors) {
    if (isTuple) {
      this.expectPlugin("recordAndTuple");
    }

    const oldInFSharpPipelineDirectBody = this.state.inFSharpPipelineDirectBody;
    this.state.inFSharpPipelineDirectBody = false;
    const node = this.startNode();
    this.next();
    node.elements = this.parseExprList(close, !isTuple, refExpressionErrors, node);
    this.state.inFSharpPipelineDirectBody = oldInFSharpPipelineDirectBody;
    return this.finishNode(node, isTuple ? "TupleExpression" : "ArrayExpression");
  }

  parseArrowExpression(node, params, isAsync, trailingCommaPos) {
    this.scope.enter(SCOPE_FUNCTION | SCOPE_ARROW);
    let flags = functionFlags(isAsync, false);

    if (!this.match(types$1.bracketL) && this.prodParam.hasIn) {
      flags |= PARAM_IN;
    }

    this.prodParam.enter(flags);
    this.initFunction(node, isAsync);
    const oldMaybeInArrowParameters = this.state.maybeInArrowParameters;

    if (params) {
      this.state.maybeInArrowParameters = true;
      this.setArrowFunctionParameters(node, params, trailingCommaPos);
    }

    this.state.maybeInArrowParameters = false;
    this.parseFunctionBody(node, true);
    this.prodParam.exit();
    this.scope.exit();
    this.state.maybeInArrowParameters = oldMaybeInArrowParameters;
    return this.finishNode(node, "ArrowFunctionExpression");
  }

  setArrowFunctionParameters(node, params, trailingCommaPos) {
    node.params = this.toAssignableList(params, trailingCommaPos, false);
  }

  parseFunctionBodyAndFinish(node, type, isMethod = false) {
    this.parseFunctionBody(node, false, isMethod);
    this.finishNode(node, type);
  }

  parseFunctionBody(node, allowExpression, isMethod = false) {
    const isExpression = allowExpression && !this.match(types$1.braceL);
    this.expressionScope.enter(newExpressionScope());

    if (isExpression) {
      node.body = this.parseMaybeAssign();
      this.checkParams(node, false, allowExpression, false);
    } else {
      const oldStrict = this.state.strict;
      const oldLabels = this.state.labels;
      this.state.labels = [];
      this.prodParam.enter(this.prodParam.currentFlags() | PARAM_RETURN);
      node.body = this.parseBlock(true, false, hasStrictModeDirective => {
        const nonSimple = !this.isSimpleParamList(node.params);

        if (hasStrictModeDirective && nonSimple) {
          const errorPos = (node.kind === "method" || node.kind === "constructor") && !!node.key ? node.key.end : node.start;
          this.raise(errorPos, ErrorMessages.IllegalLanguageModeDirective);
        }

        const strictModeChanged = !oldStrict && this.state.strict;
        this.checkParams(node, !this.state.strict && !allowExpression && !isMethod && !nonSimple, allowExpression, strictModeChanged);

        if (this.state.strict && node.id) {
          this.checkLVal(node.id, "function name", BIND_OUTSIDE, undefined, undefined, strictModeChanged);
        }
      });
      this.prodParam.exit();
      this.expressionScope.exit();
      this.state.labels = oldLabels;
    }
  }

  isSimpleParamList(params) {
    for (let i = 0, len = params.length; i < len; i++) {
      if (params[i].type !== "Identifier") return false;
    }

    return true;
  }

  checkParams(node, allowDuplicates, isArrowFunction, strictModeChanged = true) {
    const checkClashes = new Set();

    for (const param of node.params) {
      this.checkLVal(param, "function parameter list", BIND_VAR, allowDuplicates ? null : checkClashes, undefined, strictModeChanged);
    }
  }

  parseExprList(close, allowEmpty, refExpressionErrors, nodeForExtra) {
    const elts = [];
    let first = true;

    while (!this.eat(close)) {
      if (first) {
        first = false;
      } else {
        this.expect(types$1.comma);

        if (this.match(close)) {
          if (nodeForExtra) {
            this.addExtra(nodeForExtra, "trailingComma", this.state.lastTokStart);
          }

          this.next();
          break;
        }
      }

      elts.push(this.parseExprListItem(allowEmpty, refExpressionErrors));
    }

    return elts;
  }

  parseExprListItem(allowEmpty, refExpressionErrors, allowPlaceholder) {
    let elt;

    if (this.match(types$1.comma)) {
      if (!allowEmpty) {
        this.raise(this.state.pos, ErrorMessages.UnexpectedToken, ",");
      }

      elt = null;
    } else if (this.match(types$1.ellipsis)) {
      const spreadNodeStartPos = this.state.start;
      const spreadNodeStartLoc = this.state.startLoc;
      elt = this.parseParenItem(this.parseSpread(refExpressionErrors), spreadNodeStartPos, spreadNodeStartLoc);
    } else if (this.match(types$1.question)) {
      this.expectPlugin("partialApplication");

      if (!allowPlaceholder) {
        this.raise(this.state.start, ErrorMessages.UnexpectedArgumentPlaceholder);
      }

      const node = this.startNode();
      this.next();
      elt = this.finishNode(node, "ArgumentPlaceholder");
    } else {
      elt = this.parseMaybeAssignAllowIn(refExpressionErrors, this.parseParenItem);
    }

    return elt;
  }

  parseIdentifier(liberal) {
    const node = this.startNode();
    const name = this.parseIdentifierName(node.start, liberal);
    return this.createIdentifier(node, name);
  }

  createIdentifier(node, name) {
    node.name = name;
    node.loc.identifierName = name;
    return this.finishNode(node, "Identifier");
  }

  parseIdentifierName(pos, liberal) {
    let name;
    const {
      start,
      type
    } = this.state;

    if (type === types$1.name) {
      name = this.state.value;
    } else if (type.keyword) {
      name = type.keyword;
    } else {
      throw this.unexpected();
    }

    if (liberal) {
      this.state.type = types$1.name;
    } else {
      this.checkReservedWord(name, start, !!type.keyword, false);
    }

    this.next();
    return name;
  }

  checkReservedWord(word, startLoc, checkKeywords, isBinding) {
    if (word.length > 10) {
      return;
    }

    if (!canBeReservedWord(word)) {
      return;
    }

    if (word === "yield") {
      if (this.prodParam.hasYield) {
        this.raise(startLoc, ErrorMessages.YieldBindingIdentifier);
        return;
      }
    } else if (word === "await") {
      if (this.prodParam.hasAwait) {
        this.raise(startLoc, ErrorMessages.AwaitBindingIdentifier);
        return;
      } else if (this.scope.inStaticBlock && !this.scope.inNonArrowFunction) {
        this.raise(startLoc, ErrorMessages.AwaitBindingIdentifierInStaticBlock);
        return;
      } else {
        this.expressionScope.recordAsyncArrowParametersError(startLoc, ErrorMessages.AwaitBindingIdentifier);
      }
    } else if (word === "arguments") {
      if (this.scope.inClassAndNotInNonArrowFunction) {
        this.raise(startLoc, ErrorMessages.ArgumentsInClass);
        return;
      }
    }

    if (checkKeywords && isKeyword(word)) {
      this.raise(startLoc, ErrorMessages.UnexpectedKeyword, word);
      return;
    }

    const reservedTest = !this.state.strict ? isReservedWord : isBinding ? isStrictBindReservedWord : isStrictReservedWord;

    if (reservedTest(word, this.inModule)) {
      this.raise(startLoc, ErrorMessages.UnexpectedReservedWord, word);
    }
  }

  isAwaitAllowed() {
    if (this.prodParam.hasAwait) return true;

    if (this.options.allowAwaitOutsideFunction && !this.scope.inFunction) {
      return true;
    }

    return false;
  }

  parseAwait(startPos, startLoc) {
    const node = this.startNodeAt(startPos, startLoc);
    this.expressionScope.recordParameterInitializerError(node.start, ErrorMessages.AwaitExpressionFormalParameter);

    if (this.eat(types$1.star)) {
      this.raise(node.start, ErrorMessages.ObsoleteAwaitStar);
    }

    if (!this.scope.inFunction && !this.options.allowAwaitOutsideFunction) {
      if (this.isAmbiguousAwait()) {
        this.ambiguousScriptDifferentAst = true;
      } else {
        this.sawUnambiguousESM = true;
      }
    }

    if (!this.state.soloAwait) {
      node.argument = this.parseMaybeUnary(null, true);
    }

    return this.finishNode(node, "AwaitExpression");
  }

  isAmbiguousAwait() {
    return this.hasPrecedingLineBreak() || this.match(types$1.plusMin) || this.match(types$1.parenL) || this.match(types$1.bracketL) || this.match(types$1.backQuote) || this.match(types$1.regexp) || this.match(types$1.slash) || this.hasPlugin("v8intrinsic") && this.match(types$1.modulo);
  }

  parseYield() {
    const node = this.startNode();
    this.expressionScope.recordParameterInitializerError(node.start, ErrorMessages.YieldInParameter);
    this.next();
    let delegating = false;
    let argument = null;

    if (!this.hasPrecedingLineBreak()) {
      delegating = this.eat(types$1.star);

      switch (this.state.type) {
        case types$1.semi:
        case types$1.eof:
        case types$1.braceR:
        case types$1.parenR:
        case types$1.bracketR:
        case types$1.braceBarR:
        case types$1.colon:
        case types$1.comma:
          if (!delegating) break;

        default:
          argument = this.parseMaybeAssign();
      }
    }

    node.delegate = delegating;
    node.argument = argument;
    return this.finishNode(node, "YieldExpression");
  }

  checkPipelineAtInfixOperator(left, leftStartPos) {
    if (this.getPluginOption("pipelineOperator", "proposal") === "smart") {
      if (left.type === "SequenceExpression") {
        this.raise(leftStartPos, ErrorMessages.PipelineHeadSequenceExpression);
      }
    }
  }

  parseSmartPipelineBody(childExpression, startPos, startLoc) {
    this.checkSmartPipelineBodyEarlyErrors(childExpression, startPos);
    return this.parseSmartPipelineBodyInStyle(childExpression, startPos, startLoc);
  }

  checkSmartPipelineBodyEarlyErrors(childExpression, startPos) {
    if (this.match(types$1.arrow)) {
      throw this.raise(this.state.start, ErrorMessages.PipelineBodyNoArrow);
    } else if (childExpression.type === "SequenceExpression") {
      this.raise(startPos, ErrorMessages.PipelineBodySequenceExpression);
    }
  }

  parseSmartPipelineBodyInStyle(childExpression, startPos, startLoc) {
    const bodyNode = this.startNodeAt(startPos, startLoc);
    const isSimpleReference = this.isSimpleReference(childExpression);

    if (isSimpleReference) {
      bodyNode.callee = childExpression;
    } else {
      if (!this.topicReferenceWasUsedInCurrentTopicContext()) {
        this.raise(startPos, ErrorMessages.PipelineTopicUnused);
      }

      bodyNode.expression = childExpression;
    }

    return this.finishNode(bodyNode, isSimpleReference ? "PipelineBareFunction" : "PipelineTopicExpression");
  }

  isSimpleReference(expression) {
    switch (expression.type) {
      case "MemberExpression":
        return !expression.computed && this.isSimpleReference(expression.object);

      case "Identifier":
        return true;

      default:
        return false;
    }
  }

  withTopicPermittingContext(callback) {
    const outerContextTopicState = this.state.topicContext;
    this.state.topicContext = {
      maxNumOfResolvableTopics: 1,
      maxTopicIndex: null
    };

    try {
      return callback();
    } finally {
      this.state.topicContext = outerContextTopicState;
    }
  }

  withTopicForbiddingContext(callback) {
    const outerContextTopicState = this.state.topicContext;
    this.state.topicContext = {
      maxNumOfResolvableTopics: 0,
      maxTopicIndex: null
    };

    try {
      return callback();
    } finally {
      this.state.topicContext = outerContextTopicState;
    }
  }

  withSoloAwaitPermittingContext(callback) {
    const outerContextSoloAwaitState = this.state.soloAwait;
    this.state.soloAwait = true;

    try {
      return callback();
    } finally {
      this.state.soloAwait = outerContextSoloAwaitState;
    }
  }

  allowInAnd(callback) {
    const flags = this.prodParam.currentFlags();
    const prodParamToSet = PARAM_IN & ~flags;

    if (prodParamToSet) {
      this.prodParam.enter(flags | PARAM_IN);

      try {
        return callback();
      } finally {
        this.prodParam.exit();
      }
    }

    return callback();
  }

  disallowInAnd(callback) {
    const flags = this.prodParam.currentFlags();
    const prodParamToClear = PARAM_IN & flags;

    if (prodParamToClear) {
      this.prodParam.enter(flags & ~PARAM_IN);

      try {
        return callback();
      } finally {
        this.prodParam.exit();
      }
    }

    return callback();
  }

  registerTopicReference() {
    this.state.topicContext.maxTopicIndex = 0;
  }

  primaryTopicReferenceIsAllowedInCurrentTopicContext() {
    return this.state.topicContext.maxNumOfResolvableTopics >= 1;
  }

  topicReferenceWasUsedInCurrentTopicContext() {
    return this.state.topicContext.maxTopicIndex != null && this.state.topicContext.maxTopicIndex >= 0;
  }

  parseFSharpPipelineBody(prec) {
    const startPos = this.state.start;
    const startLoc = this.state.startLoc;
    this.state.potentialArrowAt = this.state.start;
    const oldInFSharpPipelineDirectBody = this.state.inFSharpPipelineDirectBody;
    this.state.inFSharpPipelineDirectBody = true;
    const ret = this.parseExprOp(this.parseMaybeUnary(), startPos, startLoc, prec);
    this.state.inFSharpPipelineDirectBody = oldInFSharpPipelineDirectBody;
    return ret;
  }

  parseModuleExpression() {
    this.expectPlugin("moduleBlocks");
    const node = this.startNode();
    this.next();
    this.eat(types$1.braceL);
    const revertScopes = this.initializeScopes(true);
    this.enterInitialScopes();
    const program = this.startNode();

    try {
      node.body = this.parseProgram(program, types$1.braceR, "module");
    } finally {
      revertScopes();
    }

    this.eat(types$1.braceR);
    return this.finishNode(node, "ModuleExpression");
  }

}

const loopLabel = {
  kind: "loop"
},
      switchLabel = {
  kind: "switch"
};
const FUNC_NO_FLAGS = 0b000,
      FUNC_STATEMENT = 0b001,
      FUNC_HANGING_STATEMENT = 0b010,
      FUNC_NULLABLE_ID = 0b100;
const loneSurrogate = /[\uD800-\uDFFF]/u;
const keywordRelationalOperator = /in(?:stanceof)?/y;

function babel7CompatTokens(tokens) {
  {
    for (let i = 0; i < tokens.length; i++) {
      const token = tokens[i];

      if (token.type === types$1.privateName) {
        const {
          loc,
          start,
          value,
          end
        } = token;
        const hashEndPos = start + 1;
        const hashEndLoc = new Position(loc.start.line, loc.start.column + 1);
        tokens.splice(i, 1, new Token({
          type: types$1.hash,
          value: "#",
          start: start,
          end: hashEndPos,
          startLoc: loc.start,
          endLoc: hashEndLoc
        }), new Token({
          type: types$1.name,
          value: value,
          start: hashEndPos,
          end: end,
          startLoc: hashEndLoc,
          endLoc: loc.end
        }));
      }
    }
  }
  return tokens;
}

class StatementParser extends ExpressionParser {
  parseTopLevel(file, program) {
    file.program = this.parseProgram(program);
    file.comments = this.state.comments;
    if (this.options.tokens) file.tokens = babel7CompatTokens(this.tokens);
    return this.finishNode(file, "File");
  }

  parseProgram(program, end = types$1.eof, sourceType = this.options.sourceType) {
    program.sourceType = sourceType;
    program.interpreter = this.parseInterpreterDirective();
    this.parseBlockBody(program, true, true, end);

    if (this.inModule && !this.options.allowUndeclaredExports && this.scope.undefinedExports.size > 0) {
      for (const [name] of Array.from(this.scope.undefinedExports)) {
        const pos = this.scope.undefinedExports.get(name);
        this.raise(pos, ErrorMessages.ModuleExportUndefined, name);
      }
    }

    return this.finishNode(program, "Program");
  }

  stmtToDirective(stmt) {
    const expr = stmt.expression;
    const directiveLiteral = this.startNodeAt(expr.start, expr.loc.start);
    const directive = this.startNodeAt(stmt.start, stmt.loc.start);
    const raw = this.input.slice(expr.start, expr.end);
    const val = directiveLiteral.value = raw.slice(1, -1);
    this.addExtra(directiveLiteral, "raw", raw);
    this.addExtra(directiveLiteral, "rawValue", val);
    directive.value = this.finishNodeAt(directiveLiteral, "DirectiveLiteral", expr.end, expr.loc.end);
    return this.finishNodeAt(directive, "Directive", stmt.end, stmt.loc.end);
  }

  parseInterpreterDirective() {
    if (!this.match(types$1.interpreterDirective)) {
      return null;
    }

    const node = this.startNode();
    node.value = this.state.value;
    this.next();
    return this.finishNode(node, "InterpreterDirective");
  }

  isLet(context) {
    if (!this.isContextual("let")) {
      return false;
    }

    return this.isLetKeyword(context);
  }

  isLetKeyword(context) {
    const next = this.nextTokenStart();
    const nextCh = this.codePointAtPos(next);

    if (nextCh === 92 || nextCh === 91) {
      return true;
    }

    if (context) return false;
    if (nextCh === 123) return true;

    if (isIdentifierStart(nextCh)) {
      keywordRelationalOperator.lastIndex = next;
      const matched = keywordRelationalOperator.exec(this.input);

      if (matched !== null) {
        const endCh = this.codePointAtPos(next + matched[0].length);

        if (!isIdentifierChar(endCh) && endCh !== 92) {
          return false;
        }
      }

      return true;
    }

    return false;
  }

  parseStatement(context, topLevel) {
    if (this.match(types$1.at)) {
      this.parseDecorators(true);
    }

    return this.parseStatementContent(context, topLevel);
  }

  parseStatementContent(context, topLevel) {
    let starttype = this.state.type;
    const node = this.startNode();
    let kind;

    if (this.isLet(context)) {
      starttype = types$1._var;
      kind = "let";
    }

    switch (starttype) {
      case types$1._break:
      case types$1._continue:
        return this.parseBreakContinueStatement(node, starttype.keyword);

      case types$1._debugger:
        return this.parseDebuggerStatement(node);

      case types$1._do:
        return this.parseDoStatement(node);

      case types$1._for:
        return this.parseForStatement(node);

      case types$1._function:
        if (this.lookaheadCharCode() === 46) break;

        if (context) {
          if (this.state.strict) {
            this.raise(this.state.start, ErrorMessages.StrictFunction);
          } else if (context !== "if" && context !== "label") {
            this.raise(this.state.start, ErrorMessages.SloppyFunction);
          }
        }

        return this.parseFunctionStatement(node, false, !context);

      case types$1._class:
        if (context) this.unexpected();
        return this.parseClass(node, true);

      case types$1._if:
        return this.parseIfStatement(node);

      case types$1._return:
        return this.parseReturnStatement(node);

      case types$1._switch:
        return this.parseSwitchStatement(node);

      case types$1._throw:
        return this.parseThrowStatement(node);

      case types$1._try:
        return this.parseTryStatement(node);

      case types$1._const:
      case types$1._var:
        kind = kind || this.state.value;

        if (context && kind !== "var") {
          this.raise(this.state.start, ErrorMessages.UnexpectedLexicalDeclaration);
        }

        return this.parseVarStatement(node, kind);

      case types$1._while:
        return this.parseWhileStatement(node);

      case types$1._with:
        return this.parseWithStatement(node);

      case types$1.braceL:
        return this.parseBlock();

      case types$1.semi:
        return this.parseEmptyStatement(node);

      case types$1._import:
        {
          const nextTokenCharCode = this.lookaheadCharCode();

          if (nextTokenCharCode === 40 || nextTokenCharCode === 46) {
              break;
            }
        }

      case types$1._export:
        {
          if (!this.options.allowImportExportEverywhere && !topLevel) {
            this.raise(this.state.start, ErrorMessages.UnexpectedImportExport);
          }

          this.next();
          let result;

          if (starttype === types$1._import) {
            result = this.parseImport(node);

            if (result.type === "ImportDeclaration" && (!result.importKind || result.importKind === "value")) {
              this.sawUnambiguousESM = true;
            }
          } else {
            result = this.parseExport(node);

            if (result.type === "ExportNamedDeclaration" && (!result.exportKind || result.exportKind === "value") || result.type === "ExportAllDeclaration" && (!result.exportKind || result.exportKind === "value") || result.type === "ExportDefaultDeclaration") {
              this.sawUnambiguousESM = true;
            }
          }

          this.assertModuleNodeAllowed(node);
          return result;
        }

      default:
        {
          if (this.isAsyncFunction()) {
            if (context) {
              this.raise(this.state.start, ErrorMessages.AsyncFunctionInSingleStatementContext);
            }

            this.next();
            return this.parseFunctionStatement(node, true, !context);
          }
        }
    }

    const maybeName = this.state.value;
    const expr = this.parseExpression();

    if (starttype === types$1.name && expr.type === "Identifier" && this.eat(types$1.colon)) {
      return this.parseLabeledStatement(node, maybeName, expr, context);
    } else {
      return this.parseExpressionStatement(node, expr);
    }
  }

  assertModuleNodeAllowed(node) {
    if (!this.options.allowImportExportEverywhere && !this.inModule) {
      this.raise(node.start, SourceTypeModuleErrorMessages.ImportOutsideModule);
    }
  }

  takeDecorators(node) {
    const decorators = this.state.decoratorStack[this.state.decoratorStack.length - 1];

    if (decorators.length) {
      node.decorators = decorators;
      this.resetStartLocationFromNode(node, decorators[0]);
      this.state.decoratorStack[this.state.decoratorStack.length - 1] = [];
    }
  }

  canHaveLeadingDecorator() {
    return this.match(types$1._class);
  }

  parseDecorators(allowExport) {
    const currentContextDecorators = this.state.decoratorStack[this.state.decoratorStack.length - 1];

    while (this.match(types$1.at)) {
      const decorator = this.parseDecorator();
      currentContextDecorators.push(decorator);
    }

    if (this.match(types$1._export)) {
      if (!allowExport) {
        this.unexpected();
      }

      if (this.hasPlugin("decorators") && !this.getPluginOption("decorators", "decoratorsBeforeExport")) {
        this.raise(this.state.start, ErrorMessages.DecoratorExportClass);
      }
    } else if (!this.canHaveLeadingDecorator()) {
      throw this.raise(this.state.start, ErrorMessages.UnexpectedLeadingDecorator);
    }
  }

  parseDecorator() {
    this.expectOnePlugin(["decorators-legacy", "decorators"]);
    const node = this.startNode();
    this.next();

    if (this.hasPlugin("decorators")) {
      this.state.decoratorStack.push([]);
      const startPos = this.state.start;
      const startLoc = this.state.startLoc;
      let expr;

      if (this.eat(types$1.parenL)) {
        expr = this.parseExpression();
        this.expect(types$1.parenR);
      } else {
        expr = this.parseIdentifier(false);

        while (this.eat(types$1.dot)) {
          const node = this.startNodeAt(startPos, startLoc);
          node.object = expr;
          node.property = this.parseIdentifier(true);
          node.computed = false;
          expr = this.finishNode(node, "MemberExpression");
        }
      }

      node.expression = this.parseMaybeDecoratorArguments(expr);
      this.state.decoratorStack.pop();
    } else {
      node.expression = this.parseExprSubscripts();
    }

    return this.finishNode(node, "Decorator");
  }

  parseMaybeDecoratorArguments(expr) {
    if (this.eat(types$1.parenL)) {
      const node = this.startNodeAtNode(expr);
      node.callee = expr;
      node.arguments = this.parseCallExpressionArguments(types$1.parenR, false);
      this.toReferencedList(node.arguments);
      return this.finishNode(node, "CallExpression");
    }

    return expr;
  }

  parseBreakContinueStatement(node, keyword) {
    const isBreak = keyword === "break";
    this.next();

    if (this.isLineTerminator()) {
      node.label = null;
    } else {
      node.label = this.parseIdentifier();
      this.semicolon();
    }

    this.verifyBreakContinue(node, keyword);
    return this.finishNode(node, isBreak ? "BreakStatement" : "ContinueStatement");
  }

  verifyBreakContinue(node, keyword) {
    const isBreak = keyword === "break";
    let i;

    for (i = 0; i < this.state.labels.length; ++i) {
      const lab = this.state.labels[i];

      if (node.label == null || lab.name === node.label.name) {
        if (lab.kind != null && (isBreak || lab.kind === "loop")) break;
        if (node.label && isBreak) break;
      }
    }

    if (i === this.state.labels.length) {
      this.raise(node.start, ErrorMessages.IllegalBreakContinue, keyword);
    }
  }

  parseDebuggerStatement(node) {
    this.next();
    this.semicolon();
    return this.finishNode(node, "DebuggerStatement");
  }

  parseHeaderExpression() {
    this.expect(types$1.parenL);
    const val = this.parseExpression();
    this.expect(types$1.parenR);
    return val;
  }

  parseDoStatement(node) {
    this.next();
    this.state.labels.push(loopLabel);
    node.body = this.withTopicForbiddingContext(() => this.parseStatement("do"));
    this.state.labels.pop();
    this.expect(types$1._while);
    node.test = this.parseHeaderExpression();
    this.eat(types$1.semi);
    return this.finishNode(node, "DoWhileStatement");
  }

  parseForStatement(node) {
    this.next();
    this.state.labels.push(loopLabel);
    let awaitAt = -1;

    if (this.isAwaitAllowed() && this.eatContextual("await")) {
      awaitAt = this.state.lastTokStart;
    }

    this.scope.enter(SCOPE_OTHER);
    this.expect(types$1.parenL);

    if (this.match(types$1.semi)) {
      if (awaitAt > -1) {
        this.unexpected(awaitAt);
      }

      return this.parseFor(node, null);
    }

    const startsWithLet = this.isContextual("let");
    const isLet = startsWithLet && this.isLetKeyword();

    if (this.match(types$1._var) || this.match(types$1._const) || isLet) {
      const init = this.startNode();
      const kind = isLet ? "let" : this.state.value;
      this.next();
      this.parseVar(init, true, kind);
      this.finishNode(init, "VariableDeclaration");

      if ((this.match(types$1._in) || this.isContextual("of")) && init.declarations.length === 1) {
        return this.parseForIn(node, init, awaitAt);
      }

      if (awaitAt > -1) {
        this.unexpected(awaitAt);
      }

      return this.parseFor(node, init);
    }

    const startsWithUnescapedName = this.match(types$1.name) && !this.state.containsEsc;
    const refExpressionErrors = new ExpressionErrors();
    const init = this.parseExpression(true, refExpressionErrors);
    const isForOf = this.isContextual("of");

    if (isForOf) {
      if (startsWithLet) {
        this.raise(init.start, ErrorMessages.ForOfLet);
      } else if (awaitAt === -1 && startsWithUnescapedName && init.type === "Identifier" && init.name === "async") {
        this.raise(init.start, ErrorMessages.ForOfAsync);
      }
    }

    if (isForOf || this.match(types$1._in)) {
      this.toAssignable(init, true);
      const description = isForOf ? "for-of statement" : "for-in statement";
      this.checkLVal(init, description);
      return this.parseForIn(node, init, awaitAt);
    } else {
      this.checkExpressionErrors(refExpressionErrors, true);
    }

    if (awaitAt > -1) {
      this.unexpected(awaitAt);
    }

    return this.parseFor(node, init);
  }

  parseFunctionStatement(node, isAsync, declarationPosition) {
    this.next();
    return this.parseFunction(node, FUNC_STATEMENT | (declarationPosition ? 0 : FUNC_HANGING_STATEMENT), isAsync);
  }

  parseIfStatement(node) {
    this.next();
    node.test = this.parseHeaderExpression();
    node.consequent = this.parseStatement("if");
    node.alternate = this.eat(types$1._else) ? this.parseStatement("if") : null;
    return this.finishNode(node, "IfStatement");
  }

  parseReturnStatement(node) {
    if (!this.prodParam.hasReturn && !this.options.allowReturnOutsideFunction) {
      this.raise(this.state.start, ErrorMessages.IllegalReturn);
    }

    this.next();

    if (this.isLineTerminator()) {
      node.argument = null;
    } else {
      node.argument = this.parseExpression();
      this.semicolon();
    }

    return this.finishNode(node, "ReturnStatement");
  }

  parseSwitchStatement(node) {
    this.next();
    node.discriminant = this.parseHeaderExpression();
    const cases = node.cases = [];
    this.expect(types$1.braceL);
    this.state.labels.push(switchLabel);
    this.scope.enter(SCOPE_OTHER);
    let cur;

    for (let sawDefault; !this.match(types$1.braceR);) {
      if (this.match(types$1._case) || this.match(types$1._default)) {
        const isCase = this.match(types$1._case);
        if (cur) this.finishNode(cur, "SwitchCase");
        cases.push(cur = this.startNode());
        cur.consequent = [];
        this.next();

        if (isCase) {
          cur.test = this.parseExpression();
        } else {
          if (sawDefault) {
            this.raise(this.state.lastTokStart, ErrorMessages.MultipleDefaultsInSwitch);
          }

          sawDefault = true;
          cur.test = null;
        }

        this.expect(types$1.colon);
      } else {
        if (cur) {
          cur.consequent.push(this.parseStatement(null));
        } else {
          this.unexpected();
        }
      }
    }

    this.scope.exit();
    if (cur) this.finishNode(cur, "SwitchCase");
    this.next();
    this.state.labels.pop();
    return this.finishNode(node, "SwitchStatement");
  }

  parseThrowStatement(node) {
    this.next();

    if (this.hasPrecedingLineBreak()) {
      this.raise(this.state.lastTokEnd, ErrorMessages.NewlineAfterThrow);
    }

    node.argument = this.parseExpression();
    this.semicolon();
    return this.finishNode(node, "ThrowStatement");
  }

  parseCatchClauseParam() {
    const param = this.parseBindingAtom();
    const simple = param.type === "Identifier";
    this.scope.enter(simple ? SCOPE_SIMPLE_CATCH : 0);
    this.checkLVal(param, "catch clause", BIND_LEXICAL);
    return param;
  }

  parseTryStatement(node) {
    this.next();
    node.block = this.parseBlock();
    node.handler = null;

    if (this.match(types$1._catch)) {
      const clause = this.startNode();
      this.next();

      if (this.match(types$1.parenL)) {
        this.expect(types$1.parenL);
        clause.param = this.parseCatchClauseParam();
        this.expect(types$1.parenR);
      } else {
        clause.param = null;
        this.scope.enter(SCOPE_OTHER);
      }

      clause.body = this.withTopicForbiddingContext(() => this.parseBlock(false, false));
      this.scope.exit();
      node.handler = this.finishNode(clause, "CatchClause");
    }

    node.finalizer = this.eat(types$1._finally) ? this.parseBlock() : null;

    if (!node.handler && !node.finalizer) {
      this.raise(node.start, ErrorMessages.NoCatchOrFinally);
    }

    return this.finishNode(node, "TryStatement");
  }

  parseVarStatement(node, kind) {
    this.next();
    this.parseVar(node, false, kind);
    this.semicolon();
    return this.finishNode(node, "VariableDeclaration");
  }

  parseWhileStatement(node) {
    this.next();
    node.test = this.parseHeaderExpression();
    this.state.labels.push(loopLabel);
    node.body = this.withTopicForbiddingContext(() => this.parseStatement("while"));
    this.state.labels.pop();
    return this.finishNode(node, "WhileStatement");
  }

  parseWithStatement(node) {
    if (this.state.strict) {
      this.raise(this.state.start, ErrorMessages.StrictWith);
    }

    this.next();
    node.object = this.parseHeaderExpression();
    node.body = this.withTopicForbiddingContext(() => this.parseStatement("with"));
    return this.finishNode(node, "WithStatement");
  }

  parseEmptyStatement(node) {
    this.next();
    return this.finishNode(node, "EmptyStatement");
  }

  parseLabeledStatement(node, maybeName, expr, context) {
    for (const label of this.state.labels) {
      if (label.name === maybeName) {
        this.raise(expr.start, ErrorMessages.LabelRedeclaration, maybeName);
      }
    }

    const kind = this.state.type.isLoop ? "loop" : this.match(types$1._switch) ? "switch" : null;

    for (let i = this.state.labels.length - 1; i >= 0; i--) {
      const label = this.state.labels[i];

      if (label.statementStart === node.start) {
        label.statementStart = this.state.start;
        label.kind = kind;
      } else {
        break;
      }
    }

    this.state.labels.push({
      name: maybeName,
      kind: kind,
      statementStart: this.state.start
    });
    node.body = this.parseStatement(context ? context.indexOf("label") === -1 ? context + "label" : context : "label");
    this.state.labels.pop();
    node.label = expr;
    return this.finishNode(node, "LabeledStatement");
  }

  parseExpressionStatement(node, expr) {
    node.expression = expr;
    this.semicolon();
    return this.finishNode(node, "ExpressionStatement");
  }

  parseBlock(allowDirectives = false, createNewLexicalScope = true, afterBlockParse) {
    const node = this.startNode();

    if (allowDirectives) {
      this.state.strictErrors.clear();
    }

    this.expect(types$1.braceL);

    if (createNewLexicalScope) {
      this.scope.enter(SCOPE_OTHER);
    }

    this.parseBlockBody(node, allowDirectives, false, types$1.braceR, afterBlockParse);

    if (createNewLexicalScope) {
      this.scope.exit();
    }

    return this.finishNode(node, "BlockStatement");
  }

  isValidDirective(stmt) {
    return stmt.type === "ExpressionStatement" && stmt.expression.type === "StringLiteral" && !stmt.expression.extra.parenthesized;
  }

  parseBlockBody(node, allowDirectives, topLevel, end, afterBlockParse) {
    const body = node.body = [];
    const directives = node.directives = [];
    this.parseBlockOrModuleBlockBody(body, allowDirectives ? directives : undefined, topLevel, end, afterBlockParse);
  }

  parseBlockOrModuleBlockBody(body, directives, topLevel, end, afterBlockParse) {
    const oldStrict = this.state.strict;
    let hasStrictModeDirective = false;
    let parsedNonDirective = false;

    while (!this.match(end)) {
      const stmt = this.parseStatement(null, topLevel);

      if (directives && !parsedNonDirective) {
        if (this.isValidDirective(stmt)) {
          const directive = this.stmtToDirective(stmt);
          directives.push(directive);

          if (!hasStrictModeDirective && directive.value.value === "use strict") {
            hasStrictModeDirective = true;
            this.setStrict(true);
          }

          continue;
        }

        parsedNonDirective = true;
        this.state.strictErrors.clear();
      }

      body.push(stmt);
    }

    if (afterBlockParse) {
      afterBlockParse.call(this, hasStrictModeDirective);
    }

    if (!oldStrict) {
      this.setStrict(false);
    }

    this.next();
  }

  parseFor(node, init) {
    node.init = init;
    this.semicolon(false);
    node.test = this.match(types$1.semi) ? null : this.parseExpression();
    this.semicolon(false);
    node.update = this.match(types$1.parenR) ? null : this.parseExpression();
    this.expect(types$1.parenR);
    node.body = this.withTopicForbiddingContext(() => this.parseStatement("for"));
    this.scope.exit();
    this.state.labels.pop();
    return this.finishNode(node, "ForStatement");
  }

  parseForIn(node, init, awaitAt) {
    const isForIn = this.match(types$1._in);
    this.next();

    if (isForIn) {
      if (awaitAt > -1) this.unexpected(awaitAt);
    } else {
      node.await = awaitAt > -1;
    }

    if (init.type === "VariableDeclaration" && init.declarations[0].init != null && (!isForIn || this.state.strict || init.kind !== "var" || init.declarations[0].id.type !== "Identifier")) {
      this.raise(init.start, ErrorMessages.ForInOfLoopInitializer, isForIn ? "for-in" : "for-of");
    } else if (init.type === "AssignmentPattern") {
      this.raise(init.start, ErrorMessages.InvalidLhs, "for-loop");
    }

    node.left = init;
    node.right = isForIn ? this.parseExpression() : this.parseMaybeAssignAllowIn();
    this.expect(types$1.parenR);
    node.body = this.withTopicForbiddingContext(() => this.parseStatement("for"));
    this.scope.exit();
    this.state.labels.pop();
    return this.finishNode(node, isForIn ? "ForInStatement" : "ForOfStatement");
  }

  parseVar(node, isFor, kind) {
    const declarations = node.declarations = [];
    const isTypescript = this.hasPlugin("typescript");
    node.kind = kind;

    for (;;) {
      const decl = this.startNode();
      this.parseVarId(decl, kind);

      if (this.eat(types$1.eq)) {
        decl.init = isFor ? this.parseMaybeAssignDisallowIn() : this.parseMaybeAssignAllowIn();
      } else {
        if (kind === "const" && !(this.match(types$1._in) || this.isContextual("of"))) {
          if (!isTypescript) {
            this.raise(this.state.lastTokEnd, ErrorMessages.DeclarationMissingInitializer, "Const declarations");
          }
        } else if (decl.id.type !== "Identifier" && !(isFor && (this.match(types$1._in) || this.isContextual("of")))) {
          this.raise(this.state.lastTokEnd, ErrorMessages.DeclarationMissingInitializer, "Complex binding patterns");
        }

        decl.init = null;
      }

      declarations.push(this.finishNode(decl, "VariableDeclarator"));
      if (!this.eat(types$1.comma)) break;
    }

    return node;
  }

  parseVarId(decl, kind) {
    decl.id = this.parseBindingAtom();
    this.checkLVal(decl.id, "variable declaration", kind === "var" ? BIND_VAR : BIND_LEXICAL, undefined, kind !== "var");
  }

  parseFunction(node, statement = FUNC_NO_FLAGS, isAsync = false) {
    const isStatement = statement & FUNC_STATEMENT;
    const isHangingStatement = statement & FUNC_HANGING_STATEMENT;
    const requireId = !!isStatement && !(statement & FUNC_NULLABLE_ID);
    this.initFunction(node, isAsync);

    if (this.match(types$1.star) && isHangingStatement) {
      this.raise(this.state.start, ErrorMessages.GeneratorInSingleStatementContext);
    }

    node.generator = this.eat(types$1.star);

    if (isStatement) {
      node.id = this.parseFunctionId(requireId);
    }

    const oldMaybeInArrowParameters = this.state.maybeInArrowParameters;
    this.state.maybeInArrowParameters = false;
    this.scope.enter(SCOPE_FUNCTION);
    this.prodParam.enter(functionFlags(isAsync, node.generator));

    if (!isStatement) {
      node.id = this.parseFunctionId();
    }

    this.parseFunctionParams(node, false);
    this.withTopicForbiddingContext(() => {
      this.parseFunctionBodyAndFinish(node, isStatement ? "FunctionDeclaration" : "FunctionExpression");
    });
    this.prodParam.exit();
    this.scope.exit();

    if (isStatement && !isHangingStatement) {
      this.registerFunctionStatementId(node);
    }

    this.state.maybeInArrowParameters = oldMaybeInArrowParameters;
    return node;
  }

  parseFunctionId(requireId) {
    return requireId || this.match(types$1.name) ? this.parseIdentifier() : null;
  }

  parseFunctionParams(node, allowModifiers) {
    this.expect(types$1.parenL);
    this.expressionScope.enter(newParameterDeclarationScope());
    node.params = this.parseBindingList(types$1.parenR, 41, false, allowModifiers);
    this.expressionScope.exit();
  }

  registerFunctionStatementId(node) {
    if (!node.id) return;
    this.scope.declareName(node.id.name, this.state.strict || node.generator || node.async ? this.scope.treatFunctionsAsVar ? BIND_VAR : BIND_LEXICAL : BIND_FUNCTION, node.id.start);
  }

  parseClass(node, isStatement, optionalId) {
    this.next();
    this.takeDecorators(node);
    const oldStrict = this.state.strict;
    this.state.strict = true;
    this.parseClassId(node, isStatement, optionalId);
    this.parseClassSuper(node);
    node.body = this.parseClassBody(!!node.superClass, oldStrict);
    return this.finishNode(node, isStatement ? "ClassDeclaration" : "ClassExpression");
  }

  isClassProperty() {
    return this.match(types$1.eq) || this.match(types$1.semi) || this.match(types$1.braceR);
  }

  isClassMethod() {
    return this.match(types$1.parenL);
  }

  isNonstaticConstructor(method) {
    return !method.computed && !method.static && (method.key.name === "constructor" || method.key.value === "constructor");
  }

  parseClassBody(hadSuperClass, oldStrict) {
    this.classScope.enter();
    const state = {
      hadConstructor: false,
      hadSuperClass
    };
    let decorators = [];
    const classBody = this.startNode();
    classBody.body = [];
    this.expect(types$1.braceL);
    this.withTopicForbiddingContext(() => {
      while (!this.match(types$1.braceR)) {
        if (this.eat(types$1.semi)) {
          if (decorators.length > 0) {
            throw this.raise(this.state.lastTokEnd, ErrorMessages.DecoratorSemicolon);
          }

          continue;
        }

        if (this.match(types$1.at)) {
          decorators.push(this.parseDecorator());
          continue;
        }

        const member = this.startNode();

        if (decorators.length) {
          member.decorators = decorators;
          this.resetStartLocationFromNode(member, decorators[0]);
          decorators = [];
        }

        this.parseClassMember(classBody, member, state);

        if (member.kind === "constructor" && member.decorators && member.decorators.length > 0) {
          this.raise(member.start, ErrorMessages.DecoratorConstructor);
        }
      }
    });
    this.state.strict = oldStrict;
    this.next();

    if (decorators.length) {
      throw this.raise(this.state.start, ErrorMessages.TrailingDecorator);
    }

    this.classScope.exit();
    return this.finishNode(classBody, "ClassBody");
  }

  parseClassMemberFromModifier(classBody, member) {
    const key = this.parseIdentifier(true);

    if (this.isClassMethod()) {
      const method = member;
      method.kind = "method";
      method.computed = false;
      method.key = key;
      method.static = false;
      this.pushClassMethod(classBody, method, false, false, false, false);
      return true;
    } else if (this.isClassProperty()) {
      const prop = member;
      prop.computed = false;
      prop.key = key;
      prop.static = false;
      classBody.body.push(this.parseClassProperty(prop));
      return true;
    }

    return false;
  }

  parseClassMember(classBody, member, state) {
    const isStatic = this.isContextual("static");

    if (isStatic) {
      if (this.parseClassMemberFromModifier(classBody, member)) {
        return;
      }

      if (this.eat(types$1.braceL)) {
        this.parseClassStaticBlock(classBody, member);
        return;
      }
    }

    this.parseClassMemberWithIsStatic(classBody, member, state, isStatic);
  }

  parseClassMemberWithIsStatic(classBody, member, state, isStatic) {
    const publicMethod = member;
    const privateMethod = member;
    const publicProp = member;
    const privateProp = member;
    const method = publicMethod;
    const publicMember = publicMethod;
    member.static = isStatic;

    if (this.eat(types$1.star)) {
      method.kind = "method";
      const isPrivateName = this.match(types$1.privateName);
      this.parseClassElementName(method);

      if (isPrivateName) {
        this.pushClassPrivateMethod(classBody, privateMethod, true, false);
        return;
      }

      if (this.isNonstaticConstructor(publicMethod)) {
        this.raise(publicMethod.key.start, ErrorMessages.ConstructorIsGenerator);
      }

      this.pushClassMethod(classBody, publicMethod, true, false, false, false);
      return;
    }

    const containsEsc = this.state.containsEsc;
    const isPrivate = this.match(types$1.privateName);
    const key = this.parseClassElementName(member);
    const isSimple = key.type === "Identifier";
    const maybeQuestionTokenStart = this.state.start;
    this.parsePostMemberNameModifiers(publicMember);

    if (this.isClassMethod()) {
      method.kind = "method";

      if (isPrivate) {
        this.pushClassPrivateMethod(classBody, privateMethod, false, false);
        return;
      }

      const isConstructor = this.isNonstaticConstructor(publicMethod);
      let allowsDirectSuper = false;

      if (isConstructor) {
        publicMethod.kind = "constructor";

        if (state.hadConstructor && !this.hasPlugin("typescript")) {
          this.raise(key.start, ErrorMessages.DuplicateConstructor);
        }

        if (isConstructor && this.hasPlugin("typescript") && member.override) {
          this.raise(key.start, ErrorMessages.OverrideOnConstructor);
        }

        state.hadConstructor = true;
        allowsDirectSuper = state.hadSuperClass;
      }

      this.pushClassMethod(classBody, publicMethod, false, false, isConstructor, allowsDirectSuper);
    } else if (this.isClassProperty()) {
      if (isPrivate) {
        this.pushClassPrivateProperty(classBody, privateProp);
      } else {
        this.pushClassProperty(classBody, publicProp);
      }
    } else if (isSimple && key.name === "async" && !containsEsc && !this.isLineTerminator()) {
      const isGenerator = this.eat(types$1.star);

      if (publicMember.optional) {
        this.unexpected(maybeQuestionTokenStart);
      }

      method.kind = "method";
      const isPrivate = this.match(types$1.privateName);
      this.parseClassElementName(method);
      this.parsePostMemberNameModifiers(publicMember);

      if (isPrivate) {
        this.pushClassPrivateMethod(classBody, privateMethod, isGenerator, true);
      } else {
        if (this.isNonstaticConstructor(publicMethod)) {
          this.raise(publicMethod.key.start, ErrorMessages.ConstructorIsAsync);
        }

        this.pushClassMethod(classBody, publicMethod, isGenerator, true, false, false);
      }
    } else if (isSimple && (key.name === "get" || key.name === "set") && !containsEsc && !(this.match(types$1.star) && this.isLineTerminator())) {
      method.kind = key.name;
      const isPrivate = this.match(types$1.privateName);
      this.parseClassElementName(publicMethod);

      if (isPrivate) {
        this.pushClassPrivateMethod(classBody, privateMethod, false, false);
      } else {
        if (this.isNonstaticConstructor(publicMethod)) {
          this.raise(publicMethod.key.start, ErrorMessages.ConstructorIsAccessor);
        }

        this.pushClassMethod(classBody, publicMethod, false, false, false, false);
      }

      this.checkGetterSetterParams(publicMethod);
    } else if (this.isLineTerminator()) {
      if (isPrivate) {
        this.pushClassPrivateProperty(classBody, privateProp);
      } else {
        this.pushClassProperty(classBody, publicProp);
      }
    } else {
      this.unexpected();
    }
  }

  parseClassElementName(member) {
    const {
      type,
      value,
      start
    } = this.state;

    if ((type === types$1.name || type === types$1.string) && member.static && value === "prototype") {
      this.raise(start, ErrorMessages.StaticPrototype);
    }

    if (type === types$1.privateName && value === "constructor") {
      this.raise(start, ErrorMessages.ConstructorClassPrivateField);
    }

    return this.parsePropertyName(member, true);
  }

  parseClassStaticBlock(classBody, member) {
    var _member$decorators;

    this.expectPlugin("classStaticBlock", member.start);
    this.scope.enter(SCOPE_CLASS | SCOPE_STATIC_BLOCK | SCOPE_SUPER);
    const oldLabels = this.state.labels;
    this.state.labels = [];
    this.prodParam.enter(PARAM);
    const body = member.body = [];
    this.parseBlockOrModuleBlockBody(body, undefined, false, types$1.braceR);
    this.prodParam.exit();
    this.scope.exit();
    this.state.labels = oldLabels;
    classBody.body.push(this.finishNode(member, "StaticBlock"));

    if ((_member$decorators = member.decorators) != null && _member$decorators.length) {
      this.raise(member.start, ErrorMessages.DecoratorStaticBlock);
    }
  }

  pushClassProperty(classBody, prop) {
    if (!prop.computed && (prop.key.name === "constructor" || prop.key.value === "constructor")) {
      this.raise(prop.key.start, ErrorMessages.ConstructorClassField);
    }

    classBody.body.push(this.parseClassProperty(prop));
  }

  pushClassPrivateProperty(classBody, prop) {
    const node = this.parseClassPrivateProperty(prop);
    classBody.body.push(node);
    this.classScope.declarePrivateName(this.getPrivateNameSV(node.key), CLASS_ELEMENT_OTHER, node.key.start);
  }

  pushClassMethod(classBody, method, isGenerator, isAsync, isConstructor, allowsDirectSuper) {
    classBody.body.push(this.parseMethod(method, isGenerator, isAsync, isConstructor, allowsDirectSuper, "ClassMethod", true));
  }

  pushClassPrivateMethod(classBody, method, isGenerator, isAsync) {
    const node = this.parseMethod(method, isGenerator, isAsync, false, false, "ClassPrivateMethod", true);
    classBody.body.push(node);
    const kind = node.kind === "get" ? node.static ? CLASS_ELEMENT_STATIC_GETTER : CLASS_ELEMENT_INSTANCE_GETTER : node.kind === "set" ? node.static ? CLASS_ELEMENT_STATIC_SETTER : CLASS_ELEMENT_INSTANCE_SETTER : CLASS_ELEMENT_OTHER;
    this.classScope.declarePrivateName(this.getPrivateNameSV(node.key), kind, node.key.start);
  }

  parsePostMemberNameModifiers(methodOrProp) {}

  parseClassPrivateProperty(node) {
    this.parseInitializer(node);
    this.semicolon();
    return this.finishNode(node, "ClassPrivateProperty");
  }

  parseClassProperty(node) {
    this.parseInitializer(node);
    this.semicolon();
    return this.finishNode(node, "ClassProperty");
  }

  parseInitializer(node) {
    this.scope.enter(SCOPE_CLASS | SCOPE_SUPER);
    this.expressionScope.enter(newExpressionScope());
    this.prodParam.enter(PARAM);
    node.value = this.eat(types$1.eq) ? this.parseMaybeAssignAllowIn() : null;
    this.expressionScope.exit();
    this.prodParam.exit();
    this.scope.exit();
  }

  parseClassId(node, isStatement, optionalId, bindingType = BIND_CLASS) {
    if (this.match(types$1.name)) {
      node.id = this.parseIdentifier();

      if (isStatement) {
        this.checkLVal(node.id, "class name", bindingType);
      }
    } else {
      if (optionalId || !isStatement) {
        node.id = null;
      } else {
        this.unexpected(null, ErrorMessages.MissingClassName);
      }
    }
  }

  parseClassSuper(node) {
    node.superClass = this.eat(types$1._extends) ? this.parseExprSubscripts() : null;
  }

  parseExport(node) {
    const hasDefault = this.maybeParseExportDefaultSpecifier(node);
    const parseAfterDefault = !hasDefault || this.eat(types$1.comma);
    const hasStar = parseAfterDefault && this.eatExportStar(node);
    const hasNamespace = hasStar && this.maybeParseExportNamespaceSpecifier(node);
    const parseAfterNamespace = parseAfterDefault && (!hasNamespace || this.eat(types$1.comma));
    const isFromRequired = hasDefault || hasStar;

    if (hasStar && !hasNamespace) {
      if (hasDefault) this.unexpected();
      this.parseExportFrom(node, true);
      return this.finishNode(node, "ExportAllDeclaration");
    }

    const hasSpecifiers = this.maybeParseExportNamedSpecifiers(node);

    if (hasDefault && parseAfterDefault && !hasStar && !hasSpecifiers || hasNamespace && parseAfterNamespace && !hasSpecifiers) {
      throw this.unexpected(null, types$1.braceL);
    }

    let hasDeclaration;

    if (isFromRequired || hasSpecifiers) {
      hasDeclaration = false;
      this.parseExportFrom(node, isFromRequired);
    } else {
      hasDeclaration = this.maybeParseExportDeclaration(node);
    }

    if (isFromRequired || hasSpecifiers || hasDeclaration) {
      this.checkExport(node, true, false, !!node.source);
      return this.finishNode(node, "ExportNamedDeclaration");
    }

    if (this.eat(types$1._default)) {
      node.declaration = this.parseExportDefaultExpression();
      this.checkExport(node, true, true);
      return this.finishNode(node, "ExportDefaultDeclaration");
    }

    throw this.unexpected(null, types$1.braceL);
  }

  eatExportStar(node) {
    return this.eat(types$1.star);
  }

  maybeParseExportDefaultSpecifier(node) {
    if (this.isExportDefaultSpecifier()) {
      this.expectPlugin("exportDefaultFrom");
      const specifier = this.startNode();
      specifier.exported = this.parseIdentifier(true);
      node.specifiers = [this.finishNode(specifier, "ExportDefaultSpecifier")];
      return true;
    }

    return false;
  }

  maybeParseExportNamespaceSpecifier(node) {
    if (this.isContextual("as")) {
      if (!node.specifiers) node.specifiers = [];
      const specifier = this.startNodeAt(this.state.lastTokStart, this.state.lastTokStartLoc);
      this.next();
      specifier.exported = this.parseModuleExportName();
      node.specifiers.push(this.finishNode(specifier, "ExportNamespaceSpecifier"));
      return true;
    }

    return false;
  }

  maybeParseExportNamedSpecifiers(node) {
    if (this.match(types$1.braceL)) {
      if (!node.specifiers) node.specifiers = [];
      node.specifiers.push(...this.parseExportSpecifiers());
      node.source = null;
      node.declaration = null;
      return true;
    }

    return false;
  }

  maybeParseExportDeclaration(node) {
    if (this.shouldParseExportDeclaration()) {
      node.specifiers = [];
      node.source = null;
      node.declaration = this.parseExportDeclaration(node);
      return true;
    }

    return false;
  }

  isAsyncFunction() {
    if (!this.isContextual("async")) return false;
    const next = this.nextTokenStart();
    return !lineBreak.test(this.input.slice(this.state.pos, next)) && this.isUnparsedContextual(next, "function");
  }

  parseExportDefaultExpression() {
    const expr = this.startNode();
    const isAsync = this.isAsyncFunction();

    if (this.match(types$1._function) || isAsync) {
      this.next();

      if (isAsync) {
        this.next();
      }

      return this.parseFunction(expr, FUNC_STATEMENT | FUNC_NULLABLE_ID, isAsync);
    } else if (this.match(types$1._class)) {
      return this.parseClass(expr, true, true);
    } else if (this.match(types$1.at)) {
      if (this.hasPlugin("decorators") && this.getPluginOption("decorators", "decoratorsBeforeExport")) {
        this.raise(this.state.start, ErrorMessages.DecoratorBeforeExport);
      }

      this.parseDecorators(false);
      return this.parseClass(expr, true, true);
    } else if (this.match(types$1._const) || this.match(types$1._var) || this.isLet()) {
      throw this.raise(this.state.start, ErrorMessages.UnsupportedDefaultExport);
    } else {
      const res = this.parseMaybeAssignAllowIn();
      this.semicolon();
      return res;
    }
  }

  parseExportDeclaration(node) {
    return this.parseStatement(null);
  }

  isExportDefaultSpecifier() {
    if (this.match(types$1.name)) {
      const value = this.state.value;

      if (value === "async" && !this.state.containsEsc || value === "let") {
        return false;
      }

      if ((value === "type" || value === "interface") && !this.state.containsEsc) {
        const l = this.lookahead();

        if (l.type === types$1.name && l.value !== "from" || l.type === types$1.braceL) {
          this.expectOnePlugin(["flow", "typescript"]);
          return false;
        }
      }
    } else if (!this.match(types$1._default)) {
      return false;
    }

    const next = this.nextTokenStart();
    const hasFrom = this.isUnparsedContextual(next, "from");

    if (this.input.charCodeAt(next) === 44 || this.match(types$1.name) && hasFrom) {
      return true;
    }

    if (this.match(types$1._default) && hasFrom) {
      const nextAfterFrom = this.input.charCodeAt(this.nextTokenStartSince(next + 4));
      return nextAfterFrom === 34 || nextAfterFrom === 39;
    }

    return false;
  }

  parseExportFrom(node, expect) {
    if (this.eatContextual("from")) {
      node.source = this.parseImportSource();
      this.checkExport(node);
      const assertions = this.maybeParseImportAssertions();

      if (assertions) {
        node.assertions = assertions;
      }
    } else {
      if (expect) {
        this.unexpected();
      } else {
        node.source = null;
      }
    }

    this.semicolon();
  }

  shouldParseExportDeclaration() {
    if (this.match(types$1.at)) {
      this.expectOnePlugin(["decorators", "decorators-legacy"]);

      if (this.hasPlugin("decorators")) {
        if (this.getPluginOption("decorators", "decoratorsBeforeExport")) {
          this.unexpected(this.state.start, ErrorMessages.DecoratorBeforeExport);
        } else {
          return true;
        }
      }
    }

    return this.state.type.keyword === "var" || this.state.type.keyword === "const" || this.state.type.keyword === "function" || this.state.type.keyword === "class" || this.isLet() || this.isAsyncFunction();
  }

  checkExport(node, checkNames, isDefault, isFrom) {
    if (checkNames) {
      if (isDefault) {
        this.checkDuplicateExports(node, "default");

        if (this.hasPlugin("exportDefaultFrom")) {
          var _declaration$extra;

          const declaration = node.declaration;

          if (declaration.type === "Identifier" && declaration.name === "from" && declaration.end - declaration.start === 4 && !((_declaration$extra = declaration.extra) != null && _declaration$extra.parenthesized)) {
            this.raise(declaration.start, ErrorMessages.ExportDefaultFromAsIdentifier);
          }
        }
      } else if (node.specifiers && node.specifiers.length) {
        for (const specifier of node.specifiers) {
          const {
            exported
          } = specifier;
          const exportedName = exported.type === "Identifier" ? exported.name : exported.value;
          this.checkDuplicateExports(specifier, exportedName);

          if (!isFrom && specifier.local) {
            const {
              local
            } = specifier;

            if (local.type !== "Identifier") {
              this.raise(specifier.start, ErrorMessages.ExportBindingIsString, local.value, exportedName);
            } else {
              this.checkReservedWord(local.name, local.start, true, false);
              this.scope.checkLocalExport(local);
            }
          }
        }
      } else if (node.declaration) {
        if (node.declaration.type === "FunctionDeclaration" || node.declaration.type === "ClassDeclaration") {
          const id = node.declaration.id;
          if (!id) throw new Error("Assertion failure");
          this.checkDuplicateExports(node, id.name);
        } else if (node.declaration.type === "VariableDeclaration") {
          for (const declaration of node.declaration.declarations) {
            this.checkDeclaration(declaration.id);
          }
        }
      }
    }

    const currentContextDecorators = this.state.decoratorStack[this.state.decoratorStack.length - 1];

    if (currentContextDecorators.length) {
      throw this.raise(node.start, ErrorMessages.UnsupportedDecoratorExport);
    }
  }

  checkDeclaration(node) {
    if (node.type === "Identifier") {
      this.checkDuplicateExports(node, node.name);
    } else if (node.type === "ObjectPattern") {
      for (const prop of node.properties) {
        this.checkDeclaration(prop);
      }
    } else if (node.type === "ArrayPattern") {
      for (const elem of node.elements) {
        if (elem) {
          this.checkDeclaration(elem);
        }
      }
    } else if (node.type === "ObjectProperty") {
      this.checkDeclaration(node.value);
    } else if (node.type === "RestElement") {
      this.checkDeclaration(node.argument);
    } else if (node.type === "AssignmentPattern") {
      this.checkDeclaration(node.left);
    }
  }

  checkDuplicateExports(node, name) {
    if (this.exportedIdentifiers.has(name)) {
      this.raise(node.start, name === "default" ? ErrorMessages.DuplicateDefaultExport : ErrorMessages.DuplicateExport, name);
    }

    this.exportedIdentifiers.add(name);
  }

  parseExportSpecifiers() {
    const nodes = [];
    let first = true;
    this.expect(types$1.braceL);

    while (!this.eat(types$1.braceR)) {
      if (first) {
        first = false;
      } else {
        this.expect(types$1.comma);
        if (this.eat(types$1.braceR)) break;
      }

      const node = this.startNode();
      node.local = this.parseModuleExportName();
      node.exported = this.eatContextual("as") ? this.parseModuleExportName() : node.local.__clone();
      nodes.push(this.finishNode(node, "ExportSpecifier"));
    }

    return nodes;
  }

  parseModuleExportName() {
    if (this.match(types$1.string)) {
      const result = this.parseStringLiteral(this.state.value);
      const surrogate = result.value.match(loneSurrogate);

      if (surrogate) {
        this.raise(result.start, ErrorMessages.ModuleExportNameHasLoneSurrogate, surrogate[0].charCodeAt(0).toString(16));
      }

      return result;
    }

    return this.parseIdentifier(true);
  }

  parseImport(node) {
    node.specifiers = [];

    if (!this.match(types$1.string)) {
      const hasDefault = this.maybeParseDefaultImportSpecifier(node);
      const parseNext = !hasDefault || this.eat(types$1.comma);
      const hasStar = parseNext && this.maybeParseStarImportSpecifier(node);
      if (parseNext && !hasStar) this.parseNamedImportSpecifiers(node);
      this.expectContextual("from");
    }

    node.source = this.parseImportSource();
    const assertions = this.maybeParseImportAssertions();

    if (assertions) {
      node.assertions = assertions;
    } else {
      const attributes = this.maybeParseModuleAttributes();

      if (attributes) {
        node.attributes = attributes;
      }
    }

    this.semicolon();
    return this.finishNode(node, "ImportDeclaration");
  }

  parseImportSource() {
    if (!this.match(types$1.string)) this.unexpected();
    return this.parseExprAtom();
  }

  shouldParseDefaultImport(node) {
    return this.match(types$1.name);
  }

  parseImportSpecifierLocal(node, specifier, type, contextDescription) {
    specifier.local = this.parseIdentifier();
    this.checkLVal(specifier.local, contextDescription, BIND_LEXICAL);
    node.specifiers.push(this.finishNode(specifier, type));
  }

  parseAssertEntries() {
    const attrs = [];
    const attrNames = new Set();

    do {
      if (this.match(types$1.braceR)) {
        break;
      }

      const node = this.startNode();
      const keyName = this.state.value;

      if (attrNames.has(keyName)) {
        this.raise(this.state.start, ErrorMessages.ModuleAttributesWithDuplicateKeys, keyName);
      }

      attrNames.add(keyName);

      if (this.match(types$1.string)) {
        node.key = this.parseStringLiteral(keyName);
      } else {
        node.key = this.parseIdentifier(true);
      }

      this.expect(types$1.colon);

      if (!this.match(types$1.string)) {
        throw this.unexpected(this.state.start, ErrorMessages.ModuleAttributeInvalidValue);
      }

      node.value = this.parseStringLiteral(this.state.value);
      this.finishNode(node, "ImportAttribute");
      attrs.push(node);
    } while (this.eat(types$1.comma));

    return attrs;
  }

  maybeParseModuleAttributes() {
    if (this.match(types$1._with) && !this.hasPrecedingLineBreak()) {
      this.expectPlugin("moduleAttributes");
      this.next();
    } else {
      if (this.hasPlugin("moduleAttributes")) return [];
      return null;
    }

    const attrs = [];
    const attributes = new Set();

    do {
      const node = this.startNode();
      node.key = this.parseIdentifier(true);

      if (node.key.name !== "type") {
        this.raise(node.key.start, ErrorMessages.ModuleAttributeDifferentFromType, node.key.name);
      }

      if (attributes.has(node.key.name)) {
        this.raise(node.key.start, ErrorMessages.ModuleAttributesWithDuplicateKeys, node.key.name);
      }

      attributes.add(node.key.name);
      this.expect(types$1.colon);

      if (!this.match(types$1.string)) {
        throw this.unexpected(this.state.start, ErrorMessages.ModuleAttributeInvalidValue);
      }

      node.value = this.parseStringLiteral(this.state.value);
      this.finishNode(node, "ImportAttribute");
      attrs.push(node);
    } while (this.eat(types$1.comma));

    return attrs;
  }

  maybeParseImportAssertions() {
    if (this.isContextual("assert") && !this.hasPrecedingLineBreak()) {
      this.expectPlugin("importAssertions");
      this.next();
    } else {
      if (this.hasPlugin("importAssertions")) return [];
      return null;
    }

    this.eat(types$1.braceL);
    const attrs = this.parseAssertEntries();
    this.eat(types$1.braceR);
    return attrs;
  }

  maybeParseDefaultImportSpecifier(node) {
    if (this.shouldParseDefaultImport(node)) {
      this.parseImportSpecifierLocal(node, this.startNode(), "ImportDefaultSpecifier", "default import specifier");
      return true;
    }

    return false;
  }

  maybeParseStarImportSpecifier(node) {
    if (this.match(types$1.star)) {
      const specifier = this.startNode();
      this.next();
      this.expectContextual("as");
      this.parseImportSpecifierLocal(node, specifier, "ImportNamespaceSpecifier", "import namespace specifier");
      return true;
    }

    return false;
  }

  parseNamedImportSpecifiers(node) {
    let first = true;
    this.expect(types$1.braceL);

    while (!this.eat(types$1.braceR)) {
      if (first) {
        first = false;
      } else {
        if (this.eat(types$1.colon)) {
          throw this.raise(this.state.start, ErrorMessages.DestructureNamedImport);
        }

        this.expect(types$1.comma);
        if (this.eat(types$1.braceR)) break;
      }

      this.parseImportSpecifier(node);
    }
  }

  parseImportSpecifier(node) {
    const specifier = this.startNode();
    const importedIsString = this.match(types$1.string);
    specifier.imported = this.parseModuleExportName();

    if (this.eatContextual("as")) {
      specifier.local = this.parseIdentifier();
    } else {
      const {
        imported
      } = specifier;

      if (importedIsString) {
        throw this.raise(specifier.start, ErrorMessages.ImportBindingIsString, imported.value);
      }

      this.checkReservedWord(imported.name, specifier.start, true, true);
      specifier.local = imported.__clone();
    }

    this.checkLVal(specifier.local, "import specifier", BIND_LEXICAL);
    node.specifiers.push(this.finishNode(specifier, "ImportSpecifier"));
  }

  isThisParam(param) {
    return param.type === "Identifier" && param.name === "this";
  }

}

class Parser extends StatementParser {
  constructor(options, input) {
    options = getOptions(options);
    super(options, input);
    this.options = options;
    this.initializeScopes();
    this.plugins = pluginsMap(this.options.plugins);
    this.filename = options.sourceFilename;
  }

  getScopeHandler() {
    return ScopeHandler;
  }

  parse() {
    this.enterInitialScopes();
    const file = this.startNode();
    const program = this.startNode();
    this.nextToken();
    file.errors = null;
    this.parseTopLevel(file, program);
    file.errors = this.state.errors;
    return file;
  }

}

function pluginsMap(plugins) {
  const pluginMap = new Map();

  for (const plugin of plugins) {
    const [name, options] = Array.isArray(plugin) ? plugin : [plugin, {}];
    if (!pluginMap.has(name)) pluginMap.set(name, options || {});
  }

  return pluginMap;
}

function parse(input, options) {
  var _options;

  if (((_options = options) == null ? void 0 : _options.sourceType) === "unambiguous") {
    options = Object.assign({}, options);

    try {
      options.sourceType = "module";
      const parser = getParser(options, input);
      const ast = parser.parse();

      if (parser.sawUnambiguousESM) {
        return ast;
      }

      if (parser.ambiguousScriptDifferentAst) {
        try {
          options.sourceType = "script";
          return getParser(options, input).parse();
        } catch (_unused) {}
      } else {
        ast.program.sourceType = "script";
      }

      return ast;
    } catch (moduleError) {
      try {
        options.sourceType = "script";
        return getParser(options, input).parse();
      } catch (_unused2) {}

      throw moduleError;
    }
  } else {
    return getParser(options, input).parse();
  }
}
function parseExpression(input, options) {
  const parser = getParser(options, input);

  if (parser.options.strictMode) {
    parser.state.strict = true;
  }

  return parser.getExpression();
}

function getParser(options, input) {
  let cls = Parser;

  if (options != null && options.plugins) {
    validatePlugins(options.plugins);
    cls = getParserClass(options.plugins);
  }

  return new cls(options, input);
}

const parserClassCache = {};

function getParserClass(pluginsFromOptions) {
  const pluginList = mixinPluginNames.filter(name => hasPlugin(pluginsFromOptions, name));
  const key = pluginList.join("/");
  let cls = parserClassCache[key];

  if (!cls) {
    cls = Parser;

    for (const plugin of pluginList) {
      cls = mixinPlugins[plugin](cls);
    }

    parserClassCache[key] = cls;
  }

  return cls;
}

exports.parse = parse;
exports.parseExpression = parseExpression;
exports.tokTypes = types$1;
//# sourceMappingURL=index.js.map
