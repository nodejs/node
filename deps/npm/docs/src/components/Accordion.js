import React from 'react'
import styled from 'styled-components'
import downCarrot from '../images/down-carrot.svg'
import upCarrot from '../images/up-carrot.svg'

const SectionButton = styled.button`
  outline: none;
  background-color: transparent;
  cursor: pointer;
  color: red;
  border: none;
  font-size: 18px;
  font-weight: bold;
  padding: 5px 0;
  transition: opacity .5s;

  &:after {
    background: center / contain no-repeat url(${(props) => props.isOpen ? upCarrot : downCarrot});
    content: '';
    height: 11px;
    width: 28px;
    display: inline-block;
  }

  &:hover {
    opacity: .6;
  }
`

class Accordion extends React.Component {
  constructor (props) {
    super(props)
    this.state = {
      isOpen: true
    }
    this.onHide = this.onHide.bind(this)
  }

  onHide () {
    this.setState({isOpen: !this.state.isOpen})
  }

  render () {
    return (
      <div>
        <SectionButton isOpen={this.state.isOpen} onClick={this.onHide}>{this.props.section}</SectionButton>
        {this.state.isOpen &&
          <div>
            {this.props.children}
          </div>
        }
      </div>
    )
  }
}

export default Accordion
