class AmountRatio {

  constructor () {
    this.json = {};
    this.nkeys = 0;
  }

  fromJSON (json) {
    this.json = json;
    this.nkeys = Object.keys(json).length;
  }

  toJSON () {
    return this.json;
  }

  likely (account, amount, currency) {
    if (this.json[account]) 
      return Math.round(100 * amount * this.json[account]) / 100 + ' ' + currency;
    else if (this.nkeys == 0)
      return amount + ' ' + currency;
    return '';
  }

  compute (ledger, accountIndex) {
    const data = {};
    return new Promise((resolve, reject) => {
      ledger.register()
      .on('data', (entry) => {
        if (accountIndex !== 0 && accountIndex < entry.postings.length) {
          const firstPosting = entry.postings[0];
          const posting = entry.postings[accountIndex];
          const amount = parseFloat(posting.commodity.formatted);
          const firstAmount = parseFloat(firstPosting.commodity.formatted);
          if (firstAmount != 0)
            data[posting.account] = amount / firstAmount;
        }
      })
      .once('end', () => {
        this.fromJSON(data);
        resolve();
      });
    });
  }
};

module.exports = AmountRatio;
