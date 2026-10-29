/** takes an error object and serializes it to a plan object */
function serializeError (input) {
  if (!(input instanceof Error)) {
    if (typeof input === 'string') {
      const error = new Error(`attempted to serialize a non-error, string String, "${input}"`)
      return serializeError(error)
    }
    const error = new Error(`attempted to serialize a non-error, ${typeof input} ${input?.constructor?.name}`)
    return serializeError(error)
  }
  // different error objects store status code differently
  // AxiosError uses `status`, other services use `statusCode`
  const statusCode = input.statusCode ?? input.status
  // CAUTION: what we serialize here gets add to the size of logs
  return {
    errorType: input.errorType ?? input.constructor.name,
    ...(input.message ? { message: input.message } : {}),
    ...(input.stack ? { stack: input.stack } : {}),
    // think of this as error code
    ...(input.code ? { code: input.code } : {}),
    // think of this as http status code
    ...(statusCode ? { statusCode } : {}),
  }
}

module.exports = {
  serializeError,
}
