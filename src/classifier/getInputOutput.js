const AmountRatio = require('../amount-ratio');

const getIO = (accountIndex, normIds, normIndex, entry) => {

  const input = {};
  const output = {};//[normIds[entry.postings[0].account], normIds[entry.postings[1].account]];

  const words = entry.payee.split(' ').filter((w) => normIndex[w]);

  // .map((w) => normIndex[w]).slice(0, numWordsInput);
  // while(input.length < numWordsInput) input.push(0);
  for (w in words)
    input[words[w]] = 1;
  if (entry.postings.length <= accountIndex) {
    output['__empty__'] = 1.0;
  }
  else {
    const firstPosting = entry.postings[0];
    const posting = entry.postings[accountIndex];
    output[posting.account] = 1.0;
    const firstAmount = parseFloat(firstPosting.commodity.formatted);
    const amount = parseFloat(posting.commodity.formatted);
    if (amount == firstAmount)
      output['__amount__'] = 1;
    else if (amount == -firstAmount)
      output['__amount__'] = 0;
    // output['amount_coef'] = acc
    // output['currency'] = 
    // output[entry.postings[0].account] = 1.0;
  }
  return {input, output};
}

module.exports = getIO;
