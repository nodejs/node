import { UndiciError } from './errors'

export = MockErrors

declare namespace MockErrors {
  /** The request does not match any registered mock dispatches. */
  export class MockNotMatchedError extends UndiciError {
    constructor(message?: string);
    name: 'MockNotMatchedError';
    code: 'UND_MOCK_ERR_MOCK_NOT_MATCHED';
  }
}
