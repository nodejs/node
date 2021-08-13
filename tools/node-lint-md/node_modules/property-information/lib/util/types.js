var powers = 0

export var boolean = increment()
export var booleanish = increment()
export var overloadedBoolean = increment()
export var number = increment()
export var spaceSeparated = increment()
export var commaSeparated = increment()
export var commaOrSpaceSeparated = increment()

function increment() {
  return 2 ** ++powers
}
