// minimal mock of the mocha Test class for formatters

module.exports = Test

function Test (result, parent) {
  this.result = result
  this._slow = 75
  this.duration = result.time
  this.title = result.name
  this.state = result.ok ? 'pass' : 'failed'
  this.pending = result.todo || result.skip || false
  if (result.diag && result.diag.source) {
    var source = result.diag.source
    this.fn = {
      toString: function () {
        return 'function(){' + source + '\n}'
      }
    }
  }

  Object.defineProperty(this, 'parent', {
    value: parent,
    writable: true,
    configurable: true,
    enumerable: false
  })
}

Test.prototype.fullTitle = function () {
  return (this.parent.fullTitle() + ' ' + (this.title || '')).trim()
}

Test.prototype.slow = function (ms){
  return 75
}

Test.prototype.fn = {
  toString: function () {
    return 'function () {\n}'
  }
}
