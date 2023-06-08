const { sigstore } = require('sigstore')
const { readFile } = require('fs/promises')

const INTOTO_PAYLOAD_TYPE = 'application/vnd.in-toto+json'
const INTOTO_STATEMENT_TYPE = 'https://in-toto.io/Statement/v0.1'
const SLSA_PREDICATE_TYPE = 'https://slsa.dev/provenance/v0.2'

const BUILDER_ID = 'https://github.com/actions/runner'
const BUILD_TYPE_PREFIX = 'https://github.com/npm/cli/gha'
const BUILD_TYPE_VERSION = 'v2'

const generateProvenance = async (subject, opts) => {
  const { env } = process
  /* istanbul ignore next - not covering missing env var case */
  const [workflowPath] = (env.GITHUB_WORKFLOW_REF || '')
    .replace(env.GITHUB_REPOSITORY + '/', '')
    .split('@')
  const payload = {
    _type: INTOTO_STATEMENT_TYPE,
    subject,
    predicateType: SLSA_PREDICATE_TYPE,
    predicate: {
      buildType: `${BUILD_TYPE_PREFIX}/${BUILD_TYPE_VERSION}`,
      builder: { id: BUILDER_ID },
      invocation: {
        configSource: {
          uri: `git+${env.GITHUB_SERVER_URL}/${env.GITHUB_REPOSITORY}@${env.GITHUB_REF}`,
          digest: {
            sha1: env.GITHUB_SHA,
          },
          entryPoint: workflowPath,
        },
        parameters: {},
        environment: {
          GITHUB_EVENT_NAME: env.GITHUB_EVENT_NAME,
          GITHUB_REF: env.GITHUB_REF,
          GITHUB_REPOSITORY: env.GITHUB_REPOSITORY,
          GITHUB_REPOSITORY_ID: env.GITHUB_REPOSITORY_ID,
          GITHUB_REPOSITORY_OWNER_ID: env.GITHUB_REPOSITORY_OWNER_ID,
          GITHUB_RUN_ATTEMPT: env.GITHUB_RUN_ATTEMPT,
          GITHUB_RUN_ID: env.GITHUB_RUN_ID,
          GITHUB_SHA: env.GITHUB_SHA,
          GITHUB_WORKFLOW_REF: env.GITHUB_WORKFLOW_REF,
          GITHUB_WORKFLOW_SHA: env.GITHUB_WORKFLOW_SHA,
        },
      },
      metadata: {
        buildInvocationId: `${env.GITHUB_RUN_ID}-${env.GITHUB_RUN_ATTEMPT}`,
        completeness: {
          parameters: false,
          environment: false,
          materials: false,
        },
        reproducible: false,
      },
      materials: [
        {
          uri: `git+${env.GITHUB_SERVER_URL}/${env.GITHUB_REPOSITORY}@${env.GITHUB_REF}`,
          digest: {
            sha1: env.GITHUB_SHA,
          },
        },
      ],
    },
  }

  return sigstore.attest(Buffer.from(JSON.stringify(payload)), INTOTO_PAYLOAD_TYPE, opts)
}

const verifyProvenance = async (subject, provenancePath) => {
  let provenanceBundle
  try {
    provenanceBundle = JSON.parse(await readFile(provenancePath))
  } catch (err) {
    err.message = `Invalid provenance provided: ${err.message}`
    throw err
  }

  const payload = extractProvenance(provenanceBundle)
  if (!payload.subject || !payload.subject.length) {
    throw new Error('No subject found in sigstore bundle payload')
  }
  if (payload.subject.length > 1) {
    throw new Error('Found more than one subject in the sigstore bundle payload')
  }

  const bundleSubject = payload.subject[0]
  if (subject.name !== bundleSubject.name) {
    throw new Error(
      `Provenance subject ${bundleSubject.name} does not match the package: ${subject.name}`
    )
  }
  if (subject.digest.sha512 !== bundleSubject.digest.sha512) {
    throw new Error('Provenance subject digest does not match the package')
  }

  await sigstore.verify(provenanceBundle)
  return provenanceBundle
}

const extractProvenance = (bundle) => {
  if (!bundle?.dsseEnvelope?.payload) {
    throw new Error('No dsseEnvelope with payload found in sigstore bundle')
  }
  try {
    return JSON.parse(Buffer.from(bundle.dsseEnvelope.payload, 'base64').toString('utf8'))
  } catch (err) {
    err.message = `Failed to parse payload from dsseEnvelope: ${err.message}`
    throw err
  }
}

module.exports = {
  generateProvenance,
  verifyProvenance,
}
