import React from 'react'
import styled from 'styled-components'
import Windows from './Windows'
import {Flex} from 'rebass'
import {CubeTopLeft, CubeMiddleLeft, CubeBottomLeft, CubeTopRight, CubeBottomRight} from './cubes'

const Container = styled(Flex)`
  background-color: ${(props) => props.theme.colors.offWhite};
  position: relative;
`

const Hero = () => {
  return (
    <Container px={1} pt={[4, 5]} pb={[6, 6, '140px']}>
      <CubeTopLeft />
      <CubeMiddleLeft />
      <CubeBottomLeft />
      <CubeTopRight />
      <CubeBottomRight />
      <Windows />
    </Container>
  )
}

export default Hero
