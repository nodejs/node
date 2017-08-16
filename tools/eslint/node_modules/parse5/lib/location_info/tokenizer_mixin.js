'use strict';

var UNICODE = require('../common/unicode');

//Aliases
var $ = UNICODE.CODE_POINTS;


exports.assign = function (tokenizer) {
    //NOTE: obtain Tokenizer proto this way to avoid module circular references
    var tokenizerProto = Object.getPrototypeOf(tokenizer),
        tokenStartOffset = -1,
        tokenCol = -1,
        tokenLine = 1,
        isEol = false,
        lineStartPosStack = [0],
        lineStartPos = 0,
        col = -1,
        line = 1;

    function attachLocationInfo(token) {
        /**
         * @typedef {Object} LocationInfo
         *
         * @property {Number} line - One-based line index
         * @property {Number} col - One-based column index
         * @property {Number} startOffset - Zero-based first character index
         * @property {Number} endOffset - Zero-based last character index
         */
        token.location = {
            line: tokenLine,
            col: tokenCol,
            startOffset: tokenStartOffset,
            endOffset: -1
        };
    }

    //NOTE: patch consumption method to track line/col information
    tokenizer._consume = function () {
        var cp = tokenizerProto._consume.call(this);

        //NOTE: LF should be in the last column of the line
        if (isEol) {
            isEol = false;
            line++;
            lineStartPosStack.push(this.preprocessor.sourcePos);
            lineStartPos = this.preprocessor.sourcePos;
        }

        if (cp === $.LINE_FEED)
            isEol = true;

        col = this.preprocessor.sourcePos - lineStartPos + 1;

        return cp;
    };

    tokenizer._unconsume = function () {
        tokenizerProto._unconsume.call(this);
        isEol = false;

        while (lineStartPos > this.preprocessor.sourcePos && lineStartPosStack.length > 1) {
            lineStartPos = lineStartPosStack.pop();
            line--;
        }

        col = this.preprocessor.sourcePos - lineStartPos + 1;
    };

    //NOTE: patch token creation methods and attach location objects
    tokenizer._createStartTagToken = function () {
        tokenizerProto._createStartTagToken.call(this);
        attachLocationInfo(this.currentToken);
    };

    tokenizer._createEndTagToken = function () {
        tokenizerProto._createEndTagToken.call(this);
        attachLocationInfo(this.currentToken);
    };

    tokenizer._createCommentToken = function () {
        tokenizerProto._createCommentToken.call(this);
        attachLocationInfo(this.currentToken);
    };

    tokenizer._createDoctypeToken = function (initialName) {
        tokenizerProto._createDoctypeToken.call(this, initialName);
        attachLocationInfo(this.currentToken);
    };

    tokenizer._createCharacterToken = function (type, ch) {
        tokenizerProto._createCharacterToken.call(this, type, ch);
        attachLocationInfo(this.currentCharacterToken);
    };

    tokenizer._createAttr = function (attrNameFirstCh) {
        tokenizerProto._createAttr.call(this, attrNameFirstCh);
        this.currentAttrLocation = {
            line: line,
            col: col,
            startOffset: this.preprocessor.sourcePos,
            endOffset: -1
        };
    };

    tokenizer._leaveAttrName = function (toState) {
        tokenizerProto._leaveAttrName.call(this, toState);
        this._attachCurrentAttrLocationInfo();
    };

    tokenizer._leaveAttrValue = function (toState) {
        tokenizerProto._leaveAttrValue.call(this, toState);
        this._attachCurrentAttrLocationInfo();
    };

    tokenizer._attachCurrentAttrLocationInfo = function () {
        this.currentAttrLocation.endOffset = this.preprocessor.sourcePos;

        if (!this.currentToken.location.attrs)
            this.currentToken.location.attrs = {};

        /**
         * @typedef {Object} StartTagLocationInfo
         * @extends LocationInfo
         *
         * @property {Dictionary<String, LocationInfo>} attrs - Start tag attributes' location info.
         */
        this.currentToken.location.attrs[this.currentAttr.name] = this.currentAttrLocation;
    };

    //NOTE: patch token emission methods to determine end location
    tokenizer._emitCurrentToken = function () {
        //NOTE: if we have pending character token make it's end location equal to the
        //current token's start location.
        if (this.currentCharacterToken)
            this.currentCharacterToken.location.endOffset = this.currentToken.location.startOffset;

        this.currentToken.location.endOffset = this.preprocessor.sourcePos + 1;
        tokenizerProto._emitCurrentToken.call(this);
    };

    tokenizer._emitCurrentCharacterToken = function () {
        //NOTE: if we have character token and it's location wasn't set in the _emitCurrentToken(),
        //then set it's location at the current preprocessor position.
        //We don't need to increment preprocessor position, since character token
        //emission is always forced by the start of the next character token here.
        //So, we already have advanced position.
        if (this.currentCharacterToken && this.currentCharacterToken.location.endOffset === -1)
            this.currentCharacterToken.location.endOffset = this.preprocessor.sourcePos;

        tokenizerProto._emitCurrentCharacterToken.call(this);
    };

    //NOTE: patch initial states for each mode to obtain token start position
    Object.keys(tokenizerProto.MODE)

        .map(function (modeName) {
            return tokenizerProto.MODE[modeName];
        })

        .forEach(function (state) {
            tokenizer[state] = function (cp) {
                tokenStartOffset = this.preprocessor.sourcePos;
                tokenLine = line;
                tokenCol = col;
                tokenizerProto[state].call(this, cp);
            };
        });
};
