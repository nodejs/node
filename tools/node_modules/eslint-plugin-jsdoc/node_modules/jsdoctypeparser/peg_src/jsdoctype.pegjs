{
  const meta = require('../lib/SyntaxType.js');
  const {
    GenericTypeSyntax,
    VariadicTypeSyntax, OptionalTypeSyntax,
    NullableTypeSyntax, NotNullableTypeSyntax,
  } = meta;
  const NodeType = require('../lib/NodeType.js');

  const NamepathOperatorType = {
    MEMBER: 'MEMBER',
    INNER_MEMBER: 'INNER_MEMBER',
    INSTANCE_MEMBER: 'INSTANCE_MEMBER',
  };
}



TopTypeExpr = _ expr:( VariadicTypeExpr
                  / UnionTypeExpr
                  / IntersectionTypeExpr // no-jsdoc, no-closure
                  / UnaryUnionTypeExpr
                  / ArrayTypeExpr // no-closure
                  / GenericTypeExpr
                  / RecordTypeExpr
                  / TupleTypeExpr // no-jsdoc, no-closure
                  / ArrowTypeExpr // no-jsdoc, no-closure
                  / FunctionTypeExpr
                  / TypeQueryExpr // no-jsdoc
                  / KeyQueryExpr // no-jsdoc, no-closure
                  / BroadNamepathExpr
                  / ParenthesizedExpr
                  / ValueExpr
                  / AnyTypeExpr
                  / UnknownTypeExpr
                  ) _ {
           return expr;
         }

/*
 * White spaces.
 */
WS  = [ \t] / [\r]? [\n]

_  = WS*

__ = WS+

/*
 * JavaScript identifier names.
 *
 * NOTE: We do not support UnicodeIDStart and \UnicodeEscapeSequence yet.
 *
 * Spec:
 *   - http://www.ecma-international.org/ecma-262/6.0/index.html#sec-names-and-keywords
 *   - http://unicode.org/reports/tr31/#Table_Lexical_Classes_for_Identifiers
 */
JsIdentifier = $([a-zA-Z_$][a-zA-Z0-9_$]*)


// It is transformed to remove left recursion.
// See https://en.wikipedia.org/wiki/Left_recursion#Removing_left_recursion
NamepathExpr = rootOwner:(
  ParenthesizedExpr /
  ImportTypeExpr / // no-jsdoc, no-closure
  TypeNameExpr
) memberPartWithOperators:(_ InfixNamepathOperator _ "event:"? _ MemberName)* {
               return memberPartWithOperators.reduce(function(owner, tokens) {
                 const operatorType = tokens[1];
                 const eventNamespace = tokens[3];
                 const MemberName = tokens[5];
                 const {quoteStyle, name: memberName} = MemberName;

                 switch (operatorType) {
                   case NamepathOperatorType.MEMBER:
                     return {
                       type: NodeType.MEMBER,
                       owner,
                       name: memberName,
                       quoteStyle,
                       hasEventPrefix: Boolean(eventNamespace),
                     };
                   case NamepathOperatorType.INSTANCE_MEMBER:
                     return {
                       type: NodeType.INSTANCE_MEMBER,
                       owner,
                       name: memberName,
                       quoteStyle,
                       hasEventPrefix: Boolean(eventNamespace),
                     };
                   case NamepathOperatorType.INNER_MEMBER:
                     return {
                       type: NodeType.INNER_MEMBER,
                       owner,
                       name: memberName,
                       quoteStyle,
                       hasEventPrefix: Boolean(eventNamespace),
                     };
                   /* istanbul ignore next */
                   default:
                     throw new Error('Unexpected operator type: "' + operatorType + '"');
                 }
               }, rootOwner);
             }


/*
 * Type name expressions.
 *
 * Examples:
 *   - string
 *   - null
 *   - Error
 *   - $
 *   - _
 *   - custom-type (JSDoc compatible)
 *
 * Spec:
 *   - https://developers.google.com/closure/compiler/docs/js-for-compiler#types
 */
TypeNameExpr = TypeNameExprJsDocFlavored

// JSDoc allows using hyphens in identifier contexts.
// See https://github.com/jsdoctypeparser/jsdoctypeparser/issues/15
TypeNameExprJsDocFlavored = name:$([a-zA-Z_$][a-zA-Z0-9_$-]*) {
                            return {
                              type: NodeType.NAME,
                              name
                            };
                          }

MemberName = "'" name:$([^\\'] / "\\".)* "'" {
               return {
                 quoteStyle: 'single',
                 name: name.replace(/\\'/g, "'")
                   .replace(/\\\\/gu, '\\')
               };
             }
           / '"' name:$([^\\"] / "\\".)* '"' {
               return {
                 quoteStyle: 'double',
                 name: name.replace(/\\"/gu, '"')
                  .replace(/\\\\/gu, '\\')
               };
             }
           / name:JsIdentifier {
             return {
               quoteStyle: 'none',
               name
             };
           };


InfixNamepathOperator = MemberTypeOperator
                      / InstanceMemberTypeOperator
                      / InnerMemberTypeOperator

QualifiedMemberName = rootOwner:TypeNameExpr memberPart:(_ "." _ TypeNameExpr)* {
                      return memberPart.reduce(function(owner, tokens) {
                        return {
                          type: NodeType.MEMBER,
                          owner,
                          name: tokens[3]
                        }
                      }, rootOwner);
                    }


/*
 * Member type expressions.
 *
 * Examples:
 *   - owner.member
 *   - superOwner.owner.member
 */
MemberTypeOperator = "." {
                     return NamepathOperatorType.MEMBER;
                   }


/*
 * Inner member type expressions.
 *
 * Examples:
 *   - owner~innerMember
 *   - superOwner~owner~innerMember
 */
InnerMemberTypeOperator = "~" {
                          return NamepathOperatorType.INNER_MEMBER;
                        }


/*
 * Instance member type expressions.
 *
 * Examples:
 *   - owner#instanceMember
 *   - superOwner#owner#instanceMember
 */
InstanceMemberTypeOperator = "#" {
                             return NamepathOperatorType.INSTANCE_MEMBER;
                           }


BroadNamepathExpr =
                  ExternalNameExpr / // no-typescript
                  ModuleNameExpr / // no-typescript
                  NamepathExpr


// no-typescript-begin
/*
 * External name expressions.
 *
 * Examples:
 *   - external:classNamespaceOrModuleName
 *   - external:path/to/file
 *   - external:path/to/file.js
 *
 * Spec:
 *   - https://jsdoc.app/tags-external.html
 */
ExternalNameExpr = "external" _ ":" _ external:(MemberName) memberPartWithOperators:(_ InfixNamepathOperator _ "event:"? _ MemberName)* {
          return memberPartWithOperators.reduce(function(owner, tokens) {
            const operatorType = tokens[1];
            const eventNamespace = tokens[3];
            const MemberName = tokens[5];
            const {quoteStyle, name: memberName} = MemberName;

            switch (operatorType) {
              case NamepathOperatorType.MEMBER:
                return {
                  type: NodeType.MEMBER,
                  owner,
                  name: memberName,
                  quoteStyle,
                  hasEventPrefix: Boolean(eventNamespace),
                };
              case NamepathOperatorType.INSTANCE_MEMBER:
                return {
                  type: NodeType.INSTANCE_MEMBER,
                  owner,
                  name: memberName,
                  quoteStyle,
                  hasEventPrefix: Boolean(eventNamespace),
                };
              case NamepathOperatorType.INNER_MEMBER:
                return {
                  type: NodeType.INNER_MEMBER,
                  owner,
                  name: memberName,
                  quoteStyle,
                  hasEventPrefix: Boolean(eventNamespace),
                };
              /* istanbul ignore next */
              default:
                throw new Error('Unexpected operator type: "' + operatorType + '"');
            }
          }, Object.assign({
            type: NodeType.EXTERNAL
          }, external));
        }
// no-typescript-end

// no-typescript-begin
/*
 * Module name expressions.
 *
 * Examples:
 *   - module:path/to/file
 *   - module:path/to/file.MyModule~Foo
 *
 * Spec:
 *   - https://jsdoc.app/tags-module.html
 *   - https://jsdoc.app/howto-commonjs-modules.html
 */
ModuleNameExpr = "module" _ ":" _ value:ModulePathExpr {
                 return {
                   type: NodeType.MODULE,
                   value,
                 };
               }



// It is transformed to remove left recursion
ModulePathExpr = rootOwner:(FilePathExpr) memberPartWithOperators:(_ InfixNamepathOperator _ "event:"? _ MemberName)* {
                 return memberPartWithOperators.reduce(function(owner, tokens) {
                   const operatorType = tokens[1];
                   const eventNamespace = tokens[3];
                   const MemberName = tokens[5];
                   const {quoteStyle, name: memberName} = MemberName;

                   switch (operatorType) {
                     case NamepathOperatorType.MEMBER:
                       return {
                         type: NodeType.MEMBER,
                         owner,
                         name: memberName,
                         quoteStyle,
                         hasEventPrefix: Boolean(eventNamespace),
                       };
                     case NamepathOperatorType.INSTANCE_MEMBER:
                       return {
                         type: NodeType.INSTANCE_MEMBER,
                         owner,
                         name: memberName,
                         quoteStyle,
                         hasEventPrefix: Boolean(eventNamespace),
                       };
                     case NamepathOperatorType.INNER_MEMBER:
                       return {
                         type: NodeType.INNER_MEMBER,
                         owner,
                         name: memberName,
                         quoteStyle,
                         hasEventPrefix: Boolean(eventNamespace),
                       };
                     /* istanbul ignore next */
                     default:
                       throw new Error('Unexpected operator type: "' + operatorType + '"');
                   }
                 }, rootOwner);
               }


FilePathExpr = "'" path:$([^\\'] / "\\".)* "'" {
                return {
                  quoteStyle: 'single',
                  type: NodeType.FILE_PATH,
                  path: path.replace(/\\'/g, "'")
                    .replace(/\\\\/gu, '\\')
                };
              }
            / '"' path:$([^\\"] / "\\".)* '"' {
                return {
                  quoteStyle: 'double',
                  type: NodeType.FILE_PATH,
                  path: path.replace(/\\"/gu, '"')
                   .replace(/\\\\/gu, '\\')
                };
              }
            / path:$([a-zA-Z0-9_$/-]+) {
               return {
                 quoteStyle: 'none',
                 type: NodeType.FILE_PATH,
                 path,
               };
             }
// no-typescript-end

/*
 * Any type expressions.
 *
 * Examples:
 *   - *
 *
 * Spec:
 *   - https://developers.google.com/closure/compiler/docs/js-for-compiler#types
 */
AnyTypeExpr = "*" {
              return { type: NodeType.ANY };
            }



/*
 * Unknown type expressions.
 *
 * Examples:
 *   - ?
 *
 * Spec:
 *   - https://developers.google.com/closure/compiler/docs/js-for-compiler#types
 */
UnknownTypeExpr = "?" {
                  return { type: NodeType.UNKNOWN };
                }



/*
 * Value type expressions.
 *
 * Example:
 *   - 123
 *   - 0.0
 *   - -123
 *   - 0b11
 *   - 0o77
 *   - 0cff
 *   - "foo"
 *   - "foo\"bar\nbuz"
 *
 */
ValueExpr = StringLiteralExpr / NumberLiteralExpr


StringLiteralExpr = '"' value:$([^\\"] / "\\".)* '"' {
                      return {
                        type: NodeType.STRING_VALUE,
                        quoteStyle: 'double',
                        string: value.replace(/\\"/gu, '"')
                            .replace(/\\\\/gu, '\\')
                      };
                    }
                  / "'" value:$([^\\'] / "\\".)* "'" {
                      return {
                        type: NodeType.STRING_VALUE,
                        quoteStyle: 'single',
                        string: value.replace(/\\'/g, "'")
                            .replace(/\\\\/gu, '\\')
                      };
                    }


NumberLiteralExpr = value:(BinNumberLiteralExpr / OctNumberLiteralExpr / HexNumberLiteralExpr / DecimalNumberLiteralExpr) {
                    return {
                      type: NodeType.NUMBER_VALUE,
                      number: value
                    };
                  }


DecimalNumberLiteralExpr = $(("+" / "-")? UnsignedDecimalNumberLiteralExpr)

UnsignedDecimalNumberLiteralExpr = $((([0-9]+ ("." [0-9]+)?) / ("." [0-9]+)) ("e" ("+" / "-")? [0-9]+)?)



BinNumberLiteralExpr = $("-"? "0b"[01]+)


OctNumberLiteralExpr = $("-"? "0o"[0-7]+)


HexNumberLiteralExpr = $("-"? "0x"[0-9a-fA-F]+)

// no-jsdoc-begin, no-closure-begin
/*
 * Intersection type expressions.
 *
 * Examples:
 *   - A & B
 *   - Person & Serializable & Loggable
 *   Spec:
 *   - https://www.typescriptlang.org/v2/docs/handbook/unions-and-intersections.html
 *   Issues to support:
 *   - https://github.com/jsdoc/jsdoc/issues/1285
 */
IntersectionTypeExpr = left:IntersectionTypeExprOperand _ "&" _ right:(IntersectionTypeExpr / IntersectionTypeExprOperand) {
                return {
                    type: NodeType.INTERSECTION,
                    left,
                    right,
                };
              }

IntersectionTypeExprOperand = UnaryUnionTypeExpr
                     / RecordTypeExpr
                     / TupleTypeExpr // no-jsdoc, no-closure
                     / ArrowTypeExpr // no-jsdoc, no-closure
                     / FunctionTypeExpr
                     / ParenthesizedExpr
                     / TypeQueryExpr // no-jsdoc
                     / KeyQueryExpr // no-jsdoc, no-closure
                     / GenericTypeExpr
                     / ArrayTypeExpr // no-closure
                     / BroadNamepathExpr
                     / ValueExpr
                     / AnyTypeExpr
                     / UnknownTypeExpr
// no-jsdoc-end, no-closure-end

/*
 * Union type expressions.
 *
 * Examples:
 *   - number|undefined
 *   - Foo|Bar|Baz
 */
UnionTypeExpr = left:UnionTypeExprOperand _ "|" _ right:(UnionTypeExpr / UnionTypeExprOperand) {
                return {
                    type: NodeType.UNION,
                    left,
                    right,
                };
              }

UnionTypeExprOperand = UnaryUnionTypeExpr
                     / RecordTypeExpr
                     / TupleTypeExpr // no-jsdoc, no-closure
                     / ArrowTypeExpr // no-jsdoc, no-closure
                     / FunctionTypeExpr
                     / ParenthesizedExpr
                     / TypeQueryExpr // no-jsdoc
                     / KeyQueryExpr // no-jsdoc, no-closure
                     / GenericTypeExpr
                     / ArrayTypeExpr // no-closure
                     / BroadNamepathExpr
                     / ValueExpr
                     / AnyTypeExpr
                     / UnknownTypeExpr


UnaryUnionTypeExpr = SuffixUnaryUnionTypeExpr
                   / PrefixUnaryUnionTypeExpr


PrefixUnaryUnionTypeExpr = PrefixOptionalTypeExpr
                         / PrefixNotNullableTypeExpr
                         / PrefixNullableTypeExpr


PrefixUnaryUnionTypeExprOperand = GenericTypeExpr
                                / RecordTypeExpr
                                / TupleTypeExpr // no-jsdoc, no-closure
                                / ArrowTypeExpr // no-jsdoc, no-closure
                                / FunctionTypeExpr
                                / ParenthesizedExpr
                                / BroadNamepathExpr
                                / ValueExpr
                                / AnyTypeExpr
                                / UnknownTypeExpr

// While the following is supported in Closure per https://github.com/google/closure-compiler/wiki/Types-in-the-Closure-Type-System#user-content-the-javascript-type-language
// ...the statement at https://jsdoc.app/tags-type.html :
// > Full support for Google Closure Compiler-style type expressions is
// > available in JSDoc 3.2 and later.
// ... is apparently outdated as the catharsis parser on which jsdoc depends,
// does not contain reference to `typeof` as an expression, so preventing
// from jsdoc mode for now.

// no-jsdoc-begin
TypeQueryExpr = operator:"typeof" __ name:QualifiedMemberName {
                return {
                    type: NodeType.TYPE_QUERY,
                    name,
                };
              }
// no-jsdoc-end

// no-jsdoc-begin, no-closure-begin
KeyQueryExpr = operator:"keyof" __ operand:KeyQueryExprOperand {
    return {
      type: NodeType.KEY_QUERY,
      value: operand,
    }
  }
  / operator:"keyof" operand:ParenthesizedExpr {
    return {
      type: NodeType.KEY_QUERY,
      value: operand,
    }
  }

KeyQueryExprOperand = UnionTypeExpr
                    / IntersectionTypeExpr // no-jsdoc, no-closure
                    / UnaryUnionTypeExpr
                    / RecordTypeExpr
                    / TupleTypeExpr // no-jsdoc, no-closure
                    / FunctionTypeExpr
                    / ParenthesizedExpr
                    / TypeQueryExpr // no-jsdoc
                    / KeyQueryExpr
                    / ArrayTypeExpr // no-closure
                    / GenericTypeExpr
                    / BroadNamepathExpr
                    / ValueExpr
                    / AnyTypeExpr
                    / UnknownTypeExpr
// no-jsdoc-end, no-closure-end

// no-jsdoc-begin, no-closure-begin
ImportTypeExpr = operator:"import" _ "(" _ path:StringLiteralExpr _ ")" {
                 return { type: NodeType.IMPORT, path };
               }
// no-jsdoc-end, no-closure-end

/*
 * Prefix nullable type expressions.
 *
 * Examples:
 *   - ?string
 *
 * Spec:
 *   - https://developers.google.com/closure/compiler/docs/js-for-compiler#types
 */
PrefixNullableTypeExpr = operator:"?" _ operand:PrefixUnaryUnionTypeExprOperand {
                         return {
                           type: NodeType.NULLABLE,
                           value: operand,
                           meta: { syntax: NullableTypeSyntax.PREFIX_QUESTION_MARK },
                         };
                       }



/*
 * Prefix not nullable type expressions.
 *
 * Examples:
 *   - !Object
 *
 * Spec:
 *   - https://developers.google.com/closure/compiler/docs/js-for-compiler#types
 */
PrefixNotNullableTypeExpr = operator:"!" _ operand:PrefixUnaryUnionTypeExprOperand {
                            return {
                              type: NodeType.NOT_NULLABLE,
                              value: operand,
                              meta: { syntax: NotNullableTypeSyntax.PREFIX_BANG },
                            };
                          }



/*
 * Prefix optional type expressions.
 * This expression is deprecated.
 *
 * Examples:
 *   - =string (deprecated)
 */
PrefixOptionalTypeExpr = operator:"=" _ operand:PrefixUnaryUnionTypeExprOperand {
                         return {
                           type: NodeType.OPTIONAL,
                           value: operand,
                           meta: { syntax: OptionalTypeSyntax.PREFIX_EQUALS_SIGN },
                         };
                       }



SuffixUnaryUnionTypeExpr = SuffixOptionalTypeExpr
                         / SuffixNullableTypeExpr
                         / SuffixNotNullableTypeExpr


SuffixUnaryUnionTypeExprOperand = PrefixUnaryUnionTypeExpr
                                / GenericTypeExpr
                                / RecordTypeExpr
                                / TupleTypeExpr // no-jsdoc, no-closure
                                / ArrowTypeExpr // no-jsdoc, no-closure
                                / FunctionTypeExpr
                                / ParenthesizedExpr
                                / BroadNamepathExpr
                                / ValueExpr
                                / AnyTypeExpr
                                / UnknownTypeExpr


/*
 * Suffix nullable type expressions.
 * This expression is deprecated.
 *
 * Examples:
 *   - string? (deprecated)
 *
 * Note:
 *   Deprecated optional type operators can be placed before optional operators.
 *   See https://github.com/google/closure-library/blob/47f9c92bb4c7de9a3d46f9921a427402910073fb/closure/goog/net/tmpnetwork.js#L50
 */
SuffixNullableTypeExpr = operand:SuffixUnaryUnionTypeExprOperand _ operator:"?" {
                         return {
                           type: NodeType.NULLABLE,
                           value: operand,
                           meta: { syntax: NullableTypeSyntax.SUFFIX_QUESTION_MARK },
                         };
                       }



/*
 * Suffix not nullable type expressions.
 * This expression is deprecated.
 *
 * Examples:
 *   - Object! (deprecated)
 */
SuffixNotNullableTypeExpr = operand:SuffixUnaryUnionTypeExprOperand _ operator:"!" {
                            return {
                              type: NodeType.NOT_NULLABLE,
                              value: operand,
                              meta: { syntax: NotNullableTypeSyntax.SUFFIX_BANG },
                            };
                          }



/*
 * Suffix optional type expressions.
 *
 * Examples:
 *   - string=
 *
 * Spec:
 *   - https://developers.google.com/closure/compiler/docs/js-for-compiler#types
 */
SuffixOptionalTypeExpr = operand:( SuffixNullableTypeExpr
                                 / SuffixNotNullableTypeExpr
                                 / SuffixUnaryUnionTypeExprOperand
                                 ) _ operator:"=" {
                         return {
                           type: NodeType.OPTIONAL,
                           value: operand,
                           meta: { syntax: OptionalTypeSyntax.SUFFIX_EQUALS_SIGN },
                         };
                       }



/*
 * Generic type expressions.
 *
 * Examples:
 *   - Function<T>
 *   - Array.<string> (Legacy Closure Library style and JSDoc3 style)
 *
 * Spec:
 *   - https://developers.google.com/closure/compiler/docs/js-for-compiler#types
 *   - https://jsdoc.app/tags-type.html
 */
GenericTypeExpr = operand:GenericTypeExprOperand _ syntax:GenericTypeStartToken _
                  params:GenericTypeExprTypeParamList _ GenericTypeEndToken {
                  return {
                    type: NodeType.GENERIC,
                    subject: operand,
                    objects: params,
                    meta: { syntax },
                  };
                }


GenericTypeExprOperand = ParenthesizedExpr
                       / BroadNamepathExpr
                       / ValueExpr
                       / AnyTypeExpr
                       / UnknownTypeExpr


GenericTypeExprTypeParamOperand = UnionTypeExpr
                                / IntersectionTypeExpr // no-jsdoc, no-closure
                                / UnaryUnionTypeExpr
                                / RecordTypeExpr
                                / TupleTypeExpr // no-jsdoc, no-closure
                                / ArrowTypeExpr // no-jsdoc, no-closure
                                / FunctionTypeExpr
                                / ParenthesizedExpr
                                / ArrayTypeExpr // no-closure
                                / GenericTypeExpr
                                / TypeQueryExpr // no-jsdoc
                                / KeyQueryExpr // no-jsdoc, no-closure
                                / BroadNamepathExpr
                                / ValueExpr
                                / AnyTypeExpr
                                / UnknownTypeExpr


GenericTypeExprTypeParamList = first:GenericTypeExprTypeParamOperand
                               restsWithComma:(_ "," _ GenericTypeExprTypeParamOperand)* {
                               return restsWithComma.reduce(function(params, tokens) {
                                 return params.concat([tokens[3]]);
                               }, [first]);
                             }


GenericTypeStartToken = GenericTypeEcmaScriptFlavoredStartToken
                      / GenericTypeTypeScriptFlavoredStartToken


GenericTypeEcmaScriptFlavoredStartToken = ".<" {
                                          return GenericTypeSyntax.ANGLE_BRACKET_WITH_DOT;
                                        }


GenericTypeTypeScriptFlavoredStartToken = "<" {
                                          return GenericTypeSyntax.ANGLE_BRACKET;
                                        }


GenericTypeEndToken = ">"


// no-closure-begin
/*
 * JSDoc style array of generic type expressions.
 *
 * Examples:
 *   - string[]
 *   - number[][]
 *
 * Spec:
 *   - https://jsdoc.app/tags-type.html
 */
// TODO: We should support complex type expressions like "Some[]![]"
ArrayTypeExpr = operand:ArrayTypeExprOperand brackets:(_ "[" _ "]")+ {
                return brackets.reduce(function(operand) {
                  return {
                    type: NodeType.GENERIC,
                    subject: {
                      type: NodeType.NAME,
                      name: 'Array'
                    },
                    objects: [ operand ],
                    meta: { syntax: GenericTypeSyntax.SQUARE_BRACKET },
                  };
                }, operand);
              }


ArrayTypeExprOperand = UnaryUnionTypeExpr
                     / RecordTypeExpr
                     / TupleTypeExpr // no-jsdoc, no-closure
                     / ArrowTypeExpr // no-jsdoc, no-closure
                     / FunctionTypeExpr
                     / ParenthesizedExpr
                     / GenericTypeExpr
                     / TypeQueryExpr // no-jsdoc
                     / KeyQueryExpr // no-jsdoc, no-closure
                     / BroadNamepathExpr
                     / ValueExpr
                     / AnyTypeExpr
                     / UnknownTypeExpr
// no-closure-end

// no-jsdoc-begin, no-closure-begin
ArrowTypeExpr = newModifier:"new"? _ paramsPart:ArrowTypeExprParamsList _ "=>" _ returnedTypeNode:FunctionTypeExprReturnableOperand {
                   return {
                     type: NodeType.ARROW,
                     params: paramsPart,
                     returns: returnedTypeNode,
                     new: newModifier
                   };
}

ArrowTypeExprParamsList = "(" _ ")" {
                      return [];
                    } /
                    "(" _ params:ArrowTypeExprParams _ ")" {
                      return params;
                    }
ArrowTypeExprParams = paramsWithComma:(JsIdentifier _ ":" _ FunctionTypeExprParamOperand? _ "," _)* lastParam:VariadicNameExpr? {
  return paramsWithComma.reduceRight(function(params, tokens) {
    const param = { type: NodeType.NAMED_PARAMETER, name: tokens[0], typeName: tokens[4] };
    return [param].concat(params);
  }, lastParam ? [lastParam] : []);
}

VariadicNameExpr = spread:"..."? _ id:JsIdentifier _ ":" _ type:FunctionTypeExprParamOperand? {
  const operand = { type: NodeType.NAMED_PARAMETER, name: id, typeName: type };
  if (spread) {
  return {
    type: NodeType.VARIADIC,
    value: operand,
    meta: { syntax: VariadicTypeSyntax.PREFIX_DOTS },
  };
  }
  else {
    return operand;
  }
}
// no-jsdoc-end, no-closure-end

/*
 * Function type expressions.
 *
 * Examples:
 *   - function(string)
 *   - function(string, ...string)
 *   - function():number
 *   - function(this:jQuery):jQuery
 *   - function(new:Error)
 *
 * Spec:
 *   - https://developers.google.com/closure/compiler/docs/js-for-compiler#types
 */
FunctionTypeExpr = "function" _ paramsPart:FunctionTypeExprParamsList _
                   returnedTypePart:(":" _ FunctionTypeExprReturnableOperand)? {
                   const returnedTypeNode = returnedTypePart ? returnedTypePart[2] : null;

                   return {
                     type: NodeType.FUNCTION,
                     params: paramsPart.params,
                     returns: returnedTypeNode,
                     this: paramsPart.modifier.nodeThis,
                     new: paramsPart.modifier.nodeNew,
                   };
                 }


FunctionTypeExprParamsList = "(" _ modifier:FunctionTypeExprModifier _ "," _ params: FunctionTypeExprParams _ ")" {
                               return { params, modifier };
                             }
                           / "(" _ modifier:FunctionTypeExprModifier _ ")" {
                               return { params: [], modifier };
                             }
                           / "(" _ ")" {
                               return { params: [], modifier: { nodeThis: null, nodeNew: null } };
                             }
                           / "(" _ params:FunctionTypeExprParams _ ")" {
                               return { params, modifier: { nodeThis: null, nodeNew: null } };
                             }


// We can specify either "this:" or "new:".
// See https://github.com/google/closure-compiler/blob/
//       91cf3603d5b0b0dc289ba73adcd0f2741aa90d89/src/
//       com/google/javascript/jscomp/parsing/JsDocInfoParser.java#L2158-L2171
FunctionTypeExprModifier = modifierThis:("this" _ ":" _ FunctionTypeExprParamOperand) {
                             return { nodeThis: modifierThis[4], nodeNew: null };
                           }
                         / modifierNew:("new" _ ":" _ FunctionTypeExprParamOperand) {
                             return { nodeThis: null, nodeNew: modifierNew[4] };
                           }


FunctionTypeExprParams = paramsWithComma:(FunctionTypeExprParamOperand _ "," _)*
                         // Variadic type is only allowed on the last parameter.
                         lastParam:(VariadicTypeExpr / VariadicTypeExprOperand)? {
                         return paramsWithComma.reduceRight(function(params, tokens) {
                           const [param] = tokens;
                           return [param].concat(params);
                         }, lastParam ? [lastParam] : []);
                       }


FunctionTypeExprParamOperand = UnionTypeExpr
                             / IntersectionTypeExpr // no-jsdoc, no-closure
                             / TypeQueryExpr // no-jsdoc
                             / KeyQueryExpr // no-jsdoc, no-closure
                             / UnaryUnionTypeExpr
                             / RecordTypeExpr
                             / TupleTypeExpr // no-jsdoc, no-closure
                             / ArrowTypeExpr // no-jsdoc, no-closure
                             / FunctionTypeExpr
                             / ParenthesizedExpr
                             / ArrayTypeExpr // no-closure
                             / GenericTypeExpr
                             / BroadNamepathExpr
                             / ValueExpr
                             / AnyTypeExpr
                             / UnknownTypeExpr


FunctionTypeExprReturnableOperand = PrefixUnaryUnionTypeExpr
                                  // Suffix unary union type operators should not be
                                  // placed here to keep operator precedence. For example,
                                  // "Foo|function():Returned=" should be parsed as
                                  // "(Foo|function():Returned)=" instead of
                                  // "Foo|(function():Returned=)". This result was expected
                                  // by Closure Library.
                                  //
                                  // See https://github.com/google/closure-library/blob/
                                  //   47f9c92bb4c7de9a3d46f9921a427402910073fb/
                                  //   closure/goog/ui/zippy.js#L47
                                  / RecordTypeExpr
                                  / TupleTypeExpr // no-jsdoc, no-closure
                                  / ArrowTypeExpr // no-jsdoc, no-closure
                                  / FunctionTypeExpr
                                  / ParenthesizedExpr
                                  / ArrayTypeExpr // no-closure
                                  / TypeQueryExpr // no-jsdoc
                                  / KeyQueryExpr // no-jsdoc, no-closure
                                  / GenericTypeExpr
                                  / BroadNamepathExpr
                                  / ValueExpr
                                  / AnyTypeExpr
                                  / UnknownTypeExpr

/*
 * Record type expressions.
 *
 * Examples:
 *   - {}
 *   - {length}
 *   - {length:number}
 *   - {toString:Function,valueOf:Function}
 *   - {'foo':*}
 *   - {a:string,b?:number} // TypeScript syntax
 *
 * Spec:
 *   - https://developers.google.com/closure/compiler/docs/js-for-compiler#types
 *   - https://www.typescriptlang.org/docs/handbook/type-checking-javascript-files.html#patterns-that-are-known-not-to-be-supported
 */
RecordTypeExpr = "{" _ entries:RecordTypeExprEntries? _ "}" {
                 return {
                   type: NodeType.RECORD,
                   entries: entries || [],
                 };
               }


RecordTypeExprEntries = first:RecordTypeExprEntry restWithComma:((_ "," /_ ";" / [ \t]* [\r]? [\n]) _ RecordTypeExprEntry)* (_ "," / _ ";" / [ \t]* [\r]? [\n])? {
                        return restWithComma.reduce(function(entries, tokens) {
                          const entry = tokens[2];
                          return entries.concat([entry]);
                        }, [first]);
                      }

RecordTypeExprEntry = readonly:("readonly" __)? keyInfo:RecordTypeExprEntryKey _
    optional:"?"?
    _ ":" _ value:RecordTypeExprEntryOperand {
                        const {quoteStyle, key} = keyInfo;
                        return {
                          type: NodeType.RECORD_ENTRY,
                          key,
                          quoteStyle,
                          value:
                            optional === '?' ? {
                              type: NodeType.OPTIONAL,
                              value,
                              meta: { syntax: OptionalTypeSyntax.SUFFIX_KEY_QUESTION_MARK },
                            } :
                            value,
                          readonly: Boolean(readonly)
                        };
                      }
                    / readonly:("readonly" __)? keyInfo:RecordTypeExprEntryKey {
                        const {quoteStyle, key} = keyInfo;
                        return {
                          type: NodeType.RECORD_ENTRY,
                          key,
                          quoteStyle,
                          value: null,
                          readonly: Boolean(readonly)
                        };
                      }


RecordTypeExprEntryKey = '"' key:$([^\\"] / "\\".)* '"' {
                           return {
                             quoteStyle: 'double',
                             key: key.replace(/\\"/gu, '"')
                               .replace(/\\\\/gu, '\\')
                           };
                       }
                       / "'" key:$([^\\'] / "\\".)* "'" {
                           return {
                             quoteStyle: 'single',
                             key: key.replace(/\\'/g, "'")
                               .replace(/\\\\/gu, '\\')
                           };
                         }
                       / key:$(JsIdentifier / UnsignedDecimalNumberLiteralExpr) {
                           return {
                             quoteStyle: 'none',
                             key
                           };
                       }


RecordTypeExprEntryOperand = UnionTypeExpr
                           / IntersectionTypeExpr // no-jsdoc, no-closure
                           / UnaryUnionTypeExpr
                           / RecordTypeExpr
                           / TupleTypeExpr // no-jsdoc, no-closure
                           / ArrowTypeExpr // no-jsdoc, no-closure
                           / FunctionTypeExpr
                           / ParenthesizedExpr
                           / ArrayTypeExpr // no-closure
                           / GenericTypeExpr
                           / BroadNamepathExpr
                           / ValueExpr
                           / AnyTypeExpr
                           / UnknownTypeExpr


// no-jsdoc-begin, no-closure-begin
/**
 * TypeScript style tuple type.
 *
 * Examples:
 *   - []
 *   - [string]
 *   - [number]
 *   - [string, number]
 *
 * Spec:
 *   - https://www.typescriptlang.org/docs/handbook/basic-types.html#tuple
 */
TupleTypeExpr = "[" _ entries:TupleTypeExprEntries _ "]" {
  return {
    type: NodeType.TUPLE,
    entries,
  }
}

TupleTypeExprEntries = restWithComma:(TupleTypeExprOperand _ "," _)*
                       // Variadic type is only allowed on the last entry.
                       last:(VariadicTypeExpr / VariadicTypeExprOperand)? {
  return restWithComma.reduceRight((entries, tokens) => {
    let [entry] = tokens;
    return [entry].concat(entries);
  }, last ? [last] : []);
}

TupleTypeExprOperand = UnionTypeExpr
                     / IntersectionTypeExpr // no-jsdoc, no-closure
                     / UnaryUnionTypeExpr
                     / RecordTypeExpr
                     / TupleTypeExpr // no-jsdoc, no-closure
                     / ArrowTypeExpr // no-jsdoc, no-closure
                     / FunctionTypeExpr
                     / ParenthesizedExpr
                     / TypeQueryExpr // no-jsdoc
                     / KeyQueryExpr // no-jsdoc, no-closure
                     / ArrayTypeExpr // no-closure
                     / GenericTypeExpr
                     / BroadNamepathExpr
                     / ValueExpr
                     / AnyTypeExpr
                     / UnknownTypeExpr
// no-jsdoc-end, no-closure-end

/*
 * Parenthesis expressions.
 *
 * Examples:
 *   - (Foo|Bar)
 *   - (module: path/to/file).Module
 *
 * Spec:
 *   - https://developers.google.com/closure/compiler/docs/js-for-compiler#types
 */
ParenthesizedExpr = "(" _ wrapped:ParenthesizedExprOperand _ ")" {
                    return {
                      type: NodeType.PARENTHESIS,
                      value: wrapped,
                    };
                  }


ParenthesizedExprOperand = UnionTypeExpr
                         / IntersectionTypeExpr // no-jsdoc, no-closure
                         / UnaryUnionTypeExpr
                         / RecordTypeExpr
                         / TupleTypeExpr // no-jsdoc, no-closure
                         / ArrowTypeExpr // no-jsdoc, no-closure
                         / FunctionTypeExpr
                         / ArrayTypeExpr // no-closure
                         / TypeQueryExpr // no-jsdoc
                         / KeyQueryExpr // no-jsdoc, no-closure
                         / GenericTypeExpr
                         / BroadNamepathExpr
                         / ValueExpr
                         / AnyTypeExpr
                         / UnknownTypeExpr



/*
 * Variadic type expressions.
 *
 * Examples:
 *   - ...string (only allow on the top level or the last function parameter)
 *   - string... (only allow on the top level)
 *   - ...
 *
 * Note:
 *   It seems that we can omit the operand.
 *   See https://github.com/google/closure-library/blob/
 *       47f9c92bb4c7de9a3d46f9921a427402910073fb/
 *       closure/goog/base.js#L1847
 *
 * Spec:
 *   - https://developers.google.com/closure/compiler/docs/js-for-compiler#types
 */
VariadicTypeExpr = PrefixVariadicTypeExpr
                 / SuffixVariadicTypeExpr
                 / AnyVariadicTypeExpr


PrefixVariadicTypeExpr = "..." operand:VariadicTypeExprOperand {
                         return {
                           type: NodeType.VARIADIC,
                           value: operand,
                           meta: { syntax: VariadicTypeSyntax.PREFIX_DOTS },
                         };
                       }


SuffixVariadicTypeExpr = operand:VariadicTypeExprOperand "..." {
                         return {
                           type: NodeType.VARIADIC,
                           value: operand,
                           meta: { syntax: VariadicTypeSyntax.SUFFIX_DOTS },
                         };
                       }


AnyVariadicTypeExpr = "..." {
                      return {
                        type: NodeType.VARIADIC,
                        value: { type: NodeType.ANY },
                        meta: { syntax: VariadicTypeSyntax.ONLY_DOTS },
                      };
                    }


VariadicTypeExprOperand = UnionTypeExpr
                        / IntersectionTypeExpr // no-jsdoc, no-closure
                        / UnaryUnionTypeExpr
                        / RecordTypeExpr
                        / TupleTypeExpr // no-jsdoc, no-closure
                        / ArrowTypeExpr // no-jsdoc, no-closure
                        / FunctionTypeExpr
                        / ParenthesizedExpr
                        / TypeQueryExpr // no-jsdoc
                        / KeyQueryExpr // no-jsdoc, no-closure
                        / ArrayTypeExpr // no-closure
                        / GenericTypeExpr
                        / BroadNamepathExpr
                        / ValueExpr
                        / AnyTypeExpr
                        / UnknownTypeExpr
