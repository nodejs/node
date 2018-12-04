def main(request, response):
  response.headers.set("Content-Type", "text/plain;charset=" + request.GET.first("label"))
  response.content = "".join(chr(byte) for byte in xrange(255))
