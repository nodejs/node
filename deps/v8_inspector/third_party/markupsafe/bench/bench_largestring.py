from markupsafe import escape


def run():
    string = '<strong>Hello World!</strong>' * 1000
    escape(string)
