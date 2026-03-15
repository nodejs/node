const { log } = require('proc-log')
const npmFetch = require('npm-registry-fetch')
const ciInfo = require('ci-info')
const fetch = require('make-fetch-happen')
const npa = require('npm-package-arg')
const libaccess = require('libnpmaccess')

/**
 * Handles OpenID Connect (OIDC) token retrieval and exchange for CI environments.
 *
 * This function is designed to work in Continuous Integration (CI) environments such as GitHub Actions,
 * GitLab, and CircleCI. It retrieves an OIDC token from the CI environment, exchanges it for an npm token, and
 * sets the token in the provided configuration for authentication with the npm registry.
 *
 * This function is intended to never throw, as it mutates the state of the `opts` and `config` objects on success.
 * OIDC is always an optional feature, and the function should not throw if OIDC is not configured by the registry.
 *
 * @see https://github.com/watson/ci-info for CI environment detection.
 * @see https://docs.github.com/en/actions/deployment/security-hardening-your-deployments/about-security-hardening-with-openid-connect for GitHub Actions OIDC.
 * @see https://circleci.com/docs/openid-connect-tokens/ for CircleCI OIDC.
 */
async function oidc ({ packageName, registry, opts, config }) {
  /*
   * This code should never run when people try to publish locally on their machines.
   * It is designed to execute only in Continuous Integration (CI) environments.
   */

  try {
    if (!(
      /** @see https://github.com/watson/ci-info/blob/v4.2.0/vendors.json#L152 */
      ciInfo.GITHUB_ACTIONS ||
      /** @see https://github.com/watson/ci-info/blob/v4.2.0/vendors.json#L161C13-L161C22 */
      ciInfo.GITLAB ||
      /** @see https://github.com/watson/ci-info/blob/v4.2.0/vendors.json#L78 */
      ciInfo.CIRCLE
    )) {
      return undefined
    }

    /**
     * Check if the environment variable `NPM_ID_TOKEN` is set.
     * In GitLab CI, the ID token is provided via an environment variable,
     * with `NPM_ID_TOKEN` serving as a predefined default. For consistency,
     * all supported CI environments are expected to support this variable.
     * In contrast, GitHub Actions uses a request-based approach to retrieve the ID token.
     * The presence of this token within GitHub Actions will override the request-based approach.
     * This variable follows the prefix/suffix convention from sigstore (e.g., `SIGSTORE_ID_TOKEN`).
     * @see https://docs.sigstore.dev/cosign/signing/overview/
     */
    let idToken = process.env.NPM_ID_TOKEN

    if (!idToken && ciInfo.GITHUB_ACTIONS) {
      /**
       * GitHub Actions provides these environment variables:
       * - `ACTIONS_ID_TOKEN_REQUEST_URL`: The URL to request the ID token.
       * - `ACTIONS_ID_TOKEN_REQUEST_TOKEN`: The token to authenticate the request.
       * Only when a workflow has the following permissions:
       * ```
       * permissions:
       *    id-token: write
       * ```
       * @see https://docs.github.com/en/actions/security-for-github-actions/security-hardening-your-deployments/configuring-openid-connect-in-cloud-providers#adding-permissions-settings
       */
      if (!(
        process.env.ACTIONS_ID_TOKEN_REQUEST_URL &&
        process.env.ACTIONS_ID_TOKEN_REQUEST_TOKEN
      )) {
        log.silly('oidc', 'Skipped because incorrect permissions for id-token within GitHub workflow')
        return undefined
      }

      /**
       * The specification for an audience is `npm:registry.npmjs.org`,
       * where "registry.npmjs.org" can be any supported registry.
       */
      const audience = `npm:${new URL(registry).hostname}`
      const url = new URL(process.env.ACTIONS_ID_TOKEN_REQUEST_URL)
      url.searchParams.append('audience', audience)
      const startTime = Date.now()
      const response = await fetch(url.href, {
        retry: opts.retry,
        headers: {
          Accept: 'application/json',
          Authorization: `Bearer ${process.env.ACTIONS_ID_TOKEN_REQUEST_TOKEN}`,
        },
      })

      const elapsedTime = Date.now() - startTime

      log.http(
        'fetch',
        `GET ${url.href} ${response.status} ${elapsedTime}ms`
      )

      const json = await response.json()

      if (!response.ok) {
        log.verbose('oidc', `Failed to fetch id_token from GitHub: received an invalid response`)
        return undefined
      }

      if (!json.value) {
        log.verbose('oidc', `Failed to fetch id_token from GitHub: missing value`)
        return undefined
      }

      idToken = json.value
    }

    if (!idToken) {
      log.silly('oidc', 'Skipped because no id_token available')
      return undefined
    }

    const parsedRegistry = new URL(registry)
    const regKey = `//${parsedRegistry.host}${parsedRegistry.pathname}`
    const authTokenKey = `${regKey}:_authToken`

    const escapedPackageName = npa(packageName).escapedName
    let response
    try {
      response = await npmFetch.json(new URL(`/-/npm/v1/oidc/token/exchange/package/${escapedPackageName}`, registry), {
        ...opts,
        [authTokenKey]: idToken, // Use the idToken as the auth token for the request
        method: 'POST',
      })
    } catch (error) {
      log.verbose('oidc', `Failed token exchange request with body message: ${error?.body?.message || 'Unknown error'}`)
      return undefined
    }

    if (!response?.token) {
      log.verbose('oidc', 'Failed because token exchange was missing the token in the response body')
      return undefined
    }

    /*
     * The "opts" object is a clone of npm.flatOptions and is passed through the `publish` command,
     * eventually reaching `otplease`. To ensure the token is accessible during the publishing process,
     * it must be directly attached to the `opts` object.
     * Additionally, the token is required by the "live" configuration or getters within `config`.
     */
    opts[authTokenKey] = response.token
    config.set(authTokenKey, response.token, 'user')
    log.verbose('oidc', `Successfully retrieved and set token`)

    try {
      const isDefaultProvenance = config.isDefault('provenance')
      // CircleCI doesn't support provenance yet, so skip the auto-enable logic
      if (isDefaultProvenance && !ciInfo.CIRCLE) {
        const [headerB64, payloadB64] = idToken.split('.')
        if (headerB64 && payloadB64) {
          const payloadJson = Buffer.from(payloadB64, 'base64').toString('utf8')
          const payload = JSON.parse(payloadJson)
          if (
            (ciInfo.GITHUB_ACTIONS && payload.repository_visibility === 'public') ||
            // only set provenance for gitlab if the repo is public and SIGSTORE_ID_TOKEN is available
            (ciInfo.GITLAB && payload.project_visibility === 'public' && process.env.SIGSTORE_ID_TOKEN)
          ) {
            const visibility = await libaccess.getVisibility(packageName, opts)
            if (visibility?.public) {
              log.verbose('oidc', `Enabling provenance`)
              opts.provenance = true
              config.set('provenance', true, 'user')
            }
          }
        }
      }
    } catch (error) {
      log.verbose('oidc', `Failed to set provenance with message: ${error?.message || 'Unknown error'}`)
    }
  } catch (error) {
    log.verbose('oidc', `Failure with message: ${error?.message || 'Unknown error'}`)
  }
  return undefined
}

module.exports = {
  oidc,
}
