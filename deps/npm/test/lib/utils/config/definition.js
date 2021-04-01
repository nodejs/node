const t = require('tap')
const Definition = require('../../../../lib/utils/config/definition.js')
const {
  typeDefs: {
    semver: { type: semver },
    Umask: { type: Umask },
    url: { type: url },
    path: { type: path },
  },
} = require('@npmcli/config')

t.test('basic definition', async t => {
  const def = new Definition('key', {
    default: 'some default value',
    type: [Number, String],
    description: 'just a test thingie',
  })
  t.same(def, {
    constructor: Definition,
    key: 'key',
    default: 'some default value',
    defaultDescription: '"some default value"',
    type: [Number, String],
    hint: '<key>',
    usage: '--key <key>',
    typeDescription: 'Number or String',
    description: 'just a test thingie',
    envExport: true,
  })
  t.matchSnapshot(def.describe(), 'human-readable description')

  const deprecated = new Definition('deprecated', {
    deprecated: 'do not use this',
    default: 1234,
    description: '  it should not be used\n  ever\n\n  not even once.\n\n',
    type: Number,
    defaultDescription: 'A number bigger than 1',
    typeDescription: 'An expression of a numeric quantity using numerals',
  })
  t.matchSnapshot(deprecated.describe(), 'description of deprecated thing')

  const nullOrUmask = new Definition('key', {
    default: null,
    type: [null, Umask],
    description: 'asdf',
  })
  t.equal(nullOrUmask.typeDescription, 'null or Octal numeric string in range 0000..0777 (0..511)')
  const nullDateOrBool = new Definition('key', {
    default: 7,
    type: [null, Date, Boolean],
    description: 'asdf',
  })
  t.equal(nullDateOrBool.typeDescription, 'null, Date, or Boolean')
  const manyPaths = new Definition('key', {
    default: ['asdf'],
    type: [path, Array],
    description: 'asdf',
  })
  t.equal(manyPaths.typeDescription, 'Path (can be set multiple times)')
  const pathOrUrl = new Definition('key', {
    default: ['https://example.com'],
    type: [path, url],
    description: 'asdf',
  })
  t.equal(pathOrUrl.typeDescription, 'Path or URL')
  const multi12 = new Definition('key', {
    default: [],
    type: [1, 2, Array],
    description: 'asdf',
  })
  t.equal(multi12.typeDescription, '1 or 2 (can be set multiple times)')
  const multi123 = new Definition('key', {
    default: [],
    type: [1, 2, 3, Array],
    description: 'asdf',
  })
  t.equal(multi123.typeDescription, '1, 2, or 3 (can be set multiple times)')
  const multi123Semver = new Definition('key', {
    default: [],
    type: [1, 2, 3, Array, semver],
    description: 'asdf',
  })
  t.equal(multi123Semver.typeDescription, '1, 2, 3, or SemVer string (can be set multiple times)')
  const hasUsage = new Definition('key', {
    default: 'test default',
    type: String,
    description: 'test description',
    usage: 'test usage',
  })
  t.equal(hasUsage.usage, 'test usage')
  const hasShort = new Definition('key', {
    default: 'test default',
    short: 't',
    type: String,
    description: 'test description',
  })
  t.equal(hasShort.usage, '-t|--key <key>')
  const hardCodedTypes = new Definition('key', {
    default: 'test default',
    type: ['string1', 'string2'],
    description: 'test description',
  })
  t.equal(hardCodedTypes.usage, '--key <string1|string2>')
  const hardCodedOptionalTypes = new Definition('key', {
    default: 'test default',
    type: [null, 'string1', 'string2'],
    description: 'test description',
  })
  t.equal(hardCodedOptionalTypes.usage, '--key <string1|string2>')
  const hasHint = new Definition('key', {
    default: 'test default',
    type: String,
    description: 'test description',
    hint: '<testparam>',
  })
  t.equal(hasHint.usage, '--key <testparam>')
  const optionalBool = new Definition('key', {
    default: null,
    type: [null, Boolean],
    description: 'asdf',
  })
  t.equal(optionalBool.usage, '--key')

  const noExported = new Definition('methane', {
    envExport: false,
    type: String,
    typeDescription: 'Greenhouse Gas',
    default: 'CH4',
    description: `
      This is bad for the environment, for our children, do not put it there.
    `,
  })
  t.equal(noExported.envExport, false, 'envExport flag is false')
  t.equal(noExported.describe(), `#### \`methane\`

* Default: "CH4"
* Type: Greenhouse Gas

This is bad for the environment, for our children, do not put it there.

This value is not exported to the environment for child processes.`)
})

t.test('missing fields', async t => {
  t.throws(() => new Definition('lacks-default', {
    description: 'no default',
    type: String,
  }), { message: 'config lacks default: lacks-default' })
  t.throws(() => new Definition('lacks-type', {
    description: 'no type',
    default: 1234,
  }), { message: 'config lacks type: lacks-type' })
  t.throws(() => new Definition(null, {
    description: 'falsey key',
    default: 1234,
    type: Number,
  }), { message: 'config lacks key: null' })
  t.throws(() => new Definition('extra-field', {
    type: String,
    default: 'extra',
    extra: 'more than is wanted',
    description: 'this is not ok',
  }), { message: 'config defines unknown field extra: extra-field' })
})

t.test('long description', async t => {
  const { stdout: { columns } } = process
  t.teardown(() => process.stdout.columns = columns)

  const long = new Definition('walden', {
    description: `
      WHEN I WROTE the following pages, or rather the bulk of them, I lived
      alone, in the woods, a mile from any neighbor, in a house which I had
      built myself, on the shore of Walden Pond, in Concord, Massachusetts, and
      earned my living by the labor of my hands only. I lived there two years
      and two months. At present I am a sojourner in civilized life again.

      I should not obtrude my affairs so much on the notice of my readers if
      very particular inquiries had not been made by my townsmen concerning my
      mode of life, which some would call impertinent, though they do not
      appear to me at all impertinent, but, considering the circumstances, very
      natural and pertinent.

      \`\`\`
      this.is('a', {
        code: 'sample',
      })

      with (multiple) {
        blocks()
      }
      \`\`\`
    `,
    default: true,
    type: Boolean,
  })
  process.stdout.columns = 40
  t.matchSnapshot(long.describe(), 'cols=40')

  process.stdout.columns = 9000
  t.matchSnapshot(long.describe(), 'cols=9000')

  process.stdout.columns = 0
  t.matchSnapshot(long.describe(), 'cols=0')

  process.stdout.columns = -1
  t.matchSnapshot(long.describe(), 'cols=-1')

  process.stdout.columns = NaN
  t.matchSnapshot(long.describe(), 'cols=NaN')
})
