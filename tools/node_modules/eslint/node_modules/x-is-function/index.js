module.exports = function isFunction (fn) {
  return Object.prototype.toString.call(fn) === '[object Function]'
}
