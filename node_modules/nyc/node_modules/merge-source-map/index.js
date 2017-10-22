var sourceMap = require('source-map')
var SourceMapConsumer = sourceMap.SourceMapConsumer
var SourceMapGenerator = sourceMap.SourceMapGenerator

module.exports = merge

/**
 * Merge old source map and new source map and return merged.
 * If old or new source map value is falsy, return another one as it is.
 *
 * @param {object|string} [oldMap] old source map object
 * @param {object|string} [newmap] new source map object
 * @return {object|undefined} merged source map object, or undefined when both old and new source map are undefined
 */
function merge(oldMap, newMap) {

  if (!oldMap)
    return newMap
  if (!newMap)
    return oldMap

  var oldMapConsumer = new SourceMapConsumer(oldMap)
  var newMapConsumer = new SourceMapConsumer(newMap)
  var mergedMapGenerator = new SourceMapGenerator()

  // iterate on new map and overwrite original position of new map with one of old map
  newMapConsumer.eachMapping(function(m) {

    // pass when `originalLine` is null.
    // It occurs in case that the node does not have origin in original code.
    if (m.originalLine == null)
      return

    var origPosInOldMap = oldMapConsumer.originalPositionFor({line: m.originalLine, column: m.originalColumn})
    if (origPosInOldMap.source == null)
      return

    mergedMapGenerator.addMapping({
      original: {
        line: origPosInOldMap.line,
        column: origPosInOldMap.column
      },
      generated: {
        line: m.generatedLine,
        column: m.generatedColumn
      },
      source: origPosInOldMap.source,
      name: origPosInOldMap.name
    })
  })

  var mergedMap = JSON.parse(mergedMapGenerator.toString())

  mergedMap.sourcesContent = mergedMap.sources.map(function (source) {
    return oldMapConsumer.sourceContentFor(source)
  })

  mergedMap.sourceRoot = oldMap.sourceRoot

  return mergedMap
}
