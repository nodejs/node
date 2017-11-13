'use strict'
module.exports = _ => {
  const start = process.hrtime()
  return _ => {
    const end = process.hrtime(start)
    const ms = Math.round(end[0]*1e6 + end[1]/1e3)/1e3
    if (!process.env.isTTY)
      console.log(ms)
    else {
      const s = Math.round(end[0]*10 + end[1]/1e8)/10
      const ss = s <= 1 ? '' : ' (' + s + 's)'
      console.log('%d%s', ms, ss)
    }
  }
}
