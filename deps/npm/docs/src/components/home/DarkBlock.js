import React from 'react'
import styled from 'styled-components'
import {Flex, Box} from 'rebass'
import {LinkButton} from '../Button'

const Container = styled(Flex)`
  background-color: ${(props) => props.theme.colors.purpleBlack};
  color: ${(props) => props.theme.colors.white};
`

const ContentWrapper = styled(Flex)`
  max-width: 640px;
  align-items: center;
`

const Text = styled.p`
  line-height: 1.5;
  text-align: center;
`

const aStyle = {
  color: '#fb3b49',
  textDecoration: 'none'
}

const DarkBlock = () => {
  return (
    <Container>
      <ContentWrapper px={4} py={6} m='auto' flexDirection='column'>
        <Text>
          <p>The current stable version of npm is <a href={'https://github.com/npm/cli/releases/latest'} style={aStyle}>available on GitHub.</a></p>
          <p>To upgrade, run: <code className={'language-text'} style={{color: 'white'}}>npm install npm@latest -g</code></p>
        </Text>
        <Box pt={4}><LinkButton to='cli-commands/npm' w={'120px'}>read docs</LinkButton></Box>
      </ContentWrapper>
    </Container>
  )
}

export default DarkBlock
