import React from 'react'
import Layout from './src/components/layout'
require('prismjs/themes/prism-tomorrow.css')
require('./src/main.css')

export const wrapPageElement = ({ element, props }) => {
  return <Layout {...props} >{element}</Layout>
}
