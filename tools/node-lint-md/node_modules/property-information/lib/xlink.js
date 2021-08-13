import {create} from './util/create.js'

export var xlink = create({
  space: 'xlink',
  transform: xlinkTransform,
  properties: {
    xLinkActuate: null,
    xLinkArcRole: null,
    xLinkHref: null,
    xLinkRole: null,
    xLinkShow: null,
    xLinkTitle: null,
    xLinkType: null
  }
})

/**
 * @param {unknown} _
 * @param {string} prop
 * @returns {string}
 */
function xlinkTransform(_, prop) {
  return 'xlink:' + prop.slice(5).toLowerCase()
}
