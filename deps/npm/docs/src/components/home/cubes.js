import styled, {css, keyframes} from 'styled-components'
import purpleCube from '../../images/purple-cube.svg'
import orangeCube from '../../images/orange-cube.svg'
import redCube from '../../images/red-cube.svg'
import purpleGradientCube from '../../images/purple-gradient-cube.svg'
import pinkGradientCube from '../../images/pink-gradient-cube.svg'

const commonCubeStyles = css`
  background-position: center;
  background-repeat: no-repeat;
  position: absolute;
`

const wiggle = keyframes`
  0% {
    transform: rotate(0deg);
  }
  33% {
    transform: rotate(8deg);
  }
  100% {
    transform: rotate(0deg);
  }
`

export const CubeTopLeft = styled.div`
  ${commonCubeStyles};
  background-image: url(${purpleCube});
  height: 35px;
  width: 35px;
  top: 10%;
  left: 8%;

  animation-name: ${wiggle};
  animation-duration: 2.5s;
  animation-delay: .5s;
  animation-iteration-count: infinite;
  animation-fill-mode: both;
  animation-timing-function: ease-in-out;
`

export const CubeMiddleLeft = styled.span`
  ${commonCubeStyles};
  background-image: url(${orangeCube});
  height: 30px;
  width: 30px;
  top: 40%;
  left: 17%;

  animation-name: ${wiggle};
  animation-duration: 2.5s;
  animation-iteration-count: infinite;
  animation-fill-mode: both;
  animation-timing-function: ease-in-out;
`

export const CubeBottomLeft = styled.span`
  ${commonCubeStyles};
  background-image: url(${redCube});
  height: 45px;
  width: 45px;
  top: 78%;
  left: 12%;

  animation-name: ${wiggle};
  animation-duration: 3s;
  animation-iteration-count: infinite;
  animation-fill-mode: both;
  animation-timing-function: ease-in-out;
`

export const CubeBottomRight = styled.span`
  ${commonCubeStyles};
  background-image: url(${pinkGradientCube});
  height: 40px;
  width: 40px;
  top: 70%;
  right: 12%;

  animation-name: ${wiggle};
  animation-duration: 2.5s;
  animation-iteration-count: infinite;
  animation-delay: .3s;
  animation-fill-mode: both;
  animation-timing-function: ease-in-out;
`

export const CubeTopRight = styled.span`
  ${commonCubeStyles};
  background-image: url(${purpleGradientCube});
  height: 40px;
  width: 40px;
  top: 14%;
  right: 12%;

  animation-name: ${wiggle};
  animation-duration: 3s;
  animation-iteration-count: infinite;
  animation-fill-mode: backwards;
  animation-timing-function: ease-in-out;
`
