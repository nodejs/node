import {characterEntities} from 'character-entities'

var own = {}.hasOwnProperty

/**
 * @param {string} characters
 * @returns {string|false}
 */
export function decodeEntity(characters) {
  return own.call(characterEntities, characters)
    ? characterEntities[characters]
    : false
}
