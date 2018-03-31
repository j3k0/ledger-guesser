const getInputOutput = require('./getInputOutput');
const getInput = require('./getInput');
const brain = require('brain.js');
const config = require('../config');

class Classifier {

  constructor () {
    this.net = new brain.NeuralNetwork({
      hiddenLayers: config.hiddenLayers
    });
  }

  fromJSON (json) {
    this.net.fromJSON(json);
  }

  likely (payee) {
    return brain.likely(getInput(payee.toUpperCase()), this.net);
  }

  train (ledger, accountIndex, {normIds}, {normIndex}) {
    return new Promise((resolve, reject) => {
      const trainingData = [];
      ledger.register()
      .on('data', function(entry) {
        // console.log(getIO(normIds, normIndex, entry));
        trainingData.push(getInputOutput(accountIndex, normIds, normIndex, entry));
        // console.log(entry.postings);
      })
      .once('end', () => {
        // log(trainingData);
        this.net.train(trainingData, config.trainingSettings);
        resolve(this.net);
      });
    });
  }
}

module.exports = Classifier;
