const definitions = require('./definitions.js')
const describeAll = () => {
  // sort not-deprecated ones to the top
  /* istanbul ignore next - typically already sorted in the definitions file,
   * but this is here so that our help doc will stay consistent if we decide
   * to move them around. */
  const sort = ([keya, {deprecated: depa}], [keyb, {deprecated: depb}]) => {
    return depa && !depb ? 1
      : !depa && depb ? -1
      : keya.localeCompare(keyb, 'en')
  }
  return Object.entries(definitions).sort(sort)
    .map(([key, def]) => def.describe())
    .join(
      '\n\n<!-- automatically generated, do not edit manually -->\n' +
        '<!-- see lib/utils/config/definitions.js -->\n\n'
    )
}
module.exports = describeAll
