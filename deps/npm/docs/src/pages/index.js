import React from 'react'
import Layout from 'src/components/Layout'
import Features from 'src/components/home/Features'
import SEO from 'src/components/seo'
import Hero from 'src/components/home/Hero'
import DarkBlock from 'src/components/home/DarkBlock'
import Footer from 'src/components/home/footer'
import {ThemeProvider} from 'styled-components'
import {theme} from 'src/theme'

const IndexPage = () => (
  <ThemeProvider theme={theme}>
    <Layout showSidebar={false}>
      <SEO title='npm cli' />
      <Hero />
      <Features />
      <DarkBlock />
      <Footer />
    </Layout>
  </ThemeProvider>
)

export default IndexPage
