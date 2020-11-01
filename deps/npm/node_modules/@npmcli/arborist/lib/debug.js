// certain assertions we should do only when testing arborist itself, because
// they are too expensive or aggressive and would break user programs if we
// miss a situation where they are actually valid.
//
// call like this:
//
// /* istanbul ignore next - debug check */
// debug(() => {
//   if (someExpensiveCheck)
//     throw new Error('expensive check should have returned false')
// })

const debug = process.env.ARBORIST_DEBUG === '1' ||
  /\barborist\b/.test(process.env.NODE_DEBUG || '') ||
  process.env.npm_package_name === '@npmcli/arborist' &&
  ['test', 'snap'].includes(process.env.npm_lifecycle_event)

module.exports = debug ? fn => fn() : () => {}
