// Create ids for all known accounts.
// The NN will try to guess the correct id.
// Returns an object with:
//  - ids: map "account name => id"
//  - accounts: the inverse map "id => account index"
//  - lastId: the largest id in the map (used for normalizing ids)
//  - normIds: map "account name => normalized id" (floating points between 0 and 1)

const loadAccountIds = (ledger) => {

  return new Promise((resolve, reject) => {
    let lastId = 0;
    const ids = {};
    const accounts = {};
    ledger.accounts()
    .on('data', function(account) {
      if (!ids[account]) {
        ++lastId;
        ids[account] = lastId;
      }
    })
    .once('end', function(){
      const normIds = {};
      for (const x in ids) {
        normIds[x] = ids[x] / lastId;
        accounts[ids[x]] = x;
      }
      resolve({ lastId, ids, normIds, accounts });
    });
  });
};

module.exports = loadAccountIds;
