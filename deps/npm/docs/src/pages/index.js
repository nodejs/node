import React from 'react'
import Features from 'src/components/home/Features'
import SEO from 'src/components/seo'
import Hero from 'src/components/home/hero'
import DarkBlock from 'src/components/home/DarkBlock'

const IndexPage = () => (
  <React.Fragment>
    <SEO title='npm cli' />
    <Hero />
    <Features />
    <DarkBlock />
  </React.Fragment>
)

export default IndexPage
