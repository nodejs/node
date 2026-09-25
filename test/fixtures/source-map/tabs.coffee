# Assignment:
number   = 42
opposite = true

# Conditions:
number = -42 if opposite

# Functions:
square = (x) -> x * x

# Arrays:
list = [1, 2, 3, 4, 5]

# Objects:
math =
	root:   Math.sqrt
	square: square
	cube:   (x) -> x * square x

# Splats:
race = (winner, runners...) ->
	print winner, runners

# Existence:
if true
	alert "I knew it!"

# Array comprehensions:
cubes = (math.cube num for num in list)
