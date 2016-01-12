//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// parser error messages
// NOTE: the error numbers should not change from version to version.
// Error numbers MUST be sorted.

LSC_ERROR_MSG( 1001, ERRnoMemory      , "Out of memory")
LSC_ERROR_MSG( 1002, ERRsyntax        , "Syntax error")
LSC_ERROR_MSG( 1003, ERRnoColon       , "Expected ':'")
LSC_ERROR_MSG( 1004, ERRnoSemic       , "Expected ';'")
LSC_ERROR_MSG( 1005, ERRnoLparen      , "Expected '('")
LSC_ERROR_MSG( 1006, ERRnoRparen      , "Expected ')'")
LSC_ERROR_MSG( 1007, ERRnoRbrack      , "Expected ']'")
LSC_ERROR_MSG( 1008, ERRnoLcurly      , "Expected '{'")
LSC_ERROR_MSG( 1009, ERRnoRcurly      , "Expected '}'")
LSC_ERROR_MSG( 1010, ERRnoIdent       , "Expected identifier")
LSC_ERROR_MSG( 1011, ERRnoEq          , "Expected '='")
LSC_ERROR_MSG( 1012, ERRnoSlash       , "Expected '/'")
LSC_ERROR_MSG( 1013, ERRbadNumber     , "Invalid number")
LSC_ERROR_MSG( 1014, ERRillegalChar   , "Invalid character")
LSC_ERROR_MSG( 1015, ERRnoStrEnd      , "Unterminated string constant")
LSC_ERROR_MSG( 1016, ERRnoCmtEnd      , "Unterminated comment")

LSC_ERROR_MSG( 1018, ERRbadReturn     , "'return' statement outside of function")
LSC_ERROR_MSG( 1019, ERRbadBreak      , "Can't have 'break' outside of loop")
LSC_ERROR_MSG( 1020, ERRbadContinue   , "Can't have 'continue' outside of loop")

LSC_ERROR_MSG( 1023, ERRbadHexDigit   , "Expected hexadecimal digit")
LSC_ERROR_MSG( 1024, ERRnoWhile       , "Expected 'while'")
LSC_ERROR_MSG( 1025, ERRbadLabel      , "Label redefined")
LSC_ERROR_MSG( 1026, ERRnoLabel       , "Label not found")
LSC_ERROR_MSG( 1027, ERRdupDefault    , "'default' can only appear once in a 'switch' statement")
LSC_ERROR_MSG( 1028, ERRnoMemberIdent , "Expected identifier, string or number")
// RETIRED Cc no longer supported ;; LSC_ERROR_MSG( 1029, ERRnoCcEnd       , "Expected '@end'")
// RETIRED Cc no longer supported ;; LSC_ERROR_MSG( 1030, ERRccOff         , "Conditional compilation is turned off")
LSC_ERROR_MSG( 1031, ERRnotConst      , "Expected constant")
// RETIRED Cc no longer supported ;; LSC_ERROR_MSG( 1032, ERRnoAt          , "Expected '@'")
LSC_ERROR_MSG( 1033, ERRnoCatch       , "Expected 'catch'")
LSC_ERROR_MSG( 1034, ERRnoVar         , "Expected 'var'")
LSC_ERROR_MSG( 1035, ERRdanglingThrow , "'throw' must be followed by an expression on the same source line")
// RETIRED ECMACP removed ;; LSC_ERROR_MSG( 1036, ERRWithNotInCP   , "'with' not available in the ECMA 327 Compact Profile")

LSC_ERROR_MSG( 1037, ERRES5NoWith     , "'with' statements are not allowed in strict mode") // string 8
LSC_ERROR_MSG( 1038, ERRES5ArgSame    , "Duplicate formal parameter names not allowed in strict mode") // string 9
LSC_ERROR_MSG( 1039, ERRES5NoOctal    , "Octal numeric literals and escape characters not allowed in strict mode") // string 1
LSC_ERROR_MSG( 1041, ERREvalUsage     , "Invalid usage of 'eval' in strict mode") // string 3
LSC_ERROR_MSG( 1042, ERRArgsUsage     , "Invalid usage of 'arguments' in strict mode") // string 3
LSC_ERROR_MSG( 1045, ERRInvalidDelete , "Calling delete on expression not allowed in strict mode") //string 4
LSC_ERROR_MSG( 1046, ERRDupeObjLit    , "Multiple definitions of a property not allowed in strict mode") //string 7
LSC_ERROR_MSG( 1047, ERRFncDeclNotSourceElement, "In strict mode, function declarations cannot be nested inside a statement or block. They may only appear at the top level or directly inside a function body.")
LSC_ERROR_MSG( 1048, ERRKeywordNotId  , "The use of a keyword for an identifier is invalid")
LSC_ERROR_MSG( 1049, ERRFutureReservedWordNotId, "The use of a future reserved word for an identifier is invalid")
LSC_ERROR_MSG( 1050, ERRFutureReservedWordInStrictModeNotId, "The use of a future reserved word for an identifier is invalid. The identifier name is reserved in strict mode.")
LSC_ERROR_MSG( 1051, ERRSetterMustHaveOneArgument  , "Setter functions must have one argument")
LSC_ERROR_MSG( 1052, ERRRedeclaration  , "Let/Const redeclaration") // "var x; let x;" is also a redeclaration
LSC_ERROR_MSG( 1053, ERRUninitializedConst  , "Const must be initialized")
LSC_ERROR_MSG( 1054, ERRDeclOutOfStmt  , "Declaration outside statement context")
LSC_ERROR_MSG( 1055, ERRAssignmentToConst  , "Assignment to const")
LSC_ERROR_MSG( 1056, ERRUnicodeOutOfRange  , "Unicode escape sequence value is higher than 0x10FFFF")
LSC_ERROR_MSG( 1057, ERRInvalidSpreadUse   , "Invalid use of the ... operator. Spread can only be used in call arguments or an array literal.")
LSC_ERROR_MSG( 1058, ERRInvalidSuper        , "Invalid use of the 'super' keyword")
LSC_ERROR_MSG( 1059, ERRInvalidSuperScope   , "The 'super' keyword cannot be used at global scope")
LSC_ERROR_MSG( 1060, ERRSuperInIndirectEval , "The 'super' keyword cannot be used in an indirect eval() call")
LSC_ERROR_MSG( 1061, ERRSuperInGlobalEval   , "The 'super' keyword cannot be used in a globally scoped eval() call")
LSC_ERROR_MSG( 1062, ERRnoDArrow      , "Expected '=>'")

LSC_ERROR_MSG( 1063, ERRInvalidCodePoint      , "Invalid codepoint value in the escape sequence.")
LSC_ERROR_MSG( 1064, ERRMissingCurlyBrace      , "Closing curly brace ('}') expected.")
LSC_ERROR_MSG( 1065, ERRRestLastArg, "The rest parameter must be the last parameter in a formals list.")
LSC_ERROR_MSG( 1066, ERRRestWithDefault, "The rest parameter cannot have a default intializer.")
LSC_ERROR_MSG( 1067, ERRUnexpectedEllipsis, "Unexpected ... operator")

LSC_ERROR_MSG( 1068, ERRDestructInit, "Destructuring declarations must have an initializer")
LSC_ERROR_MSG( 1069, ERRDestructRestLast, "Destructuring rest variables must be in the last position of the expression")
LSC_ERROR_MSG( 1070, ERRUnexpectedDefault, "Unexpected default initializer")
LSC_ERROR_MSG( 1071, ERRDestructNoOper, "Unexpected operator in destructuring expression")
LSC_ERROR_MSG( 1072, ERRDestructIDRef, "Destructuring expressions can only have identifier references")

LSC_ERROR_MSG( 1073, ERRYieldInTryCatchOrFinally, "'yield' expressions are not allowed in 'try', 'catch', or 'finally' blocks")
LSC_ERROR_MSG( 1074, ERRConstructorCannotBeGenerator, "Class constructor may not be a generator")
LSC_ERROR_MSG( 1075, ERRInvalidAssignmentTarget, "Invalid destructuring assignment target")
LSC_ERROR_MSG( 1076, ERRFormalSame, "Duplicate formal parameter names not allowed in this context")
LSC_ERROR_MSG( 1077, ERRDestructNotInit, "Destructuring declarations cannot have an initializer")
// 1078 -- removed
LSC_ERROR_MSG(1079, ERRInvalidNewTarget, "Invalid use of the 'new.target' keyword")
LSC_ERROR_MSG(1080, ERRForInNoInitAllowed, "for-in loop head declarations cannot have an initializer")
LSC_ERROR_MSG(1081, ERRForOfNoInitAllowed, "for-of loop head declarations cannot have an initializer")
