// @ts-ignore
import formatter from 'format'

export var fault = Object.assign(create(Error), {
  eval: create(EvalError),
  range: create(RangeError),
  reference: create(ReferenceError),
  syntax: create(SyntaxError),
  type: create(TypeError),
  uri: create(URIError)
})

/**
 * Create a new `EConstructor`, with the formatted `format` as a first argument.
 *
 * @template {Error} Fault
 * @template {new (reason: string) => Fault} Class
 * @param {Class} Constructor
 */
export function create(Constructor) {
  /** @type {string} */
  // @ts-ignore
  FormattedError.displayName = Constructor.displayName || Constructor.name

  return FormattedError

  /**
   * @param {string} [format]
   * @param {...unknown} values
   * @returns {Fault}
   */
  function FormattedError(format, ...values) {
    /** @type {string} */
    var reason = format ? formatter(format, ...values) : format
    return new Constructor(reason)
  }
}
