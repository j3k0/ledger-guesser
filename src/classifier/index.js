const getInputOutput = require('./getInputOutput');
const getInput = require('./getInput');
const likely = require('./likely');
const brain = require('brain.js');
const config = require('../config');
const AmountRatio = require('../amount-ratio');

const K_AMOUNT_RATIO = "__amount__";

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
    const output = this.net.run(getInput(payee.toUpperCase()), this.net);
    const account = likely(output);
    if (account === '__empty__')
      return '';
    else
      return account;
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
