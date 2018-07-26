// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

grammar Torque;

options {
  language=Cpp;
}

// parser rules start with lowercase letters, lexer rules with uppercase
MACRO: 'macro';
BUILTIN: 'builtin';
RUNTIME: 'runtime';
MODULE: 'module';
JAVASCRIPT: 'javascript';
IMPLICIT: 'implicit';
DEFERRED: 'deferred';
IF: 'if';
CAST_KEYWORD: 'cast';
CONVERT_KEYWORD: 'convert';
FOR: 'for';
WHILE: 'while';
RETURN: 'return';
CONSTEXPR: 'constexpr';
CONTINUE: 'continue';
BREAK: 'break';
GOTO: 'goto';
OTHERWISE: 'otherwise';
TRY: 'try';
CATCH: 'catch';
LABEL: 'label';
LABELS: 'labels';
TAIL: 'tail';
ISNT: 'isnt';
IS: 'is';
LET: 'let';
EXTERN: 'extern';
ASSERT: 'assert';
UNREACHABLE_TOKEN: 'unreachable';
DEBUG_TOKEN: 'debug';

ASSIGNMENT: '=';
ASSIGNMENT_OPERATOR
        : '*='
        | '/='
        | '%='
        | '+='
        | '-='
        | '<<='
        | '>>='
        | '>>>='
        | '&='
        | '^='
        | '|='
        ;

EQUAL: '==';
PLUS: '+';
MINUS: '-';
MULTIPLY: '*';
DIVIDE: '/';
MODULO: '%';
BIT_OR: '|';
BIT_AND: '&';
BIT_NOT: '~';
MAX: 'max';
MIN: 'min';
NOT_EQUAL: '!=';
LESS_THAN: '<';
LESS_THAN_EQUAL: '<=';
GREATER_THAN: '>';
GREATER_THAN_EQUAL: '>=';
SHIFT_LEFT: '<<';
SHIFT_RIGHT: '>>';
SHIFT_RIGHT_ARITHMETIC: '>>>';
VARARGS: '...';

EQUALITY_OPERATOR: EQUAL | NOT_EQUAL;

INCREMENT: '++';
DECREMENT: '--';
NOT: '!';

STRING_LITERAL : ('"' ( ESCAPE | ~('"' | '\\' | '\n' | '\r') ) * '"')
               | ('\'' ( ESCAPE | ~('\'' | '\\' | '\n' | '\r') ) * '\'');
fragment ESCAPE : '\\' ( '\'' | '\\' | '"' );

IDENTIFIER  :   [A-Za-z][0-9A-Za-z_]* ;

WS  : [ \t\r\n\f]+ -> channel(HIDDEN);

BLOCK_COMMENT
        : '/*' .*? ('*/' | EOF)  -> channel(HIDDEN)
        ;

LINE_COMMENT
        : '//' ~[\r\n]*  -> channel(HIDDEN)
        ;

fragment DECIMAL_DIGIT
        : [0-9]
        ;

fragment DECIMAL_INTEGER_LITERAL
        : '0'
        | [1-9] DECIMAL_DIGIT*
        ;

fragment EXPONENT_PART
        : [eE] [+-]? DECIMAL_DIGIT+
        ;

DECIMAL_LITERAL
        : MINUS? DECIMAL_INTEGER_LITERAL '.' DECIMAL_DIGIT* EXPONENT_PART?
        | MINUS? '.' DECIMAL_DIGIT+ EXPONENT_PART?
        | MINUS? DECIMAL_INTEGER_LITERAL EXPONENT_PART?
        ;

type : CONSTEXPR? IDENTIFIER
     | BUILTIN '(' typeList ')' '=>' type
     ;
typeList : (type (',' type)*)?;
genericSpecializationTypeList: '<' typeList '>';

optionalGenericTypeList: ('<' IDENTIFIER ':' 'type' (',' IDENTIFIER ':' 'type')* '>')?;

typeListMaybeVarArgs: '(' type? (',' type)* (',' VARARGS)? ')'
                    | '(' VARARGS ')';

labelParameter: IDENTIFIER ( '(' typeList ')' )?;

optionalType: (':' type)?;
optionalLabelList: (LABELS labelParameter (',' labelParameter)*)?;
optionalOtherwise: (OTHERWISE IDENTIFIER (',' IDENTIFIER)*)?;

parameter: IDENTIFIER ':' type?;
parameterList
        : '(' parameter? (',' parameter)* ')'
        | '(' parameter ',' parameter ',' VARARGS IDENTIFIER ')';
labelDeclaration: IDENTIFIER parameterList?;

expression
        : conditionalExpression;

conditionalExpression
        : logicalORExpression
        | conditionalExpression '?' logicalORExpression ':' logicalORExpression;

logicalORExpression
        : logicalANDExpression
        | logicalORExpression '||' logicalANDExpression;

logicalANDExpression
        : bitwiseExpression
        | logicalANDExpression '&&' bitwiseExpression;

bitwiseExpression
        : equalityExpression
        | bitwiseExpression op=(BIT_AND | BIT_OR)  equalityExpression;

equalityExpression
        : relationalExpression
        | equalityExpression
          op=(EQUAL | NOT_EQUAL)
          relationalExpression;

relationalExpression
        : shiftExpression
        | relationalExpression
          op=(LESS_THAN | LESS_THAN_EQUAL | GREATER_THAN | GREATER_THAN_EQUAL)
          shiftExpression;

shiftExpression
        : additiveExpression
        | shiftExpression op=(SHIFT_RIGHT | SHIFT_LEFT | SHIFT_RIGHT_ARITHMETIC) additiveExpression;

additiveExpression
        : multiplicativeExpression
        | additiveExpression op=(PLUS | MINUS) multiplicativeExpression;

multiplicativeExpression
        : unaryExpression
        | multiplicativeExpression op=(MULTIPLY | DIVIDE | MODULO) unaryExpression;

unaryExpression
        : assignmentExpression
        | op=(PLUS | MINUS | BIT_NOT | NOT) unaryExpression;

locationExpression
        : IDENTIFIER genericSpecializationTypeList?
        | locationExpression '.' IDENTIFIER
        | locationExpression '[' expression ']';

incrementDecrement
        : INCREMENT locationExpression
        | DECREMENT locationExpression
        | locationExpression op=INCREMENT
        | locationExpression op=DECREMENT
        ;

assignment
        : incrementDecrement
        | locationExpression ((ASSIGNMENT | ASSIGNMENT_OPERATOR) expression)?;

assignmentExpression
        : primaryExpression
        | assignment;

primaryExpression
        : helperCall
        | DECIMAL_LITERAL
        | STRING_LITERAL
        | CAST_KEYWORD '<' type '>' '(' expression ')' OTHERWISE IDENTIFIER
        | CONVERT_KEYWORD '<' type '>' '(' expression ')'
        | ('(' expression ')');

forInitialization : variableDeclarationWithInitialization?;
forLoop: FOR '(' forInitialization ';' expression ';' assignment ')' statementBlock;

rangeSpecifier: '[' begin=expression? ':' end=expression? ']';
forOfRange: rangeSpecifier?;
forOfLoop: FOR '(' variableDeclaration 'of' expression forOfRange ')' statementBlock;

argument: expression;
argumentList: '(' argument? (',' argument)* ')';

helperCall: (MIN | MAX | IDENTIFIER) genericSpecializationTypeList? argumentList optionalOtherwise;

labelReference: IDENTIFIER;
variableDeclaration: LET IDENTIFIER ':' type;
variableDeclarationWithInitialization: variableDeclaration (ASSIGNMENT expression)?;
helperCallStatement: (TAIL)? helperCall;
expressionStatement: assignment;
ifStatement: IF CONSTEXPR? '(' expression ')' statementBlock ('else' statementBlock)?;
whileLoop: WHILE '(' expression ')' statementBlock;
returnStatement: RETURN expression?;
breakStatement: BREAK;
continueStatement: CONTINUE;
gotoStatement: GOTO labelReference argumentList?;
handlerWithStatement: (CATCH IDENTIFIER | LABEL labelDeclaration) statementBlock;
tryCatch: TRY statementBlock handlerWithStatement+;

diagnosticStatement: (ASSERT '(' expression ')') | UNREACHABLE_TOKEN | DEBUG_TOKEN;

statement : variableDeclarationWithInitialization ';'
          | helperCallStatement ';'
          | expressionStatement ';'
          | returnStatement ';'
          | breakStatement ';'
          | continueStatement ';'
          | gotoStatement ';'
          | ifStatement
          | diagnosticStatement ';'
          | whileLoop
          | forOfLoop
          | forLoop
          | tryCatch
          ;

statementList : statement*;
statementScope : DEFERRED? '{' statementList '}';
statementBlock
        : statement
        | statementScope;

helperBody : statementScope;

extendsDeclaration: 'extends' IDENTIFIER;
generatesDeclaration: 'generates' STRING_LITERAL;
constexprDeclaration: 'constexpr' STRING_LITERAL;
typeDeclaration : 'type' IDENTIFIER extendsDeclaration? generatesDeclaration? constexprDeclaration?';';

externalBuiltin : EXTERN JAVASCRIPT? BUILTIN IDENTIFIER optionalGenericTypeList '(' typeList ')' optionalType ';';
externalMacro : EXTERN (IMPLICIT? 'operator' STRING_LITERAL)? MACRO IDENTIFIER optionalGenericTypeList typeListMaybeVarArgs optionalType optionalLabelList ';';
externalRuntime : EXTERN RUNTIME IDENTIFIER typeListMaybeVarArgs optionalType ';';
builtinDeclaration : JAVASCRIPT? BUILTIN IDENTIFIER optionalGenericTypeList parameterList optionalType helperBody;
genericSpecialization: IDENTIFIER genericSpecializationTypeList parameterList optionalType optionalLabelList helperBody;
macroDeclaration : MACRO IDENTIFIER optionalGenericTypeList parameterList optionalType optionalLabelList helperBody;
constDeclaration : 'const' IDENTIFIER ':' type '=' STRING_LITERAL ';';

declaration
        : typeDeclaration
        | builtinDeclaration
        | genericSpecialization
        | macroDeclaration
        | externalMacro
        | externalBuiltin
        | externalRuntime
        | constDeclaration;

moduleDeclaration : MODULE IDENTIFIER '{' declaration* '}';

file: (moduleDeclaration | declaration)*;
