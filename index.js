// const log = () => {};

const Ledger = require('ledger-cli').Ledger;
const LedgerAnalyzer = require('./src/ledger-analyzer');
const Classifier = require('./src/classifier');
// const AmountRatio = require('./src/amount-ratio');
const log = require('./src/log').debug;

// Read process arguments
const readArgv = () => {

  let accountIndex = -1;
  // first argument: --train or --guess
  const train = (process.argv[2] === '--train');
  // second argument: ledgerFile or jsonFile
  const ledgerFile = train ? process.argv[3] : null;
  const jsonFile = train ? null : process.argv[3];
  // thirds argument: accountIndex
  if (train && process.argv[4].slice(0,8) === '--index=')
    accountIndex = (+process.argv[4].slice(8));
  // fourth argument: payee
  const payee = train ? null : process.argv[4];
  // const amount = train ? null : process.argv[5];
  // const currency = train ? null : process.argv[6];
  return {accountIndex, train, payee/*, amount, currency*/, ledgerFile, jsonFile};
};

const argv = readArgv();

function usage() {
  console.error("index.js", "--train", "<ledger-file>", "--index=[1-9]");
  console.error("index.js", "--guess", "<json-file>", "<payee>", "<amount>", "<currency>");
  console.error();
  console.error("Use --guess to guess the account name from the payee.");
  console.error("Use --train to train the neural network for the specified index.");
  console.error();
  console.error("Example usage:");
  console.error("    ", "index.js", "--train", "journal.txt", "--index=1");
  console.error("    ", "... wait until the neural network is fully trained based on your data.");
  console.error();
  process.exit(1);
}

if (argv.train && argv.accountIndex < 0)
  usage();
if (!argv.train && !argv.payee)
  usage();

const classifier = new Classifier();
// const amountRatio = new AmountRatio();

const train = async () => {

  var ledger = new Ledger({ file: argv.ledgerFile });
  const analyzer = new LedgerAnalyzer(ledger);

  try {
    const {accountIds, words} = await analyzer.load();
    // const findAccount = (({ lastId, accounts}, id) => {
    //   const idIndex = Math.round(id * lastId);
    //   return accounts[idIndex] || '';
    // });
    // console.log(findAccount(accountIds, accountIds.normIds['Assets:Bank:Lbp:Ccp']));
    // console.log(findAccount(accountIds, 0.5));
    // console.log(findAccount(accountIds, 1.0));
    const net = await classifier.train(ledger, argv.accountIndex, accountIds, words);
    // await amountRatio.compute(ledger, argv.accountIndex);
    console.log(JSON.stringify({
      net: net.toJSON(),
      // amountRatio: amountRatio.toJSON(),
    }));
    // const test = 'CDN OVH ASDADADSASDADA';
    // console.log(net.run(getInput(test)));
    // console.log(brain.likely(getInput(test), net));
  }
  catch (e) {
    console.error(e);
    process.exit(1);
  }
}

if (argv.train) {
  train();
}
else {
  const json = require(argv.jsonFile);
  classifier.fromJSON(json.net);
  // amountRatio.fromJSON(json.amountRatio);
  const account = classifier.likely(argv.payee);
  console.log(account);// + '  ' + amountRatio.likely(account, argv.amount, argv.currency));
}
