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

  likely (payee, amount, currency) {
    const output = this.net.run(getInput(payee.toUpperCase()), this.net);
    let finalAmount = null;
    if (output[K_AMOUNT_RATIO] !== undefined) {
      if (output[K_AMOUNT_RATIO] > 0.5)
        finalAmount = amount;
      else
        finalAmount = -amount;
      delete output[K_AMOUNT_RATIO];
    }
    const account = likely(output);
    if (account === '__empty__')
      return '';
    return account + (finalAmount !== null ? ('  ' + finalAmount + ' ' + currency) : '');
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
