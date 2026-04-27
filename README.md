# Ledger Guesser

Payee-to-account classifier for ledger-cli journals. Naive Bayes over
bag-of-words with add-one smoothing. Single C file, no dependencies.

## Build

```
gcc -Os -Wall -o ledger-guesser-c ledger-guesser.c -lm
```

## Usage

### Training

Input is the output of `ledger print` on the journal you want to learn from.
The target class for each transaction is the second posting's account
(matches the index=1 convention of the previous brain.js implementation).

```
ledger -f journal.txt print | ./ledger-guesser-c train /dev/stdin model.tsv
```

### Guessing

```
./ledger-guesser-c guess model.tsv "PAYEE STRING"
```

Prints a single account name on stdout.

## License

MIT © Jean-Christophe Hoelt
