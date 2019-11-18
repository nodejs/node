exports.replaceDollarWithPercentPair = replaceDollarWithPercentPair
exports.convertToSetCommand = convertToSetCommand
exports.convertToSetCommands = convertToSetCommands

function convertToSetCommand(key, value) {
    var line = ""
    key = key || ""
    key = key.trim()
    value = value || ""
    value = value.trim()
    if(key && value && value.length > 0) {
        line = "@SET " + key + "=" + replaceDollarWithPercentPair(value) + "\r\n"
    }
    return line
}

function extractVariableValuePairs(declarations) {
    var pairs = {}
    declarations.map(function(declaration) {
        var split = declaration.split("=")
        pairs[split[0]]=split[1]
    })
    return pairs
}

function convertToSetCommands(variableString) {
    var variableValuePairs = extractVariableValuePairs(variableString.split(" "))
    var variableDeclarationsAsBatch = ""
    Object.keys(variableValuePairs).forEach(function (key) {
        variableDeclarationsAsBatch += convertToSetCommand(key, variableValuePairs[key])
    })
    return variableDeclarationsAsBatch
}

function replaceDollarWithPercentPair(value) {
    var dollarExpressions = /\$\{?([^\$@#\?\- \t{}:]+)\}?/g
    var result = ""
    var startIndex = 0
    do {
        var match = dollarExpressions.exec(value)
        if(match) {
            var betweenMatches = value.substring(startIndex, match.index) || ""
            result +=  betweenMatches + "%" + match[1] + "%"
            startIndex = dollarExpressions.lastIndex
        }
    } while (dollarExpressions.lastIndex > 0)
    result += value.substr(startIndex)
    return result
}


