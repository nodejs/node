import { SourceResolver } from '../src/source-resolver';
import { expect } from 'chai';
import { describe, it } from 'mocha';

describe('SourceResolver', () => {
  it('should be constructible', () => {
    const a: SourceResolver = new SourceResolver();
    expect(a.sources.length).to.equal(0);
  });
});
