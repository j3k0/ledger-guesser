const log = require('./log');

module.exports = {
  trainingSettings: {
    iterations: 500,        // the maximum times to iterate the training data
    errorThresh: 0.0005,    // the acceptable error percentage from training data
    log: log.debug.bind(log) // true to use console.log, when a function is supplied it is used
  },
  hiddenLayers: [],
};
