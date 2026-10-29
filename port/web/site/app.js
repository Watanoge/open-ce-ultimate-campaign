/*
APP.JS

The launcher page (index.html): it checks that the browser can run the game,
copies the game data in from the player's disc image (xiso-worker.js) and
the player's Custom Edition maps (custom-maps-worker.js), mods
(mods-worker.js) and tag profiles (tag-profiles-worker.js), exports those
(as zips: archive.js) and the saved games, and starts the game (halo.js and
halo.wasm: tools/web_build.py), whose canvas then fills the page.

- The game's threads need SharedArrayBuffer, so a cross-origin isolated
  page: GitHub Pages cannot send the headers for it, the service worker
  (sw.js) adds them, and the first visit loads the page once more under it.
- The game reads its maps, and keeps its saved games, config.toml and
  debug.txt, in the Origin Private File System (port/web/src/web_main.c
  mounts it at /data). Each origin has its own: the github.io address and
  the site's own domain do not share them.
- Query parameters, for testing: ?set=NAME=value (any number) passes a
  setting's environment variable (port/linux/README.md, the settings
  table), ?room=<secret> joins a room (net.js; ?brokers= names other
  message brokers, for internet play's signalling too), ?menu=<screen> opens
  a menu screen (debug.menu_open), ?map=<level> starts a level (a10, or
  levels\a10\a10: the init.txt the game reads, which the page removes again
  the next time), ?play starts the game at once when it can.
- ?join=<invite> (a halo://join/ link's code, which native builds and the
  game here make: port/linux/src/p2p.c) joins that internet game once the
  game starts; the Servers panel (servers.js) sets it with Play here.
*/

'use strict';

const $ = (id) => document.getElementById(id);
const params = new URLSearchParams(location.search);
// the Xbox window ends the game's memory: tools/web_build.py WEB_MEMORY_BYTES
const WEB_MEMORY_BYTES = 0x88000000;
const MAXIMUM_LOG_LINES = 20000;

// ---------- the log: the game's output, for the player to send

const log = [];

function logLine(text) {
  for (const line of String(text).replace(/\r/g, '').split('\n')) {
    if (line === '') continue;
    log.push(line);
  }
  if (log.length > MAXIMUM_LOG_LINES) log.splice(0, log.length - MAXIMUM_LOG_LINES);
}

function download(name, blob) {
  const link = document.createElement('a');
  link.href = URL.createObjectURL(blob);
  link.download = name;
  document.body.append(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(link.href), 10000);
}

async function readStorageText(name) {
  try {
    const root = await navigator.storage.getDirectory();
    const file = await (await root.getFileHandle(name)).getFile();
    return await file.text();
  } catch {
    return null;
  }
}

async function downloadLog() {
  let text = `OpenCE web ${JSON.stringify(stamp)}\n${navigator.userAgent}\n\n` + log.join('\n') + '\n';
  // (debug.txt, which the game writes, when it is not open: before or after
  // a game)
  if (!gameStarted || gameStopped) {
    const debug = await readStorageText('debug.txt');
    if (debug) text += '\n---------- debug.txt\n' + debug.slice(-2000000);
  }
  download('opence-web-log.txt', new Blob([text], { type: 'text/plain' }));
}

// ---------- errors: the game's own, not a browser extension's

function isExtensionNoise(message, source) {
  const text = String(message || '') + ' ' + String(source || '');
  return /chrome-extension:|moz-extension:|safari-extension:|MetaMask|ethereum/i.test(text);
}

window.addEventListener('error', (event) => {
  if (isExtensionNoise(event.message, event.filename)) return;
  logLine('page error: ' + (event.error && event.error.stack ? event.error.stack : event.message));
});
window.addEventListener('unhandledrejection', (event) => {
  const reason = event.reason;
  const text = reason && reason.stack ? reason.stack : String(reason);
  if (isExtensionNoise(text)) return;
  logLine('page error: ' + text);
});

// ---------- the build (version.json) and its updates

let stamp = {};

async function loadStamp() {
  try {
    stamp = await (await fetch('version.json', { cache: 'no-cache' })).json();
  } catch {
    stamp = {};
  }
  $('about-build').textContent = stamp.build ? `${stamp.build} (${stamp.flavor})` : `local (${stamp.flavor || 'unknown'})`;
  $('about-commit').textContent = stamp.commit ? stamp.commit.slice(0, 12) : '–';
  $('about-network').textContent = stamp.network_version != null ? String(stamp.network_version) : '–';
}

async function checkForUpdate() {
  if (!navigator.serviceWorker || !navigator.serviceWorker.controller || !stamp.version) return;
  try {
    const latest = await (await fetch('version.json?latest', { cache: 'no-store' })).json();
    if (latest.version && latest.version !== stamp.version) {
      $('update-text').textContent = latest.build ? `Build ${latest.build} is out.` : 'A new build is out.';
      $('update').hidden = false;
    }
  } catch {
    // offline: the build there is plays
  }
}

$('update-button').addEventListener('click', () => {
  $('update-button').disabled = true;
  $('update-text').textContent = 'Updating…';
  navigator.serviceWorker.addEventListener('message', (event) => {
    if (event.data === 'updated') location.reload();
    else if (event.data === 'update-failed') $('update-text').textContent = 'The update could not be downloaded.';
  });
  navigator.serviceWorker.controller.postMessage('update');
});

// ---------- cross-origin isolation (sw.js)

async function isolate() {
  if (window.crossOriginIsolated || !('serviceWorker' in navigator) || !window.isSecureContext) return;
  try {
    await navigator.serviceWorker.register('sw.js');
    await navigator.serviceWorker.ready;
  } catch (error) {
    logLine('service worker: ' + error);
    return;
  }
  // once: a page the worker still does not isolate stays as it is
  if (!sessionStorage.getItem('opence-isolating')) {
    sessionStorage.setItem('opence-isolating', '1');
    $('checks-note').textContent = 'Preparing the page (it loads once more)…';
    $('checks-note').hidden = false;
    location.reload();
    await new Promise(() => {});
  }
}

// ---------- the browser's checks

const results = {};

function addCheck(name, state, text, detail) {
  results[name] = state;
  const item = document.createElement('li');
  item.className = state;
  item.append(text);
  if (detail) {
    const span = document.createElement('span');
    span.className = 'detail';
    span.textContent = ' ' + detail;
    item.append(span);
  }
  $('checks').append(item);
}

function workerWebGL2() {
  return new Promise((resolve) => {
    const source = "try { postMessage(!!new OffscreenCanvas(1, 1).getContext('webgl2')); } catch (e) { postMessage(false); }";
    let worker;
    try {
      worker = new Worker(URL.createObjectURL(new Blob([source], { type: 'text/javascript' })));
    } catch {
      resolve(false);
      return;
    }
    const timer = setTimeout(() => { worker.terminate(); resolve(false); }, 5000);
    worker.onmessage = (event) => { clearTimeout(timer); worker.terminate(); resolve(event.data === true); };
    worker.onerror = () => { clearTimeout(timer); worker.terminate(); resolve(false); };
  });
}

function memoryAvailable() {
  try {
    const pages = WEB_MEMORY_BYTES / 65536;
    new WebAssembly.Memory({ initial: pages, maximum: pages, shared: true });
    return true;
  } catch {
    return false;
  }
}

async function runChecks() {
  $('checks').textContent = '';
  addCheck('secure', window.isSecureContext ? 'good' : 'bad', 'A secure page (https)');
  addCheck('isolated', window.crossOriginIsolated ? 'good' : 'bad', 'Threads (cross-origin isolation)',
    window.crossOriginIsolated ? '' : 'reload the page; a private window may not allow it');
  const webgl = typeof OffscreenCanvas !== 'undefined' && await workerWebGL2();
  addCheck('webgl', webgl ? 'good' : 'bad', 'WebGL 2 from a worker');
  const opfs = !!(navigator.storage && navigator.storage.getDirectory);
  addCheck('opfs', opfs ? 'good' : 'bad', "The site's private storage (OPFS)");
  const memory = window.crossOriginIsolated && memoryAvailable();
  addCheck('memory', memory ? 'good' : 'bad', 'Memory for the game (2.1 GB)',
    memory ? '' : 'this browser could not give the game that much: a 64-bit browser on a computer has it');
  const required = ['secure', 'isolated', 'webgl', 'opfs', 'memory'];
  return required.every((name) => results[name] === 'good');
}

// ---------- the game data (xiso-worker.js)

let dataReady = false;

function formatBytes(bytes) {
  return bytes >= 1e9 ? (bytes / 1e9).toFixed(2) + ' GB' : Math.round(bytes / 1e6) + ' MB';
}

async function refreshData() {
  dataReady = false;
  let text = 'No game data yet: choose your disc image of Halo: Combat Evolved for the Xbox.';
  try {
    const root = await navigator.storage.getDirectory();
    const maps = await root.getDirectoryHandle('maps');
    const marker = await (await maps.getFileHandle('.complete')).getFile();
    const complete = JSON.parse(await marker.text());
    text = `${complete.files.length} files of maps (${formatBytes(complete.bytes)}), ready.`;
    dataReady = true;
  } catch {
    // none, or a copy that did not finish
  }
  $('data-status').textContent = text;
  $('delete-data').disabled = !dataReady;
  await refreshStorage();
  updatePlay();
}

async function refreshStorage() {
  if (!navigator.storage || !navigator.storage.estimate) return;
  const { usage, quota } = await navigator.storage.estimate();
  const persisted = navigator.storage.persisted ? await navigator.storage.persisted() : false;
  $('storage-status').textContent = `This site keeps ${formatBytes(usage || 0)} of the ${formatBytes(quota || 0)} ` +
    `the browser allows it. ${persisted ? 'The browser keeps it.' : 'The browser may clear it when space runs low.'}`;
  $('persist').disabled = persisted;
}

function importData(message) {
  const worker = new Worker('xiso-worker.js');
  $('import-progress').hidden = false;
  $('import-bar').style.width = '0';
  $('import-text').textContent = 'Reading…';
  setImporting(true);
  // (asked before the copy: the maps are 2 GB the browser should not drop)
  if (navigator.storage && navigator.storage.persist) navigator.storage.persist().catch(() => {});
  worker.onmessage = async (event) => {
    const data = event.data;
    if (data.type === 'progress') {
      $('import-bar').style.width = (100 * data.done / data.total).toFixed(1) + '%';
      $('import-text').textContent = `Copying ${data.file}: ${formatBytes(data.done)} of ${formatBytes(data.total)}`;
      return;
    }
    worker.terminate();
    setImporting(false);
    if (data.type === 'done') {
      $('import-text').textContent = `Copied ${data.files} files.`;
      logLine(`import: ${data.files} files, ${data.bytes} bytes`);
    } else {
      $('import-text').textContent = data.message;
      logLine('import failed: ' + data.message);
    }
    await refreshData();
  };
  worker.onerror = (event) => {
    worker.terminate();
    setImporting(false);
    $('import-text').textContent = 'The copy failed: ' + event.message;
  };
  worker.postMessage(message);
}

function setImporting(importing) {
  for (const id of ['image-label', 'folder-label']) $(id).classList.toggle('disabled', importing);
  $('image-input').disabled = importing;
  $('folder-input').disabled = importing;
  $('play').disabled = importing || !canPlay();
}

$('image-input').addEventListener('change', (event) => {
  const file = event.target.files[0];
  event.target.value = '';
  if (file) importData({ type: 'image', file });
});
$('folder-input').addEventListener('change', (event) => {
  const files = [...event.target.files];
  event.target.value = '';
  if (files.length) importData({ type: 'folder', files });
});

// ---------- Custom Edition maps (custom-maps-worker.js): /data/custom_maps,
// the folder and one level of subfolders, as the game lists them
// (port/linux/game/custom_edition_maps.c)

async function refreshCustomMaps() {
  let maps = 0;
  let bytes = 0;
  const resources = [];
  try {
    const root = await navigator.storage.getDirectory();
    const folder = await root.getDirectoryHandle('custom_maps');
    const count = async (directory, top) => {
      for await (const [name, handle] of directory.entries()) {
        if (handle.kind === 'directory') {
          if (top) await count(handle, false);
          continue;
        }
        const lower = name.toLowerCase();
        if (!lower.endsWith('.map')) continue;
        const size = (await handle.getFile()).size;
        bytes += size;
        if (top && ['bitmaps.map', 'sounds.map', 'loc.map'].includes(lower)) resources.push(lower);
        else maps++;
      }
    };
    await count(folder, true);
  } catch {
    // none yet
  }
  const missing = ['bitmaps.map', 'sounds.map', 'loc.map'].filter((name) => !resources.includes(name));
  let text = 'None yet.';
  if (maps || resources.length) {
    text = `${maps} map${maps === 1 ? '' : 's'} (${formatBytes(bytes)})` +
      (missing.length ? `; ${missing.join(', ')} missing: the maps cannot be played without them.` : ', ready.');
  }
  $('custom-status').textContent = text;
  $('custom-remove').disabled = !(maps || resources.length);
  $('custom-export').disabled = !(maps || resources.length);
  await refreshStorage();
}

// a worker's progress: the bar, and what it is doing to which file
function showProgress(data) {
  $('import-bar').style.width = (100 * data.done / (data.total || 1)).toFixed(1) + '%';
  $('import-text').textContent = `${data.verb || 'Copying'} ${data.file}: ${formatBytes(data.done)} of ${formatBytes(data.total)}`;
}

function customMapsTask(message) {
  const worker = new Worker('custom-maps-worker.js');
  const busy = (on) => {
    for (const id of ['custom-label', 'custom-files-label']) $(id).classList.toggle('disabled', on);
    $('custom-input').disabled = on;
    $('custom-files-input').disabled = on;
    $('custom-remove').disabled = on;
    $('custom-export').disabled = on;
  };
  busy(true);
  $('import-progress').hidden = false;
  $('import-bar').style.width = '0';
  $('import-text').textContent = message.type === 'remove' ? 'Removing…' : 'Reading…';
  worker.onmessage = async (event) => {
    const data = event.data;
    if (data.type === 'progress') {
      showProgress(data);
      return;
    }
    worker.terminate();
    busy(false);
    if (data.type === 'error') {
      $('import-text').textContent = data.message;
      logLine('custom maps: ' + data.message);
    } else if (message.type === 'remove') {
      $('import-text').textContent = 'Removed the Custom Edition maps.';
    } else if (message.type === 'export') {
      $('import-text').textContent = `Exported ${data.files} files (${formatBytes(data.bytes)}).`;
      download('opence-custom-maps.zip', data.blob);
    } else {
      const left = data.skipped.length ? ` Left out ${data.skipped.length}: ` +
        data.skipped.slice(0, 5).map((entry) => `${entry.path} (${entry.why})`).join(', ') +
        (data.skipped.length > 5 ? ', …' : '') + '.' : '';
      $('import-text').textContent = `Copied ${data.files} files.${left}`;
      logLine(`custom maps: ${data.files} files, ${data.bytes} bytes`);
      for (const entry of data.skipped) logLine(`custom maps: left out ${entry.path}: ${entry.why}`);
    }
    await refreshCustomMaps();
  };
  worker.onerror = async (event) => {
    worker.terminate();
    busy(false);
    $('import-text').textContent = 'The copy failed: ' + event.message;
    await refreshCustomMaps();
  };
  if (navigator.storage && navigator.storage.persist) navigator.storage.persist().catch(() => {});
  worker.postMessage(message);
}

for (const id of ['custom-input', 'custom-files-input']) {
  $(id).addEventListener('change', (event) => {
    const files = [...event.target.files];
    event.target.value = '';
    if (files.length) customMapsTask({ type: 'add', files });
  });
}
$('custom-export').addEventListener('click', () => customMapsTask({ type: 'export' }));
$('custom-remove').addEventListener('click', () => {
  if (confirm('Remove the Custom Edition maps from this browser (their resource maps too)?')) {
    customMapsTask({ type: 'remove' });
  }
});

// ---------- mods (mods-worker.js): /data/mods, all of them played unless
// the game's main menu's Mods turns them off (port/linux/src/port_mods.c)

// the name a mod's mod.toml gives it, else its folder's
async function modName(directory, folder) {
  try {
    const text = await (await (await directory.getFileHandle('mod.toml')).getFile()).text();
    const match = /^\s*name\s*=\s*"([^"\n]*)"/m.exec(text);
    if (match && match[1].trim()) return match[1].trim();
  } catch {
    // (none: not a mod the game plays)
  }
  return folder;
}

async function refreshMods() {
  const list = $('mods-list');
  const found = [];
  try {
    const root = await navigator.storage.getDirectory();
    const mods = await root.getDirectoryHandle('mods');
    for await (const [folder, handle] of mods.entries()) {
      if (handle.kind === 'directory') found.push({ folder, name: await modName(handle, folder) });
    }
  } catch {
    // no mods folder yet
  }
  // (as the game orders them by default: their folders' names, whatever the
  // case of their A to Z, port_mods.c's names_compare)
  const key = (folder) => folder.replace(/[A-Z]/g, (letter) => letter.toLowerCase());
  found.sort((first, second) => (key(first.folder) < key(second.folder) ? -1 : 1));
  list.replaceChildren(...found.map(({ folder, name }) => {
    const item = document.createElement('li');
    const label = document.createElement('span');
    const detail = document.createElement('span');
    const remove = document.createElement('button');
    label.textContent = name;
    detail.className = 'detail';
    detail.textContent = ` ${folder}`;
    label.append(detail);
    remove.textContent = 'Remove';
    remove.className = 'danger';
    remove.addEventListener('click', () => {
      if (confirm(`Remove the mod ${name} from this browser?`)) modTask({ type: 'remove', name: folder });
    });
    item.append(label, remove);
    return item;
  }));
  $('mods-status').textContent = found.length ? `${found.length} mod${found.length === 1 ? '' : 's'} in this browser.` :
    'No mods yet.';
  summarize('summary-mods', null, found.length ? `${found.length} mod${found.length === 1 ? '' : 's'}` : 'None yet');
  await refreshStorage();
}

function modTask(message) {
  const worker = new Worker('mods-worker.js');
  $('mod-label').classList.add('disabled');
  $('mod-input').disabled = true;
  worker.onmessage = async (event) => {
    const data = event.data;
    if (data.type === 'progress') {
      $('mods-status').textContent = `Copying ${data.file}: ${formatBytes(data.done)} of ${formatBytes(data.total)}`;
      return;
    }
    worker.terminate();
    $('mod-label').classList.remove('disabled');
    $('mod-input').disabled = false;
    await refreshMods();
    if (data.type === 'error') $('mods-status').textContent = data.message;
    else if (message.type === 'add') logLine(`mods: ${data.name}, ${data.files} files, ${data.bytes} bytes`);
  };
  worker.onerror = async (event) => {
    worker.terminate();
    $('mod-label').classList.remove('disabled');
    $('mod-input').disabled = false;
    await refreshMods();
    $('mods-status').textContent = 'The copy failed: ' + event.message;
  };
  worker.postMessage(message);
}

$('mod-input').addEventListener('change', (event) => {
  const files = [...event.target.files];
  event.target.value = '';
  if (files.length) modTask({ type: 'add', files });
});

// ---------- tag profiles (tag-profiles-worker.js): /data/tag_injector, a
// folder each, and the multiplayer and campaign profiles that play them
// (port/linux/src/port_tag_profiles.c, port_tag_mp_profiles.c)

// a TOML file's string of that key, as the game writes it
function tomlString(text, key) {
  const match = new RegExp(`^\\s*${key}\\s*=\\s*"((?:[^"\\\\\\n]|\\\\.)*)"`, 'm').exec(text);
  return match ? match[1].replace(/\\(.)/g, '$1') : null;
}

async function refreshProfiles() {
  const found = [];
  // (and an earlier build's folders of them, until the game or an import
  // moves them)
  const kinds = { multiplayer: [], campaign: [], '.multiplayer': [], '.campaign': [] };
  try {
    const root = await (await navigator.storage.getDirectory()).getDirectoryHandle('tag_injector');
    for await (const [folder, handle] of root.entries()) {
      if (handle.kind !== 'directory') continue;
      if (Object.hasOwn(kinds, folder)) {
        for await (const [name, file] of handle.entries()) {
          if (file.kind === 'file' && /\.toml$/i.test(name)) {
            kinds[folder].push(tomlString(await (await file.getFile()).text(), 'profile') || '');
          }
        }
        continue;
      }
      if (folder.startsWith('.') || /^(multiplayer|campaign)$/i.test(folder)) continue;
      let name = null;
      let bytes = 0;
      for await (const [file, fileHandle] of handle.entries()) {
        if (fileHandle.kind !== 'file') continue;
        const contents = await fileHandle.getFile();
        bytes += contents.size;
        if (file === 'profile.toml') name = tomlString(await contents.text(), 'name') || folder;
      }
      if (name !== null) found.push({ folder, name, bytes });
    }
  } catch {
    // no profiles yet
  }
  const key = (text) => text.toLowerCase();
  found.sort((first, second) => (key(first.name) < key(second.name) ? -1 : 1));
  $('profiles-list').replaceChildren(...found.map(({ folder, name, bytes }) => {
    const item = document.createElement('li');
    const label = document.createElement('span');
    const detail = document.createElement('span');
    const exporting = document.createElement('button');
    const playing = Object.values(kinds).flat().filter((profile) => key(profile) === key(folder)).length;
    label.textContent = name;
    detail.className = 'detail';
    detail.textContent = ` ${formatBytes(bytes)}` + (folder !== name ? `, ${folder}` : '') +
      (playing ? `, ${playing} multiplayer or campaign profile${playing === 1 ? '' : 's'}` : '');
    label.append(detail);
    exporting.textContent = 'Export';
    exporting.addEventListener('click', () => profilesTask({ type: 'export', folder }));
    item.append(label, exporting);
    return item;
  }));
  const multiplayer = kinds.multiplayer.length + kinds['.multiplayer'].length;
  const campaign = kinds.campaign.length + kinds['.campaign'].length;
  $('profiles-status').textContent = found.length ?
    `${found.length} tag profile${found.length === 1 ? '' : 's'}, ${multiplayer} multiplayer and ${campaign} campaign ` +
    `profile${campaign === 1 ? '' : 's'} in this browser.` : 'No tag profiles yet.';
  $('profiles-export').disabled = !found.length;
  await refreshStorage();
}

function profilesTask(message) {
  const worker = new Worker('tag-profiles-worker.js');
  const controls = ['profiles-zip-input', 'profiles-folder-input', 'profiles-export'];
  const busy = (on) => {
    for (const id of ['profiles-zip-label', 'profiles-folder-label']) $(id).classList.toggle('disabled', on);
    for (const id of controls) $(id).disabled = on;
    for (const button of $('profiles-list').querySelectorAll('button')) button.disabled = on;
  };
  busy(true);
  $('profiles-status').textContent = message.type === 'export' ? 'Packing…' : 'Reading…';
  worker.onmessage = async (event) => {
    const data = event.data;
    if (data.type === 'progress') {
      $('profiles-status').textContent = `${data.verb} ${data.file}: ${formatBytes(data.done)} of ${formatBytes(data.total)}`;
      return;
    }
    worker.terminate();
    busy(false);
    if (data.type === 'conflicts') {
      const names = [...data.profiles, ...data.others];
      await refreshProfiles();
      if (confirm(`Replace ${names.length === 1 ? 'this' : 'these'}, already in this browser? ` +
        `${names.slice(0, 8).join(', ')}${names.length > 8 ? ', …' : ''}`)) {
        profilesTask({ ...message, replace: true });
      }
      return;
    }
    await refreshProfiles();
    if (data.type === 'error') {
      $('profiles-status').textContent = data.message;
      logLine('tag profiles: ' + data.message);
    } else if (message.type === 'export') {
      download(data.name, data.blob);
    } else {
      const left = data.skipped.length ? ` Left out ${data.skipped.length}: ` +
        data.skipped.slice(0, 5).map((entry) => `${entry.path} (${entry.why})`).join(', ') +
        (data.skipped.length > 5 ? ', …' : '') + '.' : '';
      $('profiles-status').textContent = `Imported ${data.profiles.length} profile${data.profiles.length === 1 ? '' : 's'}` +
        (data.others.length ? ` and ${data.others.length} multiplayer or campaign profile${data.others.length === 1 ? '' : 's'}` : '') +
        ` (${formatBytes(data.bytes)}).${left}`;
      logLine(`tag profiles: imported ${data.profiles.join(', ')}; ${data.others.join(', ')}; ${data.bytes} bytes`);
      for (const entry of data.skipped) logLine(`tag profiles: left out ${entry.path}: ${entry.why}`);
    }
  };
  worker.onerror = async (event) => {
    worker.terminate();
    busy(false);
    await refreshProfiles();
    $('profiles-status').textContent = 'The copy failed: ' + event.message;
  };
  if (message.type === 'import' && navigator.storage && navigator.storage.persist) navigator.storage.persist().catch(() => {});
  worker.postMessage(message);
}

for (const id of ['profiles-zip-input', 'profiles-folder-input']) {
  $(id).addEventListener('change', (event) => {
    const files = [...event.target.files];
    event.target.value = '';
    if (files.length) profilesTask({ type: 'import', files });
  });
}
$('profiles-export').addEventListener('click', () => profilesTask({ type: 'export', folder: null }));

$('delete-data').addEventListener('click', async () => {
  if (!confirm('Delete the maps copied into this browser? Saved games stay.')) return;
  const root = await navigator.storage.getDirectory();
  await root.removeEntry('maps', { recursive: true }).catch(() => {});
  // (and the game's copies of them, save/z/cache000.map and on)
  try {
    const drive = await (await root.getDirectoryHandle('save')).getDirectoryHandle('z');
    for await (const [name] of drive.entries()) {
      if (/^cache\d+\.map$/i.test(name)) await drive.removeEntry(name).catch(() => {});
    }
  } catch {
    // no saved games yet
  }
  await refreshData();
});

// everything the site keeps in this browser: the private storage (OPFS), the
// service worker and its caches (sw.js), and the page's own storage; the page
// then loads as on a first visit. (Listened to from the start: a launcher
// that stalls on what it reads can still be reset.)
$('delete-everything').addEventListener('click', async () => {
  if (gameElsewhere) {
    alert('The game is running in another tab of this browser: quit it or close that tab first.');
    return;
  }
  if (!confirm('Delete everything this site keeps in this browser: the maps, Custom Edition maps, mods, tag ' +
    'profiles, saved games, replays and settings? Export what you want to keep first.')) return;
  const button = $('delete-everything');
  button.disabled = true;
  $('storage-status').textContent = 'Deleting…';
  const failed = [];
  try {
    const root = await navigator.storage.getDirectory();
    const names = [];
    for await (const [name] of root.entries()) names.push(name);
    for (const name of names) {
      $('storage-status').textContent = `Deleting ${name}…`;
      await root.removeEntry(name, { recursive: true }).catch((error) => failed.push(`${name} (${error.name})`));
    }
  } catch {
    // (no private storage)
  }
  try {
    for (const name of await caches.keys()) await caches.delete(name);
  } catch {
    // (no caches)
  }
  try {
    for (const registration of await navigator.serviceWorker.getRegistrations()) await registration.unregister();
  } catch {
    // (no service worker)
  }
  try {
    if (indexedDB.databases) {
      for (const { name } of await indexedDB.databases()) if (name) indexedDB.deleteDatabase(name);
    }
  } catch {
    // (none)
  }
  try {
    localStorage.clear();
    sessionStorage.clear();
  } catch {
    // (none)
  }
  logLine('deleted everything' + (failed.length ? `; not: ${failed.join(', ')}` : ''));
  if (failed.length) {
    $('storage-status').textContent = `Could not delete ${failed.join(', ')}: close this site's other tabs and ` +
      'try again.';
    button.disabled = false;
    return;
  }
  location.replace(location.pathname);
});

$('persist').addEventListener('click', async () => {
  if (navigator.storage && navigator.storage.persist) await navigator.storage.persist();
  await refreshStorage();
});

// ---------- saved games out, as a zip (archive.js)

// the saved games and profiles, not the maps the game keeps a copy of as the
// Xbox's hard disk did (save/z/cache000.map and on: hundreds of MB the game
// makes again)
async function collect(directory, prefix, entries) {
  for await (const [name, handle] of directory.entries()) {
    if (/^cache\d+\.map$/i.test(name) || (prefix === 'save/z/' && name === 'maps')) continue;
    if (handle.kind === 'directory') await collect(handle, prefix + name + '/', entries);
    else entries.push({ name: prefix + name, data: new Uint8Array(await (await handle.getFile()).arrayBuffer()) });
  }
}

$('export-saves').addEventListener('click', async () => {
  if (gameStarted && !gameStopped) {
    alert('Quit the game first: its files are open while it runs.');
    return;
  }
  if (gameElsewhere) {
    alert('Quit the game in the other tab first: its files are open while it runs.');
    return;
  }
  const entries = [];
  const root = await navigator.storage.getDirectory();
  try {
    await collect(await root.getDirectoryHandle('save'), 'save/', entries);
  } catch {
    // no saved games yet
  }
  const config = await readStorageText('config.toml');
  if (config != null) entries.push({ name: 'config.toml', data: new TextEncoder().encode(config) });
  if (!entries.length) {
    alert('There are no saved games yet.');
    return;
  }
  download('opence-saves.zip', await window.HaloArchive.zip(entries));
});

$('download-log').addEventListener('click', downloadLog);
$('fatal-log-download').addEventListener('click', downloadLog);
$('fatal-reload').addEventListener('click', () => location.reload());

// ---------- the game

let checksPassed = false;
let gameStarted = false;
let gameStopped = false;
let wakeLock = null;

function canPlay() {
  return checksPassed && dataReady && !gameStarted && !gameElsewhere;
}

// The game in another tab of this browser (the site's other pages share its
// storage, whose files a game keeps open): the tab playing holds a lock, and
// the others wait for it to go, Play off meanwhile.
const GAME_LOCK = 'opence-game';
let gameElsewhere = false;

async function watchGameElsewhere() {
  if (!navigator.locks || !navigator.locks.query) return;
  try {
    const { held } = await navigator.locks.query();
    if (!held.some((lock) => lock.name === GAME_LOCK)) return;
  } catch {
    return;
  }
  gameElsewhere = true;
  updatePlay();
  // (granted once the other tab closes or quits its game)
  navigator.locks.request(GAME_LOCK, { mode: 'shared' }, () => {
    gameElsewhere = false;
    updatePlay();
  }).catch(() => {});
}

// the lock, held as long as this page is: false when another tab has it
function holdGameLock() {
  if (!navigator.locks) return Promise.resolve(true);
  return new Promise((resolve) => {
    navigator.locks.request(GAME_LOCK, { ifAvailable: true }, (lock) => {
      resolve(!!lock);
      return lock ? new Promise(() => {}) : null;
    }).catch(() => resolve(true));
  });
}

// the Ready to play screen's rows: a row good or bad, and its value
function summarize(id, good, text) {
  $(id).classList.toggle('good', good === true);
  $(id).classList.toggle('bad', good === false);
  $(id + '-value').textContent = text;
}

function updatePlay() {
  $('play').disabled = !canPlay();
  $('play-note').textContent = !checksPassed ? 'This browser cannot run the game: see This browser, below.' :
    gameElsewhere ? 'The game is already running in another tab of this browser: play there, or quit it or close ' +
      'that tab, and Play here turns on.' :
    !dataReady ? 'The game data is needed first.' : '';
  summarize('summary-browser', checksPassed, checksPassed ? 'Runs the game' : 'Cannot run the game');
  summarize('summary-data', dataReady, dataReady ? $('data-status').textContent : 'Needed first');
}

// init.txt in the data folder: the console commands the game runs as it
// starts. ?map= writes one, and the page removes it on the next visit
// without ?map=, only if it wrote it.
const INIT_MARKER = 'opence-page-wrote-init';

async function prepareInit() {
  const root = await navigator.storage.getDirectory();
  let wrote = false;
  try {
    wrote = localStorage.getItem(INIT_MARKER) === '1';
  } catch {
    // (no localStorage: nothing to undo)
  }
  if (params.has('map')) {
    const name = params.get('map');
    const level = name.includes('\\') ? name : `levels\\${name}\\${name}`;
    if (!/^[A-Za-z0-9_\\]+$/.test(level)) return;
    const handle = await root.getFileHandle('init.txt', { create: true });
    const writable = await handle.createWritable();
    await writable.write(`map_name ${level}\r\n`);
    await writable.close();
    try {
      localStorage.setItem(INIT_MARKER, '1');
    } catch {
      // as above
    }
  } else if (wrote) {
    await root.removeEntry('init.txt').catch(() => {});
    try {
      localStorage.removeItem(INIT_MARKER);
    } catch {
      // as above
    }
  }
}

// ---------- rooms (net.js)

function roomLink(secret) {
  return `${location.origin}${location.pathname}?room=${secret}`;
}

function showRoom(status) {
  const inRoom = !!status;
  $('room-create').hidden = inRoom || gameStarted;
  $('room-copy').hidden = !inRoom;
  $('room-leave').hidden = !inRoom || gameStarted;
  if (!inRoom) return;
  const brokers = status.brokers ? '' : ' (finding the message brokers…)';
  $('room-status').textContent = status.peers ?
    `In a room with ${status.connected} of ${status.peers} other browsers connected${brokers}. Your address: ${status.address}.` :
    `In a room, alone so far${brokers}: send its link to your friends.`;
}

async function joinRoom(secret) {
  const brokers = (params.get('brokers') || '').split(',').filter((url) => /^wss?:\/\//.test(url));
  showRoom(await window.HaloNet.join(secret, brokers));
  history.replaceState(null, '', `?${new URLSearchParams({ ...Object.fromEntries(params), room: secret })}`);
}

window.HaloNet.onChange(showRoom);

$('room-create').addEventListener('click', () => joinRoom(window.HaloNet.newSecret()));
$('room-copy').addEventListener('click', async () => {
  const status = window.HaloNet.status();
  if (!status) return;
  try {
    await navigator.clipboard.writeText(roomLink(status.secret));
    $('room-copy').textContent = 'Copied';
    setTimeout(() => { $('room-copy').textContent = "Copy the room's link"; }, 1500);
  } catch {
    prompt("The room's link:", roomLink(status.secret));
  }
});
$('room-leave').addEventListener('click', () => {
  window.HaloNet.leave();
  params.delete('room');
  history.replaceState(null, '', params.toString() ? `?${params}` : location.pathname);
});

function gameArguments() {
  const args = [];
  // (the game's address in the room: port/web/src/web_net.c)
  if (window.HaloNet.address()) args.push('--HALO_WEB_ADDRESS=' + window.HaloNet.address());
  for (const value of params.getAll('set')) {
    if (/^[A-Z0-9_]+=/.test(value)) args.push('--' + value);
  }
  if (params.has('menu')) args.push('--HALO_MENU_OPEN=' + params.get('menu'));
  // internet play's brokers (port/linux/src/p2p_signal.c), if not its own
  const brokers = (params.get('brokers') || '').split(',').filter((url) => /^wss?:\/\/[^,\s]+$/.test(url));
  if (brokers.length) args.push('--HALO_WEB_BROKERS=' + brokers.join(','));
  // an invite to join (its code, or a whole link)
  const invite = /([0-9a-f]{64})\s*$/i.exec(params.get('join') || '');
  if (invite) args.push('halo://join/' + invite[1].toLowerCase());
  return args;
}

function showFatal(text) {
  gameStopped = true;
  $('fatal-text').textContent = text;
  $('fatal-log').textContent = log.slice(-60).join('\n');
  $('fatal').hidden = false;
  if (document.exitPointerLock) document.exitPointerLock();
}

// a game hosted for the internet: its invite as this page's address with
// ?join= (which native builds take too), and a button that copies it (a
// page may write the clipboard only when it is clicked)
function showInvite(text) {
  const code = /([0-9a-f]{64})/.exec(text);
  if (!code) return;
  $('invite-link').value = `${location.origin}${location.pathname}?join=${code[1]}`;
  $('invite-copy').textContent = 'Copy';
  $('invite').hidden = false;
}

$('invite-copy').addEventListener('click', async () => {
  try {
    await navigator.clipboard.writeText($('invite-link').value);
    $('invite-copy').textContent = 'Copied';
  } catch {
    $('invite-link').select();
  }
});
$('invite-close').addEventListener('click', () => { $('invite').hidden = true; });

// Play here on the Servers panel (servers.js): the server's invite as ?join=,
// and the game starts at once when it can
function joinServer(code) {
  if (!/^[0-9a-f]{64}$/.test(code) || gameStarted) return;
  params.set('join', code);
  history.replaceState(null, '', `?${params}`);
  if (canPlay()) {
    play();
    return;
  }
  location.hash = '#play-panel';
  updatePlay();
  $('play-note').textContent += ' Play then joins the server you chose.';
}

function loadScript(src) {
  return new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = src;
    script.onload = resolve;
    script.onerror = () => reject(new Error('could not load ' + src));
    document.head.append(script);
  });
}

async function play() {
  if (!canPlay()) return;
  if (!(await holdGameLock())) {
    await watchGameElsewhere();
    return;
  }
  gameStarted = true;
  watchFrames();
  $('launcher').hidden = true;
  $('update').hidden = true;
  $('game').hidden = false;
  const canvas = $('canvas');
  canvas.focus();
  try {
    if (navigator.wakeLock) wakeLock = await navigator.wakeLock.request('screen');
  } catch {
    // (a phone may still dim)
  }
  try {
    await prepareInit();
    await loadScript('halo.js');
    const options = {
      canvas,
      arguments: gameArguments(),
      print: (text) => { logLine(text); console.log(text); },
      printErr: (text) => {
        logLine(text);
        console.warn(text);
        if (!$('starting').hidden && /data root:/.test(text)) $('starting-text').textContent = 'Loading the menus…';
      },
      haloNetSend: (address, reliable, bytes) => window.HaloNet.send(address, reliable, bytes),
      // internet play's WebRTC connections (p2p.js)
      haloP2P: (command, connection, bytes) => window.HaloP2P.command(command, connection, bytes),
      haloMessage: (kind, text) => {
        logLine(text);
        if (kind === 2) showInvite(text);
        if (kind === 3) showFatal(text);
        // (the game quits, as the player asked: the page starts again)
        if (kind === 4) location.reload();
      },
      // a question the game waits on (sdl_platform.c's platform_ask: the
      // mods that cannot be played together), answered with a button's index
      haloAsk: (title, text, labels, answer) => {
        logLine(`${title}: ${text}`);
        $('ask-title').textContent = title;
        $('ask-text').textContent = text;
        $('ask-buttons').replaceChildren(...labels.map((label, index) => {
          const button = document.createElement('button');
          button.textContent = label;
          if (index === 0) button.className = 'primary';
          button.addEventListener('click', () => {
            $('ask').hidden = true;
            answer(index);
          });
          return button;
        }));
        // (the dialog is the page's: out of the game's full screen)
        if (document.fullscreenElement) document.exitFullscreen().catch(() => {});
        $('ask').hidden = false;
      },
      onAbort: (what) => showFatal('The game stopped: ' + what),
      onExit: (status) => {
        if (status) showFatal(`The game stopped (status ${status}). The log below says why.`);
        else location.reload();
      },
    };
    // (the module these options become, which p2p.js calls into as soon as
    // the game asks it for a connection)
    window.HaloP2P.attach(options);
    game = await window.createHalo(options);
    // the on-screen touch controls (touch.js), and the room's frames (net.js)
    window.HaloTouch.start(game, $('touch'));
    window.HaloNet.attach(game);
  } catch (error) {
    showFatal(String(error && error.message ? error.message : error));
  }
}

// While the game wants the mouse for the aim (its word in web_shared.h,
// port/web/src), a click on it takes the pointer lock back, from the click
// itself: the game's own request comes from its thread, after the click, and
// the browser refuses that once the player has let the lock go (Escape).
const MOUSE_WANTED_WORD = 48;

$('canvas').addEventListener('pointerdown', (event) => {
  const canvas = event.currentTarget;
  if (event.pointerType !== 'mouse' || !game || !game._web_state || document.pointerLockElement === canvas) return;
  if (!game.HEAPU32[(game._web_state() >> 2) + MOUSE_WANTED_WORD]) return;
  const request = canvas.requestPointerLock();
  // (refused for a moment after Escape: the next click asks again)
  if (request && request.catch) request.catch(() => {});
});

// The game counts the frames it shows in its memory (web_state,
// port/web/src/web_main.c), which the page reads: while the count stands
// still, the game is starting or loading a map, and the page says so.
let game = null;

function watchFrames() {
  let shown = -1;
  let changed = performance.now();
  const timer = setInterval(() => {
    if (gameStopped) {
      clearInterval(timer);
      $('starting').hidden = true;
      return;
    }
    if (!game || !game._web_state) return;
    const now = game.HEAPU32[game._web_state() >> 2];
    if (now !== shown) {
      if (shown < 0 && now === 0) return;
      shown = now;
      changed = performance.now();
      $('starting').hidden = true;
    } else if (performance.now() - changed > 600 && $('starting').hidden) {
      $('starting-text').textContent = 'Loading…';
      $('starting').hidden = false;
    }
  }, 200);
}

$('play').addEventListener('click', play);

$('fullscreen').addEventListener('click', async () => {
  try {
    if (document.fullscreenElement) await document.exitFullscreen();
    else await $('game').requestFullscreen({ navigationUI: 'hide' });
    // (Escape is the game's in fullscreen too, where the browser lets it)
    if (navigator.keyboard && navigator.keyboard.lock && document.fullscreenElement) {
      await navigator.keyboard.lock(['Escape']).catch(() => {});
    }
  } catch (error) {
    logLine('fullscreen: ' + error);
  }
});

$('leave').addEventListener('click', () => {
  if (confirm('Quit the game? Progress since the last checkpoint is lost.')) location.reload();
});

// Tab is the game's (switch weapons, the scoreboard), not the page's focus;
// the key still reaches the game (SDL listens on the window)
window.addEventListener('keydown', (event) => {
  if (gameStarted && !gameStopped && event.key === 'Tab') event.preventDefault();
}, true);

$('canvas').addEventListener('click', () => $('canvas').focus());
$('canvas').addEventListener('contextmenu', (event) => event.preventDefault());

document.addEventListener('visibilitychange', async () => {
  if (gameStarted && !gameStopped && document.visibilityState === 'visible' && navigator.wakeLock && !wakeLock) {
    try {
      wakeLock = await navigator.wakeLock.request('screen');
    } catch {
      // as before
    }
  }
  if (document.visibilityState === 'hidden') wakeLock = null;
});

// ---------- start

(async () => {
  await isolate();
  await loadStamp();
  await watchGameElsewhere();
  checksPassed = await runChecks();
  if (!checksPassed) {
    $('checks-note').textContent = 'Desktop Chrome, Edge and Firefox run the game; Safari 17 or later may.';
    $('checks-note').hidden = false;
  }
  if (results.opfs === 'good') {
    await refreshData();
    await refreshCustomMaps();
    await refreshMods();
    await refreshProfiles();
  } else {
    updatePlay();
  }
  checkForUpdate();
  window.HaloServers.start({
    networkVersion: () => stamp.network_version,
    visible: () => !gameStarted,
    onPlay: joinServer,
  });
  if (/^[0-9a-f]{32}$/.test(params.get('room') || '')) await joinRoom(params.get('room'));
  if (params.has('play')) play();
})();
