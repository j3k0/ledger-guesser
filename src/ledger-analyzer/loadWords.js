const loadWords = (ledger) => {
  return new Promise((resolve, reject) => {
    const counts = {};
    ledger.register()
    .on('data', (entry) => {
      const ew = entry.payee.split(' ' );
      ew.forEach((w) => {
        counts[w] = (counts[w] | 0) + 1;
      });
    })
    .once('end', () => {
      let lastIndex = 0;
      const indexFromWord = {};
      const wordFromIndex = {};
      const normIndex = {};
      for (const w in counts) {
        if (counts[w] >= 2) {
          ++lastIndex;
          indexFromWord[w] = lastIndex;
          wordFromIndex[lastIndex] = w;
        }
      }
      for (const w in indexFromWord) {
        normIndex[w] = indexFromWord[w] / lastIndex;
      }
      // console.log(wordFromIndex);
      resolve({lastIndex, indexFromWord, wordFromIndex, normIndex});
    });
  });
};

module.exports = loadWords;
