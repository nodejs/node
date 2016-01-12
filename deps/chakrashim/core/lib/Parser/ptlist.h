//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
/*****************************************************************************/
#ifndef PTNODE
#error  Define PTNODE before including this file.
#endif
/*****************************************************************************/
//
//     Node oper
//                    , "Node name"
//                                          , pcode
//                                                    , parse node kind
//                                                                  , flags
//                                                                                          , JSON Name
//

PTNODE(knopNone       , "<none>"           , Nop      , None        , fnopNone              , ""                               )

/***************************************************************************
    Leaf nodes.
***************************************************************************/
PTNODE(knopName       , "name"             , Nop      , Pid         , fnopLeaf              , "NameExpr"                       )
PTNODE(knopInt        , "int const"        , Nop      , Int         , fnopLeaf|fnopConst    , "NumberLit"                      )
PTNODE(knopFlt        , "flt const"        , Nop      , Flt         , fnopLeaf|fnopConst    , "NumberLit"                      )
PTNODE(knopStr        , "str const"        , Nop      , Pid         , fnopLeaf|fnopConst    , "StringLit"                      )
PTNODE(knopRegExp     , "reg expr"         , Nop      , Pid         , fnopLeaf|fnopConst    , "RegExprLit"                     )
PTNODE(knopThis       , "this"             , Nop      , None        , fnopLeaf              , "ThisExpr"                       )
PTNODE(knopSuper      , "super"            , Nop      , None        , fnopLeaf              , "SuperExpr"                      )
PTNODE(knopNewTarget  , "new.target"       , Nop      , None        , fnopLeaf              , "NewTargetExpr"                  )
PTNODE(knopNull       , "null"             , Nop      , None        , fnopLeaf              , "NullLit"                        )
PTNODE(knopFalse      , "false"            , Nop      , None        , fnopLeaf              , "FalseLit"                       )
PTNODE(knopTrue       , "true"             , Nop      , None        , fnopLeaf              , "TrueLit"                        )
PTNODE(knopEmpty      , "empty"            , Nop      , None        , fnopLeaf              , "EmptStmt"                       )
PTNODE(knopYieldLeaf  , "yield leaf"       , Nop      , None        , fnopLeaf              , "YieldLeafExpr"                  )

/***************************************************************************
Unary operators.
***************************************************************************/
PTNODE(knopNot        , "~"                , Nop      , Uni         , fnopUni               , "BitNotOper"                     )
PTNODE(knopNeg        , "unary -"          , Nop      , Uni         , fnopUni               , "NegOper"                        )
PTNODE(knopPos        , "unary +"          , Nop      , Uni         , fnopUni               , "PosOper"                        )
PTNODE(knopLogNot     , "!"                , Nop      , Uni         , fnopUni               , "LogNotOper"                     )
PTNODE(knopEllipsis   , "..."              , Nop      , Uni         , fnopUni               , "Spread"                         )
// ___compact range : do not add or remove in this range.
//    Gen code of  OP_LclIncPost,.. depends on parallel tables with this range
PTNODE(knopIncPost    , "++ post"          , Nop      , Uni         , fnopUni|fnopAsg       , "PostIncExpr"                    )
PTNODE(knopDecPost    , "-- post"          , Nop      , Uni         , fnopUni|fnopAsg       , "PostDecExpr"                    )
PTNODE(knopIncPre     , "++ pre"           , Nop      , Uni         , fnopUni|fnopAsg       , "PreIncExpr"                     )
PTNODE(knopDecPre     , "-- pre"           , Nop      , Uni         , fnopUni|fnopAsg       , "PreDecExpr"                     )
//___end range
PTNODE(knopTypeof     , "typeof"           , Nop      , Uni         , fnopUni               , "TypeOfExpr"                     )
PTNODE(knopVoid       , "void"             , Nop      , Uni         , fnopUni               , "VoidExpr"                       )
PTNODE(knopDelete     , "delete"           , Nop      , Uni         , fnopUni               , "DeleteStmt"                     )
PTNODE(knopArray      , "arr cnst"         , Nop      , ArrLit      , fnopUni               , "ArrayExpr"                      )
PTNODE(knopObject     , "obj cnst"         , Nop      , Uni         , fnopUni               , "ObjectExpr"                     )
PTNODE(knopTempRef    , "temp ref"         , Nop      , Uni         , fnopUni               , "TempRef"                        )
PTNODE(knopComputedName,"[name]"           , Nop      , Uni         , fnopUni               , "ComputedNameExpr"               )
PTNODE(knopYield      , "yield"            , Nop      , Uni         , fnopUni|fnopAsg       , "YieldExpr"                      )
PTNODE(knopYieldStar  , "yield *"          , Nop      , Uni         , fnopUni|fnopAsg       , "YieldStarExpr"                  )
PTNODE(knopAwait      , "await"            , Nop      , Uni         , fnopUni               , "AwaitExpr"                      )
PTNODE(knopAsyncSpawn , "asyncspawn"       , Nop      , Bin         , fnopBin               , "AsyncSpawnExpr"                 )

/***************************************************************************
Binary and ternary operators.
***************************************************************************/
PTNODE(knopAdd        , "+"                , Add_A    , Bin         , fnopBin               , "AddOper"                        )
PTNODE(knopSub        , "-"                , Sub_A    , Bin         , fnopBin               , "SubOper"                        )
PTNODE(knopMul        , "*"                , Mul_A    , Bin         , fnopBin               , "MulOper"                        )
PTNODE(knopDiv        , "/"                , Div_A    , Bin         , fnopBin               , "DivOper"                        )
PTNODE(knopExpo       , "**"               , Expo_A   , Bin         , fnopBin               , "ExpoOper"                       )
PTNODE(knopMod        , "%"                , Rem_A    , Bin         , fnopBin               , "ModOper"                        )
PTNODE(knopOr         , "|"                , Or_A     , Bin         , fnopBin               , "BitOrOper"                      )
PTNODE(knopXor        , "^"                , Xor_A    , Bin         , fnopBin               , "BitXorOper"                     )
PTNODE(knopAnd        , "&"                , And_A    , Bin         , fnopBin               , "BitAndOper"                     )
PTNODE(knopEq         , "=="               , OP(Eq)   , Bin         , fnopBin|fnopRel       , "EqualOper"                      )
PTNODE(knopNe         , "!="               , OP(Neq)  , Bin         , fnopBin|fnopRel       , "NotEqualOper"                   )
PTNODE(knopLt         , "<"                , OP(Lt)   , Bin         , fnopBin|fnopRel       , "LessThanOper"                   )
PTNODE(knopLe         , "<="               , OP(Le)   , Bin         , fnopBin|fnopRel       , "LessThanEqualOper"              )
PTNODE(knopGe         , ">="               , OP(Ge)   , Bin         , fnopBin|fnopRel       , "GreaterThanEqualOper"           )
PTNODE(knopGt         , ">"                , OP(Gt)   , Bin         , fnopBin|fnopRel       , "GreaterThanOper"                )
PTNODE(knopCall       , "()"               , Nop      , Call        , fnopBin               , "CallExpr"                       )
PTNODE(knopDot        , "."                , Nop      , Bin         , fnopBin               , "DotOper"                        )
PTNODE(knopAsg        , "="                , Nop      , Bin         , fnopBin|fnopAsg       , "AssignmentOper"                 )
PTNODE(knopInstOf     , "instanceof"       , IsInst   , Bin         , fnopBin|fnopRel       , "InstanceOfExpr"                 )
PTNODE(knopIn         , "in"               , IsIn     , Bin         , fnopBin|fnopRel       , "InOper"                         )
PTNODE(knopEqv        , "==="              , OP(SrEq) , Bin         , fnopBin|fnopRel       , "StrictEqualOper"                )
PTNODE(knopNEqv       , "!=="              , OP(SrNeq), Bin         , fnopBin|fnopRel       , "NotStrictEqualOper"             )
PTNODE(knopComma      , ","                , Nop      , Bin         , fnopBin               , "CommaOper"                      )
PTNODE(knopLogOr      , "||"               , Nop      , Bin         , fnopBin               , "LogOrOper"                      )
PTNODE(knopLogAnd     , "&&"               , Nop      , Bin         , fnopBin               , "LogAndOper"                     )
PTNODE(knopLsh        , "<<"               , Shl_A    , Bin         , fnopBin               , "LeftShiftOper"                  )
PTNODE(knopRsh        , ">>"               , Shr_A    , Bin         , fnopBin               , "RightShiftOper"                 )
PTNODE(knopRs2        , ">>>"              , ShrU_A   , Bin         , fnopBin               , "UnsignedRightShiftOper"         )
PTNODE(knopNew        , "new"              , Nop      , Call        , fnopBin               , "NewExpr"                        )
PTNODE(knopIndex      , "[]"               , Nop      , Bin         , fnopBin               , "IndexOper"                      )
PTNODE(knopQmark      , "?"                , Nop      , Tri         , fnopBin               , "IfExpr"                         )

// ___compact range : do not add or remove in this range.
//    Gen code of  OP_LclAsg*,.. depends on parallel tables with this range
PTNODE(knopAsgAdd     , "+="               , Add_A    , Bin         , fnopBin|fnopAsg       , "AddAssignExpr"                  )
PTNODE(knopAsgSub     , "-="               , Sub_A    , Bin         , fnopBin|fnopAsg       , "SubAssignExpr"                  )
PTNODE(knopAsgMul     , "*="               , Mul_A    , Bin         , fnopBin|fnopAsg       , "MulAssignExpr"                  )
PTNODE(knopAsgDiv     , "/="               , Div_A    , Bin         , fnopBin|fnopAsg       , "DivAssignExpr"                  )
PTNODE(knopAsgExpo    , "**="              , Expo_A   , Bin         , fnopBin|fnopAsg       , "ExpoAssignExpr"                 )
PTNODE(knopAsgMod     , "%="               , Rem_A    , Bin         , fnopBin|fnopAsg       , "ModAssignExpr"                  )
PTNODE(knopAsgAnd     , "&="               , And_A    , Bin         , fnopBin|fnopAsg       , "BitAndAssignExpr"               )
PTNODE(knopAsgXor     , "^="               , Xor_A    , Bin         , fnopBin|fnopAsg       , "BitXorAssignExpr"               )
PTNODE(knopAsgOr      , "|="               , Or_A     , Bin         , fnopBin|fnopAsg       , "BitOrAssignExpr"                )
PTNODE(knopAsgLsh     , "<<="              , Shl_A    , Bin         , fnopBin|fnopAsg       , "LeftShiftAssignExpr"            )
PTNODE(knopAsgRsh     , ">>="              , Shr_A    , Bin         , fnopBin|fnopAsg       , "RightShiftAssignExpr"           )
PTNODE(knopAsgRs2     , ">>>="             , ShrU_A   , Bin         , fnopBin|fnopAsg       , "UnsignedRightShiftAssignExpr"   )
//___end range

PTNODE(knopMember     , ":"                , Nop      , Bin         , fnopNotExprStmt|fnopBin, "MemberOper"                    )
PTNODE(knopMemberShort, "membShort"        , Nop      , Bin         , fnopNotExprStmt|fnopBin, "ShorthandMember"               )
PTNODE(knopSetMember  , "set"              , Nop      , Bin         , fnopBin                , "SetDecl"                       )
PTNODE(knopGetMember  , "get"              , Nop      , Bin         , fnopBin                , "GetDecl"                       )
/***************************************************************************
General nodes.
***************************************************************************/
PTNODE(knopList       , "<list>"           , Nop      , Bin         , fnopBinList|fnopNotExprStmt, ""                          )
PTNODE(knopVarDecl    , "varDcl"           , Nop      , Var         , fnopNotExprStmt        , "VarDecl"                       )
PTNODE(knopConstDecl  , "constDcl"         , Nop      , Var         , fnopNotExprStmt        , "ConstDecl"                     )
PTNODE(knopLetDecl    , "letDcl"           , Nop      , Var         , fnopNotExprStmt        , "LetDecl"                       )
PTNODE(knopTemp       , "temp"             , Nop      , Var         , fnopNone               , "Temp"                          )
PTNODE(knopFncDecl    , "fncDcl"           , Nop      , Fnc         , fnopLeaf               , "FuncDecl"                      )
PTNODE(knopClassDecl  , "classDecl"        , Nop      , Class       , fnopLeaf               , "ClassDecl"                     )
PTNODE(knopProg       , "program"          , Nop      , Prog        , fnopNotExprStmt        , "Unit"                          )
PTNODE(knopEndCode    , "<endcode>"        , Nop      , None        , fnopNotExprStmt        , ""                              )
PTNODE(knopDebugger   , "debugger"         , Nop      , None        , fnopNotExprStmt        , "DebuggerStmt"                  )
PTNODE(knopFor        , "for"              , Nop      , For         , fnopNotExprStmt|fnopCleanup|fnopBreak|fnopContinue , "ForStmtm"       )
PTNODE(knopIf         , "if"               , Nop      , If          , fnopNotExprStmt        , "IfStmt"                        )
PTNODE(knopWhile      , "while"            , Nop      , While       , fnopNotExprStmt|fnopCleanup|fnopBreak|fnopContinue , "WhileStmt"      )
PTNODE(knopDoWhile    , "do-while"         , Nop      , While       , fnopNotExprStmt|fnopCleanup|fnopBreak|fnopContinue , "DoWhileStmt"    )
PTNODE(knopForIn      , "for in"           , Nop      , ForIn       , fnopNotExprStmt|fnopCleanup|fnopBreak|fnopContinue , "ForInStmt"      )
PTNODE(knopForOf      , "for of"           , Nop      , ForOf       , fnopNotExprStmt|fnopCleanup|fnopBreak|fnopContinue , "ForOfStmt"      )
PTNODE(knopBlock      , "{}"               , Nop      , Block       , fnopNotExprStmt        , "Block"                         )
PTNODE(knopStrTemplate, "``"               , Nop      , StrTemplate , fnopNone               , "StringTemplateDecl"            )
PTNODE(knopWith       , "with"             , Nop      , With        , fnopNotExprStmt        , "WithStmt"                      )
PTNODE(knopBreak      , "break"            , Nop      , Jump        , fnopNotExprStmt        , "BreakStmt"                     )
PTNODE(knopContinue   , "continue"         , Nop      , Jump        , fnopNotExprStmt        , "ContinueStmt"                  )
PTNODE(knopLabel      , "label"            , Nop      , Label       , fnopNotExprStmt        , "LabelDecl"                     )
PTNODE(knopSwitch     , "switch"           , Nop      , Switch      , fnopNotExprStmt|fnopBreak, "SwitchStmt"                  )
PTNODE(knopCase       , "case"             , Nop      , Case        , fnopNotExprStmt        , "CaseStmt"                      )
PTNODE(knopTryCatch   , "try-catch"        , Nop      , TryCatch    , fnopNotExprStmt        , "TryCatchStmt"                  )
PTNODE(knopCatch      , "catch"            , Nop      , Catch       , fnopNotExprStmt|fnopCleanup, "CatchClause"               )
PTNODE(knopReturn     , "return"           , Nop      , Return      , fnopNotExprStmt        , "ReturnStmt"                    )
PTNODE(knopTry        , "try"              , Nop      , Try         , fnopNotExprStmt|fnopCleanup, "TryStmt"                   )
PTNODE(knopThrow      , "throw"            , Nop      , Uni         , fnopNotExprStmt        , "ThrowStmt"                     )
PTNODE(knopFinally    , "finally"          , Nop      , Finally     , fnopNotExprStmt|fnopCleanup, "FinallyStmt"               )
PTNODE(knopTryFinally , "try-finally"      , Nop      , TryFinally  , fnopNotExprStmt        , "TryFinallyStmt"                )
PTNODE(knopObjectPattern, "{} = "          , Nop      , Uni         , fnopUni                , "ObjectAssignmentPattern"       )
PTNODE(knopObjectPatternMember, "{:} = "   , Nop      , Bin         , fnopBin                , "ObjectAssignmentPatternMember" )
PTNODE(knopArrayPattern, "[] = "           , Nop      , ArrLit      , fnopUni                , "ArrayAssignmentPattern"        )
PTNODE(knopParamPattern, "({[]})"          , Nop      , ParamPattern, fnopUni                , "DestructurePattern"            )


#undef PTNODE
#undef OP
