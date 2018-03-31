// const log = () => {};

const Ledger = require('ledger-cli').Ledger;
const LedgerAnalyzer = require('./src/ledger-analyzer');
const Classifier = require('./src/classifier');
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
  const amount = train ? null : process.argv[5];
  return {accountIndex, train, payee, amount, ledgerFile, jsonFile};
};

const argv = readArgv();

function usage() {
  console.log("index.js", "--train", "<ledger-file>", "--index=[1-9]");
  console.log("index.js", "--guess", "<json-file>", "<payee>", "<amount>");
  console.log();
  console.log("Use --guess to guess the account name from the payee.");
  console.log("Use --train to train the neural network for the specified index.");
  console.log();
  console.log("Example usage:");
  console.log("    ", "index.js", "--train", "journal.txt", "--index=1");
  console.log("    ", "... wait until the neural network is fully trained based on your data.");
  console.log();
  process.exit(1);
}

if (argv.train && argv.accountIndex < 0)
  usage();
if (!argv.train && !argv.payee)
  usage();

const classifier = new Classifier();

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
    console.log(JSON.stringify(net.toJSON()));
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
  classifier.fromJSON(require(argv.jsonFile));
  console.log(classifier.likely(argv.payee));
}

// Train the classifier
//transactions.forEach((transaction) => {
//});
