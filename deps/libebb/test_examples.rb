require 'test/unit'
require 'socket'

REQ = "GET /hello/%d HTTP/1.1\r\n\r\n"
HOST = '0.0.0.0'
PORT = 5000

class TCPSocket
  def full_send(string)
    written = 0
    while(written < string.length) 
      sent = write(string)
      string = string.slice(sent, 10000)
    end
    written
  end

  def full_read
    response = ""
    while chunk = read(10000)
      response += chunk
    end
    response
  end

end

class EbbTest < Test::Unit::TestCase

  def setup
    @socket = TCPSocket.new(HOST, PORT)
  end

  def test_bad_req
    @socket.full_send("hello")
    assert_equal "", @socket.full_read
    #assert @socket.closed?
  end
  
  def test_single
    written = 0
    req = REQ % 1
    @socket.full_send(req)
    response = @socket.full_read()
    count = 0
    response.scan("hello world") { count += 1 }

    assert_equal 1, count
  end

  def test_pipeline
    written = 0
    req = (REQ % 1) + (REQ % 2) + (REQ % 3) + (REQ % 4)
    @socket.full_send(req)
    response = @socket.full_read()
    count = 0
    response.scan("hello world") { count += 1 }
    assert_equal 4, count
  end
end
