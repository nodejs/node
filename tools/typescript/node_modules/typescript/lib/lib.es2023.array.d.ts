/*! *****************************************************************************
Copyright (c) Microsoft Corporation. All rights reserved.
Licensed under the Apache License, Version 2.0 (the "License"); you may not use
this file except in compliance with the License. You may obtain a copy of the
License at http://www.apache.org/licenses/LICENSE-2.0

THIS CODE IS PROVIDED ON AN *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
MERCHANTABLITY OR NON-INFRINGEMENT.

See the Apache Version 2.0 License for specific language governing permissions
and limitations under the License.
***************************************************************************** */


/// <reference no-default-lib="true"/>

interface Array<T> {
    /**
     * Returns the value of the last element in the array where predicate is true, and undefined
     * otherwise.
     * @param predicate findLast calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found, findLast
     * immediately returns that element value. Otherwise, findLast returns undefined.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLast<S extends T>(predicate: (value: T, index: number, array: T[]) => value is S, thisArg?: any): S | undefined;
    findLast(predicate: (value: T, index: number, array: T[]) => unknown, thisArg?: any): T | undefined;

    /**
     * Returns the index of the last element in the array where predicate is true, and -1
     * otherwise.
     * @param predicate findLastIndex calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found,
     * findLastIndex immediately returns that element index. Otherwise, findLastIndex returns -1.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLastIndex(predicate: (value: T, index: number, array: T[]) => unknown, thisArg?: any): number;

    /**
     * Returns a copy of an array with its elements reversed.
     */
    toReversed(): T[];

    /**
     * Returns a copy of an array with its elements sorted.
     * @param compareFn Function used to determine the order of the elements. It is expected to return
     * a negative value if the first argument is less than the second argument, zero if they're equal, and a positive
     * value otherwise. If omitted, the elements are sorted in ascending, ASCII character order.
     * ```ts
     * [11, 2, 22, 1].toSorted((a, b) => a - b) // [1, 2, 11, 22]
     * ```
     */
    toSorted(compareFn?: (a: T, b: T) => number): T[];

    /**
     * Copies an array and removes elements and, if necessary, inserts new elements in their place. Returns the copied array.
     * @param start The zero-based location in the array from which to start removing elements.
     * @param deleteCount The number of elements to remove.
     * @param items Elements to insert into the copied array in place of the deleted elements.
     * @returns The copied array.
     */
    toSpliced(start: number, deleteCount: number, ...items: T[]): T[];

    /**
     * Copies an array and removes elements while returning the remaining elements.
     * @param start The zero-based location in the array from which to start removing elements.
     * @param deleteCount The number of elements to remove.
     * @returns A copy of the original array with the remaining elements.
     */
    toSpliced(start: number, deleteCount?: number): T[];

    /**
     * Copies an array, then overwrites the value at the provided index with the
     * given value. If the index is negative, then it replaces from the end
     * of the array.
     * @param index The index of the value to overwrite. If the index is
     * negative, then it replaces from the end of the array.
     * @param value The value to write into the copied array.
     * @returns The copied array with the updated value.
     */
    with(index: number, value: T): T[];
}

interface ReadonlyArray<T> {
    /**
     * Returns the value of the last element in the array where predicate is true, and undefined
     * otherwise.
     * @param predicate findLast calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found, findLast
     * immediately returns that element value. Otherwise, findLast returns undefined.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLast<S extends T>(
        predicate: (value: T, index: number, array: readonly T[]) => value is S,
        thisArg?: any,
    ): S | undefined;
    findLast(
        predicate: (value: T, index: number, array: readonly T[]) => unknown,
        thisArg?: any,
    ): T | undefined;

    /**
     * Returns the index of the last element in the array where predicate is true, and -1
     * otherwise.
     * @param predicate findLastIndex calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found,
     * findLastIndex immediately returns that element index. Otherwise, findLastIndex returns -1.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLastIndex(
        predicate: (value: T, index: number, array: readonly T[]) => unknown,
        thisArg?: any,
    ): number;

    /**
     * Copies the array and returns the copied array with all of its elements reversed.
     */
    toReversed(): T[];

    /**
     * Copies and sorts the array.
     * @param compareFn Function used to determine the order of the elements. It is expected to return
     * a negative value if the first argument is less than the second argument, zero if they're equal, and a positive
     * value otherwise. If omitted, the elements are sorted in ascending, ASCII character order.
     * ```ts
     * [11, 2, 22, 1].toSorted((a, b) => a - b) // [1, 2, 11, 22]
     * ```
     */
    toSorted(compareFn?: (a: T, b: T) => number): T[];

    /**
     * Copies an array and removes elements while, if necessary, inserting new elements in their place, returning the remaining elements.
     * @param start The zero-based location in the array from which to start removing elements.
     * @param deleteCount The number of elements to remove.
     * @param items Elements to insert into the copied array in place of the deleted elements.
     * @returns A copy of the original array with the remaining elements.
     */
    toSpliced(start: number, deleteCount: number, ...items: T[]): T[];

    /**
     * Copies an array and removes elements while returning the remaining elements.
     * @param start The zero-based location in the array from which to start removing elements.
     * @param deleteCount The number of elements to remove.
     * @returns A copy of the original array with the remaining elements.
     */
    toSpliced(start: number, deleteCount?: number): T[];

    /**
     * Copies an array, then overwrites the value at the provided index with the
     * given value. If the index is negative, then it replaces from the end
     * of the array
     * @param index The index of the value to overwrite. If the index is
     * negative, then it replaces from the end of the array.
     * @param value The value to insert into the copied array.
     * @returns A copy of the original array with the inserted value.
     */
    with(index: number, value: T): T[];
}

interface Int8Array {
    /**
     * Returns the value of the last element in the array where predicate is true, and undefined
     * otherwise.
     * @param predicate findLast calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found, findLast
     * immediately returns that element value. Otherwise, findLast returns undefined.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLast<S extends number>(
        predicate: (
            value: number,
            index: number,
            array: Int8Array,
        ) => value is S,
        thisArg?: any,
    ): S | undefined;
    findLast(
        predicate: (value: number, index: number, array: Int8Array) => unknown,
        thisArg?: any,
    ): number | undefined;

    /**
     * Returns the index of the last element in the array where predicate is true, and -1
     * otherwise.
     * @param predicate findLastIndex calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found,
     * findLastIndex immediately returns that element index. Otherwise, findLastIndex returns -1.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLastIndex(
        predicate: (value: number, index: number, array: Int8Array) => unknown,
        thisArg?: any,
    ): number;

    /**
     * Copies the array and returns the copy with the elements in reverse order.
     */
    toReversed(): Int8Array;

    /**
     * Copies and sorts the array.
     * @param compareFn Function used to determine the order of the elements. It is expected to return
     * a negative value if the first argument is less than the second argument, zero if they're equal, and a positive
     * value otherwise. If omitted, the elements are sorted in ascending order.
     * ```ts
     * const myNums = Int8Array.from([11, 2, 22, 1]);
     * myNums.toSorted((a, b) => a - b) // Int8Array(4) [1, 2, 11, 22]
     * ```
     */
    toSorted(compareFn?: (a: number, b: number) => number): Int8Array;

    /**
     * Copies the array and inserts the given number at the provided index.
     * @param index The index of the value to overwrite. If the index is
     * negative, then it replaces from the end of the array.
     * @param value The value to insert into the copied array.
     * @returns A copy of the original array with the inserted value.
     */
    with(index: number, value: number): Int8Array;
}

interface Uint8Array {
    /**
     * Returns the value of the last element in the array where predicate is true, and undefined
     * otherwise.
     * @param predicate findLast calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found, findLast
     * immediately returns that element value. Otherwise, findLast returns undefined.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLast<S extends number>(
        predicate: (
            value: number,
            index: number,
            array: Uint8Array,
        ) => value is S,
        thisArg?: any,
    ): S | undefined;
    findLast(
        predicate: (value: number, index: number, array: Uint8Array) => unknown,
        thisArg?: any,
    ): number | undefined;

    /**
     * Returns the index of the last element in the array where predicate is true, and -1
     * otherwise.
     * @param predicate findLastIndex calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found,
     * findLastIndex immediately returns that element index. Otherwise, findLastIndex returns -1.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLastIndex(
        predicate: (value: number, index: number, array: Uint8Array) => unknown,
        thisArg?: any,
    ): number;

    /**
     * Copies the array and returns the copy with the elements in reverse order.
     */
    toReversed(): Uint8Array;

    /**
     * Copies and sorts the array.
     * @param compareFn Function used to determine the order of the elements. It is expected to return
     * a negative value if the first argument is less than the second argument, zero if they're equal, and a positive
     * value otherwise. If omitted, the elements are sorted in ascending order.
     * ```ts
     * const myNums = Uint8Array.from([11, 2, 22, 1]);
     * myNums.toSorted((a, b) => a - b) // Uint8Array(4) [1, 2, 11, 22]
     * ```
     */
    toSorted(compareFn?: (a: number, b: number) => number): Uint8Array;

    /**
     * Copies the array and inserts the given number at the provided index.
     * @param index The index of the value to overwrite. If the index is
     * negative, then it replaces from the end of the array.
     * @param value The value to insert into the copied array.
     * @returns A copy of the original array with the inserted value.
     */
    with(index: number, value: number): Uint8Array;
}

interface Uint8ClampedArray {
    /**
     * Returns the value of the last element in the array where predicate is true, and undefined
     * otherwise.
     * @param predicate findLast calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found, findLast
     * immediately returns that element value. Otherwise, findLast returns undefined.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLast<S extends number>(
        predicate: (
            value: number,
            index: number,
            array: Uint8ClampedArray,
        ) => value is S,
        thisArg?: any,
    ): S | undefined;
    findLast(
        predicate: (
            value: number,
            index: number,
            array: Uint8ClampedArray,
        ) => unknown,
        thisArg?: any,
    ): number | undefined;

    /**
     * Returns the index of the last element in the array where predicate is true, and -1
     * otherwise.
     * @param predicate findLastIndex calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found,
     * findLastIndex immediately returns that element index. Otherwise, findLastIndex returns -1.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLastIndex(
        predicate: (
            value: number,
            index: number,
            array: Uint8ClampedArray,
        ) => unknown,
        thisArg?: any,
    ): number;

    /**
     * Copies the array and returns the copy with the elements in reverse order.
     */
    toReversed(): Uint8ClampedArray;

    /**
     * Copies and sorts the array.
     * @param compareFn Function used to determine the order of the elements. It is expected to return
     * a negative value if the first argument is less than the second argument, zero if they're equal, and a positive
     * value otherwise. If omitted, the elements are sorted in ascending order.
     * ```ts
     * const myNums = Uint8ClampedArray.from([11, 2, 22, 1]);
     * myNums.toSorted((a, b) => a - b) // Uint8ClampedArray(4) [1, 2, 11, 22]
     * ```
     */
    toSorted(compareFn?: (a: number, b: number) => number): Uint8ClampedArray;

    /**
     * Copies the array and inserts the given number at the provided index.
     * @param index The index of the value to overwrite. If the index is
     * negative, then it replaces from the end of the array.
     * @param value The value to insert into the copied array.
     * @returns A copy of the original array with the inserted value.
     */
    with(index: number, value: number): Uint8ClampedArray;
}

interface Int16Array {
    /**
     * Returns the value of the last element in the array where predicate is true, and undefined
     * otherwise.
     * @param predicate findLast calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found, findLast
     * immediately returns that element value. Otherwise, findLast returns undefined.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLast<S extends number>(
        predicate: (
            value: number,
            index: number,
            array: Int16Array,
        ) => value is S,
        thisArg?: any,
    ): S | undefined;
    findLast(
        predicate: (value: number, index: number, array: Int16Array) => unknown,
        thisArg?: any,
    ): number | undefined;

    /**
     * Returns the index of the last element in the array where predicate is true, and -1
     * otherwise.
     * @param predicate findLastIndex calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found,
     * findLastIndex immediately returns that element index. Otherwise, findLastIndex returns -1.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLastIndex(
        predicate: (value: number, index: number, array: Int16Array) => unknown,
        thisArg?: any,
    ): number;

    /**
     * Copies the array and returns the copy with the elements in reverse order.
     */
    toReversed(): Int16Array;

    /**
     * Copies and sorts the array.
     * @param compareFn Function used to determine the order of the elements. It is expected to return
     * a negative value if the first argument is less than the second argument, zero if they're equal, and a positive
     * value otherwise. If omitted, the elements are sorted in ascending order.
     * ```ts
     * const myNums = Int16Array.from([11, 2, -22, 1]);
     * myNums.toSorted((a, b) => a - b) // Int16Array(4) [-22, 1, 2, 11]
     * ```
     */
    toSorted(compareFn?: (a: number, b: number) => number): Int16Array;

    /**
     * Copies the array and inserts the given number at the provided index.
     * @param index The index of the value to overwrite. If the index is
     * negative, then it replaces from the end of the array.
     * @param value The value to insert into the copied array.
     * @returns A copy of the original array with the inserted value.
     */
    with(index: number, value: number): Int16Array;
}

interface Uint16Array {
    /**
     * Returns the value of the last element in the array where predicate is true, and undefined
     * otherwise.
     * @param predicate findLast calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found, findLast
     * immediately returns that element value. Otherwise, findLast returns undefined.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLast<S extends number>(
        predicate: (
            value: number,
            index: number,
            array: Uint16Array,
        ) => value is S,
        thisArg?: any,
    ): S | undefined;
    findLast(
        predicate: (
            value: number,
            index: number,
            array: Uint16Array,
        ) => unknown,
        thisArg?: any,
    ): number | undefined;

    /**
     * Returns the index of the last element in the array where predicate is true, and -1
     * otherwise.
     * @param predicate findLastIndex calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found,
     * findLastIndex immediately returns that element index. Otherwise, findLastIndex returns -1.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLastIndex(
        predicate: (
            value: number,
            index: number,
            array: Uint16Array,
        ) => unknown,
        thisArg?: any,
    ): number;

    /**
     * Copies the array and returns the copy with the elements in reverse order.
     */
    toReversed(): Uint16Array;

    /**
     * Copies and sorts the array.
     * @param compareFn Function used to determine the order of the elements. It is expected to return
     * a negative value if the first argument is less than the second argument, zero if they're equal, and a positive
     * value otherwise. If omitted, the elements are sorted in ascending order.
     * ```ts
     * const myNums = Uint16Array.from([11, 2, 22, 1]);
     * myNums.toSorted((a, b) => a - b) // Uint16Array(4) [1, 2, 11, 22]
     * ```
     */
    toSorted(compareFn?: (a: number, b: number) => number): Uint16Array;

    /**
     * Copies the array and inserts the given number at the provided index.
     * @param index The index of the value to overwrite. If the index is
     * negative, then it replaces from the end of the array.
     * @param value The value to insert into the copied array.
     * @returns A copy of the original array with the inserted value.
     */
    with(index: number, value: number): Uint16Array;
}

interface Int32Array {
    /**
     * Returns the value of the last element in the array where predicate is true, and undefined
     * otherwise.
     * @param predicate findLast calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found, findLast
     * immediately returns that element value. Otherwise, findLast returns undefined.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLast<S extends number>(
        predicate: (
            value: number,
            index: number,
            array: Int32Array,
        ) => value is S,
        thisArg?: any,
    ): S | undefined;
    findLast(
        predicate: (value: number, index: number, array: Int32Array) => unknown,
        thisArg?: any,
    ): number | undefined;

    /**
     * Returns the index of the last element in the array where predicate is true, and -1
     * otherwise.
     * @param predicate findLastIndex calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found,
     * findLastIndex immediately returns that element index. Otherwise, findLastIndex returns -1.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLastIndex(
        predicate: (value: number, index: number, array: Int32Array) => unknown,
        thisArg?: any,
    ): number;

    /**
     * Copies the array and returns the copy with the elements in reverse order.
     */
    toReversed(): Int32Array;

    /**
     * Copies and sorts the array.
     * @param compareFn Function used to determine the order of the elements. It is expected to return
     * a negative value if the first argument is less than the second argument, zero if they're equal, and a positive
     * value otherwise. If omitted, the elements are sorted in ascending order.
     * ```ts
     * const myNums = Int32Array.from([11, 2, -22, 1]);
     * myNums.toSorted((a, b) => a - b) // Int32Array(4) [-22, 1, 2, 11]
     * ```
     */
    toSorted(compareFn?: (a: number, b: number) => number): Int32Array;

    /**
     * Copies the array and inserts the given number at the provided index.
     * @param index The index of the value to overwrite. If the index is
     * negative, then it replaces from the end of the array.
     * @param value The value to insert into the copied array.
     * @returns A copy of the original array with the inserted value.
     */
    with(index: number, value: number): Int32Array;
}

interface Uint32Array {
    /**
     * Returns the value of the last element in the array where predicate is true, and undefined
     * otherwise.
     * @param predicate findLast calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found, findLast
     * immediately returns that element value. Otherwise, findLast returns undefined.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLast<S extends number>(
        predicate: (
            value: number,
            index: number,
            array: Uint32Array,
        ) => value is S,
        thisArg?: any,
    ): S | undefined;
    findLast(
        predicate: (
            value: number,
            index: number,
            array: Uint32Array,
        ) => unknown,
        thisArg?: any,
    ): number | undefined;

    /**
     * Returns the index of the last element in the array where predicate is true, and -1
     * otherwise.
     * @param predicate findLastIndex calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found,
     * findLastIndex immediately returns that element index. Otherwise, findLastIndex returns -1.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLastIndex(
        predicate: (
            value: number,
            index: number,
            array: Uint32Array,
        ) => unknown,
        thisArg?: any,
    ): number;

    /**
     * Copies the array and returns the copy with the elements in reverse order.
     */
    toReversed(): Uint32Array;

    /**
     * Copies and sorts the array.
     * @param compareFn Function used to determine the order of the elements. It is expected to return
     * a negative value if the first argument is less than the second argument, zero if they're equal, and a positive
     * value otherwise. If omitted, the elements are sorted in ascending order.
     * ```ts
     * const myNums = Uint32Array.from([11, 2, 22, 1]);
     * myNums.toSorted((a, b) => a - b) // Uint32Array(4) [1, 2, 11, 22]
     * ```
     */
    toSorted(compareFn?: (a: number, b: number) => number): Uint32Array;

    /**
     * Copies the array and inserts the given number at the provided index.
     * @param index The index of the value to overwrite. If the index is
     * negative, then it replaces from the end of the array.
     * @param value The value to insert into the copied array.
     * @returns A copy of the original array with the inserted value.
     */
    with(index: number, value: number): Uint32Array;
}

interface Float32Array {
    /**
     * Returns the value of the last element in the array where predicate is true, and undefined
     * otherwise.
     * @param predicate findLast calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found, findLast
     * immediately returns that element value. Otherwise, findLast returns undefined.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLast<S extends number>(
        predicate: (
            value: number,
            index: number,
            array: Float32Array,
        ) => value is S,
        thisArg?: any,
    ): S | undefined;
    findLast(
        predicate: (
            value: number,
            index: number,
            array: Float32Array,
        ) => unknown,
        thisArg?: any,
    ): number | undefined;

    /**
     * Returns the index of the last element in the array where predicate is true, and -1
     * otherwise.
     * @param predicate findLastIndex calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found,
     * findLastIndex immediately returns that element index. Otherwise, findLastIndex returns -1.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLastIndex(
        predicate: (
            value: number,
            index: number,
            array: Float32Array,
        ) => unknown,
        thisArg?: any,
    ): number;

    /**
     * Copies the array and returns the copy with the elements in reverse order.
     */
    toReversed(): Float32Array;

    /**
     * Copies and sorts the array.
     * @param compareFn Function used to determine the order of the elements. It is expected to return
     * a negative value if the first argument is less than the second argument, zero if they're equal, and a positive
     * value otherwise. If omitted, the elements are sorted in ascending order.
     * ```ts
     * const myNums = Float32Array.from([11.25, 2, -22.5, 1]);
     * myNums.toSorted((a, b) => a - b) // Float32Array(4) [-22.5, 1, 2, 11.5]
     * ```
     */
    toSorted(compareFn?: (a: number, b: number) => number): Float32Array;

    /**
     * Copies the array and inserts the given number at the provided index.
     * @param index The index of the value to overwrite. If the index is
     * negative, then it replaces from the end of the array.
     * @param value The value to insert into the copied array.
     * @returns A copy of the original array with the inserted value.
     */
    with(index: number, value: number): Float32Array;
}

interface Float64Array {
    /**
     * Returns the value of the last element in the array where predicate is true, and undefined
     * otherwise.
     * @param predicate findLast calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found, findLast
     * immediately returns that element value. Otherwise, findLast returns undefined.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLast<S extends number>(
        predicate: (
            value: number,
            index: number,
            array: Float64Array,
        ) => value is S,
        thisArg?: any,
    ): S | undefined;
    findLast(
        predicate: (
            value: number,
            index: number,
            array: Float64Array,
        ) => unknown,
        thisArg?: any,
    ): number | undefined;

    /**
     * Returns the index of the last element in the array where predicate is true, and -1
     * otherwise.
     * @param predicate findLastIndex calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found,
     * findLastIndex immediately returns that element index. Otherwise, findLastIndex returns -1.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLastIndex(
        predicate: (
            value: number,
            index: number,
            array: Float64Array,
        ) => unknown,
        thisArg?: any,
    ): number;

    /**
     * Copies the array and returns the copy with the elements in reverse order.
     */
    toReversed(): Float64Array;

    /**
     * Copies and sorts the array.
     * @param compareFn Function used to determine the order of the elements. It is expected to return
     * a negative value if the first argument is less than the second argument, zero if they're equal, and a positive
     * value otherwise. If omitted, the elements are sorted in ascending order.
     * ```ts
     * const myNums = Float64Array.from([11.25, 2, -22.5, 1]);
     * myNums.toSorted((a, b) => a - b) // Float64Array(4) [-22.5, 1, 2, 11.5]
     * ```
     */
    toSorted(compareFn?: (a: number, b: number) => number): Float64Array;

    /**
     * Copies the array and inserts the given number at the provided index.
     * @param index The index of the value to overwrite. If the index is
     * negative, then it replaces from the end of the array.
     * @param value The value to insert into the copied array.
     * @returns A copy of the original array with the inserted value.
     */
    with(index: number, value: number): Float64Array;
}

interface BigInt64Array {
    /**
     * Returns the value of the last element in the array where predicate is true, and undefined
     * otherwise.
     * @param predicate findLast calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found, findLast
     * immediately returns that element value. Otherwise, findLast returns undefined.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLast<S extends bigint>(
        predicate: (
            value: bigint,
            index: number,
            array: BigInt64Array,
        ) => value is S,
        thisArg?: any,
    ): S | undefined;
    findLast(
        predicate: (
            value: bigint,
            index: number,
            array: BigInt64Array,
        ) => unknown,
        thisArg?: any,
    ): bigint | undefined;

    /**
     * Returns the index of the last element in the array where predicate is true, and -1
     * otherwise.
     * @param predicate findLastIndex calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found,
     * findLastIndex immediately returns that element index. Otherwise, findLastIndex returns -1.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLastIndex(
        predicate: (
            value: bigint,
            index: number,
            array: BigInt64Array,
        ) => unknown,
        thisArg?: any,
    ): number;

    /**
     * Copies the array and returns the copy with the elements in reverse order.
     */
    toReversed(): BigInt64Array;

    /**
     * Copies and sorts the array.
     * @param compareFn Function used to determine the order of the elements. It is expected to return
     * a negative value if the first argument is less than the second argument, zero if they're equal, and a positive
     * value otherwise. If omitted, the elements are sorted in ascending order.
     * ```ts
     * const myNums = BigInt64Array.from([11n, 2n, -22n, 1n]);
     * myNums.toSorted((a, b) => Number(a - b)) // BigInt64Array(4) [-22n, 1n, 2n, 11n]
     * ```
     */
    toSorted(compareFn?: (a: bigint, b: bigint) => number): BigInt64Array;

    /**
     * Copies the array and inserts the given bigint at the provided index.
     * @param index The index of the value to overwrite. If the index is
     * negative, then it replaces from the end of the array.
     * @param value The value to insert into the copied array.
     * @returns A copy of the original array with the inserted value.
     */
    with(index: number, value: bigint): BigInt64Array;
}

interface BigUint64Array {
    /**
     * Returns the value of the last element in the array where predicate is true, and undefined
     * otherwise.
     * @param predicate findLast calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found, findLast
     * immediately returns that element value. Otherwise, findLast returns undefined.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLast<S extends bigint>(
        predicate: (
            value: bigint,
            index: number,
            array: BigUint64Array,
        ) => value is S,
        thisArg?: any,
    ): S | undefined;
    findLast(
        predicate: (
            value: bigint,
            index: number,
            array: BigUint64Array,
        ) => unknown,
        thisArg?: any,
    ): bigint | undefined;

    /**
     * Returns the index of the last element in the array where predicate is true, and -1
     * otherwise.
     * @param predicate findLastIndex calls predicate once for each element of the array, in descending
     * order, until it finds one where predicate returns true. If such an element is found,
     * findLastIndex immediately returns that element index. Otherwise, findLastIndex returns -1.
     * @param thisArg If provided, it will be used as the this value for each invocation of
     * predicate. If it is not provided, undefined is used instead.
     */
    findLastIndex(
        predicate: (
            value: bigint,
            index: number,
            array: BigUint64Array,
        ) => unknown,
        thisArg?: any,
    ): number;

    /**
     * Copies the array and returns the copy with the elements in reverse order.
     */
    toReversed(): BigUint64Array;

    /**
     * Copies and sorts the array.
     * @param compareFn Function used to determine the order of the elements. It is expected to return
     * a negative value if the first argument is less than the second argument, zero if they're equal, and a positive
     * value otherwise. If omitted, the elements are sorted in ascending order.
     * ```ts
     * const myNums = BigUint64Array.from([11n, 2n, 22n, 1n]);
     * myNums.toSorted((a, b) => Number(a - b)) // BigUint64Array(4) [1n, 2n, 11n, 22n]
     * ```
     */
    toSorted(compareFn?: (a: bigint, b: bigint) => number): BigUint64Array;

    /**
     * Copies the array and inserts the given bigint at the provided index.
     * @param index The index of the value to overwrite. If the index is
     * negative, then it replaces from the end of the array.
     * @param value The value to insert into the copied array.
     * @returns A copy of the original array with the inserted value.
     */
    with(index: number, value: bigint): BigUint64Array;
}
