var Gauge = require('./')
var gaugeDefault = require('./themes.js')
var onExit = require('signal-exit')

var activeGauge

onExit(function () {
  activeGauge.disable()
})

var themes = gaugeDefault.getThemeNames()

nextBar()
function nextBar () {
  var themeName = themes.shift()

  console.log('Demoing output for ' + themeName)

  var gt = new Gauge(process.stderr, {
    updateInterval: 50,
    theme: themeName,
    cleanupOnExit: false,
  })
  activeGauge = gt

  var progress = 0

  var cnt = 0
  var pulse = setInterval(function () {
    gt.pulse('this is a thing that happened ' + (++cnt))
  }, 110)
  var prog = setInterval(function () {
    progress += 0.04
    gt.show(themeName + ':' + Math.round(progress * 1000), progress)
    if (progress >= 1) {
      clearInterval(prog)
      clearInterval(pulse)
      gt.disable()
      if (themes.length) {
        nextBar()
      }
    }
  }, 100)
  gt.show()
}
