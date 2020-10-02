const url = require('url')

module.exports = setWarning

function setWarning (reqOrRes, code, message, replace) {
  //   Warning    = "Warning" ":" 1#warning-value
  // warning-value = warn-code SP warn-agent SP warn-text [SP warn-date]
  // warn-code  = 3DIGIT
  // warn-agent = ( host [ ":" port ] ) | pseudonym
  //                 ; the name or pseudonym of the server adding
  //                 ; the Warning header, for use in debugging
  // warn-text  = quoted-string
  // warn-date  = <"> HTTP-date <">
  // (https://tools.ietf.org/html/rfc2616#section-14.46)
  const host = new url.URL(reqOrRes.url).host
  const jsonMessage = JSON.stringify(message)
  const jsonDate = JSON.stringify(new Date().toUTCString())
  const header = replace ? 'set' : 'append'

  reqOrRes.headers[header](
    'Warning',
    `${code} ${host} ${jsonMessage} ${jsonDate}`
  )
}
