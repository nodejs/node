" Vim syntax file
" Language:	Jinja template
" Maintainer:	Armin Ronacher <armin.ronacher@active-4.com>
" Last Change:	2008 May 9
" Version:      1.1
"
" Known Bugs:
"   because of odd limitations dicts and the modulo operator
"   appear wrong in the template.
"
" Changes:
"
"     2008 May 9:     Added support for Jinja2 changes (new keyword rules)

" .vimrc variable to disable html highlighting
if !exists('g:jinja_syntax_html')
   let g:jinja_syntax_html=1
endif

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if !exists("main_syntax")
  if version < 600
    syntax clear
  elseif exists("b:current_syntax")
  finish
endif
  let main_syntax = 'jinja'
endif

" Pull in the HTML syntax.
if g:jinja_syntax_html
  if version < 600
    so <sfile>:p:h/html.vim
  else
    runtime! syntax/html.vim
    unlet b:current_syntax
  endif
endif

syntax case match

" Jinja template built-in tags and parameters (without filter, macro, is and raw, they
" have special threatment)
syn keyword jinjaStatement containedin=jinjaVarBlock,jinjaTagBlock,jinjaNested contained and if else in not or recursive as import

syn keyword jinjaStatement containedin=jinjaVarBlock,jinjaTagBlock,jinjaNested contained is filter skipwhite nextgroup=jinjaFilter
syn keyword jinjaStatement containedin=jinjaTagBlock contained macro skipwhite nextgroup=jinjaFunction
syn keyword jinjaStatement containedin=jinjaTagBlock contained block skipwhite nextgroup=jinjaBlockName

" Variable Names
syn match jinjaVariable containedin=jinjaVarBlock,jinjaTagBlock,jinjaNested contained /[a-zA-Z_][a-zA-Z0-9_]*/
syn keyword jinjaSpecial containedin=jinjaVarBlock,jinjaTagBlock,jinjaNested contained false true none False True None loop super caller varargs kwargs

" Filters
syn match jinjaOperator "|" containedin=jinjaVarBlock,jinjaTagBlock,jinjaNested contained skipwhite nextgroup=jinjaFilter
syn match jinjaFilter contained /[a-zA-Z_][a-zA-Z0-9_]*/
syn match jinjaFunction contained /[a-zA-Z_][a-zA-Z0-9_]*/
syn match jinjaBlockName contained /[a-zA-Z_][a-zA-Z0-9_]*/

" Jinja template constants
syn region jinjaString containedin=jinjaVarBlock,jinjaTagBlock,jinjaNested contained start=/"/ skip=/\(\\\)\@<!\(\(\\\\\)\@>\)*\\"/ end=/"/
syn region jinjaString containedin=jinjaVarBlock,jinjaTagBlock,jinjaNested contained start=/'/ skip=/\(\\\)\@<!\(\(\\\\\)\@>\)*\\'/ end=/'/
syn match jinjaNumber containedin=jinjaVarBlock,jinjaTagBlock,jinjaNested contained /[0-9]\+\(\.[0-9]\+\)\?/

" Operators
syn match jinjaOperator containedin=jinjaVarBlock,jinjaTagBlock,jinjaNested contained /[+\-*\/<>=!,:]/
syn match jinjaPunctuation containedin=jinjaVarBlock,jinjaTagBlock,jinjaNested contained /[()\[\]]/
syn match jinjaOperator containedin=jinjaVarBlock,jinjaTagBlock,jinjaNested contained /\./ nextgroup=jinjaAttribute
syn match jinjaAttribute contained /[a-zA-Z_][a-zA-Z0-9_]*/

" Jinja template tag and variable blocks
syn region jinjaNested matchgroup=jinjaOperator start="(" end=")" transparent display containedin=jinjaVarBlock,jinjaTagBlock,jinjaNested contained
syn region jinjaNested matchgroup=jinjaOperator start="\[" end="\]" transparent display containedin=jinjaVarBlock,jinjaTagBlock,jinjaNested contained
syn region jinjaNested matchgroup=jinjaOperator start="{" end="}" transparent display containedin=jinjaVarBlock,jinjaTagBlock,jinjaNested contained
syn region jinjaTagBlock matchgroup=jinjaTagDelim start=/{%-\?/ end=/-\?%}/ containedin=ALLBUT,jinjaTagBlock,jinjaVarBlock,jinjaRaw,jinjaString,jinjaNested,jinjaComment

syn region jinjaVarBlock matchgroup=jinjaVarDelim start=/{{-\?/ end=/-\?}}/ containedin=ALLBUT,jinjaTagBlock,jinjaVarBlock,jinjaRaw,jinjaString,jinjaNested,jinjaComment

" Jinja template 'raw' tag
syn region jinjaRaw matchgroup=jinjaRawDelim start="{%\s*raw\s*%}" end="{%\s*endraw\s*%}" containedin=ALLBUT,jinjaTagBlock,jinjaVarBlock,jinjaString,jinjaComment

" Jinja comments
syn region jinjaComment matchgroup=jinjaCommentDelim start="{#" end="#}" containedin=ALLBUT,jinjaTagBlock,jinjaVarBlock,jinjaString

" Block start keywords.  A bit tricker.  We only highlight at the start of a
" tag block and only if the name is not followed by a comma or equals sign
" which usually means that we have to deal with an assignment.
syn match jinjaStatement containedin=jinjaTagBlock contained /\({%-\?\s*\)\@<=\<[a-zA-Z_][a-zA-Z0-9_]*\>\(\s*[,=]\)\@!/

" and context modifiers
syn match jinjaStatement containedin=jinjaTagBlock contained /\<with\(out\)\?\s\+context\>/


" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet
if version >= 508 || !exists("did_jinja_syn_inits")
  if version < 508
    let did_jinja_syn_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif

  HiLink jinjaPunctuation jinjaOperator
  HiLink jinjaAttribute jinjaVariable
  HiLink jinjaFunction jinjaFilter

  HiLink jinjaTagDelim jinjaTagBlock
  HiLink jinjaVarDelim jinjaVarBlock
  HiLink jinjaCommentDelim jinjaComment
  HiLink jinjaRawDelim jinja

  HiLink jinjaSpecial Special
  HiLink jinjaOperator Normal
  HiLink jinjaRaw Normal
  HiLink jinjaTagBlock PreProc
  HiLink jinjaVarBlock PreProc
  HiLink jinjaStatement Statement
  HiLink jinjaFilter Function
  HiLink jinjaBlockName Function
  HiLink jinjaVariable Identifier
  HiLink jinjaString Constant
  HiLink jinjaNumber Constant
  HiLink jinjaComment Comment

  delcommand HiLink
endif

let b:current_syntax = "jinja"

if main_syntax == 'jinja'
  unlet main_syntax
endif
