/**
 * @param {*} output
 * @returns {*}
 */
const likely = (output) => {
  let maxProp = null;
  let maxValue = -1;
  for (const prop in output) {
    const value = output[prop];
    if (value > maxValue) {
      maxProp = prop;
      maxValue = value
    }
  }
  return maxProp;
}

module.exports = likely;
