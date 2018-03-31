const getInput = (payee) => {
  const words = payee.split(' '); // .filter((w) => normIndex[w]);
  const input = {};
  for (w in words)
    input[words[w]] = 1;
  return input;
}

module.exports = getInput;
