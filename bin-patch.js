const fs = require('fs');

if (process.argv.length < 4) {
    console.log('Usage: node bin-patch.js <stock_bin> <patch_json> <output_bin>');
    process.exit(1);
}

const stockPath = process.argv[2];
const patchPath = process.argv[3];
const outPath = process.argv[4];

if (fs.existsSync(outPath)) {
    console.error(`Error: Output file ${outPath} already exists. Aborting to prevent overwrite.`);
    process.exit(1);
}

try {
    const stockBuf = fs.readFileSync(stockPath);
    const patchData = JSON.parse(fs.readFileSync(patchPath, 'utf8'));
    
    // Create a copy to modify
    const outBuf = Buffer.from(stockBuf);

    console.log(`Applying ${patchData.diffCount} patches to ${stockPath}...`);

    for (const p of patchData.patches) {
        const newBytes = Buffer.from(p.new, 'hex');
        if (newBytes.length !== p.length) {
            throw new Error(`Patch length mismatch at offset ${p.offset}`);
        }
        newBytes.copy(outBuf, p.offset);
    }

    fs.writeFileSync(outPath, outBuf);
    console.log(`Success! Modded binary written to ${outPath}`);

} catch (e) {
    console.error('Patch Error:', e.message);
    process.exit(1);
}
