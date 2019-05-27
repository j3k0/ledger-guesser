# Ledger Guesser

> Harness the power of machine learning with ledger-cli

_experimental_ but functional.

## Install

```
git clone https://github.com/j3k0/ledger-guesser.git
cd ledger-guesser
npm install
```

## Usage

### Training

```
./ledger-guesser --train JOURNAL_FILE.txt
```

Will take quite some time (depending on the size of your journal). This will
generate files `~/.ledger-guesser-nn-[0-9a-f].json`.

### Guessing

Once the neural network has been trained, you can make it guess the full ledger
entry.

```
./ledger-guesser PAYEE AMOUNT CURRENCY [DATE]
```

**Example**

```
$ ./ledger-guesser 'GOOGLE GSUITE_ USD10.00' -10.00 USD 2019/05/21

2019/05/21 GOOGLE GSUITE_ USD10.00
  Assets:Bank:Blom:Usd   -10.00 USD
  Expenses:Tools:Gsuite   10.00 USD

```

### Lower-level usage

The `./ledger-guesser` script make use of `index.js` to create 16 different
files: 1 for each potential line in the ledger entry.

`node index.js" --train journal_file.txt --index=0` will output training output
in JSON for the first line of the a ledger entry. (index=1 for the second line,
etc).

`node index.js  --guess training.json PAYEE AMOUNT CURRENCY` will output a line
of ledger entry.

**Example**

```
$ node index.js --train journal.txt --index=0 > line-0.json
$ node index.js --train journal.txt --index=1 > line-1.json
$ node index.js --guess line-0.json 'GOOGLE GSUITE_ USD10.00' -10.00 USD
Assets:Bank:Blom:Usd  -10.00 USD
$ node index.js --guess line-1.json 'GOOGLE GSUITE_ USD10.00' -10.00 USD
Expenses:Tools:Gsuite  10.00 USD
```

## With ledger-autosync

For my usage, I use a forked version of `ledger-autosync` that uses
`ledger-guesser` for guessing account names. This allows to import full CSV/OFX
files and create a pretty accurate output.

https://github.com/j3k0/ledger-autosync/tree/fovea

The interesting commit is here: https://github.com/j3k0/ledger-autosync/commit/24de64d2d711199b7e0898325aceeedeb09bb596#diff-235eadfde015c04f77e02a6063e19c2cR199

A proper integration is left as an exercice to the user! ;-)

## Contributing

PRs accepted.

## License

MIT © Jean-Christophe Hoelt
