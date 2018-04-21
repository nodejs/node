/// <reference types="node" />
import { SmartBuffer } from './smartbuffer';
/**
 * Error strings
 */
declare const ERRORS: {
    INVALID_ENCODING: string;
    INVALID_SMARTBUFFER_SIZE: string;
    INVALID_SMARTBUFFER_BUFFER: string;
    INVALID_SMARTBUFFER_OBJECT: string;
    INVALID_OFFSET: string;
    INVALID_OFFSET_NON_NUMBER: string;
    INVALID_LENGTH: string;
    INVALID_LENGTH_NON_NUMBER: string;
    INVALID_TARGET_OFFSET: string;
    INVALID_TARGET_LENGTH: string;
    INVALID_READ_BEYOND_BOUNDS: string;
    INVALID_WRITE_BEYOND_BOUNDS: string;
};
/**
 * Checks if a given encoding is a valid Buffer encoding. (Throws an exception if check fails)
 *
 * @param { String } encoding The encoding string to check.
 */
declare function checkEncoding(encoding: BufferEncoding): void;
/**
 * Checks if a given number is a finite integer. (Throws an exception if check fails)
 *
 * @param { Number } value The number value to check.
 */
declare function isFiniteInteger(value: number): boolean;
/**
 * Checks if a length value is valid. (Throws an exception if check fails)
 *
 * @param { Number } length The value to check.
 */
declare function checkLengthValue(length: any): void;
/**
 * Checks if a offset value is valid. (Throws an exception if check fails)
 *
 * @param { Number } offset The value to check.
 */
declare function checkOffsetValue(offset: any): void;
/**
 * Checks if a target offset value is out of bounds. (Throws an exception if check fails)
 *
 * @param { Number } offset The offset value to check.
 * @param { SmartBuffer } buff The SmartBuffer instance to check against.
 */
declare function checkTargetOffset(offset: number, buff: SmartBuffer): void;
export { ERRORS, isFiniteInteger, checkEncoding, checkOffsetValue, checkLengthValue, checkTargetOffset };
