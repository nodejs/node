/**
 * Throw a given error.
 *
 * @param {Error | null | undefined} [error]
 */
export function bail(error) {
  if (error) {
    throw error
  }
}
