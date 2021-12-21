const fs = require('fs');

const bench = fs.readdirSync(__dirname).filter((file) => file.match(/\.bench\.js$/));

(async () => {
  for (const size of [16, 1024, 1024 * 1024])
    for (const b of bench) {
      for (const t of ['Float64', 'Uint32']) {
        console.log(`${b}`);
        // eslint-disable-next-line no-await-in-loop
        await require(`${__dirname}/${b}`)(t, size);
        console.log(`\n\n`);
      }
    }
})();
