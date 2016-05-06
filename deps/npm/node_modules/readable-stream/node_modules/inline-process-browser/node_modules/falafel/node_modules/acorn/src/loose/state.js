import {tokenizer, SourceLocation, tokTypes as tt} from ".."

export function LooseParser(input, options) {
  this.toks = tokenizer(input, options)
  this.options = this.toks.options
  this.input = this.toks.input
  this.tok = this.last = {type: tt.eof, start: 0, end: 0}
  if (this.options.locations) {
    let here = this.toks.curPosition()
    this.tok.loc = new SourceLocation(this.toks, here, here)
  }
  this.ahead = []; // Tokens ahead
  this.context = []; // Indentation contexted
  this.curIndent = 0
  this.curLineStart = 0
  this.nextLineStart = this.lineEnd(this.curLineStart) + 1
}
