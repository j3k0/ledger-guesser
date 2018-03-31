const loadAccountIds = require('./loadAccountIds');
const loadWords = require('./loadWords');

class LedgerAnalyzer {
  constructor (ledger) {
    this.ledger = ledger;
  }

  async load () {
    const accountIds = await loadAccountIds(this.ledger);
    const words = await loadWords(this.ledger);
    return {accountIds, words};
  }
}

module.exports = LedgerAnalyzer;
