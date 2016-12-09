from markupsafe import escape


def run():
    string = 'Hello World!' * 1000
    escape(string)
