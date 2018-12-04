def main(request, response):
  response.headers.set("Content-Type", "text/plain;charset=" + request.GET.first("label"))
  response.content = "hello encoding"
