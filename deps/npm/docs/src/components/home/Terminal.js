import React from 'react'
import styled, {keyframes} from 'styled-components'
import {Flex, Box, Button as RebassButton} from 'rebass'
import closeX from '../../images/x.svg'
import {LinkButton} from '../Button'
import bracket from '../../images/bracket.svg'

const TerminalBody = styled(Flex)`
  background-color: ${(props) => props.theme.colors.purpleBlack};
  border: 2px solid ${(props) => props.theme.colors.purpleBlack};
  color: ${(props) => props.theme.colors.white};
  flex-direction: column;
  max-width: 620px;
  width: 100%;
  height: 100%;
  box-shadow: 0px 0px 17px 1px #dc3bc180;
  border-radius: 2px;
  top: ${(props) => props.top};
  left: ${(props) => props.left};  
  right: 0;
  position: absolute;
`

const Top = styled(Flex)`
  background-color: ${(props) => props.theme.colors.white};
  height: 18px;
`

const SiteName = styled(Flex)`
  font-size: 45px;
  font-family: 'Inconsolata', sans-serif;
  font-weight: 700;
  letter-spacing: 5px;
  text-shadow: 3px 2px 4px #abf1e04d;

  @media screen and (min-width: ${(props) => props.theme.breakpoints.TABLET}) {
    font-size: 70px;
  }
`

const Bottom = styled(Flex)`
  flex-direction: column;
  padding: 30px;

  @media screen and (min-width: ${(props) => props.theme.breakpoints.TABLET}) {
    font-size: 70px;
    padding: 30px 50px;

  }
`

const blink = keyframes`
  0% {
    opacity: 0;
  }
  50% {
    opacity 1;
  }
  100% {
    opacity: 0;
  }
`

const Cursor = styled.span`
  color: ${(props) => props.theme.colors.red};
  text-shadow: none;
  opacity: 1;
  animation: ${blink};
  animation-duration: 3s;
  animation-iteration-count: infinite;
  animation-fill-mode: both;
`

const Bracket = styled.span`
  background: center / contain no-repeat url(${bracket});
  width: 25px;
  margin-right: 5px;
  margin-top: 10px;
`

const Text = styled.span`
  font-size: 15px;
  font-weight: 400;
  letter-spacing: 1px;
  line-height: 1.4;

  @media screen and (min-width: ${(props) => props.theme.breakpoints.TABLET}) {
    font-size: 18px;
  }
`

const ModalButton = styled(RebassButton)`
  cursor: pointer;
  background: center no-repeat  url(${closeX});
  width: 14px;
  height: 14px;
`

const Terminal = ({onClose, top, left}) => {
  return (
    <TerminalBody m={'auto'} top={top} left={left}>
      <Top alignItems='center'>
        <ModalButton onClick={onClose} ml={1} p={1} />
      </Top>
      <Bottom>
        <SiteName py={3}><Bracket />npm cli <Cursor>_</Cursor></SiteName>
        <Text>
          The intelligent package manager for the Node Javascript Platform. Install stuff and get coding!
        </Text>
        <Box mx={'auto'} my={4}>
          <LinkButton to='/cli-commands/npm'>
            read docs
          </LinkButton>
        </Box>
      </Bottom>
    </TerminalBody>
  )
}

export default Terminal
