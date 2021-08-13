import {constants} from 'micromark-util-symbol/constants.js'

/**
 * Like `Array#splice`, but smarter for giant arrays.
 *
 * `Array#splice` takes all items to be inserted as individual argument which
 * causes a stack overflow in V8 when trying to insert 100k items for instance.
 *
 * Otherwise, this does not return the removed items, and takes `items` as an
 * array instead of rest parameters.
 *
 * @template {unknown} T
 * @param {T[]} list
 * @param {number} start
 * @param {number} remove
 * @param {T[]} items
 * @returns {void}
 */
export function splice(list, start, remove, items) {
  const end = list.length
  let chunkStart = 0
  /** @type {unknown[]} */
  let parameters

  // Make start between zero and `end` (included).
  if (start < 0) {
    start = -start > end ? 0 : end + start
  } else {
    start = start > end ? end : start
  }

  remove = remove > 0 ? remove : 0

  // No need to chunk the items if there’s only a couple (10k) items.
  if (items.length < constants.v8MaxSafeChunkSize) {
    parameters = Array.from(items)
    parameters.unshift(start, remove)
    // @ts-expect-error Hush, it’s fine.
    ;[].splice.apply(list, parameters)
  } else {
    // Delete `remove` items starting from `start`
    if (remove) [].splice.apply(list, [start, remove])

    // Insert the items in chunks to not cause stack overflows.
    while (chunkStart < items.length) {
      parameters = items.slice(
        chunkStart,
        chunkStart + constants.v8MaxSafeChunkSize
      )
      parameters.unshift(start, 0)
      // @ts-expect-error Hush, it’s fine.
      ;[].splice.apply(list, parameters)

      chunkStart += constants.v8MaxSafeChunkSize
      start += constants.v8MaxSafeChunkSize
    }
  }
}

/**
 * Append `items` (an array) at the end of `list` (another array).
 * When `list` was empty, returns `items` instead.
 *
 * This prevents a potentially expensive operation when `list` is empty,
 * and adds items in batches to prevent V8 from hanging.
 *
 * @template {unknown} T
 * @param {T[]} list
 * @param {T[]} items
 * @returns {T[]}
 */
export function push(list, items) {
  if (list.length > 0) {
    splice(list, list.length, 0, items)
    return list
  }

  return items
}
