/* eslint-disable no-console */
'use strict';

const log = (...args) => console.error(...args);

module.exports = Object.assign(log, {
  debug: log,

  info (...args) {
    console.error(...args);
  },

  error (...args) {
    console.error(...args);
  },

  warn (...args) {
    console.error(...args);
  },
});
