"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.drawContent = void 0;
/**
 * Shared function to draw horizontal borders, rows or the entire table
 */
const drawContent = (contents, separatorConfig) => {
    const { separatorGetter, drawSeparator } = separatorConfig;
    const contentSize = contents.length;
    const result = [];
    if (drawSeparator(0, contentSize)) {
        result.push(separatorGetter(0, contentSize));
    }
    contents.forEach((content, contentIndex) => {
        result.push(content);
        // Only append the middle separator if the content is not the last
        if (contentIndex + 1 < contentSize && drawSeparator(contentIndex + 1, contentSize)) {
            result.push(separatorGetter(contentIndex + 1, contentSize));
        }
    });
    if (drawSeparator(contentSize, contentSize)) {
        result.push(separatorGetter(contentSize, contentSize));
    }
    return result.join('');
};
exports.drawContent = drawContent;
