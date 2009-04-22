#!/usr/bin/env ruby

def test(description)
  pid = fork do
    exec(File.dirname(__FILE__) + "/echo")
  end

  begin
    sleep 0.5 # give time for the server to start
    yield(pid)
  rescue
    puts "\033[1;31mFAIL\033[m: #{description}"
    raise $!
  ensure
    `kill -9 #{pid}`
  end
  puts "\033[1;32mPASS\033[m: #{description}"
end

test("make sure echo server works") do 
  socket = TCPSocket.open("localhost", 5000)
  w = socket.write("hello");
  raise "error" unless w == 5

  got = socket.recv(5);
  raise "error" unless got == "hello" 

  socket.close
end

test("doing nothing should not timeout the server") do |pid|
  10.times do
    print "."
    STDOUT.flush
    if Process.waitpid(pid, Process::WNOHANG)
      raise "server died when it shouldn't have"
    end
    sleep 1
  end
  puts ""
end

test("connecting and doing nothing to should timeout in 5 seconds") do |pid|
  socket = TCPSocket.open("localhost", 5000)
  i = 0
  10.times do
    print "."
    STDOUT.flush
    break if Process.waitpid(pid, Process::WNOHANG)
    sleep 1
    i+=1
  end
  puts ""
  raise "died too soon (after #{i} seconds)" if i < 5
  raise "died too late (after #{i} seconds)" if i > 6
end


test("connecting and writing once to should timeout in 5 seconds") do |pid|
  socket = TCPSocket.open("localhost", 5000)
  w = socket.write("hello");
  raise "error" unless w == 5

  i = 0
  10.times do
    print "."
    STDOUT.flush
    break if Process.waitpid(pid, Process::WNOHANG)
    sleep 1
    i+=1
  end
  puts ""
  raise "died too soon (after #{i} seconds)" if i < 5
  raise "died too late (after #{i} seconds)" if i > 6
end

test("connecting waiting 3, writing once to should timeout in 8 seconds") do |pid|
  socket = TCPSocket.open("localhost", 5000)

  sleep 3

  w = socket.write("hello");
  raise "error" unless w == 5

  i = 0
  10.times do
    print "."
    STDOUT.flush
    break if Process.waitpid(pid, Process::WNOHANG)
    sleep 1
    i+=1
  end
  puts ""
  raise "died too soon (after #{i} seconds)" if i < 5
  raise "died too late (after #{i} seconds)" if i > 6
end
