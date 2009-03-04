require 'net/http'
require 'tempfile'

$node = File.join(File.dirname(__FILE__), "../node")
$tf = Tempfile.open("node")
$tf.puts(DATA.read)
$tf.close

def assert(x, msg = "")
  raise(msg) unless x
end

def assert_equal(x, y, msg = "")
  assert(x == y, "#{x.inspect} != #{y.inspect} #{msg}")
end

def wait_for_server(host, port)
  loop do
    begin
      socket = ::TCPSocket.open(host, port)
      return
    rescue Errno::ECONNREFUSED
      $stderr.print "." if $DEBUG
      $stderr.flush
      sleep 0.2
    end
  end
end

