/* eslint-disable node/no-unsupported-features/es-syntax */
// We copy the regular expression so as to be able to always
// ensure the exec expression is a global one (and thereby prevent recursion)

/**
 *
 * @param {RegExtras} RegExtras
 * @returns {void}
 */
function addPrototypeMethods (RegExtras) {
  RegExtras.prototype.entries = function * (str) {
    let matches, i = 0;
    const regex = RegExtras.mixinRegex(this.regex, 'g');
    while ((matches = regex.exec(str)) !== null) {
      yield [i++, matches];
    }
  };

  RegExtras.prototype.values = function * (str) {
    let matches;
    const regex = RegExtras.mixinRegex(this.regex, 'g');
    while ((matches = regex.exec(str)) !== null) {
      yield matches;
    }
  };

  RegExtras.prototype.keys = function * (str) {
    let i = 0;
    const regex = RegExtras.mixinRegex(this.regex, 'g');
    while (regex.exec(str) !== null) {
      yield i++;
    }
  };
}

export default addPrototypeMethods;
