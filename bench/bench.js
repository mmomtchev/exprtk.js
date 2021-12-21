const fs = require('fs');

const bench = fs.readdirSync(__dirname).filter((file) => file.match(/\.bench\.js$/));

(async () => {
  for (const b of bench) {
    for (const t of ['Float64', 'Uint8']) {
      console.log(`${b}`);
      // eslint-disable-next-line no-await-in-loop
      await require(`${__dirname}/${b}`)(t, 1024 * 1024);
      console.log(`\n\n`);
    }
  }
})();
