const toCamelCase = (str) => {
  return str.toLowerCase().replace(/^[a-z]/gu, (init) => {
    return init.toUpperCase();
  }).replace(/_(?<wordInit>[a-z])/gu, (_, n1, o, s, {wordInit}) => {
    return wordInit.toUpperCase();
  });
};

export default toCamelCase;
