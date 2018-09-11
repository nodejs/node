def main(request, response):
    """Code for generating large responses where the actual response data
    isn't very important.

    Two request parameters:
    size (required): An integer number of bytes (no suffix) or kilobytes
                     ("kb" suffix) or megabytes ("Mb" suffix).
    string (optional): The string to repeat in the response. Defaults to "a".

    Example:
        /resources/large.py?size=32Mb&string=ab
    """
    if not "size" in request.GET:
        400, "Need an integer bytes parameter"

    bytes_value = request.GET.first("size")

    chunk_size = 1024

    multipliers = {"kb": 1024,
                   "Mb": 1024*1024}

    suffix = bytes_value[-2:]
    if suffix in multipliers:
        multiplier = multipliers[suffix]
        bytes_value = bytes_value[:-2] * multiplier

    try:
        num_bytes = int(bytes_value)
    except ValueError:
        return 400, "Bytes must be an integer possibly with a kb or Mb suffix"

    string = str(request.GET.first("string", "a"))

    chunk = string * chunk_size

    def content():
        bytes_sent = 0
        while bytes_sent < num_bytes:
            if num_bytes - bytes_sent < len(chunk):
                yield chunk[num_bytes - bytes_sent]
            else:
                yield chunk
            bytes_sent += len(chunk)
    return [("Content-Type", "text/plain")], content()
