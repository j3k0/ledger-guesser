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
    const posting = entry.postings[accountIndex];
    output[posting.account] = 1.0;
  }
  return {input, output};
}

module.exports = getIO;
