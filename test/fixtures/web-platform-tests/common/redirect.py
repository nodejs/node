def main(request, response):
    """Simple handler that causes redirection.

    The request should typically have two query parameters:
    status - The status to use for the redirection. Defaults to 302.
    location - The resource to redirect to.
    """
    status = 302
    if "status" in request.GET:
        try:
            status = int(request.GET.first("status"))
        except ValueError:
            pass

    response.status = status

    location = request.GET.first("location")

    response.headers.set("Location", location)
