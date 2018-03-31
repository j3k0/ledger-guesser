module.exports = {
  normalize: (ratio) => {
    if (ratio < -1.0)
      ratio = -1.0;
    if (ratio > 1.0)
      ratio = 1.0;
    return (ratio + 1.0) * 0.5;
  },
  denormalize: (norm) => norm * 2.0 - 1.0,
};
