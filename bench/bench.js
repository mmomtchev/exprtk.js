const fs = require('fs');
const process = require('process');

const bench = fs.readdirSync(__dirname).filter((file) => file.match(/\.bench\.js$/));

(async () => {
  for (const fn of ['simple', 'complex'])
    for (const size of [16, 1024, 1024 * 1024])
      for (const b of bench) {
        if (process.argv[2] && !b.match(process.argv[2]))
          continue;
        if (process.argv[3] && !fn.match(process.argv[3]))
          continue;
        for (const t of ['Float64', 'Uint32']) {
          if (process.argv[4] && !t.match(process.argv[4]))
            continue;
          if (require('..')[t] === undefined) {
            console.log(`${t} not built`);
            continue;
          }
          console.log(`${b}`);
          // eslint-disable-next-line no-await-in-loop
          await require(`${__dirname}/${b}`)(t, size, fn);
          console.log(`\n\n`);
        }
      }
})();
