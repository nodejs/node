import React from 'react'

import Layout from 'src/components/Layout'
import SEO from 'src/components/seo'

import {ThemeProvider} from 'styled-components'
import {theme} from 'src/theme'

const NotFoundPage = () => (
  <ThemeProvider theme={theme}>
    <Layout>
      <SEO title='404: Not found' />
      <h1>NOT FOUND</h1>
      <p>You just hit a route that doesn&#39;t exist... the sadness.</p>
    </Layout>
  </ThemeProvider>
)

export default NotFoundPage
