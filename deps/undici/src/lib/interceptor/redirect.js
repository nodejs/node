'use strict'

const RedirectHandler = require('../handler/redirect-handler')
const { kInterceptorKind } = require('../core/symbols.js')

function createRedirectInterceptor ({ maxRedirections: defaultMaxRedirections, throwOnMaxRedirect: defaultThrowOnMaxRedirect, stripHeadersOnRedirect: defaultStripHeadersOnRedirect, stripHeadersOnCrossOriginRedirect: defaultStripHeadersOnCrossOriginRedirect } = {}) {
  const interceptor = (dispatch) => {
    return function Intercept (opts, handler) {
      const { maxRedirections = defaultMaxRedirections, throwOnMaxRedirect = defaultThrowOnMaxRedirect, stripHeadersOnRedirect = defaultStripHeadersOnRedirect, stripHeadersOnCrossOriginRedirect = defaultStripHeadersOnCrossOriginRedirect, ...rest } = opts

      if (maxRedirections == null || maxRedirections === 0) {
        return dispatch(opts, handler)
      }

      const dispatchOpts = { ...rest, throwOnMaxRedirect, stripHeadersOnRedirect, stripHeadersOnCrossOriginRedirect } // Stop sub dispatcher from also redirecting.
      const redirectHandler = new RedirectHandler(dispatch, maxRedirections, dispatchOpts, handler)
      return dispatch(dispatchOpts, redirectHandler)
    }
  }
  interceptor[kInterceptorKind] = 'redirect'
  return interceptor
}

module.exports = createRedirectInterceptor
