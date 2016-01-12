//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// These byte codes _cannot_ appear in the regexOpCode field of a Node.
RegOp(GreedyStar       ,Rev,Lng, 0 , 0 )   // 00/00 *   label, cchMinRem
RegOp(RevBranch        ,Rev,Lng,Lng, 0 )   // 01/01     label, cchMinRem, pnode->cchMinTot
RegOp(GreedyLoop       ,Rev,Lng,Lng,Lng)   // 02/02     label, min, max, cchMinRem
RegOp(LoopInit         ,Fwd,Int,Lng, 0 )   // 03/03     label, #, min
RegOp(GreedyLoopTest   ,Rev,Int,Lng,Lng)   // 04/04     label, #, max-min, cchMinRem

RegOp(NonGreedyStar    ,Rev,Lng, 0 , 0 )   //    05 *   label, cchMinRem
RegOp(NonGreedyLoop    ,Rev,Lng,Lng,Lng)   //    06     label, min, max, cchMinRem
RegOp(NonGreedyLoopTest,Rev,Int,Lng,Lng)   //    07     label, #, max-min, cchMinRem

RegOp(Open             ,Int, 0 , 0 , 0 )   // 05/08 (   #
RegOp(Close            ,Int, 0 , 0 , 0 )   // 06/09 )   #
RegOp(MatchOne         ,Chr, 0 , 0 , 0 )   // 07/0A     character
RegOp(Need             ,Lng, 0 , 0 , 0 )   // 08/0B     cchMinRem
RegOp(Fail             , 0 , 0 , 0 , 0 )   // 09/0C     no way to match this
RegOp(Jump             ,Fwd, 0 , 0 , 0 )   // 0A/0D     label

RegOp(PosLookahead     ,Fwd, 0 , 0 , 0 )   //    0E     label
RegOp(NegLookahead     ,Fwd, 0 , 0 , 0 )   //    0F     label
RegOp(LookaheadEnd     , 0 , 0 , 0 , 0 )   //    10

RegOp(End              , 0 , 0 , 0 , 0 )   // 0B/11

// These byte codes _can_ appear in the regexOpCode field of a Node.
RegOp(Branch           ,Fwd, 0 , 0 , 0 )   // 0C/12     label
RegOp(Match            ,Cch, 0 , 0 , 0 )   // 0D/13     cch, characters
RegOp(MatchGroup       ,Int, 0 , 0 , 0 )   // 0E/14 \1  #
RegOp(Head             , 0 , 0 , 0 , 0 )   // 0F/15 ^
RegOp(Tail             , 0 , 0 , 0 , 0 )   // 10/16 $
RegOp(WordBound        , 0 , 0 , 0 , 0 )   // 11/17 \b
RegOp(NotWordBound     , 0 , 0 , 0 , 0 )   // 12/18 \B
RegOp(Any              , 0 , 0 , 0 , 0 )   // 13/19     No longer used
RegOp(AnyOf            ,Cch, 0 , 0 , 0 )   // 14/1A []  cch, transition characters
RegOp(AnyBut           ,Cch, 0 , 0 , 0 )   // 15/1B [^] cch, transition characters
RegOp(Digit            , 0 , 0 , 0 , 0 )   // 16/1C \d
RegOp(NotDigit         , 0 , 0 , 0 , 0 )   // 17/1D \D
RegOp(Space            , 0 , 0 , 0 , 0 )   // 18/1E \s
RegOp(NotSpace         , 0 , 0 , 0 , 0 )   // 19/1F \S
RegOp(Letter           , 0 , 0 , 0 , 0 )   // 1A/20 \w
RegOp(NotLetter        , 0 , 0 , 0 , 0 )   // 1B/21 \W
RegOp(NotLF            , 0 , 0 , 0 , 0 )   // 1C/22 .
RegOp(Dummy            , 0 , 0 , 0 , 0 )   // 1D/23    Dummy. Default opcode to check for absence of opcode

#undef RegOp
