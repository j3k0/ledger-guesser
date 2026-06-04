// gcc -Os -Wall -o ledger-guesser-c ledger-guesser.c -lm
//
// Payee-to-account classifier for ledger-cli journals.
// Naive Bayes over bag-of-words with add-one smoothing and IDF weighting.
//
// IDF mode (env IDF_MODE, default "log"):
//   log  — idf(w) = log(V / df(w))  (standard IR smoothing; default)
//   raw  — idf(w) = V / df(w), where V = vocabulary size
//   none — idf(w) = 1  (disable weighting, revert to plain NB)
//
// Usage:
//   ledger-guesser-c train <journal.txt> <model.tsv>
//   ledger-guesser-c guess <model.tsv> "<payee>"
//
// Journal format: output of `ledger print "^ACCOUNT"`. The target class for
// each transaction is the second posting's account (matches the existing
// index=1 convention of the old brain.js-based ledger-guesser).

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ACCOUNTS_PER_TX 32
#define MAX_TOKENS 64
#define MAX_LINE 4096

// -----------------------------------------------------------------------------
// Hashing
// -----------------------------------------------------------------------------

static uint64_t fnv1a(const char *s) {
  uint64_t h = 1469598103934665603ULL;
  while (*s) {
    h ^= (unsigned char)*s++;
    h *= 1099511628211ULL;
  }
  return h;
}

static uint64_t mix64(uint64_t x) {
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  return x;
}

static void die(const char *msg) {
  fprintf(stderr, "ledger-guesser: %s\n", msg);
  exit(1);
}

static void *xcalloc(size_t n, size_t sz) {
  void *p = calloc(n, sz);
  if (!p) die("out of memory");
  return p;
}

static void *xrealloc(void *p, size_t sz) {
  void *q = realloc(p, sz);
  if (!q) die("out of memory");
  return q;
}

static char *xstrdup(const char *s) {
  char *p = strdup(s);
  if (!p) die("out of memory");
  return p;
}

// -----------------------------------------------------------------------------
// String table (intern strings -> 1-based integer id)
// -----------------------------------------------------------------------------

typedef struct {
  char **items;      // items[id - 1]
  int n_items;
  int cap;
  int *ht;           // open-addressing; 0 = empty slot, else stores id
  size_t ht_size;
  size_t ht_mask;
} StrTbl;

static void strtbl_init(StrTbl *t) {
  t->items = NULL;
  t->n_items = 0;
  t->cap = 0;
  t->ht_size = 1024;
  t->ht_mask = t->ht_size - 1;
  t->ht = xcalloc(t->ht_size, sizeof(int));
}

static void strtbl_grow_ht(StrTbl *t) {
  size_t new_size = t->ht_size * 2;
  size_t new_mask = new_size - 1;
  int *new_ht = xcalloc(new_size, sizeof(int));
  for (size_t i = 0; i < t->ht_size; ++i) {
    int id = t->ht[i];
    if (id > 0) {
      uint64_t h = fnv1a(t->items[id - 1]);
      size_t pos = h & new_mask;
      while (new_ht[pos]) pos = (pos + 1) & new_mask;
      new_ht[pos] = id;
    }
  }
  free(t->ht);
  t->ht = new_ht;
  t->ht_size = new_size;
  t->ht_mask = new_mask;
}

// Intern s; return 1-based id. Strings are copied.
static int strtbl_intern(StrTbl *t, const char *s) {
  if ((size_t)(t->n_items + 1) * 2 > t->ht_size) strtbl_grow_ht(t);
  uint64_t h = fnv1a(s);
  size_t pos = h & t->ht_mask;
  while (t->ht[pos]) {
    if (strcmp(t->items[t->ht[pos] - 1], s) == 0) return t->ht[pos];
    pos = (pos + 1) & t->ht_mask;
  }
  if (t->n_items >= t->cap) {
    t->cap = t->cap ? t->cap * 2 : 128;
    t->items = xrealloc(t->items, (size_t)t->cap * sizeof(char *));
  }
  t->items[t->n_items++] = xstrdup(s);
  t->ht[pos] = t->n_items;
  return t->n_items;
}

// Lookup without inserting. Returns 0 if not found.
static int strtbl_find(const StrTbl *t, const char *s) {
  if (!t->ht_size) return 0;
  uint64_t h = fnv1a(s);
  size_t pos = h & t->ht_mask;
  while (t->ht[pos]) {
    if (strcmp(t->items[t->ht[pos] - 1], s) == 0) return t->ht[pos];
    pos = (pos + 1) & t->ht_mask;
  }
  return 0;
}

// -----------------------------------------------------------------------------
// (word_id, account_id) -> count hashmap
// -----------------------------------------------------------------------------

typedef struct {
  uint64_t key;   // 0 = empty; key = (word_id << 32) | account_id; ids are 1-based
  int count;
} WACEntry;

typedef struct {
  WACEntry *e;
  size_t size;
  size_t mask;
  size_t n;
} WACMap;

static void wac_init(WACMap *m) {
  m->size = 1024;
  m->mask = m->size - 1;
  m->n = 0;
  m->e = xcalloc(m->size, sizeof(WACEntry));
}

static void wac_grow(WACMap *m) {
  size_t old_size = m->size;
  WACEntry *old = m->e;
  m->size *= 2;
  m->mask = m->size - 1;
  m->e = xcalloc(m->size, sizeof(WACEntry));
  for (size_t i = 0; i < old_size; ++i) {
    if (old[i].key) {
      size_t pos = mix64(old[i].key) & m->mask;
      while (m->e[pos].key) pos = (pos + 1) & m->mask;
      m->e[pos] = old[i];
    }
  }
  free(old);
}

static inline uint64_t wac_key(int word_id, int account_id) {
  return ((uint64_t)(uint32_t)word_id << 32) | (uint32_t)account_id;
}

static void wac_inc(WACMap *m, int word_id, int account_id, int delta) {
  if (m->n * 2 >= m->size) wac_grow(m);
  uint64_t k = wac_key(word_id, account_id);
  size_t pos = mix64(k) & m->mask;
  while (m->e[pos].key && m->e[pos].key != k) pos = (pos + 1) & m->mask;
  if (!m->e[pos].key) {
    m->e[pos].key = k;
    m->n++;
  }
  m->e[pos].count += delta;
}

static int wac_get(const WACMap *m, int word_id, int account_id) {
  uint64_t k = wac_key(word_id, account_id);
  size_t pos = mix64(k) & m->mask;
  while (m->e[pos].key) {
    if (m->e[pos].key == k) return m->e[pos].count;
    pos = (pos + 1) & m->mask;
  }
  return 0;
}

// -----------------------------------------------------------------------------
// Model
// -----------------------------------------------------------------------------

typedef struct {
  StrTbl words;
  StrTbl accounts;
  int *acc_total;    // per-account token occurrences (sum of word-counts); 1-based
  int acc_total_cap;
  int *acc_tx;       // per-account transaction count (for priors); 1-based
  int acc_tx_cap;
  int *word_df;      // per-word document frequency (# tx containing the word); 1-based
  int word_df_cap;
  int n_tx;
  WACMap wac;
} Model;

static void model_init(Model *m) {
  strtbl_init(&m->words);
  strtbl_init(&m->accounts);
  m->acc_total = NULL;
  m->acc_total_cap = 0;
  m->acc_tx = NULL;
  m->acc_tx_cap = 0;
  m->word_df = NULL;
  m->word_df_cap = 0;
  m->n_tx = 0;
  wac_init(&m->wac);
}

static void ensure_int_array(int **arr, int *cap, int needed) {
  if (needed <= *cap) return;
  int new_cap = *cap ? *cap : 128;
  while (new_cap < needed) new_cap *= 2;
  *arr = xrealloc(*arr, (size_t)new_cap * sizeof(int));
  for (int i = *cap; i < new_cap; ++i) (*arr)[i] = 0;
  *cap = new_cap;
}

// -----------------------------------------------------------------------------
// Tokenization
// -----------------------------------------------------------------------------

// Uppercase-fold (ASCII) in place and split on whitespace. Modifies `buf`.
// Drops tokens shorter than 2 bytes or containing no alphabetic character.
// Writes token pointers into `tokens` (pointing into `buf`).
// Returns token count (clamped to max).
static int tokenize(char *buf, char **tokens, int max) {
  for (char *p = buf; *p; ++p)
    if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);

  int n = 0;
  char *p = buf;
  while (*p && n < max) {
    while (*p && isspace((unsigned char)*p)) ++p;
    if (!*p) break;
    char *start = p;
    while (*p && !isspace((unsigned char)*p)) ++p;
    char saved = *p;
    if (*p) *p = 0;
    int keep = (p - start) >= 2;
    if (keep) {
      int has_alpha = 0;
      for (const char *q = start; *q; ++q) {
        unsigned char c = (unsigned char)*q;
        if (isalpha(c) || c >= 0x80) { has_alpha = 1; break; }
      }
      keep = has_alpha;
    }
    if (keep) tokens[n++] = start;
    if (saved) ++p;
  }
  return n;
}

// -----------------------------------------------------------------------------
// Journal walker
// -----------------------------------------------------------------------------

static int is_date_start(const char *s, size_t len) {
  return len >= 10 &&
    isdigit((unsigned char)s[0]) && isdigit((unsigned char)s[1]) &&
    isdigit((unsigned char)s[2]) && isdigit((unsigned char)s[3]) &&
    (s[4] == '/' || s[4] == '-') &&
    isdigit((unsigned char)s[5]) && isdigit((unsigned char)s[6]) &&
    (s[7] == '/' || s[7] == '-') &&
    isdigit((unsigned char)s[8]) && isdigit((unsigned char)s[9]);
}

// Extract payee into `out` (size at least MAX_LINE). `line` is a transaction
// header like "YYYY/MM/DD * PAYEE TEXT  ; comment". Strips date, status flag,
// and any `  ;` trailing comment.
static void extract_payee(const char *line, char *out) {
  const char *p = line + 10;
  while (*p == ' ' || *p == '\t') ++p;
  if (*p == '*' || *p == '!') {
    ++p;
    while (*p == ' ' || *p == '\t') ++p;
  }
  size_t len = strlen(p);
  if (len >= MAX_LINE) len = MAX_LINE - 1;
  memcpy(out, p, len);
  out[len] = 0;

  // drop trailing `  ;...` comment
  char *semi = strstr(out, "  ;");
  if (semi) *semi = 0;

  // trim trailing whitespace
  size_t n = strlen(out);
  while (n && (out[n-1] == ' ' || out[n-1] == '\t' ||
               out[n-1] == '\n' || out[n-1] == '\r'))
    out[--n] = 0;
}

// Extract account name from a posting line into `out`. Returns 1 on success.
// Accepts lines with amount ("  Assets:X   12.00 EUR") or without
// ("  Expenses:Y"). Ignores comment-only lines.
static int extract_account(const char *line, char *out, size_t out_size) {
  const char *p = line;
  while (*p == ' ' || *p == '\t') ++p;
  if (!*p || *p == ';') return 0;
  const char *start = p;
  while (*p) {
    if ((p[0] == ' ' && p[1] == ' ') || p[0] == '\t') break;
    ++p;
  }
  size_t len = (size_t)(p - start);
  if (len == 0 || len >= out_size) return 0;
  memcpy(out, start, len);
  out[len] = 0;
  return 1;
}

// Strip trailing newline/carriage return in place.
static void chomp(char *s) {
  size_t n = strlen(s);
  while (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = 0;
}

// -----------------------------------------------------------------------------
// Training
// -----------------------------------------------------------------------------

// Absorb one transaction's (payee, accounts[]) into the model.
// Convention: target class = accounts[1] (matches current index=1 training).
static void train_tx(Model *m, const char *payee, char **accounts, int n_accounts) {
  if (n_accounts < 2) return;
  int a_id = strtbl_intern(&m->accounts, accounts[1]);
  ensure_int_array(&m->acc_tx, &m->acc_tx_cap, a_id);
  ensure_int_array(&m->acc_total, &m->acc_total_cap, a_id);
  m->acc_tx[a_id - 1]++;
  m->n_tx++;

  char buf[MAX_LINE];
  size_t n = strlen(payee);
  if (n >= MAX_LINE) n = MAX_LINE - 1;
  memcpy(buf, payee, n);
  buf[n] = 0;

  char *tokens[MAX_TOKENS];
  int n_tokens = tokenize(buf, tokens, MAX_TOKENS);
  int seen_ids[MAX_TOKENS];
  int n_seen = 0;
  for (int i = 0; i < n_tokens; ++i) {
    int w_id = strtbl_intern(&m->words, tokens[i]);
    wac_inc(&m->wac, w_id, a_id, 1);
    m->acc_total[a_id - 1]++;
    int dup = 0;
    for (int j = 0; j < n_seen; ++j) if (seen_ids[j] == w_id) { dup = 1; break; }
    if (!dup) {
      ensure_int_array(&m->word_df, &m->word_df_cap, w_id);
      m->word_df[w_id - 1]++;
      seen_ids[n_seen++] = w_id;
    }
  }
}

static void train_from_journal(Model *m, const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) {
    fprintf(stderr, "ledger-guesser: cannot open %s: %s\n", path, strerror(errno));
    exit(1);
  }
  char *line = NULL;
  size_t cap = 0;
  ssize_t len;
  char payee[MAX_LINE];
  char acc[512];
  char *accounts[MAX_ACCOUNTS_PER_TX];
  int n_accounts = 0;
  int in_tx = 0;
  payee[0] = 0;

  while ((len = getline(&line, &cap, f)) > 0) {
    chomp(line);
    size_t ll = strlen(line);

    if (ll == 0) {
      if (in_tx) {
        train_tx(m, payee, accounts, n_accounts);
        for (int i = 0; i < n_accounts; ++i) free(accounts[i]);
        n_accounts = 0;
        in_tx = 0;
      }
      continue;
    }
    if (is_date_start(line, ll)) {
      if (in_tx) {
        train_tx(m, payee, accounts, n_accounts);
        for (int i = 0; i < n_accounts; ++i) free(accounts[i]);
        n_accounts = 0;
      }
      extract_payee(line, payee);
      in_tx = 1;
      continue;
    }
    if (in_tx) {
      if (extract_account(line, acc, sizeof(acc))) {
        if (n_accounts < MAX_ACCOUNTS_PER_TX)
          accounts[n_accounts++] = xstrdup(acc);
      }
    }
  }
  if (in_tx) {
    train_tx(m, payee, accounts, n_accounts);
    for (int i = 0; i < n_accounts; ++i) free(accounts[i]);
  }
  free(line);
  fclose(f);
}

// -----------------------------------------------------------------------------
// Model I/O (TSV)
// -----------------------------------------------------------------------------

static void model_save(const Model *m, const char *path) {
  FILE *f = fopen(path, "w");
  if (!f) {
    fprintf(stderr, "ledger-guesser: cannot write %s: %s\n", path, strerror(errno));
    exit(1);
  }
  fprintf(f, "V\tversion\t1\n");
  fprintf(f, "N\tn_tx\t%d\n", m->n_tx);
  fprintf(f, "N\tn_accounts\t%d\n", m->accounts.n_items);
  fprintf(f, "N\tn_words\t%d\n", m->words.n_items);

  for (int i = 0; i < m->accounts.n_items; ++i) {
    int id = i + 1;
    int tx = id <= m->acc_tx_cap ? m->acc_tx[id - 1] : 0;
    int tot = id <= m->acc_total_cap ? m->acc_total[id - 1] : 0;
    fprintf(f, "A\t%s\t%d\t%d\n", m->accounts.items[i], tx, tot);
  }

  for (int i = 0; i < m->words.n_items; ++i) {
    int id = i + 1;
    int df = id <= m->word_df_cap ? m->word_df[id - 1] : 0;
    fprintf(f, "D\t%s\t%d\n", m->words.items[i], df);
  }

  for (size_t i = 0; i < m->wac.size; ++i) {
    uint64_t k = m->wac.e[i].key;
    if (!k) continue;
    int w_id = (int)(k >> 32);
    int a_id = (int)(k & 0xffffffffu);
    fprintf(f, "W\t%s\t%s\t%d\n",
            m->words.items[w_id - 1],
            m->accounts.items[a_id - 1],
            m->wac.e[i].count);
  }
  fclose(f);
}

static void model_load(Model *m, const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) {
    fprintf(stderr, "ledger-guesser: cannot open %s: %s\n", path, strerror(errno));
    exit(1);
  }
  char *line = NULL;
  size_t cap = 0;
  ssize_t len;

  while ((len = getline(&line, &cap, f)) > 0) {
    chomp(line);
    if (!line[0] || line[0] == '#') continue;
    if (line[0] == 'V') continue;
    if (line[0] == 'N') {
      char key[64];
      int val;
      if (sscanf(line, "N\t%63[^\t]\t%d", key, &val) == 2) {
        if (strcmp(key, "n_tx") == 0) m->n_tx = val;
      }
      continue;
    }
    if (line[0] == 'A') {
      // A\t<name>\t<tx>\t<tot>
      char *p = line + 1;
      if (*p != '\t') continue;
      ++p;
      char *name = p;
      char *t1 = strchr(p, '\t');
      if (!t1) continue;
      *t1 = 0;
      char *t2 = strchr(t1 + 1, '\t');
      if (!t2) continue;
      int tx = atoi(t1 + 1);
      int tot = atoi(t2 + 1);
      int a_id = strtbl_intern(&m->accounts, name);
      ensure_int_array(&m->acc_tx, &m->acc_tx_cap, a_id);
      ensure_int_array(&m->acc_total, &m->acc_total_cap, a_id);
      m->acc_tx[a_id - 1] = tx;
      m->acc_total[a_id - 1] = tot;
      continue;
    }
    if (line[0] == 'D') {
      // D\t<word>\t<df>
      char *p = line + 1;
      if (*p != '\t') continue;
      ++p;
      char *word = p;
      char *t1 = strchr(p, '\t');
      if (!t1) continue;
      *t1 = 0;
      int df = atoi(t1 + 1);
      int w_id = strtbl_intern(&m->words, word);
      ensure_int_array(&m->word_df, &m->word_df_cap, w_id);
      m->word_df[w_id - 1] = df;
      continue;
    }
    if (line[0] == 'W') {
      // W\t<word>\t<account>\t<count>
      char *p = line + 1;
      if (*p != '\t') continue;
      ++p;
      char *word = p;
      char *t1 = strchr(p, '\t');
      if (!t1) continue;
      *t1 = 0;
      char *account = t1 + 1;
      char *t2 = strchr(account, '\t');
      if (!t2) continue;
      *t2 = 0;
      int count = atoi(t2 + 1);
      int w_id = strtbl_intern(&m->words, word);
      int a_id = strtbl_intern(&m->accounts, account);
      wac_inc(&m->wac, w_id, a_id, count);
      continue;
    }
  }
  free(line);
  fclose(f);
}

// -----------------------------------------------------------------------------
// Guessing
// -----------------------------------------------------------------------------

// Naive Bayes with IDF-weighted per-word log-likelihood:
//   argmax_a [ log P(a) + sum_{w in payee} idf(w) * log P(w | a) ]
// Add-one smoothing: P(w|a) = (count(w,a) + 1) / (total(a) + V)
// Prior: P(a) = tx(a) / n_tx
// IDF: idf(w) = log(V / df(w))  (log mode, default)
//   where V = vocabulary size, df(w) = number of training tx containing w.
// Raw IDF (V / df(w)) overshoots on rare tokens, overwhelming the confidence-
// margin abstention and dropping precision from 91% to 60% on held-out data.
// Env IDF_MODE: "log" (default), "raw", "none" (plain NB without IDF).
//
// Returns 0 on a confident match (printed to stdout), 1 on abstain
// (no payee token seen at training time — caller should route to a
// suspense account).
static int guess(const Model *m, const char *payee) {
  int n_acc = m->accounts.n_items;
  if (n_acc == 0) die("empty model");

  // IDF mode selection: log (default), raw, or none
  int idf_mode = 1;  // 0 = raw (V/df), 1 = log (log(V/df)), 2 = none (1.0)
  const char *idf_env = getenv("IDF_MODE");
  if (idf_env && *idf_env) {
    if (strcmp(idf_env, "raw") == 0) idf_mode = 0;
    else if (strcmp(idf_env, "none") == 0) idf_mode = 2;
    // "log" or unknown falls through to default (1)
  }

  char buf[MAX_LINE];
  size_t n = strlen(payee);
  if (n >= MAX_LINE) n = MAX_LINE - 1;
  memcpy(buf, payee, n);
  buf[n] = 0;

  char *tokens[MAX_TOKENS];
  int n_tokens = tokenize(buf, tokens, MAX_TOKENS);

  int token_ids[MAX_TOKENS];
  double idf_weights[MAX_TOKENS];
  int n_resolved = 0;
  int V = m->words.n_items;
  for (int i = 0; i < n_tokens; ++i) {
    int id = strtbl_find(&m->words, tokens[i]);
    if (!id) continue;
    token_ids[n_resolved] = id;
    int df = (id <= m->word_df_cap) ? m->word_df[id - 1] : 0;
    if (df <= 0) df = 1;  // safety: should not happen for resolved tokens
    switch (idf_mode) {
      case 0:  idf_weights[n_resolved] = (double)V / (double)df; break;
      case 1:  idf_weights[n_resolved] = log((double)V / (double)df); break;
      default: idf_weights[n_resolved] = 1.0; break;
    }
    n_resolved++;
  }
  if (n_resolved == 0) return 1;

  double *scores = xcalloc(n_acc + 1, sizeof(double));
  double max_score = -INFINITY;
  int best_acc = 0;
  for (int a = 1; a <= n_acc; ++a) {
    int tx = a <= m->acc_tx_cap ? m->acc_tx[a - 1] : 0;
    int tot = a <= m->acc_total_cap ? m->acc_total[a - 1] : 0;
    if (tx == 0) {
      scores[a] = -INFINITY;
      continue;
    }
    double s = log((double)tx / (double)m->n_tx);
    double denom = log((double)(tot + V));
    for (int i = 0; i < n_resolved; ++i) {
      int c = wac_get(&m->wac, token_ids[i], a);
      s += idf_weights[i] * (log((double)(c + 1)) - denom);
    }
    scores[a] = s;
    if (s > max_score) {
      max_score = s;
      best_acc = a;
    }
  }
  if (best_acc == 0) {
    free(scores);
    die("no trained accounts");
  }

  // Confidence-margin abstention: abstain when the posterior probability
  // of the top account (softmax over per-account log-scores) is below a
  // threshold. Default 30%; override with CONF_PCT env var.
  double conf_pct = 30.0;
  const char *env = getenv("CONF_PCT");
  if (env && *env) conf_pct = atof(env);
  double sum_exp = 0.0;
  for (int a = 1; a <= n_acc; ++a)
    if (scores[a] > -INFINITY) sum_exp += exp(scores[a] - max_score);
  double log_top_prob = -log(sum_exp);  // = max_score - logsumexp(scores)
  free(scores);
  if (log_top_prob < log(conf_pct / 100.0)) return 1;

  printf("%s\n", m->accounts.items[best_acc - 1]);
  return 0;
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

static void usage(void) {
  fprintf(stderr,
    "Usage:\n"
    "  ledger-guesser-c train <journal.txt> <model.tsv>\n"
    "  ledger-guesser-c guess <model.tsv> \"<payee>\"\n");
  exit(1);
}

int main(int argc, char **argv) {
  if (argc < 2) usage();
  if (strcmp(argv[1], "train") == 0) {
    if (argc != 4) usage();
    Model m;
    model_init(&m);
    train_from_journal(&m, argv[2]);
    if (m.n_tx == 0) {
      fprintf(stderr, "ledger-guesser: no usable transactions in %s\n", argv[2]);
      exit(1);
    }
    model_save(&m, argv[3]);
    return 0;
  }
  if (strcmp(argv[1], "guess") == 0) {
    if (argc != 4) usage();
    Model m;
    model_init(&m);
    model_load(&m, argv[2]);
    if (m.n_tx == 0) die("empty model");
    return guess(&m, argv[3]);
  }
  usage();
  return 1;
}
