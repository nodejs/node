/**
 * @typedef {import('hast').Properties} Properties
 * @typedef {import('hast').Element} Element
 */

var search = /[#.]/g

/**
 * Create a hast element from a simple CSS selector.
 *
 * @param selector A simple CSS selector.
 *   Can contain a tag-name (`foo`), classes (`.bar`), and an ID (`#baz`).
 *   Multiple classes are allowed.
 *   Uses the last ID if multiple IDs are found.
 * @param [defaultTagName='div'] Tag name to use if `selector` does not specify one.
 */
export const parseSelector =
  /**
   * @type {(
   *  <Selector extends string, DefaultTagName extends string = 'div'>(selector?: Selector, defaultTagName?: DefaultTagName) => Element & {tagName: import('./extract.js').ExtractTagName<Selector, DefaultTagName>}
   * )}
   */
  (
    /**
     * @param {string} [selector]
     * @param {string} [defaultTagName='div']
     * @returns {Element}
     */
    function (selector, defaultTagName = 'div') {
      var value = selector || ''
      /** @type {Properties} */
      var props = {}
      var start = 0
      /** @type {string} */
      var subvalue
      /** @type {string} */
      var previous
      /** @type {RegExpMatchArray} */
      var match

      while (start < value.length) {
        search.lastIndex = start
        match = search.exec(value)
        subvalue = value.slice(start, match ? match.index : value.length)

        if (subvalue) {
          if (!previous) {
            defaultTagName = subvalue
          } else if (previous === '#') {
            props.id = subvalue
          } else if (Array.isArray(props.className)) {
            props.className.push(subvalue)
          } else {
            props.className = [subvalue]
          }

          start += subvalue.length
        }

        if (match) {
          previous = match[0]
          start++
        }
      }

      return {
        type: 'element',
        tagName: defaultTagName,
        properties: props,
        children: []
      }
    }
  )
