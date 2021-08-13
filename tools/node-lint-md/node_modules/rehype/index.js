import {unified} from 'unified'
import rehypeParse from 'rehype-parse'
import rehypeStringify from 'rehype-stringify'

export const rehype = unified().use(rehypeParse).use(rehypeStringify).freeze()
