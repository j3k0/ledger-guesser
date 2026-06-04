# Ledger Guesser

Payee-to-account classifier for ledger-cli journals. Naive Bayes over
bag-of-words with add-one (Laplace) smoothing and IDF weighting.
Single C file, no dependencies beyond libc and libm.

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

Prints a single account name on stdout. Returns 0 on a confident match, 1 on
abstention (caller should route to a suspense account).

### IDF weighting

Each word's log-probability contribution is weighted by its inverse document
frequency so rare, discriminative tokens (like `CEREBRAS`) outweigh common ones
(like `INC` or `(CARD)`).

The `IDF_MODE` environment variable selects the weighting formula:

| Value      | Formula                 | Behaviour                                          |
|------------|-------------------------|----------------------------------------------------|
| `log`      | `log(V / df(w))`        | Standard IR smoothing. **Default.** Best precision/abstention balance. |
| `raw`      | `V / df(w)`             | Raw inverse frequency. Stronger rare-word boost but overwhelms the confidence margin, dropping precision on held-out data. |
| `none`     | `1`                     | Disable IDF. Reverts to plain Naive Bayes.         |

V = vocabulary size (number of unique training words), df(w) = number of
training transactions containing word w.

The `CONF_PCT` environment variable (default 30) sets the confidence threshold.
A guess is abstained when the top account's posterior probability falls below
this percentage.

## License

MIT © Jean-Christophe Hoelt
