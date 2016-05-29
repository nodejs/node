from markupsafe import escape


def run():
    string = '<strong>Hello World!</strong>' + 'x' * 100000
    escape(string)
