import {unified} from 'unified'
import remarkParse from 'remark-parse'
import remarkStringify from 'remark-stringify'

export const remark = unified().use(remarkParse).use(remarkStringify).freeze()
