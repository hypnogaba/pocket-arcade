// install.js: run in the badge emulator page (solana.com/breakpoint/badge/emulator)
// after window.__b holds build/emu-bundle.b64. Unpacks it (gzip, then
// name\0 + u32 length + bytes per file) and copies every file into the
// emulated card with the console's `lua put`, then starts the app.
(async () => {
  const ID = 'pocket-arcade';
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  const det = document.querySelector('details.console');
  det.open = true;
  const pre = det.querySelector('pre');
  const send = async (cmd, wait = 25) => {
    const i = document.querySelector('#emu-cmd');
    const set = Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value').set;
    set.call(i, cmd);
    i.dispatchEvent(new Event('input', { bubbles: true }));
    i.form.requestSubmit();
    await sleep(wait);
  };
  const bin = Uint8Array.from(atob(window.__b), (c) => c.charCodeAt(0));
  const raw = new Uint8Array(await new Response(new Blob([bin]).stream().pipeThrough(new DecompressionStream('gzip'))).arrayBuffer());
  const files = [];
  for (let p = 0; p < raw.length; ) {
    let z = p;
    while (raw[z]) z++;
    const name = new TextDecoder().decode(raw.slice(p, z));
    const len = raw[z + 1] | (raw[z + 2] << 8) | (raw[z + 3] << 16) | (raw[z + 4] << 24);
    files.push([name, raw.slice(z + 5, z + 5 + len)]);
    p = z + 5 + len;
  }
  await send('open home', 1500);
  const report = [];
  for (const [name, data] of files) {
    let s = '';
    for (let k = 0; k < data.length; k += 0x8000) s += String.fromCharCode(...data.subarray(k, k + 0x8000));
    const b64 = btoa(s);
    await send(`lua put ${ID} ${name} begin`, 200);
    for (let k = 0; k < b64.length; k += 128) await send(`lua put ${ID} ${name} ${b64.slice(k, k + 128)}`);
    await send(`lua put ${ID} ${name} end`, 600);
    report.push(`${name} ${data.length} bytes`);
  }
  await send(`lua sums ${ID}`, 1500);
  await send(`lua run ${ID}`, 3000);
  window.__installed = report.join('\n') + '\n--- console tail ---\n' + pre.textContent.slice(-2500);
})();
