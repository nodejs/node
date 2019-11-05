import React from 'react'
import Layout from 'src/components/Layout'
import {graphql} from 'gatsby'
import styled, { ThemeProvider } from 'styled-components'
import {theme} from 'src/theme'
import FoundTypo from 'src/components/FoundTypo'
import Scripts from 'src/components/Scripts'
const version = require('../../../package.json').version

const Content = styled.div`
  max-width: 760px;
  margin: auto;
  padding: 0 30px 120px;
`

const Page = ({data}) => {
  const pageData = data.markdownRemark
  const html = pageData.html.replace(/(npm-)+([a-zA-Z\\.-]*)<\/h1>/g, 'npm $2</h1>')

  return (
    <ThemeProvider theme={theme}>
      <Layout showSidebar>
        <Content className='documentation'>
          <div dangerouslySetInnerHTML={{
            __html: html.replace(/@VERSION@/g, version)
          }} />
          <FoundTypo />
          <Scripts />
        </Content>
      </Layout>
    </ThemeProvider>
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
