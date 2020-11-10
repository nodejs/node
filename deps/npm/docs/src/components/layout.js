import React from 'react'
import Navbar from './navbar'
import Sidebar from './Sidebar'
import {Flex, Box} from 'rebass'
import { theme } from 'src/theme'
import { ThemeProvider } from 'styled-components'

const IS_STATIC = process.env.GATSBY_IS_STATIC

const Layout = ({children, path}) => {
  const showSidebar = IS_STATIC || path.match(/cli-commands|configuring-npm|using-npm/)

  return (
    <ThemeProvider theme={theme}>
      <Navbar />
      <Flex w={1}>
        {showSidebar && <Sidebar />}
        <Box width={1}>{children}</Box>
      </Flex>
    </ThemeProvider>
  )
}

export default Layout
