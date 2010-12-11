var binding = process.binding('os');

exports.getHostname = binding.getHostname;
