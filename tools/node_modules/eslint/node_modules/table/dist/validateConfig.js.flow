// eslint-disable-next-line import/default
import validators from '../dist/validators';

/**
 * @param {string} schemaId
 * @param {formatData~config} config
 * @returns {undefined}
 */
export default (schemaId, config = {}) => {
  const validate = validators[schemaId];
  if (!validate(config)) {
    const errors = validate.errors.map((error) => {
      return {
        dataPath: error.dataPath,
        message: error.message,
        params: error.params,
        schemaPath: error.schemaPath,
      };
    });

    /* eslint-disable no-console */
    console.log('config', config);
    console.log('errors', errors);
    /* eslint-enable no-console */

    throw new Error('Invalid config.');
  }
};
