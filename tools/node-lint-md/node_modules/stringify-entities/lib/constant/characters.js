import {characterEntitiesHtml4} from 'character-entities-html4'

/** @type {Object.<string, string>} */
export var characters = {}

var own = {}.hasOwnProperty
/** @type {string} */
var key

for (key in characterEntitiesHtml4) {
  if (own.call(characterEntitiesHtml4, key)) {
    characters[characterEntitiesHtml4[key]] = key
  }
}
