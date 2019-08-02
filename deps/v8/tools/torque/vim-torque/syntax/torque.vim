" Copyright 2018 the V8 project authors. All rights reserved.
" Use of this source code is governed by a BSD-style license that can be
" found in the LICENSE file.

if !exists("main_syntax")
  " quit when a syntax file was already loaded
  if exists("b:current_syntax")
    finish
  endif
  let main_syntax = 'torque'
elseif exists("b:current_syntax") && b:current_syntax == "torque"
  finish
endif

let s:cpo_save = &cpo
set cpo&vim

syn match   torqueLineComment      "\/\/.*" contains=@Spell
syn region  torqueComment	   start="/\*"  end="\*/" contains=@Spell
syn region  torqueStringS	   start=+'+  skip=+\\\\\|\\'+  end=+'\|$+

syn keyword torqueAssert assert check debug unreachable
syn keyword torqueAtom True False Undefined Hole Null
syn keyword torqueBoolean true false
syn keyword torqueBranch break continue goto
syn keyword torqueConditional if else typeswitch otherwise
syn match torqueConstant /\v<[A-Z][A-Z0-9_]+>/
syn match torqueConstant /\v<k[A-Z][A-Za-z0-9]*>/
syn keyword torqueFunction macro builtin runtime intrinsic
syn keyword torqueKeyword cast convert from_constexpr min max unsafe_cast
syn keyword torqueLabel case
syn keyword torqueMatching try label catch
syn keyword torqueModifier extern javascript constexpr transitioning transient weak export
syn match torqueNumber /\v<[0-9]+(\.[0-9]*)?>/
syn match torqueNumber /\v<0x[0-9a-fA-F]+>/
syn keyword torqueOperator operator
syn keyword torqueRel extends generates labels
syn keyword torqueRepeat while for of
syn keyword torqueStatement return tail
syn keyword torqueStructure module struct type class
syn keyword torqueVariable const let

syn match torqueType /\v(\<)@<=([A-Za-z][0-9A-Za-z_]*)(>)@=/
syn match torqueType /\v(:\s*(constexpr\s*)?)@<=([A-Za-z][0-9A-Za-z_]*)/
" Include some common types also
syn keyword torqueType Arguments void never
syn keyword torqueType Tagged Smi HeapObject Object
syn keyword torqueType int32 uint32 int64 intptr uintptr float32 float64
syn keyword torqueType bool string
syn keyword torqueType int31 RawPtr AbstractCode Code JSReceiver Context String
syn keyword torqueType Oddball HeapNumber Number BigInt Numeric Boolean JSProxy
syn keyword torqueType JSObject JSArray JSFunction JSBoundFunction Callable Map

hi def link torqueAssert		Statement
hi def link torqueAtom		Constant
hi def link torqueBoolean		Boolean
hi def link torqueBranch		Conditional
hi def link torqueComment		Comment
hi def link torqueConditional		Conditional
hi def link torqueConstant		Constant
hi def link torqueFunction		Function
hi def link torqueKeyword		Keyword
hi def link torqueLabel		Label
hi def link torqueLineComment		Comment
hi def link torqueMatching		Exception
hi def link torqueModifier		StorageClass
hi def link torqueNumber		Number
hi def link torqueOperator		Operator
hi def link torqueRel		StorageClass
hi def link torqueRepeat		Repeat
hi def link torqueStatement		Statement
hi def link torqueStringS		String
hi def link torqueStructure		Structure
hi def link torqueType		Type
hi def link torqueVariable		Identifier

let b:current_syntax = "torque"
if main_syntax == 'torque'
  unlet main_syntax
endif
let &cpo = s:cpo_save
unlet s:cpo_save

" vim: set ts=8:
