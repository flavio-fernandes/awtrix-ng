// Capability policy, not chip names, controls optional features.
const assert = require('node:assert/strict');
const { boot, goto } = require('./harness');
(async () => {
  for (const enabled of [false, true]) {
    const { dom, window, store, netlog } = await boot({ caps: {
      scripting: enabled, transitions: [],
      audio: { buzzer: true, track: false, mp3: enabled, radio: enabled }
    }});
    try {

      const nav = () => window.document.querySelector('#nav a[href="#/scripts"]');
      assert.equal(!!nav(), enabled, 'script navigation follows capability');
      await goto(window, '#/system');
      const view = () => window.document.querySelector('#view');
      assert.equal(view().textContent.includes('Run scripts'), enabled, 'script toggle follows capability');
      if (!enabled) assert.match(view().textContent, /not available on this board/i);
      await goto(window, '#/scripts');
      if (!enabled) {
        assert.match(view().textContent, /not available on this board/i);
        assert.equal(view().querySelector('textarea'), null, 'direct route cannot expose editor');
        assert(!netlog.some(s => s.includes('/api/v1/scripts/shared')), 'no script polling');
      } else assert(view().querySelector('textarea'), 'enabled editor remains available');
      await goto(window, '#/audio');
      assert.equal(!!view().querySelector('#sec-mp3'), enabled);
      assert.equal(!!view().querySelector('#sec-radio'), enabled);
      assert(view().querySelector('#sec-melodies'), 'RTTTL remains available');
      if (!enabled) assert.match(view().textContent, /not available on this board/i);
      console.log(`PASS capabilities scripting/mp3/radio=${enabled}`);
    } finally { dom.window.close(); }
  }
})().then(() => process.exit(0)).catch(e => { console.error(e); process.exit(1); });
