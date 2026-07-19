// UUID validation regex
const UUID_REGEX = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i

const validateUUID = (value, fieldName) => {
  if (!UUID_REGEX.test(value)) {
    throw new Error(`${fieldName} must be a valid UUID`)
  }
}

module.exports = { UUID_REGEX, validateUUID }
