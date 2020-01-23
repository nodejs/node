import React from 'react'
import Terminal from './Terminal'
import styled from 'styled-components'

const Container = styled.div`
  position: relative;
  height: 350px;
  width: 80%;
  margin: auto;
  left: -4%;

  @media screen and (min-width: ${(props) => props.theme.breakpoints.TABLET}) {
    height: 400px;
  }
`

class Windows extends React.Component {
  constructor (props) {
    super(props)
    this.state = {
      showTopTerminal: true,
      showMiddleTerminal: true,
      showBottomTerminal: true,
      counter: 0
    }
    this.onHide = this.onHide.bind(this)
  }

  onHide (terminal) {
    this.setState({ [terminal]: false, counter: this.state.counter + 1 }, () => {
      if (this.state.counter === 3) {
        this.setState({
          showTopTerminal: true,
          showMiddleTerminal: true,
          showBottomTerminal: true,
          counter: 0
        })
      }
    })
  }

  render () {
    return (
      <Container>
        {this.state.showTopTerminal &&
          <Terminal
            onClose={() => this.onHide('showTopTerminal')}
            top={'0%'}
            left={'0%'}
          />
        }

        {this.state.showMiddleTerminal &&
          <Terminal
            onClose={() => this.onHide('showMiddleTerminal')}
            top={'8%'}
            left={'5%'}
          />
        }

        {this.state.showBottomTerminal &&
          <Terminal
            onClose={() => this.onHide('showBottomTerminal')}
            top={'16%'}
            left={'10%'}
          />
        }
      </Container>
    )
  }
}

export default Windows
