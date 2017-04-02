import {reservedWords, keywords} from "./identifier"
import {types as tt} from "./tokentype"
import {lineBreak} from "./whitespace"
import {getOptions} from "./options"

// Registered plugins
export const plugins = {}

function keywordRegexp(words) {
  return new RegExp("^(" + words.replace(/ /g, "|") + ")$")
}

export class Parser {
  constructor(options, input, startPos) {
    this.options = options = getOptions(options)
    this.sourceFile = options.sourceFile
    this.keywords = keywordRegexp(keywords[options.ecmaVersion >= 6 ? 6 : 5])
    let reserved = ""
    if (!options.allowReserved) {
      for (let v = options.ecmaVersion;; v--)
        if (reserved = reservedWords[v]) break
      if (options.sourceType == "module") reserved += " await"
    }
    this.reservedWords = keywordRegexp(reserved)
    let reservedStrict = (reserved ? reserved + " " : "") + reservedWords.strict
    this.reservedWordsStrict = keywordRegexp(reservedStrict)
    this.reservedWordsStrictBind = keywordRegexp(reservedStrict + " " + reservedWords.strictBind)
    this.input = String(input)

    // Used to signal to callers of `readWord1` whether the word
    // contained any escape sequences. This is needed because words with
    // escape sequences must not be interpreted as keywords.
    this.containsEsc = false

    // Load plugins
    this.loadPlugins(options.plugins)

    // Set up token state

    // The current position of the tokenizer in the input.
    if (startPos) {
      this.pos = startPos
      this.lineStart = this.input.lastIndexOf("\n", startPos - 1) + 1
      this.curLine = this.input.slice(0, this.lineStart).split(lineBreak).length
    } else {
      this.pos = this.lineStart = 0
      this.curLine = 1
    }

    // Properties of the current token:
    // Its type
    this.type = tt.eof
    // For tokens that include more information than their type, the value
    this.value = null
    // Its start and end offset
    this.start = this.end = this.pos
    // And, if locations are used, the {line, column} object
    // corresponding to those offsets
    this.startLoc = this.endLoc = this.curPosition()

    // Position information for the previous token
    this.lastTokEndLoc = this.lastTokStartLoc = null
    this.lastTokStart = this.lastTokEnd = this.pos

    // The context stack is used to superficially track syntactic
    // context to predict whether a regular expression is allowed in a
    // given position.
    this.context = this.initialContext()
    this.exprAllowed = true

    // Figure out if it's a module code.
    this.inModule = options.sourceType === "module"
    this.strict = this.inModule || this.strictDirective(this.pos)

    // Used to signify the start of a potential arrow function
    this.potentialArrowAt = -1

    // Flags to track whether we are in a function, a generator, an async function.
    this.inFunction = this.inGenerator = this.inAsync = false
    // Positions to delayed-check that yield/await does not exist in default parameters.
    this.yieldPos = this.awaitPos = 0
    // Labels in scope.
    this.labels = []

    // If enabled, skip leading hashbang line.
    if (this.pos === 0 && options.allowHashBang && this.input.slice(0, 2) === "#!")
      this.skipLineComment(2)

    // Scope tracking for duplicate variable names (see scope.js)
    this.scopeStack = []
    this.enterFunctionScope()
  }

  // DEPRECATED Kept for backwards compatibility until 3.0 in case a plugin uses them
  isKeyword(word) { return this.keywords.test(word) }
  isReservedWord(word) { return this.reservedWords.test(word) }

  extend(name, f) {
    this[name] = f(this[name])
  }

  loadPlugins(pluginConfigs) {
    for (let name in pluginConfigs) {
      let plugin = plugins[name]
      if (!plugin) throw new Error("Plugin '" + name + "' not found")
      plugin(this, pluginConfigs[name])
    }
  }

  parse() {
    let node = this.options.program || this.startNode()
    this.nextToken()
    return this.parseTopLevel(node)
  }
}
