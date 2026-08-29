const fs = require('fs');
const path = require('path');

if (process.argv.length < 3) {
    console.log('Usage: node bin-diff.js <original_bin> <modified_bin> [output_patch.json]');
    process.exit(1);
}

const origPath = process.argv[2];
const modPath = process.argv[3];
const patchPath = process.argv[4] || 'diff.json';

try {
    const origBuf = fs.readFileSync(origPath);
    const modBuf = fs.readFileSync(modPath);

    if (origBuf.length !== modBuf.length) {
        console.error('Error: Files are different sizes. Binary diff requires identical dimensions.');
        process.exit(1);
    }

    const diffs = [];
    let start = -1;

    for (let i = 0; i < origBuf.length; i++) {
        if (origBuf[i] !== modBuf[i]) {
            if (start === -1) start = i;
        } else {
            if (start !== -1) {
                diffs.push({
                    offset: start,
                    length: i - start,
                    old: origBuf.slice(start, i).toString('hex'),
                    new: modBuf.slice(start, i).toString('hex')
                });
                start = -1;
            }
        }
    }

    // Handle trailing diff
    if (start !== -1) {
        diffs.push({
            offset: start,
            length: origBuf.length - start,
            old: origBuf.slice(start).toString('hex'),
            new: modBuf.slice(start).toString('hex')
        });
    }

    fs.writeFileSync(patchPath, JSON.stringify({
        original: origPath,
        modified: modPath,
        diffCount: diffs.length,
        patches: diffs
    }, null, 2));

    console.log(`Successfully generated ${diffs.length} patch segments to ${patchPath}`);

} catch (e) {
    console.error('File Error:', e.message);
    process.exit(1);
}
