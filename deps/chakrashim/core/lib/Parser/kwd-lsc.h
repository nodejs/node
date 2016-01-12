//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#ifndef KEYWORD
#define KEYWORD(tk,f,prec2,nop2,prec1,nop1,name)
#endif //!KEYWORD

#ifndef S_KEYWORD
#define S_KEYWORD(name,f,lab)
#endif //!S_KEYWORD

//      token          reserved word? (see fidXXX values in enum, in hash.h)
//                        binary operator precedence
//                            binary operator
//                                         unary operator precedence
//                                             unary operator
//                                                          name

KEYWORD(tkABSTRACT    ,0, No, knopNone   , No, knopNone   , abstract)
KEYWORD(tkASSERT      ,0, No, knopNone   , No, knopNone   , assert)
KEYWORD(tkAWAIT       ,1, No, knopNone   ,Uni, knopAwait  , await)
KEYWORD(tkBOOLEAN     ,0, No, knopNone   , No, knopNone   , boolean)
KEYWORD(tkBREAK       ,1, No, knopNone   , No, knopNone   , break)
KEYWORD(tkBYTE        ,0, No, knopNone   , No, knopNone   , byte)
KEYWORD(tkCASE        ,1, No, knopNone   , No, knopNone   , case)
KEYWORD(tkCATCH       ,1, No, knopNone   , No, knopNone   , catch)
KEYWORD(tkCHAR        ,0, No, knopNone   , No, knopNone   , char)
KEYWORD(tkCONTINUE    ,1, No, knopNone   , No, knopNone   , continue)
KEYWORD(tkDEBUGGER    ,1, No, knopNone   , No, knopNone   , debugger)
KEYWORD(tkDECIMAL     ,0, No, knopNone   , No, knopNone   , decimal)
KEYWORD(tkDEFAULT     ,1, No, knopNone   , No, knopNone   , default)
KEYWORD(tkDELETE      ,1, No, knopNone   ,Uni, knopDelete , delete)
KEYWORD(tkDO          ,1, No, knopNone   , No, knopNone   , do)
KEYWORD(tkDOUBLE      ,0, No, knopNone   , No, knopNone   , double)
KEYWORD(tkELSE        ,1, No, knopNone   , No, knopNone   , else)
KEYWORD(tkENSURE      ,0, No, knopNone   , No, knopNone   , ensure)
KEYWORD(tkEVENT       ,0, No, knopNone   , No, knopNone   , event)
KEYWORD(tkFALSE       ,1, No, knopNone   , No, knopNone   , false)
KEYWORD(tkFINAL       ,0, No, knopNone   , No, knopNone   , final)
KEYWORD(tkFINALLY     ,1, No, knopNone   , No, knopNone   , finally)
KEYWORD(tkFLOAT       ,0, No, knopNone   , No, knopNone   , float)
KEYWORD(tkFOR         ,1, No, knopNone   , No, knopNone   , for)
KEYWORD(tkFUNCTION    ,1, No, knopNone   , No, knopNone   , function)
KEYWORD(tkGET         ,0, No, knopNone   , No, knopNone   , get)
KEYWORD(tkGOTO        ,0, No, knopNone   , No, knopNone   , goto)
KEYWORD(tkIF          ,1, No, knopNone   , No, knopNone   , if)
KEYWORD(tkIN          ,1, Cmp, knopIn ,    No, knopNone   , in)
KEYWORD(tkINSTANCEOF  ,1, Cmp,knopInstOf , No, knopNone   , instanceof)
KEYWORD(tkINT         ,0, No, knopNone   , No, knopNone   , int)
KEYWORD(tkINTERNAL    ,0, No, knopNone   , No, knopNone   , internal)
KEYWORD(tkINVARIANT   ,0, No, knopNone   , No, knopNone   , invariant)
KEYWORD(tkLONG        ,0, No, knopNone   , No, knopNone   , long)
KEYWORD(tkNAMESPACE   ,0, No, knopNone   , No, knopNone   , namespace)
KEYWORD(tkNATIVE      ,0, No, knopNone   , No, knopNone   , native)
KEYWORD(tkNEW         ,1, No, knopNone   , No, knopNone   , new)
KEYWORD(tkNULL        ,1, No, knopNone   , No, knopNone   , null)
KEYWORD(tkREQUIRE     ,0, No, knopNone   , No, knopNone   , require)
KEYWORD(tkRETURN      ,1, No, knopNone   , No, knopNone   , return)
KEYWORD(tkSBYTE       ,0, No, knopNone   , No, knopNone   , sbyte)
KEYWORD(tkSET         ,0, No, knopNone   , No, knopNone   , set)
KEYWORD(tkSHORT       ,0, No, knopNone   , No, knopNone   , short)
KEYWORD(tkSWITCH      ,1, No, knopNone   , No, knopNone   , switch)
KEYWORD(tkSYNCHRONIZED,0, No, knopNone   , No, knopNone   , synchronized)
KEYWORD(tkTHIS        ,1, No, knopNone   , No, knopNone   , this)
KEYWORD(tkTHROW       ,1, No, knopNone   , No, knopNone   , throw)
KEYWORD(tkTHROWS      ,0, No, knopNone   , No, knopNone   , throws)
KEYWORD(tkTRANSIENT   ,0, No, knopNone   , No, knopNone   , transient)
KEYWORD(tkTRUE        ,1, No, knopNone   , No, knopNone   , true)
KEYWORD(tkTRY         ,1, No, knopNone   , No, knopNone   , try)
KEYWORD(tkTYPEOF      ,1, No, knopNone   ,Uni, knopTypeof , typeof)
KEYWORD(tkUINT        ,0, No, knopNone   , No, knopNone   , uint)
KEYWORD(tkULONG       ,0, No, knopNone   , No, knopNone   , ulong)
KEYWORD(tkUSE         ,0, No, knopNone   , No, knopNone   , use)
KEYWORD(tkUSHORT      ,0, No, knopNone   , No, knopNone   , ushort)
KEYWORD(tkVAR         ,1, No, knopNone   , No, knopNone   , var)
KEYWORD(tkVOID        ,1, No, knopNone   ,Uni, knopVoid   , void)
KEYWORD(tkVOLATILE    ,0, No, knopNone   , No, knopNone   , volatile)
KEYWORD(tkWHILE       ,1, No, knopNone   , No, knopNone   , while)
KEYWORD(tkWITH        ,1, No, knopNone   , No, knopNone   , with)

// Future reserved words that become keywords in ES6
KEYWORD(tkCLASS       ,1, No, knopNone   , No, knopNone   , class)
KEYWORD(tkCONST       ,1, No, knopNone   , No, knopNone   , const)
KEYWORD(tkEXPORT      ,1, No, knopNone   , No, knopNone   , export)
KEYWORD(tkEXTENDS     ,1, No, knopNone   , No, knopNone   , extends)
KEYWORD(tkIMPORT      ,1, No, knopNone   , No, knopNone   , import)
KEYWORD(tkSUPER       ,1, No, knopNone   , No, knopNone   , super)
// Note: yield is still treated as an identifier in non-strict, non-generator functions
// and is special cased in jsscan.js when generating kwd-swtch.h
// Note: yield is a weird operator in that it has assignment expression level precedence
// but looks like a unary operator
KEYWORD(tkYIELD       ,1, No, knopNone   ,Asg, knopYield  , yield)

// Future reserved words in strict and non-strict modes
KEYWORD(tkENUM        ,1, No, knopNone   , No, knopNone   , enum)

// Additional future reserved words in strict mode
KEYWORD(tkIMPLEMENTS  ,2, No, knopNone   , No, knopNone   , implements)
KEYWORD(tkINTERFACE   ,2, No, knopNone   , No, knopNone   , interface)
KEYWORD(tkLET         ,2, No, knopNone   , No, knopNone   , let)
KEYWORD(tkPACKAGE     ,2, No, knopNone   , No, knopNone   , package)
KEYWORD(tkPRIVATE     ,2, No, knopNone   , No, knopNone   , private)
KEYWORD(tkPROTECTED   ,2, No, knopNone   , No, knopNone   , protected)
KEYWORD(tkPUBLIC      ,2, No, knopNone   , No, knopNone   , public)
KEYWORD(tkSTATIC      ,2, No, knopNone   , No, knopNone   , static)

S_KEYWORD(LEval        ,3, eval)
S_KEYWORD(LArguments   ,3, arguments)
S_KEYWORD(LTarget      ,3, target)

#undef KEYWORD

#ifndef TOK_DCL
#define TOK_DCL(tk,prec2,nop2,prec1,nop1)
#endif //!TOK_DCL

// The identifier token must follow the last identifier keyword
TOK_DCL(tkID            , No, knopNone   , No, knopNone)

// Non-operator non-identifier tokens
TOK_DCL(tkSColon        , No, knopNone   , No, knopNone   ) // ;
TOK_DCL(tkRParen        , No, knopNone   , No, knopNone   ) // )
TOK_DCL(tkRBrack        , No, knopNone   , No, knopNone   ) // ]
TOK_DCL(tkLCurly        , No, knopNone   , No, knopNone   ) // {
TOK_DCL(tkRCurly        , No, knopNone   , No, knopNone   ) // }

// Operator non-identifier tokens
TOK_DCL(tkComma         ,Cma, knopComma  , No, knopNone   ) // ,
TOK_DCL(tkDArrow        ,Asg, knopFncDecl, No, knopNone   ) // =>
TOK_DCL(tkAsg           ,Asg, knopAsg    , No, knopNone   ) // =
TOK_DCL(tkAsgAdd        ,Asg, knopAsgAdd , No, knopNone   ) // +=
TOK_DCL(tkAsgSub        ,Asg, knopAsgSub , No, knopNone   ) // -=
TOK_DCL(tkAsgMul        ,Asg, knopAsgMul , No, knopNone   ) // *=
TOK_DCL(tkAsgDiv        ,Asg, knopAsgDiv , No, knopNone   ) // /=
TOK_DCL(tkAsgExpo       ,Asg, knopAsgExpo, No, knopNone   ) // **=
TOK_DCL(tkAsgMod        ,Asg, knopAsgMod , No, knopNone   ) // %=
TOK_DCL(tkAsgAnd        ,Asg, knopAsgAnd , No, knopNone   ) // &=
TOK_DCL(tkAsgXor        ,Asg, knopAsgXor , No, knopNone   ) // ^=
TOK_DCL(tkAsgOr         ,Asg, knopAsgOr  , No, knopNone   ) // |=
TOK_DCL(tkAsgLsh        ,Asg, knopAsgLsh , No, knopNone   ) // <<=
TOK_DCL(tkAsgRsh        ,Asg, knopAsgRsh , No, knopNone   ) // >>=
TOK_DCL(tkAsgRs2        ,Asg, knopAsgRs2 , No, knopNone   ) // >>>=
TOK_DCL(tkQMark         ,Que, knopQmark  , No, knopNone   ) // ?
TOK_DCL(tkColon         , No, knopNone   , No, knopNone   ) // :
TOK_DCL(tkLogOr         ,Lor, knopLogOr  , No, knopNone   ) // ||
TOK_DCL(tkLogAnd        ,Lan, knopLogAnd , No, knopNone   ) // &&
TOK_DCL(tkOr            ,Bor, knopOr     , No, knopNone   ) // |
TOK_DCL(tkXor           ,Xor, knopXor    , No, knopNone   ) // ^
TOK_DCL(tkAnd           ,Ban, knopAnd    , No, knopNone   ) // &
TOK_DCL(tkEQ            ,Equ, knopEq     , No, knopNone   ) // ==
TOK_DCL(tkNE            ,Equ, knopNe     , No, knopNone   ) // !=
TOK_DCL(tkEqv           ,Equ, knopEqv    , No, knopNone   ) // ===
TOK_DCL(tkNEqv          ,Equ, knopNEqv   , No, knopNone   ) // !==
TOK_DCL(tkLT            ,Cmp, knopLt     , No, knopNone   ) // <
TOK_DCL(tkLE            ,Cmp, knopLe     , No, knopNone   ) // <=
TOK_DCL(tkGT            ,Cmp, knopGt     , No, knopNone   ) // >
TOK_DCL(tkGE            ,Cmp, knopGe     , No, knopNone   ) // >=
TOK_DCL(tkLsh           ,Shf, knopLsh    , No, knopNone   ) // <<
TOK_DCL(tkRsh           ,Shf, knopRsh    , No, knopNone   ) // >>
TOK_DCL(tkRs2           ,Shf, knopRs2    , No, knopNone   ) // >>>
TOK_DCL(tkAdd           ,Add, knopAdd    ,Uni, knopPos    ) // +
TOK_DCL(tkSub           ,Add, knopSub    ,Uni, knopNeg    ) // -
TOK_DCL(tkExpo          ,Expo, knopExpo  , No, knopNone   ) // **
TOK_DCL(tkStar          ,Mul, knopMul    , No, knopNone   ) // *
TOK_DCL(tkDiv           ,Mul, knopDiv    , No, knopNone   ) // /
TOK_DCL(tkPct           ,Mul, knopMod    , No, knopNone   ) // %
TOK_DCL(tkTilde         , No, knopNone   ,Uni, knopNot    ) // ~
TOK_DCL(tkBang          , No, knopNone   ,Uni, knopLogNot ) // !
TOK_DCL(tkInc           , No, knopNone   ,Uni, knopIncPre ) // ++
TOK_DCL(tkDec           , No, knopNone   ,Uni, knopDecPre ) // --
TOK_DCL(tkEllipsis      , No, knopNone   ,Spr, knopEllipsis ) // ...
TOK_DCL(tkLParen        , No, knopNone   , No, knopNone   ) // (
TOK_DCL(tkLBrack        , No, knopNone   , No, knopNone   ) // [
TOK_DCL(tkDot           , No, knopNone   , No, knopNone   ) // .

// String template tokens
TOK_DCL(tkStrTmplBasic  , No, knopNone   , No, knopNone   ) // `...`
TOK_DCL(tkStrTmplBegin  , No, knopNone   , No, knopNone   ) // `...${
TOK_DCL(tkStrTmplMid    , No, knopNone   , No, knopNone   ) // }...${  Note: tkStrTmplMid and tkStrTmplEnd tokens do not actually contain the opening '}' character.
TOK_DCL(tkStrTmplEnd    , No, knopNone   , No, knopNone   ) // }...`         Since the scanner can't disambiguate a tkRCurly which is part of the expression, literal, or string template syntax
                                                            //               we check to make sure the token after parsing the expression is a tkRCurly and put the scanner into a string template
                                                            //               scanning mode which will scan the string literal and search for the closing '${' or '`'.

TOK_DCL(tkComment       , No, knopNone, No, knopNone ) // Comment for syntax coloring
TOK_DCL(tkScanError     , No, knopNone, No, knopNone ) // Error in syntax coloring


#undef TOK_DCL
