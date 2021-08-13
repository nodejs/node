/**
 * @typedef {import('micromark-util-types').Event} Event
 */

import {subtokenize} from 'micromark-util-subtokenize'

/**
 * @param {Event[]} events
 * @returns {Event[]}
 */
export function postprocess(events) {
  while (!subtokenize(events)) {
    // Empty
  }

  return events
}
