(function (global, factory) {
    typeof exports === 'object' && typeof module !== 'undefined' ? factory(exports) :
    typeof define === 'function' && define.amd ? define(['exports'], factory) :
    (global = typeof globalThis !== 'undefined' ? globalThis : global || self, factory(global.jtpp = {}));
}(this, (function (exports) { 'use strict';

    function tokenToString(token) {
        if (token.text !== undefined && token.text !== '') {
            return `'${token.type}' with value '${token.text}'`;
        }
        else {
            return `'${token.type}'`;
        }
    }
    class NoParsletFoundError extends Error {
        constructor(token) {
            super(`No parslet found for token: ${tokenToString(token)}`);
            this.token = token;
            Object.setPrototypeOf(this, NoParsletFoundError.prototype);
        }
        getToken() {
            return this.token;
        }
    }
    class EarlyEndOfParseError extends Error {
        constructor(token) {
            super(`The parsing ended early. The next token was: ${tokenToString(token)}`);
            this.token = token;
            Object.setPrototypeOf(this, EarlyEndOfParseError.prototype);
        }
        getToken() {
            return this.token;
        }
    }
    class UnexpectedTypeError extends Error {
        constructor(result, message) {
            let error = `Unexpected type: '${result.type}'.`;
            if (message !== undefined) {
                error += ` Message: ${message}`;
            }
            super(error);
            Object.setPrototypeOf(this, UnexpectedTypeError.prototype);
        }
    }
    // export class UnexpectedTokenError extends Error {
    //   private expected: Token
    //   private found: Token
    //
    //   constructor (expected: Token, found: Token) {
    //     super(`The parsing ended early. The next token was: ${tokenToString(token)}`)
    //
    //     this.token = token
    //
    //     Object.setPrototypeOf(this, EarlyEndOfParseError.prototype)
    //   }
    //
    //   getToken() {
    //     return this.token
    //   }
    // }

    function makePunctuationRule(type) {
        return text => {
            if (text.startsWith(type)) {
                return { type, text: type };
            }
            else {
                return null;
            }
        };
    }
    function getQuoted(text) {
        let position = 0;
        let char;
        const mark = text[0];
        let escaped = false;
        if (mark !== '\'' && mark !== '"') {
            return null;
        }
        while (position < text.length) {
            position++;
            char = text[position];
            if (!escaped && char === mark) {
                position++;
                break;
            }
            escaped = !escaped && char === '\\';
        }
        if (char !== mark) {
            throw new Error('Unterminated String');
        }
        return text.slice(0, position);
    }
    const identifierStartRegex = /[$_\p{ID_Start}]|\\u\p{Hex_Digit}{4}|\\u\{0*(?:\p{Hex_Digit}{1,5}|10\p{Hex_Digit}{4})\}/u;
    // A hyphen is not technically allowed, but to keep it liberal for now,
    //  adding it here
    const identifierContinueRegex = /[$\-\p{ID_Continue}\u200C\u200D]|\\u\p{Hex_Digit}{4}|\\u\{0*(?:\p{Hex_Digit}{1,5}|10\p{Hex_Digit}{4})\}/u;
    function getIdentifier(text) {
        let char = text[0];
        if (!identifierStartRegex.test(char)) {
            return null;
        }
        let position = 1;
        do {
            char = text[position];
            if (!identifierContinueRegex.test(char)) {
                break;
            }
            position++;
        } while (position < text.length);
        return text.slice(0, position);
    }
    const numberRegex = /[0-9]/;
    function getNumber(text) {
        let position = 0;
        let char;
        do {
            char = text[position];
            if (!numberRegex.test(char)) {
                break;
            }
            position++;
        } while (position < text.length);
        if (position === 0) {
            return null;
        }
        return text.slice(0, position);
    }
    const identifierRule = text => {
        const value = getIdentifier(text);
        if (value == null) {
            return null;
        }
        return {
            type: 'Identifier',
            text: value
        };
    };
    function makeKeyWordRule(type) {
        return text => {
            if (!text.startsWith(type)) {
                return null;
            }
            const prepends = text[type.length];
            if (prepends !== undefined && identifierContinueRegex.test(prepends)) {
                return null;
            }
            return {
                type: type,
                text: type
            };
        };
    }
    const stringValueRule = text => {
        const value = getQuoted(text);
        if (value == null) {
            return null;
        }
        return {
            type: 'StringValue',
            text: value
        };
    };
    const eofRule = text => {
        if (text.length > 0) {
            return null;
        }
        return {
            type: 'EOF',
            text: ''
        };
    };
    const numberRule = text => {
        const value = getNumber(text);
        if (value === null) {
            return null;
        }
        return {
            type: 'Number',
            text: value
        };
    };
    const rules = [
        eofRule,
        makePunctuationRule('=>'),
        makePunctuationRule('('),
        makePunctuationRule(')'),
        makePunctuationRule('{'),
        makePunctuationRule('}'),
        makePunctuationRule('['),
        makePunctuationRule(']'),
        makePunctuationRule('|'),
        makePunctuationRule('&'),
        makePunctuationRule('<'),
        makePunctuationRule('>'),
        makePunctuationRule(','),
        makePunctuationRule(';'),
        makePunctuationRule('*'),
        makePunctuationRule('?'),
        makePunctuationRule('!'),
        makePunctuationRule('='),
        makePunctuationRule(':'),
        makePunctuationRule('...'),
        makePunctuationRule('.'),
        makePunctuationRule('#'),
        makePunctuationRule('~'),
        makePunctuationRule('/'),
        makePunctuationRule('@'),
        makeKeyWordRule('undefined'),
        makeKeyWordRule('null'),
        makeKeyWordRule('function'),
        makeKeyWordRule('this'),
        makeKeyWordRule('new'),
        makeKeyWordRule('module'),
        makeKeyWordRule('event'),
        makeKeyWordRule('external'),
        makeKeyWordRule('typeof'),
        makeKeyWordRule('keyof'),
        makeKeyWordRule('readonly'),
        makeKeyWordRule('import'),
        identifierRule,
        stringValueRule,
        numberRule
    ];
    class Lexer {
        constructor() {
            this.text = '';
        }
        lex(text) {
            this.text = text;
            this.current = undefined;
            this.next = undefined;
            this.advance();
        }
        token() {
            if (this.current === undefined) {
                throw new Error('Lexer not lexing');
            }
            return this.current;
        }
        peek() {
            if (this.next === undefined) {
                this.next = this.read();
            }
            return this.next;
        }
        last() {
            return this.previous;
        }
        advance() {
            this.previous = this.current;
            if (this.next !== undefined) {
                this.current = this.next;
                this.next = undefined;
                return;
            }
            this.current = this.read();
        }
        read() {
            const text = this.text.trim();
            for (const rule of rules) {
                const token = rule(text);
                if (token !== null) {
                    this.text = text.slice(token.text.length);
                    return token;
                }
            }
            throw new Error('Unexpected Token ' + text);
        }
    }

    function assertTerminal(result) {
        if (result === undefined) {
            throw new Error('Unexpected undefined');
        }
        if (result.type === 'JsdocTypeKeyValue' || result.type === 'JsdocTypeParameterList' || result.type === 'JsdocTypeProperty' || result.type === 'JsdocTypeReadonlyProperty') {
            throw new UnexpectedTypeError(result);
        }
        return result;
    }
    function assertPlainKeyValueOrTerminal(result) {
        if (result.type === 'JsdocTypeKeyValue') {
            return assertPlainKeyValue(result);
        }
        return assertTerminal(result);
    }
    function assertPlainKeyValueOrName(result) {
        if (result.type === 'JsdocTypeName') {
            return result;
        }
        return assertPlainKeyValue(result);
    }
    function assertPlainKeyValue(result) {
        if (!isPlainKeyValue(result)) {
            if (result.type === 'JsdocTypeKeyValue') {
                throw new UnexpectedTypeError(result, 'Expecting no left side expression.');
            }
            else {
                throw new UnexpectedTypeError(result);
            }
        }
        return result;
    }
    function assertNumberOrVariadicName(result) {
        var _a;
        if (result.type === 'JsdocTypeVariadic') {
            if (((_a = result.element) === null || _a === void 0 ? void 0 : _a.type) === 'JsdocTypeName') {
                return result;
            }
            throw new UnexpectedTypeError(result);
        }
        if (result.type !== 'JsdocTypeNumber' && result.type !== 'JsdocTypeName') {
            throw new UnexpectedTypeError(result);
        }
        return result;
    }
    function isPlainKeyValue(result) {
        return result.type === 'JsdocTypeKeyValue' && !result.meta.hasLeftSideExpression;
    }

    // higher precedence = higher importance
    var Precedence;
    (function (Precedence) {
        Precedence[Precedence["ALL"] = 0] = "ALL";
        Precedence[Precedence["PARAMETER_LIST"] = 1] = "PARAMETER_LIST";
        Precedence[Precedence["OBJECT"] = 2] = "OBJECT";
        Precedence[Precedence["KEY_VALUE"] = 3] = "KEY_VALUE";
        Precedence[Precedence["UNION"] = 4] = "UNION";
        Precedence[Precedence["INTERSECTION"] = 5] = "INTERSECTION";
        Precedence[Precedence["PREFIX"] = 6] = "PREFIX";
        Precedence[Precedence["POSTFIX"] = 7] = "POSTFIX";
        Precedence[Precedence["TUPLE"] = 8] = "TUPLE";
        Precedence[Precedence["SYMBOL"] = 9] = "SYMBOL";
        Precedence[Precedence["OPTIONAL"] = 10] = "OPTIONAL";
        Precedence[Precedence["NULLABLE"] = 11] = "NULLABLE";
        Precedence[Precedence["KEY_OF_TYPE_OF"] = 12] = "KEY_OF_TYPE_OF";
        Precedence[Precedence["FUNCTION"] = 13] = "FUNCTION";
        Precedence[Precedence["ARROW"] = 14] = "ARROW";
        Precedence[Precedence["GENERIC"] = 15] = "GENERIC";
        Precedence[Precedence["NAME_PATH"] = 16] = "NAME_PATH";
        Precedence[Precedence["ARRAY_BRACKETS"] = 17] = "ARRAY_BRACKETS";
        Precedence[Precedence["PARENTHESIS"] = 18] = "PARENTHESIS";
        Precedence[Precedence["SPECIAL_TYPES"] = 19] = "SPECIAL_TYPES";
    })(Precedence || (Precedence = {}));

    class Parser {
        constructor(grammar, lexer) {
            this.lexer = lexer !== null && lexer !== void 0 ? lexer : new Lexer();
            const { prefixParslets, infixParslets } = grammar;
            this.prefixParslets = prefixParslets;
            this.infixParslets = infixParslets;
        }
        parseText(text) {
            this.lexer.lex(text);
            const result = this.parseType(Precedence.ALL);
            if (!this.consume('EOF')) {
                throw new EarlyEndOfParseError(this.getToken());
            }
            return result;
        }
        getPrefixParslet() {
            return this.prefixParslets.find(p => p.accepts(this.getToken().type, this.peekToken().type));
        }
        getInfixParslet(precedence) {
            return this.infixParslets.find(p => {
                return p.getPrecedence() > precedence && p.accepts(this.getToken().type, this.peekToken().type);
            });
        }
        canParseType() {
            return this.getPrefixParslet() !== undefined;
        }
        parseType(precedence) {
            return assertTerminal(this.parseIntermediateType(precedence));
        }
        parseIntermediateType(precedence) {
            const parslet = this.getPrefixParslet();
            if (parslet === undefined) {
                throw new NoParsletFoundError(this.getToken());
            }
            const result = parslet.parsePrefix(this);
            return this.parseInfixIntermediateType(result, precedence);
        }
        parseInfixIntermediateType(result, precedence) {
            let parslet = this.getInfixParslet(precedence);
            while (parslet !== undefined) {
                result = parslet.parseInfix(this, result);
                parslet = this.getInfixParslet(precedence);
            }
            return result;
        }
        consume(type) {
            if (this.lexer.token().type !== type) {
                return false;
            }
            this.lexer.advance();
            return true;
        }
        getToken() {
            return this.lexer.token();
        }
        peekToken() {
            return this.lexer.peek();
        }
        previousToken() {
            return this.lexer.last();
        }
        getLexer() {
            return this.lexer;
        }
    }

    class SymbolParslet {
        accepts(type) {
            return type === '(';
        }
        getPrecedence() {
            return Precedence.SYMBOL;
        }
        parseInfix(parser, left) {
            if (left.type !== 'JsdocTypeName') {
                throw new Error('Symbol expects a name on the left side. (Reacting on \'(\')');
            }
            parser.consume('(');
            const result = {
                type: 'JsdocTypeSymbol',
                value: left.value
            };
            if (!parser.consume(')')) {
                const next = parser.parseIntermediateType(Precedence.SYMBOL);
                result.element = assertNumberOrVariadicName(next);
                if (!parser.consume(')')) {
                    throw new Error('Symbol does not end after value');
                }
            }
            return result;
        }
    }

    class ArrayBracketsParslet {
        accepts(type, next) {
            return type === '[' && next === ']';
        }
        getPrecedence() {
            return Precedence.ARRAY_BRACKETS;
        }
        parseInfix(parser, left) {
            parser.consume('[');
            parser.consume(']');
            return {
                type: 'JsdocTypeGeneric',
                left: {
                    type: 'JsdocTypeName',
                    value: 'Array'
                },
                elements: [
                    assertTerminal(left)
                ],
                meta: {
                    brackets: 'square',
                    dot: false
                }
            };
        }
    }

    class StringValueParslet {
        accepts(type) {
            return type === 'StringValue';
        }
        getPrecedence() {
            return Precedence.PREFIX;
        }
        parsePrefix(parser) {
            const token = parser.getToken();
            parser.consume('StringValue');
            return {
                type: 'JsdocTypeStringValue',
                value: token.text.slice(1, -1),
                meta: {
                    quote: token.text[0] === '\'' ? 'single' : 'double'
                }
            };
        }
    }

    class BaseFunctionParslet {
        getParameters(value) {
            let parameters;
            if (value.type === 'JsdocTypeParameterList') {
                parameters = value.elements;
            }
            else if (value.type === 'JsdocTypeParenthesis') {
                parameters = [value.element];
            }
            else {
                throw new UnexpectedTypeError(value);
            }
            return parameters.map(p => assertPlainKeyValueOrTerminal(p));
        }
        getUnnamedParameters(value) {
            const parameters = this.getParameters(value);
            if (parameters.some(p => p.type === 'JsdocTypeKeyValue')) {
                throw new Error('No parameter should be named');
            }
            return parameters;
        }
    }

    class FunctionParslet extends BaseFunctionParslet {
        constructor(options) {
            super();
            this.allowWithoutParenthesis = options.allowWithoutParenthesis;
            this.allowNamedParameters = options.allowNamedParameters;
            this.allowNoReturnType = options.allowNoReturnType;
        }
        accepts(type) {
            return type === 'function';
        }
        getPrecedence() {
            return Precedence.FUNCTION;
        }
        parsePrefix(parser) {
            parser.consume('function');
            const hasParenthesis = parser.getToken().type === '(';
            if (!hasParenthesis) {
                if (!this.allowWithoutParenthesis) {
                    throw new Error('function is missing parameter list');
                }
                return {
                    type: 'JsdocTypeName',
                    value: 'function'
                };
            }
            const result = {
                type: 'JsdocTypeFunction',
                parameters: [],
                arrow: false,
                parenthesis: hasParenthesis
            };
            const value = parser.parseIntermediateType(Precedence.FUNCTION);
            if (this.allowNamedParameters === undefined) {
                result.parameters = this.getUnnamedParameters(value);
            }
            else {
                result.parameters = this.getParameters(value);
                for (const p of result.parameters) {
                    if (p.type === 'JsdocTypeKeyValue' && (!this.allowNamedParameters.includes(p.key) || p.meta.quote !== undefined)) {
                        throw new Error(`only allowed named parameters are ${this.allowNamedParameters.join(',')} but got ${p.type}`);
                    }
                }
            }
            if (parser.consume(':')) {
                result.returnType = parser.parseType(Precedence.PREFIX);
            }
            else {
                if (!this.allowNoReturnType) {
                    throw new Error('function is missing return type');
                }
            }
            return result;
        }
    }

    class UnionParslet {
        accepts(type) {
            return type === '|';
        }
        getPrecedence() {
            return Precedence.UNION;
        }
        parseInfix(parser, left) {
            parser.consume('|');
            const elements = [];
            do {
                elements.push(parser.parseType(Precedence.UNION));
            } while (parser.consume('|'));
            return {
                type: 'JsdocTypeUnion',
                elements: [assertTerminal(left), ...elements]
            };
        }
    }

    function isQuestionMarkUnknownType(next) {
        return next === 'EOF' || next === '|' || next === ',' || next === ')' || next === '>';
    }

    class SpecialTypesParslet {
        accepts(type, next) {
            return (type === '?' && isQuestionMarkUnknownType(next)) || type === 'null' || type === 'undefined' || type === '*';
        }
        getPrecedence() {
            return Precedence.SPECIAL_TYPES;
        }
        parsePrefix(parser) {
            if (parser.consume('null')) {
                return {
                    type: 'JsdocTypeNull'
                };
            }
            if (parser.consume('undefined')) {
                return {
                    type: 'JsdocTypeUndefined'
                };
            }
            if (parser.consume('*')) {
                return {
                    type: 'JsdocTypeAny'
                };
            }
            if (parser.consume('?')) {
                return {
                    type: 'JsdocTypeUnknown'
                };
            }
            throw new Error('Unacceptable token: ' + parser.getToken().text);
        }
    }

    class GenericParslet {
        accepts(type, next) {
            return type === '<' || (type === '.' && next === '<');
        }
        getPrecedence() {
            return Precedence.GENERIC;
        }
        parseInfix(parser, left) {
            const dot = parser.consume('.');
            parser.consume('<');
            const objects = [];
            do {
                objects.push(parser.parseType(Precedence.PARAMETER_LIST));
            } while (parser.consume(','));
            if (!parser.consume('>')) {
                throw new Error('Unterminated generic parameter list');
            }
            return {
                type: 'JsdocTypeGeneric',
                left: assertTerminal(left),
                elements: objects,
                meta: {
                    brackets: 'angle',
                    dot
                }
            };
        }
    }

    class ParenthesisParslet {
        accepts(type, next) {
            return type === '(';
        }
        getPrecedence() {
            return Precedence.PARENTHESIS;
        }
        parsePrefix(parser) {
            parser.consume('(');
            if (parser.consume(')')) {
                return {
                    type: 'JsdocTypeParameterList',
                    elements: []
                };
            }
            const result = parser.parseIntermediateType(Precedence.ALL);
            if (!parser.consume(')')) {
                throw new Error('Unterminated parenthesis');
            }
            if (result.type === 'JsdocTypeParameterList') {
                return result;
            }
            else if (result.type === 'JsdocTypeKeyValue' && isPlainKeyValue(result)) {
                return {
                    type: 'JsdocTypeParameterList',
                    elements: [result]
                };
            }
            return {
                type: 'JsdocTypeParenthesis',
                element: assertTerminal(result)
            };
        }
    }

    class NumberParslet {
        accepts(type, next) {
            return type === 'Number';
        }
        getPrecedence() {
            return Precedence.PREFIX;
        }
        parsePrefix(parser) {
            const token = parser.getToken();
            parser.consume('Number');
            return {
                type: 'JsdocTypeNumber',
                value: parseInt(token.text, 10)
            };
        }
    }

    class ParameterListParslet {
        constructor(option) {
            this.allowTrailingComma = option.allowTrailingComma;
        }
        accepts(type, next) {
            return type === ',';
        }
        getPrecedence() {
            return Precedence.PARAMETER_LIST;
        }
        parseInfix(parser, left) {
            const elements = [
                assertPlainKeyValueOrTerminal(left)
            ];
            parser.consume(',');
            do {
                try {
                    const next = parser.parseIntermediateType(Precedence.PARAMETER_LIST);
                    elements.push(assertPlainKeyValueOrTerminal(next));
                }
                catch (e) {
                    if (this.allowTrailingComma && e instanceof NoParsletFoundError) {
                        break;
                    }
                    else {
                        throw e;
                    }
                }
            } while (parser.consume(','));
            if (elements.length > 0 && elements.slice(0, -1).some(e => e.type === 'JsdocTypeVariadic')) {
                throw new Error('Only the last parameter may be a rest parameter');
            }
            return {
                type: 'JsdocTypeParameterList',
                elements
            };
        }
    }

    class NullablePrefixParslet {
        accepts(type, next) {
            return type === '?' && !isQuestionMarkUnknownType(next);
        }
        getPrecedence() {
            return Precedence.NULLABLE;
        }
        parsePrefix(parser) {
            parser.consume('?');
            return {
                type: 'JsdocTypeNullable',
                element: parser.parseType(Precedence.NULLABLE),
                meta: {
                    position: 'prefix'
                }
            };
        }
    }
    class NullableInfixParslet {
        accepts(type, next) {
            return type === '?';
        }
        getPrecedence() {
            return Precedence.NULLABLE;
        }
        parseInfix(parser, left) {
            parser.consume('?');
            return {
                type: 'JsdocTypeNullable',
                element: assertTerminal(left),
                meta: {
                    position: 'suffix'
                }
            };
        }
    }

    class OptionalParslet {
        accepts(type, next) {
            return type === '=';
        }
        getPrecedence() {
            return Precedence.OPTIONAL;
        }
        parsePrefix(parser) {
            parser.consume('=');
            return {
                type: 'JsdocTypeOptional',
                element: parser.parseType(Precedence.OPTIONAL),
                meta: {
                    position: 'prefix'
                }
            };
        }
        parseInfix(parser, left) {
            parser.consume('=');
            return {
                type: 'JsdocTypeOptional',
                element: assertTerminal(left),
                meta: {
                    position: 'suffix'
                }
            };
        }
    }

    class NotNullableParslet {
        accepts(type, next) {
            return type === '!';
        }
        getPrecedence() {
            return Precedence.NULLABLE;
        }
        parsePrefix(parser) {
            parser.consume('!');
            return {
                type: 'JsdocTypeNotNullable',
                element: parser.parseType(Precedence.NULLABLE),
                meta: {
                    position: 'prefix'
                }
            };
        }
        parseInfix(parser, left) {
            parser.consume('!');
            return {
                type: 'JsdocTypeNotNullable',
                element: assertTerminal(left),
                meta: {
                    position: 'suffix'
                }
            };
        }
    }

    const baseGrammar = () => {
        return {
            prefixParslets: [
                new NullablePrefixParslet(),
                new OptionalParslet(),
                new NumberParslet(),
                new ParenthesisParslet(),
                new SpecialTypesParslet(),
                new NotNullableParslet()
            ],
            infixParslets: [
                new ParameterListParslet({
                    allowTrailingComma: true
                }),
                new GenericParslet(),
                new UnionParslet(),
                new OptionalParslet(),
                new NullableInfixParslet(),
                new NotNullableParslet()
            ]
        };
    };

    class NamePathParslet {
        constructor(opts) {
            this.allowJsdocNamePaths = opts.allowJsdocNamePaths;
            this.allowedPropertyTokenTypes = [
                'Identifier',
                'StringValue',
                'Number'
            ];
        }
        accepts(type, next) {
            return (type === '.' && next !== '<') || (this.allowJsdocNamePaths && (type === '~' || type === '#'));
        }
        getPrecedence() {
            return Precedence.NAME_PATH;
        }
        parseInfix(parser, left) {
            let type;
            if (parser.consume('.')) {
                type = 'property';
            }
            else if (parser.consume('~')) {
                type = 'inner';
            }
            else {
                parser.consume('#');
                type = 'instance';
            }
            let right;
            const tokenType = this.allowedPropertyTokenTypes.find(token => parser.getToken().type === token);
            if (tokenType !== undefined) {
                const value = parser.getToken().text;
                parser.consume(tokenType);
                right = {
                    type: 'JsdocTypeProperty',
                    value: value
                };
            }
            else {
                const next = parser.parseIntermediateType(Precedence.NAME_PATH);
                if (next.type === 'JsdocTypeName' && next.value === 'event') {
                    right = {
                        type: 'JsdocTypeProperty',
                        value: 'event'
                    };
                }
                else if (next.type === 'JsdocTypeSpecialNamePath' && next.specialType === 'event') {
                    right = next;
                }
                else {
                    const validTokens = this.allowedPropertyTokenTypes.join(', ');
                    throw new Error(`Unexpected property value. Expecting token of type ${validTokens} or 'event' ` +
                        `name path. Next token is of type: ${parser.getToken().type}`);
                }
            }
            return {
                type: 'JsdocTypeNamePath',
                left: assertTerminal(left),
                right,
                pathType: type
            };
        }
    }

    class KeyValueParslet {
        constructor(opts) {
            this.allowKeyTypes = opts.allowKeyTypes;
            this.allowOptional = opts.allowOptional;
            this.allowReadonly = opts.allowReadonly;
        }
        accepts(type, next) {
            return type === ':';
        }
        getPrecedence() {
            return Precedence.KEY_VALUE;
        }
        parseInfix(parser, left) {
            let optional = false;
            let readonlyProperty = false;
            if (this.allowOptional && left.type === 'JsdocTypeNullable') {
                optional = true;
                left = left.element;
            }
            if (this.allowReadonly && left.type === 'JsdocTypeReadonlyProperty') {
                readonlyProperty = true;
                left = left.element;
            }
            if (left.type === 'JsdocTypeNumber' || left.type === 'JsdocTypeName' || left.type === 'JsdocTypeStringValue') {
                parser.consume(':');
                let quote;
                if (left.type === 'JsdocTypeStringValue') {
                    quote = left.meta.quote;
                }
                return {
                    type: 'JsdocTypeKeyValue',
                    key: left.value.toString(),
                    right: parser.parseType(Precedence.KEY_VALUE),
                    optional: optional,
                    readonly: readonlyProperty,
                    meta: {
                        quote,
                        hasLeftSideExpression: false
                    }
                };
            }
            else {
                if (!this.allowKeyTypes) {
                    throw new UnexpectedTypeError(left);
                }
                parser.consume(':');
                return {
                    type: 'JsdocTypeKeyValue',
                    left: assertTerminal(left),
                    right: parser.parseType(Precedence.KEY_VALUE),
                    meta: {
                        hasLeftSideExpression: true
                    }
                };
            }
        }
    }

    class VariadicParslet {
        constructor(opts) {
            this.allowEnclosingBrackets = opts.allowEnclosingBrackets;
        }
        accepts(type) {
            return type === '...';
        }
        getPrecedence() {
            return Precedence.PREFIX;
        }
        parsePrefix(parser) {
            parser.consume('...');
            const brackets = this.allowEnclosingBrackets && parser.consume('[');
            if (!parser.canParseType()) {
                if (brackets) {
                    throw new Error('Empty square brackets for variadic are not allowed.');
                }
                return {
                    type: 'JsdocTypeVariadic',
                    meta: {
                        position: undefined,
                        squareBrackets: false
                    }
                };
            }
            const element = parser.parseType(Precedence.PREFIX);
            if (brackets && !parser.consume(']')) {
                throw new Error('Unterminated variadic type. Missing \']\'');
            }
            return {
                type: 'JsdocTypeVariadic',
                element: assertTerminal(element),
                meta: {
                    position: 'prefix',
                    squareBrackets: brackets
                }
            };
        }
        parseInfix(parser, left) {
            parser.consume('...');
            return {
                type: 'JsdocTypeVariadic',
                element: assertTerminal(left),
                meta: {
                    position: 'suffix',
                    squareBrackets: false
                }
            };
        }
    }

    class NameParslet {
        constructor(options) {
            this.allowedAdditionalTokens = options.allowedAdditionalTokens;
        }
        accepts(type, next) {
            return type === 'Identifier' || type === 'this' || type === 'new' || this.allowedAdditionalTokens.includes(type);
        }
        getPrecedence() {
            return Precedence.PREFIX;
        }
        parsePrefix(parser) {
            const token = parser.getToken();
            parser.consume('Identifier') || parser.consume('this') || parser.consume('new') ||
                this.allowedAdditionalTokens.some(type => parser.consume(type));
            return {
                type: 'JsdocTypeName',
                value: token.text
            };
        }
    }

    const moduleGrammar = () => ({
        prefixParslets: [
            new SpecialNamePathParslet({
                allowedTypes: ['event']
            }),
            new NameParslet({
                allowedAdditionalTokens: ['module', 'external']
            }),
            new NumberParslet(),
            new StringValueParslet()
        ],
        infixParslets: [
            new NamePathParslet({
                allowJsdocNamePaths: true
            })
        ]
    });

    class SpecialNamePathParslet {
        constructor(opts) {
            this.allowedTypes = opts.allowedTypes;
        }
        accepts(type, next) {
            return this.allowedTypes.includes(type);
        }
        getPrecedence() {
            return Precedence.PREFIX;
        }
        parsePrefix(parser) {
            const type = this.allowedTypes.find(type => parser.consume(type));
            if (!parser.consume(':')) {
                return {
                    type: 'JsdocTypeName',
                    value: type
                };
            }
            const moduleParser = new Parser(moduleGrammar(), parser.getLexer());
            let result;
            let token = parser.getToken();
            if (parser.consume('StringValue')) {
                result = {
                    type: 'JsdocTypeSpecialNamePath',
                    value: token.text.slice(1, -1),
                    specialType: type,
                    meta: {
                        quote: token.text[0] === '\'' ? 'single' : 'double'
                    }
                };
            }
            else {
                let value = '';
                const allowed = ['Identifier', '@', '/'];
                while (allowed.some(type => parser.consume(type))) {
                    value += token.text;
                    token = parser.getToken();
                }
                result = {
                    type: 'JsdocTypeSpecialNamePath',
                    value,
                    specialType: type,
                    meta: {
                        quote: undefined
                    }
                };
            }
            return assertTerminal(moduleParser.parseInfixIntermediateType(result, Precedence.ALL));
        }
    }

    class ObjectParslet {
        constructor(opts) {
            this.allowKeyTypes = opts.allowKeyTypes;
        }
        accepts(type) {
            return type === '{';
        }
        getPrecedence() {
            return Precedence.OBJECT;
        }
        parsePrefix(parser) {
            parser.consume('{');
            const result = {
                type: 'JsdocTypeObject',
                meta: {
                    separator: 'comma'
                },
                elements: []
            };
            if (!parser.consume('}')) {
                let separator;
                while (true) {
                    let field = parser.parseIntermediateType(Precedence.OBJECT);
                    let optional = false;
                    if (field.type === 'JsdocTypeNullable') {
                        optional = true;
                        field = field.element;
                    }
                    if (field.type === 'JsdocTypeNumber' || field.type === 'JsdocTypeName' || field.type === 'JsdocTypeStringValue') {
                        let quote;
                        if (field.type === 'JsdocTypeStringValue') {
                            quote = field.meta.quote;
                        }
                        result.elements.push({
                            type: 'JsdocTypeKeyValue',
                            key: field.value.toString(),
                            right: undefined,
                            optional: optional,
                            readonly: false,
                            meta: {
                                quote,
                                hasLeftSideExpression: false
                            }
                        });
                    }
                    else if (field.type === 'JsdocTypeKeyValue') {
                        result.elements.push(field);
                    }
                    else {
                        throw new UnexpectedTypeError(field);
                    }
                    if (parser.consume(',')) {
                        separator = 'comma';
                    }
                    else if (parser.consume(';')) {
                        separator = 'semicolon';
                    }
                    else {
                        break;
                    }
                }
                result.meta.separator = separator !== null && separator !== void 0 ? separator : 'comma';
                if (!parser.consume('}')) {
                    throw new Error('Unterminated record type. Missing \'}\'');
                }
            }
            return result;
        }
    }

    const jsdocGrammar = () => {
        const { prefixParslets, infixParslets } = baseGrammar();
        return {
            prefixParslets: [
                ...prefixParslets,
                new ObjectParslet({
                    allowKeyTypes: true
                }),
                new FunctionParslet({
                    allowWithoutParenthesis: true,
                    allowNamedParameters: ['this', 'new'],
                    allowNoReturnType: true
                }),
                new StringValueParslet(),
                new SpecialNamePathParslet({
                    allowedTypes: ['module', 'external', 'event']
                }),
                new VariadicParslet({
                    allowEnclosingBrackets: true
                }),
                new NameParslet({
                    allowedAdditionalTokens: ['keyof']
                })
            ],
            infixParslets: [
                ...infixParslets,
                new SymbolParslet(),
                new ArrayBracketsParslet(),
                new NamePathParslet({
                    allowJsdocNamePaths: true
                }),
                new KeyValueParslet({
                    allowKeyTypes: true,
                    allowOptional: false,
                    allowReadonly: false
                }),
                new VariadicParslet({
                    allowEnclosingBrackets: true
                })
            ]
        };
    };

    class TypeOfParslet {
        accepts(type, next) {
            return type === 'typeof';
        }
        getPrecedence() {
            return Precedence.KEY_OF_TYPE_OF;
        }
        parsePrefix(parser) {
            parser.consume('typeof');
            return {
                type: 'JsdocTypeTypeof',
                element: assertTerminal(parser.parseType(Precedence.KEY_OF_TYPE_OF))
            };
        }
    }

    const closureGrammar = () => {
        const { prefixParslets, infixParslets } = baseGrammar();
        return {
            prefixParslets: [
                ...prefixParslets,
                new ObjectParslet({
                    allowKeyTypes: false
                }),
                new NameParslet({
                    allowedAdditionalTokens: ['event', 'external']
                }),
                new TypeOfParslet(),
                new FunctionParslet({
                    allowWithoutParenthesis: false,
                    allowNamedParameters: ['this', 'new'],
                    allowNoReturnType: true
                }),
                new VariadicParslet({
                    allowEnclosingBrackets: false
                }),
                new NameParslet({
                    allowedAdditionalTokens: ['keyof']
                }),
                new SpecialNamePathParslet({
                    allowedTypes: ['module']
                })
            ],
            infixParslets: [
                ...infixParslets,
                new NamePathParslet({
                    allowJsdocNamePaths: true
                }),
                new KeyValueParslet({
                    allowKeyTypes: false,
                    allowOptional: false,
                    allowReadonly: false
                }),
                new SymbolParslet()
            ]
        };
    };

    class TupleParslet {
        constructor(opts) {
            this.allowQuestionMark = opts.allowQuestionMark;
        }
        accepts(type, next) {
            return type === '[';
        }
        getPrecedence() {
            return Precedence.TUPLE;
        }
        parsePrefix(parser) {
            parser.consume('[');
            const result = {
                type: 'JsdocTypeTuple',
                elements: []
            };
            if (parser.consume(']')) {
                return result;
            }
            const typeList = parser.parseIntermediateType(Precedence.ALL);
            if (typeList.type === 'JsdocTypeParameterList') {
                if (typeList.elements[0].type === 'JsdocTypeKeyValue') {
                    result.elements = typeList.elements.map(assertPlainKeyValue);
                }
                else {
                    result.elements = typeList.elements.map(assertTerminal);
                }
            }
            else {
                if (typeList.type === 'JsdocTypeKeyValue') {
                    result.elements = [assertPlainKeyValue(typeList)];
                }
                else {
                    result.elements = [assertTerminal(typeList)];
                }
            }
            if (!parser.consume(']')) {
                throw new Error('Unterminated \'[\'');
            }
            if (!this.allowQuestionMark && result.elements.some((e) => e.type === 'JsdocTypeUnknown')) {
                throw new Error('Question mark in tuple not allowed');
            }
            return result;
        }
    }

    class KeyOfParslet {
        accepts(type, next) {
            return type === 'keyof';
        }
        getPrecedence() {
            return Precedence.KEY_OF_TYPE_OF;
        }
        parsePrefix(parser) {
            parser.consume('keyof');
            return {
                type: 'JsdocTypeKeyof',
                element: assertTerminal(parser.parseType(Precedence.KEY_OF_TYPE_OF))
            };
        }
    }

    class ImportParslet {
        accepts(type, next) {
            return type === 'import';
        }
        getPrecedence() {
            return Precedence.PREFIX;
        }
        parsePrefix(parser) {
            parser.consume('import');
            if (!parser.consume('(')) {
                throw new Error('Missing parenthesis after import keyword');
            }
            const path = parser.parseType(Precedence.PREFIX);
            if (path.type !== 'JsdocTypeStringValue') {
                throw new Error('Only string values are allowed as paths for imports');
            }
            if (!parser.consume(')')) {
                throw new Error('Missing closing parenthesis after import keyword');
            }
            return {
                type: 'JsdocTypeImport',
                element: path
            };
        }
    }

    class ArrowFunctionParslet extends BaseFunctionParslet {
        accepts(type, next) {
            return type === '=>';
        }
        getPrecedence() {
            return Precedence.ARROW;
        }
        parseInfix(parser, left) {
            parser.consume('=>');
            return {
                type: 'JsdocTypeFunction',
                parameters: this.getParameters(left).map(assertPlainKeyValueOrName),
                arrow: true,
                parenthesis: true,
                returnType: parser.parseType(Precedence.ALL)
            };
        }
    }

    class IntersectionParslet {
        accepts(type) {
            return type === '&';
        }
        getPrecedence() {
            return Precedence.INTERSECTION;
        }
        parseInfix(parser, left) {
            parser.consume('&');
            const elements = [];
            do {
                elements.push(parser.parseType(Precedence.INTERSECTION));
            } while (parser.consume('&'));
            return {
                type: 'JsdocTypeIntersection',
                elements: [assertTerminal(left), ...elements]
            };
        }
    }

    class ReadonlyPropertyParslet {
        accepts(type, next) {
            return type === 'readonly';
        }
        getPrecedence() {
            return Precedence.PREFIX;
        }
        parsePrefix(parser) {
            parser.consume('readonly');
            return {
                type: 'JsdocTypeReadonlyProperty',
                element: parser.parseType(Precedence.KEY_VALUE)
            };
        }
    }

    const typescriptGrammar = () => {
        const { prefixParslets, infixParslets } = baseGrammar();
        // module seems not to be supported
        return {
            parallel: [
                moduleGrammar()
            ],
            prefixParslets: [
                ...prefixParslets,
                new ObjectParslet({
                    allowKeyTypes: false
                }),
                new TypeOfParslet(),
                new KeyOfParslet(),
                new ImportParslet(),
                new StringValueParslet(),
                new FunctionParslet({
                    allowWithoutParenthesis: true,
                    allowNoReturnType: false,
                    allowNamedParameters: ['this', 'new']
                }),
                new TupleParslet({
                    allowQuestionMark: false
                }),
                new VariadicParslet({
                    allowEnclosingBrackets: false
                }),
                new NameParslet({
                    allowedAdditionalTokens: ['event', 'external']
                }),
                new SpecialNamePathParslet({
                    allowedTypes: ['module']
                }),
                new ReadonlyPropertyParslet()
            ],
            infixParslets: [
                ...infixParslets,
                new ArrayBracketsParslet(),
                new ArrowFunctionParslet(),
                new NamePathParslet({
                    allowJsdocNamePaths: false
                }),
                new KeyValueParslet({
                    allowKeyTypes: false,
                    allowOptional: true,
                    allowReadonly: true
                }),
                new IntersectionParslet()
            ]
        };
    };

    const parsers = {
        jsdoc: new Parser(jsdocGrammar()),
        closure: new Parser(closureGrammar()),
        typescript: new Parser(typescriptGrammar())
    };
    /**
     * This function parses the given expression in the given mode and produces a {@link ParseResult}.
     * @param expression
     * @param mode
     */
    function parse(expression, mode) {
        return parsers[mode].parseText(expression);
    }
    /**
     * This function tries to parse the given expression in multiple modes and returns the first successful
     * {@link ParseResult}. By default it tries `'typescript'`, `'closure'` and `'jsdoc'` in this order. If
     * no mode was successful it throws the error that was produced by the last parsing attempt.
     * @param expression
     * @param modes
     */
    function tryParse(expression, modes = ['typescript', 'closure', 'jsdoc']) {
        let error;
        for (const mode of modes) {
            try {
                return parsers[mode].parseText(expression);
            }
            catch (e) {
                error = e;
            }
        }
        throw error;
    }

    function transform(rules, parseResult) {
        const rule = rules[parseResult.type];
        if (rule === undefined) {
            throw new Error(`In this set of transform rules exists no rule for type ${parseResult.type}.`);
        }
        return rule(parseResult, aParseResult => transform(rules, aParseResult));
    }
    function notAvailableTransform(parseResult) {
        throw new Error('This transform is not available. Are you trying the correct parsing mode?');
    }
    function extractSpecialParams(source) {
        const result = {
            params: []
        };
        for (const param of source.parameters) {
            if (param.type === 'JsdocTypeKeyValue' && param.meta.quote === undefined) {
                if (param.key === 'this') {
                    result.this = param.right;
                }
                else if (param.key === 'new') {
                    result.new = param.right;
                }
                else {
                    result.params.push(param);
                }
            }
            else {
                result.params.push(param);
            }
        }
        return result;
    }

    function applyPosition(position, target, value) {
        return position === 'prefix' ? value + target : target + value;
    }
    function quote(value, quote) {
        switch (quote) {
            case 'double':
                return `"${value}"`;
            case 'single':
                return `'${value}'`;
            case undefined:
                return value;
        }
    }
    function stringifyRules() {
        return {
            JsdocTypeParenthesis: (result, transform) => `(${result.element !== undefined ? transform(result.element) : ''})`,
            JsdocTypeKeyof: (result, transform) => `keyof ${transform(result.element)}`,
            JsdocTypeFunction: (result, transform) => {
                if (!result.arrow) {
                    let stringified = 'function';
                    if (!result.parenthesis) {
                        return stringified;
                    }
                    stringified += `(${result.parameters.map(transform).join(', ')})`;
                    if (result.returnType !== undefined) {
                        stringified += `: ${transform(result.returnType)}`;
                    }
                    return stringified;
                }
                else {
                    if (result.returnType === undefined) {
                        throw new Error('Arrow function needs a return type.');
                    }
                    return `(${result.parameters.map(transform).join(', ')}) => ${transform(result.returnType)}`;
                }
            },
            JsdocTypeName: result => result.value,
            JsdocTypeTuple: (result, transform) => `[${result.elements.map(transform).join(', ')}]`,
            JsdocTypeVariadic: (result, transform) => result.meta.position === undefined
                ? '...'
                : applyPosition(result.meta.position, transform(result.element), '...'),
            JsdocTypeNamePath: (result, transform) => {
                const joiner = result.pathType === 'inner' ? '~' : result.pathType === 'instance' ? '#' : '.';
                return `${transform(result.left)}${joiner}${transform(result.right)}`;
            },
            JsdocTypeStringValue: result => quote(result.value, result.meta.quote),
            JsdocTypeAny: () => '*',
            JsdocTypeGeneric: (result, transform) => {
                if (result.meta.brackets === 'square') {
                    const element = result.elements[0];
                    const transformed = transform(element);
                    if (element.type === 'JsdocTypeUnion' || element.type === 'JsdocTypeIntersection') {
                        return `(${transformed})[]`;
                    }
                    else {
                        return `${transformed}[]`;
                    }
                }
                else {
                    return `${transform(result.left)}${result.meta.dot ? '.' : ''}<${result.elements.map(transform).join(', ')}>`;
                }
            },
            JsdocTypeImport: (result, transform) => `import(${transform(result.element)})`,
            JsdocTypeKeyValue: (result, transform) => {
                if (isPlainKeyValue(result)) {
                    let text = '';
                    if (result.readonly) {
                        text += 'readonly ';
                    }
                    text += quote(result.key, result.meta.quote);
                    if (result.optional) {
                        text += '?';
                    }
                    if (result.right === undefined) {
                        return text;
                    }
                    else {
                        return text + `: ${transform(result.right)}`;
                    }
                }
                else {
                    return `${transform(result.left)}: ${transform(result.right)}`;
                }
            },
            JsdocTypeSpecialNamePath: result => `${result.specialType}:${quote(result.value, result.meta.quote)}`,
            JsdocTypeNotNullable: (result, transform) => applyPosition(result.meta.position, transform(result.element), '!'),
            JsdocTypeNull: () => 'null',
            JsdocTypeNullable: (result, transform) => applyPosition(result.meta.position, transform(result.element), '?'),
            JsdocTypeNumber: result => result.value.toString(),
            JsdocTypeObject: (result, transform) => `{${result.elements.map(transform).join((result.meta.separator === 'comma' ? ',' : ';') + ' ')}}`,
            JsdocTypeOptional: (result, transform) => applyPosition(result.meta.position, transform(result.element), '='),
            JsdocTypeSymbol: (result, transform) => `${result.value}(${result.element !== undefined ? transform(result.element) : ''})`,
            JsdocTypeTypeof: (result, transform) => `typeof ${transform(result.element)}`,
            JsdocTypeUndefined: () => 'undefined',
            JsdocTypeUnion: (result, transform) => result.elements.map(transform).join(' | '),
            JsdocTypeUnknown: () => '?',
            JsdocTypeIntersection: (result, transform) => result.elements.map(transform).join(' & '),
            JsdocTypeProperty: result => result.value
        };
    }
    const storedStringifyRules = stringifyRules();
    function stringify(result) {
        return transform(storedStringifyRules, result);
    }

    const reservedWords = [
        'null',
        'true',
        'false',
        'break',
        'case',
        'catch',
        'class',
        'const',
        'continue',
        'debugger',
        'default',
        'delete',
        'do',
        'else',
        'export',
        'extends',
        'finally',
        'for',
        'function',
        'if',
        'import',
        'in',
        'instanceof',
        'new',
        'return',
        'super',
        'switch',
        'this',
        'throw',
        'try',
        'typeof',
        'var',
        'void',
        'while',
        'with',
        'yield'
    ];
    function makeName(value) {
        const result = {
            type: 'NameExpression',
            name: value
        };
        if (reservedWords.includes(value)) {
            result.reservedWord = true;
        }
        return result;
    }
    const catharsisTransformRules = {
        JsdocTypeOptional: (result, transform) => {
            const transformed = transform(result.element);
            transformed.optional = true;
            return transformed;
        },
        JsdocTypeNullable: (result, transform) => {
            const transformed = transform(result.element);
            transformed.nullable = true;
            return transformed;
        },
        JsdocTypeNotNullable: (result, transform) => {
            const transformed = transform(result.element);
            transformed.nullable = false;
            return transformed;
        },
        JsdocTypeVariadic: (result, transform) => {
            if (result.element === undefined) {
                throw new Error('dots without value are not allowed in catharsis mode');
            }
            const transformed = transform(result.element);
            transformed.repeatable = true;
            return transformed;
        },
        JsdocTypeAny: () => ({
            type: 'AllLiteral'
        }),
        JsdocTypeNull: () => ({
            type: 'NullLiteral'
        }),
        JsdocTypeStringValue: result => makeName(quote(result.value, result.meta.quote)),
        JsdocTypeUndefined: () => ({
            type: 'UndefinedLiteral'
        }),
        JsdocTypeUnknown: () => ({
            type: 'UnknownLiteral'
        }),
        JsdocTypeFunction: (result, transform) => {
            const params = extractSpecialParams(result);
            const transformed = {
                type: 'FunctionType',
                params: params.params.map(transform)
            };
            if (params.this !== undefined) {
                transformed.this = transform(params.this);
            }
            if (params.new !== undefined) {
                transformed.new = transform(params.new);
            }
            if (result.returnType !== undefined) {
                transformed.result = transform(result.returnType);
            }
            return transformed;
        },
        JsdocTypeGeneric: (result, transform) => ({
            type: 'TypeApplication',
            applications: result.elements.map(o => transform(o)),
            expression: transform(result.left)
        }),
        JsdocTypeSpecialNamePath: result => makeName(result.specialType + ':' + quote(result.value, result.meta.quote)),
        JsdocTypeName: result => {
            if (result.value !== 'function') {
                return makeName(result.value);
            }
            else {
                return {
                    type: 'FunctionType',
                    params: []
                };
            }
        },
        JsdocTypeNumber: result => makeName(result.value.toString()),
        JsdocTypeObject: (result, transform) => {
            const transformed = {
                type: 'RecordType',
                fields: []
            };
            for (const field of result.elements) {
                if (field.type !== 'JsdocTypeKeyValue') {
                    transformed.fields.push({
                        type: 'FieldType',
                        key: transform(field),
                        value: undefined
                    });
                }
                else {
                    transformed.fields.push(transform(field));
                }
            }
            return transformed;
        },
        JsdocTypeUnion: (result, transform) => ({
            type: 'TypeUnion',
            elements: result.elements.map(e => transform(e))
        }),
        JsdocTypeKeyValue: (result, transform) => {
            if (isPlainKeyValue(result)) {
                return {
                    type: 'FieldType',
                    key: makeName(quote(result.key, result.meta.quote)),
                    value: result.right === undefined ? undefined : transform(result.right)
                };
            }
            else {
                return {
                    type: 'FieldType',
                    key: transform(result.left),
                    value: transform(result.right)
                };
            }
        },
        JsdocTypeNamePath: (result, transform) => {
            const leftResult = transform(result.left);
            let rightValue;
            if (result.right.type === 'JsdocTypeSpecialNamePath') {
                rightValue = transform(result.right).name;
            }
            else {
                rightValue = result.right.value;
            }
            const joiner = result.pathType === 'inner' ? '~' : result.pathType === 'instance' ? '#' : '.';
            return makeName(`${leftResult.name}${joiner}${rightValue}`);
        },
        JsdocTypeSymbol: result => {
            let value = '';
            let element = result.element;
            let trailingDots = false;
            if ((element === null || element === void 0 ? void 0 : element.type) === 'JsdocTypeVariadic') {
                if (element.meta.position === 'prefix') {
                    value = '...';
                }
                else {
                    trailingDots = true;
                }
                element = element.element;
            }
            if ((element === null || element === void 0 ? void 0 : element.type) === 'JsdocTypeName') {
                value += element.value;
            }
            else if ((element === null || element === void 0 ? void 0 : element.type) === 'JsdocTypeNumber') {
                value += element.value.toString();
            }
            if (trailingDots) {
                value += '...';
            }
            return makeName(`${result.value}(${value})`);
        },
        JsdocTypeParenthesis: (result, transform) => transform(assertTerminal(result.element)),
        JsdocTypeImport: notAvailableTransform,
        JsdocTypeKeyof: notAvailableTransform,
        JsdocTypeTuple: notAvailableTransform,
        JsdocTypeTypeof: notAvailableTransform,
        JsdocTypeIntersection: notAvailableTransform,
        JsdocTypeProperty: notAvailableTransform
    };
    function catharsisTransform(result) {
        return transform(catharsisTransformRules, result);
    }

    function getQuoteStyle(quote) {
        switch (quote) {
            case undefined:
                return 'none';
            case 'single':
                return 'single';
            case 'double':
                return 'double';
        }
    }
    function getMemberType(type) {
        switch (type) {
            case 'inner':
                return 'INNER_MEMBER';
            case 'instance':
                return 'INSTANCE_MEMBER';
            case 'property':
                return 'MEMBER';
        }
    }
    function nestResults(type, results) {
        if (results.length === 2) {
            return {
                type,
                left: results[0],
                right: results[1]
            };
        }
        else {
            return {
                type,
                left: results[0],
                right: nestResults(type, results.slice(1))
            };
        }
    }
    const jtpRules = {
        JsdocTypeOptional: (result, transform) => ({
            type: 'OPTIONAL',
            value: transform(result.element),
            meta: {
                syntax: result.meta.position === 'prefix' ? 'PREFIX_EQUAL_SIGN' : 'SUFFIX_EQUALS_SIGN'
            }
        }),
        JsdocTypeNullable: (result, transform) => ({
            type: 'NULLABLE',
            value: transform(result.element),
            meta: {
                syntax: result.meta.position === 'prefix' ? 'PREFIX_QUESTION_MARK' : 'SUFFIX_QUESTION_MARK'
            }
        }),
        JsdocTypeNotNullable: (result, transform) => ({
            type: 'NOT_NULLABLE',
            value: transform(result.element),
            meta: {
                syntax: result.meta.position === 'prefix' ? 'PREFIX_BANG' : 'SUFFIX_BANG'
            }
        }),
        JsdocTypeVariadic: (result, transform) => {
            const transformed = {
                type: 'VARIADIC',
                meta: {
                    syntax: result.meta.position === 'prefix'
                        ? 'PREFIX_DOTS'
                        : result.meta.position === 'suffix' ? 'SUFFIX_DOTS' : 'ONLY_DOTS'
                }
            };
            if (result.element !== undefined) {
                transformed.value = transform(result.element);
            }
            return transformed;
        },
        JsdocTypeName: result => ({
            type: 'NAME',
            name: result.value
        }),
        JsdocTypeTypeof: (result, transform) => ({
            type: 'TYPE_QUERY',
            name: transform(result.element)
        }),
        JsdocTypeTuple: (result, transform) => ({
            type: 'TUPLE',
            entries: result.elements.map(transform)
        }),
        JsdocTypeKeyof: (result, transform) => ({
            type: 'KEY_QUERY',
            value: transform(result.element)
        }),
        JsdocTypeImport: result => ({
            type: 'IMPORT',
            path: {
                type: 'STRING_VALUE',
                quoteStyle: getQuoteStyle(result.element.meta.quote),
                string: result.element.value
            }
        }),
        JsdocTypeUndefined: () => ({
            type: 'NAME',
            name: 'undefined'
        }),
        JsdocTypeAny: () => ({
            type: 'ANY'
        }),
        JsdocTypeFunction: (result, transform) => {
            const specialParams = extractSpecialParams(result);
            const transformed = {
                type: result.arrow ? 'ARROW' : 'FUNCTION',
                params: specialParams.params.map(param => {
                    if (param.type === 'JsdocTypeKeyValue') {
                        if (param.right === undefined) {
                            throw new Error('Function parameter without \':\' is not expected to be \'KEY_VALUE\'');
                        }
                        return {
                            type: 'NAMED_PARAMETER',
                            name: param.key,
                            typeName: transform(param.right)
                        };
                    }
                    else {
                        return transform(param);
                    }
                }),
                new: null,
                returns: null
            };
            if (specialParams.this !== undefined) {
                transformed.this = transform(specialParams.this);
            }
            else if (!result.arrow) {
                transformed.this = null;
            }
            if (specialParams.new !== undefined) {
                transformed.new = transform(specialParams.new);
            }
            if (result.returnType !== undefined) {
                transformed.returns = transform(result.returnType);
            }
            return transformed;
        },
        JsdocTypeGeneric: (result, transform) => {
            const transformed = {
                type: 'GENERIC',
                subject: transform(result.left),
                objects: result.elements.map(transform),
                meta: {
                    syntax: result.meta.brackets === 'square' ? 'SQUARE_BRACKET' : result.meta.dot ? 'ANGLE_BRACKET_WITH_DOT' : 'ANGLE_BRACKET'
                }
            };
            if (result.meta.brackets === 'square' && result.elements[0].type === 'JsdocTypeFunction' && !result.elements[0].parenthesis) {
                transformed.objects[0] = {
                    type: 'NAME',
                    name: 'function'
                };
            }
            return transformed;
        },
        JsdocTypeKeyValue: (result, transform) => {
            if (!isPlainKeyValue(result)) {
                throw new Error('Keys may not be typed in jsdoctypeparser.');
            }
            if (result.right === undefined) {
                return {
                    type: 'RECORD_ENTRY',
                    key: result.key.toString(),
                    quoteStyle: getQuoteStyle(result.meta.quote),
                    value: null,
                    readonly: false
                };
            }
            let right = transform(result.right);
            if (result.optional) {
                right = {
                    type: 'OPTIONAL',
                    value: right,
                    meta: {
                        syntax: 'SUFFIX_KEY_QUESTION_MARK'
                    }
                };
            }
            return {
                type: 'RECORD_ENTRY',
                key: result.key.toString(),
                quoteStyle: getQuoteStyle(result.meta.quote),
                value: right,
                readonly: false
            };
        },
        JsdocTypeObject: (result, transform) => {
            const entries = [];
            for (const field of result.elements) {
                if (field.type === 'JsdocTypeKeyValue') {
                    entries.push(transform(field));
                }
            }
            return {
                type: 'RECORD',
                entries
            };
        },
        JsdocTypeSpecialNamePath: result => {
            if (result.specialType !== 'module') {
                throw new Error(`jsdoctypeparser does not support type ${result.specialType} at this point.`);
            }
            return {
                type: 'MODULE',
                value: {
                    type: 'FILE_PATH',
                    quoteStyle: getQuoteStyle(result.meta.quote),
                    path: result.value
                }
            };
        },
        JsdocTypeNamePath: (result, transform) => {
            let hasEventPrefix = false;
            let name;
            let quoteStyle;
            if (result.right.type === 'JsdocTypeSpecialNamePath' && result.right.specialType === 'event') {
                hasEventPrefix = true;
                name = result.right.value;
                quoteStyle = getQuoteStyle(result.right.meta.quote);
            }
            else {
                let quote;
                let value = result.right.value;
                if (value[0] === '\'') {
                    quote = 'single';
                    value = value.slice(1, -1);
                }
                else if (value[0] === '"') {
                    quote = 'double';
                    value = value.slice(1, -1);
                }
                name = `${value}`;
                quoteStyle = getQuoteStyle(quote);
            }
            const transformed = {
                type: getMemberType(result.pathType),
                owner: transform(result.left),
                name,
                quoteStyle,
                hasEventPrefix
            };
            if (transformed.owner.type === 'MODULE') {
                const tModule = transformed.owner;
                transformed.owner = transformed.owner.value;
                tModule.value = transformed;
                return tModule;
            }
            else {
                return transformed;
            }
        },
        JsdocTypeUnion: (result, transform) => nestResults('UNION', result.elements.map(transform)),
        JsdocTypeParenthesis: (result, transform) => ({
            type: 'PARENTHESIS',
            value: transform(assertTerminal(result.element))
        }),
        JsdocTypeNull: () => ({
            type: 'NAME',
            name: 'null'
        }),
        JsdocTypeUnknown: () => ({
            type: 'UNKNOWN'
        }),
        JsdocTypeStringValue: result => ({
            type: 'STRING_VALUE',
            quoteStyle: getQuoteStyle(result.meta.quote),
            string: result.value
        }),
        JsdocTypeIntersection: (result, transform) => nestResults('INTERSECTION', result.elements.map(transform)),
        JsdocTypeNumber: result => ({
            type: 'NUMBER_VALUE',
            number: result.value.toString()
        }),
        JsdocTypeSymbol: notAvailableTransform,
        JsdocTypeProperty: notAvailableTransform
    };
    function jtpTransform(result) {
        return transform(jtpRules, result);
    }

    function identityTransformRules() {
        return {
            JsdocTypeIntersection: (result, transform) => ({
                type: 'JsdocTypeIntersection',
                elements: result.elements.map(transform)
            }),
            JsdocTypeGeneric: (result, transform) => ({
                type: 'JsdocTypeGeneric',
                left: transform(result.left),
                elements: result.elements.map(transform),
                meta: {
                    dot: result.meta.dot,
                    brackets: result.meta.brackets
                }
            }),
            JsdocTypeNullable: result => result,
            JsdocTypeUnion: (result, transform) => ({
                type: 'JsdocTypeUnion',
                elements: result.elements.map(transform)
            }),
            JsdocTypeUnknown: result => result,
            JsdocTypeUndefined: result => result,
            JsdocTypeTypeof: (result, transform) => ({
                type: 'JsdocTypeTypeof',
                element: transform(result.element)
            }),
            JsdocTypeSymbol: (result, transform) => {
                const transformed = {
                    type: 'JsdocTypeSymbol',
                    value: result.value
                };
                if (result.element !== undefined) {
                    transformed.element = transform(result.element);
                }
                return transformed;
            },
            JsdocTypeOptional: (result, transform) => ({
                type: 'JsdocTypeOptional',
                element: transform(result.element),
                meta: {
                    position: result.meta.position
                }
            }),
            JsdocTypeObject: (result, transform) => ({
                type: 'JsdocTypeObject',
                meta: {
                    separator: 'comma'
                },
                elements: result.elements.map(transform)
            }),
            JsdocTypeNumber: result => result,
            JsdocTypeNull: result => result,
            JsdocTypeNotNullable: (result, transform) => ({
                type: 'JsdocTypeNotNullable',
                element: transform(result.element),
                meta: {
                    position: result.meta.position
                }
            }),
            JsdocTypeSpecialNamePath: result => result,
            JsdocTypeKeyValue: (result, transform) => {
                if (isPlainKeyValue(result)) {
                    return {
                        type: 'JsdocTypeKeyValue',
                        key: result.key,
                        right: result.right === undefined ? undefined : transform(result.right),
                        optional: result.optional,
                        readonly: result.readonly,
                        meta: result.meta
                    };
                }
                else {
                    return {
                        type: 'JsdocTypeKeyValue',
                        left: transform(result.left),
                        right: transform(result.right),
                        meta: result.meta
                    };
                }
            },
            JsdocTypeImport: (result, transform) => ({
                type: 'JsdocTypeImport',
                element: transform(result.element)
            }),
            JsdocTypeAny: result => result,
            JsdocTypeStringValue: result => result,
            JsdocTypeNamePath: result => result,
            JsdocTypeVariadic: (result, transform) => {
                const transformed = {
                    type: 'JsdocTypeVariadic',
                    meta: {
                        position: result.meta.position,
                        squareBrackets: result.meta.squareBrackets
                    }
                };
                if (result.element !== undefined) {
                    transformed.element = transform(result.element);
                }
                return transformed;
            },
            JsdocTypeTuple: (result, transform) => ({
                type: 'JsdocTypeTuple',
                elements: result.elements.map(transform)
            }),
            JsdocTypeName: result => result,
            JsdocTypeFunction: (result, transform) => {
                const transformed = {
                    type: 'JsdocTypeFunction',
                    arrow: result.arrow,
                    parameters: result.parameters.map(transform),
                    parenthesis: result.parenthesis
                };
                if (result.returnType !== undefined) {
                    transformed.returnType = transform(result.returnType);
                }
                return transformed;
            },
            JsdocTypeKeyof: (result, transform) => ({
                type: 'JsdocTypeKeyof',
                element: transform(result.element)
            }),
            JsdocTypeParenthesis: (result, transform) => ({
                type: 'JsdocTypeParenthesis',
                element: transform(result.element)
            }),
            JsdocTypeProperty: result => result
        };
    }

    const visitorKeys = {
        JsdocTypeAny: [],
        JsdocTypeFunction: ['parameters', 'returnType'],
        JsdocTypeGeneric: ['left', 'elements'],
        JsdocTypeImport: [],
        JsdocTypeIntersection: ['elements'],
        JsdocTypeKeyof: ['element'],
        JsdocTypeKeyValue: ['right'],
        JsdocTypeName: [],
        JsdocTypeNamePath: ['left', 'right'],
        JsdocTypeNotNullable: ['element'],
        JsdocTypeNull: [],
        JsdocTypeNullable: ['element'],
        JsdocTypeNumber: [],
        JsdocTypeObject: ['elements'],
        JsdocTypeOptional: ['element'],
        JsdocTypeParenthesis: ['element'],
        JsdocTypeSpecialNamePath: [],
        JsdocTypeStringValue: [],
        JsdocTypeSymbol: ['element'],
        JsdocTypeTuple: ['elements'],
        JsdocTypeTypeof: ['element'],
        JsdocTypeUndefined: [],
        JsdocTypeUnion: ['elements'],
        JsdocTypeUnknown: [],
        JsdocTypeVariadic: ['element'],
        JsdocTypeProperty: []
    };

    function _traverse(node, parentNode, property, onEnter, onLeave) {
        onEnter === null || onEnter === void 0 ? void 0 : onEnter(node, parentNode, property);
        const keysToVisit = visitorKeys[node.type];
        if (keysToVisit !== undefined) {
            for (const key of keysToVisit) {
                const value = node[key];
                if (value !== undefined) {
                    if (Array.isArray(value)) {
                        for (const element of value) {
                            _traverse(element, node, key, onEnter, onLeave);
                        }
                    }
                    else {
                        _traverse(value, node, key, onEnter, onLeave);
                    }
                }
            }
        }
        onLeave === null || onLeave === void 0 ? void 0 : onLeave(node, parentNode, property);
    }
    function traverse(node, onEnter, onLeave) {
        _traverse(node, undefined, undefined, onEnter, onLeave);
    }

    exports.catharsisTransform = catharsisTransform;
    exports.identityTransformRules = identityTransformRules;
    exports.jtpTransform = jtpTransform;
    exports.parse = parse;
    exports.stringify = stringify;
    exports.stringifyRules = stringifyRules;
    exports.transform = transform;
    exports.traverse = traverse;
    exports.tryParse = tryParse;
    exports.visitorKeys = visitorKeys;

    Object.defineProperty(exports, '__esModule', { value: true });

})));
