// Delegates to `succ` on sucecss or to `fail` on error
// ex: Thing.load(123, iferr(cb, thing => ...))
const iferr = (fail, succ) => (err, ...a) => err ? fail(err) : succ(...a)

// Like iferr, but also catches errors thrown from `succ` and passes to `fail`
const tiferr = (fail, succ) => iferr(fail, (...a) => {
  try { succ(...a) }
  catch (err) { fail(err) }
})

// Delegate to the success function on success, throws the error otherwise
// ex: Thing.load(123, throwerr(thing => ...))
const throwerr = iferr.bind(null, err => { throw err })

// Prints errors when one is passed, or does nothing otherwise
// ex: Thing.load(123, printerr)
const printerr = iferr(err => console.error(err), () => {})

module.exports = exports = iferr
exports.iferr = iferr
exports.tiferr = tiferr
exports.throwerr = throwerr
exports.printerr = printerr
