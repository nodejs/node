// This module uses import.defer to import the http builtin module.
// It's dynamically imported by a test module, to ensure the HTTP
// module is actually present in the module list after importing
// this middle module.

// Import the http builtin module and export all its properties.
import defer * as http from 'node:http';
export { http };
