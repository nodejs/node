import React from 'react'
import {graphql} from 'gatsby'
import styled from 'styled-components'
import FoundTypo from 'src/components/FoundTypo'
import Scripts from 'src/components/scripts'
const version = require('../../../package.json').version

const Content = styled.div`
  max-width: 760px;
  margin: auto;
  padding: 0 30px 120px;
`

const Page = ({data}) => {
  const pageData = data.markdownRemark
  const html = pageData.html.replace(/@VERSION@/g, version)
    .replace(/(npm-)+([a-zA-Z\\.-]*)(\((1|5|7)\))<\/h1>/, 'npm $2</h1>')
    .replace(/([a-zA-Z\\.-]*)(\((1|5|7)\))<\/h1>/, '$1</h1>')

  return (
    <Content className='documentation'>
      <div dangerouslySetInnerHTML={{ __html: html }} />
      <FoundTypo />
      <Scripts />
    </Content>
  )
}

export default Page

export const query = graphql`
  query($slug: String!) {
    markdownRemark(fields: { slug: { eq: $slug } }) {
      html
      fields {
        slug
      }
    }
  }
`
