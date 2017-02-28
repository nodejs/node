'use strict';

const LinkRegEx = /^\[.+?\]\s*:.*$/gm;
const StartsWithAlpha = /^\[[a-z]/i;

function XOR(a, b) {
  return (a && !b) || (!a && b);
}

function sortHelper(a, b) {
  const AStartsWithAlpha = StartsWithAlpha.test(a);
  const BStartsWithAlpha = StartsWithAlpha.test(b);

  if (XOR(AStartsWithAlpha, BStartsWithAlpha)) {
    return AStartsWithAlpha ? -1 : 1;
  } else {
    return a === b ? 0 : (a < b ? -1 : 1);
  }
}

module.exports = function linksOrderLinter(fileName, fileContents, errors) {
  const links = fileContents.match(LinkRegEx) || [];
  const sortedLinks = links.slice().sort(sortHelper);

  if (links.some((link, idx) => sortedLinks[idx] !== link)) {
    errors.push(
      `Links in ${fileName} are not sorted in case-sensitive ascending order`
    );
  }
};
