// Capability policy, not chip names, controls optional features.
const assert = require('node:assert/strict');
const { boot, goto, flush } = require('./harness');
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
  // A page opened straight on a gated route is drawn before /device and /capabilities answer.
  // Each gated view waits for them itself, so a Pico never ends up showing what it lacks.
  const pico = { scripting: false, transitions: [],
    audio: { buzzer: true, track: false, mp3: false, radio: false } };
  for (const [route, device, capabilities] of [['#/audio', 300, 600], ['#/audio', 600, 300],
                                               ['#/system', 300, 600], ['#/system', 600, 300]]) {
    const { dom, window } = await boot({ caps: pico, url: 'http://localhost/' + route,
      device: { soc: 'rp2040', updateImage: 'firmware-galactic-unicorn.uf2' },
      delays: { '/api/v1/device': device, '/api/v1/capabilities': capabilities } });
    try {
      await flush(1000);
      const view = window.document.querySelector('#view');
      assert.equal(view.querySelector('#sec-mp3, #sec-radio'), null, route + ': no MP3 or radio');
      assert.equal(view.textContent.includes('Run scripts'), false, route + ': no script toggle');
      assert.equal(view.querySelector('input[type=file][accept=".bin"]'), null,
        route + ': no .bin upload');
      if (route === '#/audio') assert(view.querySelector('#sec-melodies'), 'RTTTL remains available');
    } finally { dom.window.close(); }
  }
  console.log('PASS capabilities on a route opened before the board answers');
})().then(() => process.exit(0)).catch(e => { console.error(e); process.exit(1); });
