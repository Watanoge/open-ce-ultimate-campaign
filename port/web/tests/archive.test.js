// The zips of the page's exports and imports (port/web/site/archive.js), and
// what the importers make of a zip's paths: the tag profiles'
// (tag-profiles-worker.js) and the Custom Edition maps'
// (custom-maps-worker.js). node --test "port/web/tests/*.test.js"

'use strict';

const test = require('node:test');
const assert = require('node:assert');
const zlib = require('node:zlib');
const archive = require('../site/archive.js');
const profiles = require('../site/tag-profiles-worker.js');
const custom = require('../site/custom-maps-worker.js');

async function readAll(stream) {
  const chunks = [];
  for await (const chunk of stream) chunks.push(chunk);
  return Buffer.concat(chunks);
}

function asFile(blob, name) {
  return new File([blob], name);
}

test('a zip made is read back, its files stored', async () => {
  const pack = new Uint8Array(300000).map((_, index) => (index * 7) & 0xFF);
  const blob = await archive.zip([
    { name: 'tag_injector/Brutes/profile.toml', data: new TextEncoder().encode('name = "Brutes"\n') },
    { name: 'tag_injector/Brutes/tags.pack', data: new Blob([pack]) },
    { name: 'tag_injector/Brutes/empty', data: new Uint8Array(0) },
  ]);
  const files = await archive.open(asFile(blob, 'Brutes.zip'));
  assert.deepStrictEqual(files.map((file) => [file.webkitRelativePath, file.size]), [
    ['tag_injector/Brutes/profile.toml', 16],
    ['tag_injector/Brutes/tags.pack', 300000],
    ['tag_injector/Brutes/empty', 0],
  ]);
  assert.deepStrictEqual(new Uint8Array(await readAll(files[1].stream())), pack);
  assert.deepStrictEqual(new Uint8Array(await files[1].slice(1000, 1010).arrayBuffer()), pack.slice(1000, 1010));
  assert.strictEqual((await readAll(files[0].stream())).toString(), 'name = "Brutes"\n');
  assert.strictEqual((await readAll(files[2].stream())).length, 0);
});

// a zip of one file, deflated, as another program makes them (its CRC
// right or wrong), with what macOS's Finder adds
function deflatedZip(name, bytes, crc = archive.crc32(bytes)) {
  const nameBytes = Buffer.from(name);
  const data = zlib.deflateRawSync(bytes);
  const local = Buffer.alloc(30);
  local.writeUInt32LE(0x04034b50, 0);
  local.writeUInt16LE(20, 4);
  local.writeUInt16LE(8, 8);
  local.writeUInt32LE(crc, 14);
  local.writeUInt32LE(data.length, 18);
  local.writeUInt32LE(bytes.length, 22);
  local.writeUInt16LE(nameBytes.length, 26);
  const central = Buffer.alloc(46);
  central.writeUInt32LE(0x02014b50, 0);
  central.writeUInt16LE(20, 6);
  central.writeUInt16LE(8, 10);
  central.writeUInt32LE(crc, 16);
  central.writeUInt32LE(data.length, 20);
  central.writeUInt32LE(bytes.length, 24);
  central.writeUInt16LE(nameBytes.length, 28);
  const noise = Buffer.from('__MACOSX/');
  const folder = Buffer.alloc(46);
  folder.writeUInt32LE(0x02014b50, 0);
  folder.writeUInt16LE(noise.length, 28);
  const directory = Buffer.concat([central, nameBytes, folder, noise]);
  const offset = local.length + nameBytes.length + data.length;
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(2, 8);
  end.writeUInt16LE(2, 10);
  end.writeUInt32LE(directory.length, 12);
  end.writeUInt32LE(offset, 16);
  return new File([local, nameBytes, data, directory, end], 'maps.zip');
}

test('a deflated file is inflated as it is read, and its CRC checked', async () => {
  const bytes = Buffer.alloc(200000, 'halo');
  const [file, ...rest] = await archive.open(deflatedZip('Firefight\\relic.map', bytes));
  assert.strictEqual(rest.length, 0);
  assert.strictEqual(file.webkitRelativePath, 'Firefight/relic.map');
  assert.deepStrictEqual(await readAll(file.stream()), bytes);
  assert.deepStrictEqual(Buffer.from(await file.slice(4, 12).arrayBuffer()), bytes.subarray(4, 12));
  const [damaged] = await archive.open(deflatedZip('a.map', bytes, 1234));
  await assert.rejects(readAll(damaged.stream()), /damaged/);
});

test('not a zip', async () => {
  await assert.rejects(archive.open(new File([Buffer.alloc(100)], 'x.zip')), /not a zip/);
});

test('more than 65535 files: a zip64 end, read back', async () => {
  const entries = Array.from({ length: 70000 }, (_, index) => ({ name: `f${index}`, data: new Uint8Array([index & 0xFF]) }));
  const files = await archive.open(asFile(await archive.zip(entries), 'many.zip'));
  assert.strictEqual(files.length, 70000);
  assert.strictEqual(files[69999].webkitRelativePath, 'f69999');
  assert.deepStrictEqual([...(await readAll(files[69999].stream()))], [69999 & 0xFF]);
});

const named = (path) => ({ path, file: { size: 1 } });

test('tag profiles: an export, a profile\'s folder, a zip of its files', () => {
  const found = profiles.profileFiles([
    named('tag_injector/Brutes/profile.toml'),
    named('tag_injector/Brutes/carbine.weap.tagref'),
    named('tag_injector/Brutes/tags.pack'),
    named('tag_injector/Brutes/sub/x'),
    named('tag_injector/multiplayer/Shotgun Madness.toml'),
    named('tag_injector/.campaign/Brute Invasion.toml'),
    named('tag_injector/.shared/host/profile.toml'),
    named('tag_injector/Empty/readme.txt'),
    named('tag_injector/Bad$Name/profile.toml'),
  ]);
  assert.deepStrictEqual(found.profiles.map((profile) => [profile.folder, profile.files.map((file) => file.name)]),
    [['Brutes', ['profile.toml', 'carbine.weap.tagref', 'tags.pack']]]);
  assert.deepStrictEqual(found.others.map((other) => `${other.kind}/${other.name}`),
    ['multiplayer/Shotgun Madness.toml', 'campaign/Brute Invasion.toml']);
  assert.deepStrictEqual(found.skipped.map((entry) => entry.path), ['Empty', 'Bad$Name']);

  assert.deepStrictEqual(profiles.profileFiles([named('Brutes/profile.toml')]).profiles.map((p) => p.folder), ['Brutes']);
  assert.deepStrictEqual(profiles.profileFiles([named('profile.toml'), named('tags.pack')], 'Elites (2).zip')
    .profiles.map((p) => [p.folder, p.files.length]), [['Elites 2', 2]]);
  assert.throws(() => profiles.profileFiles([named('maps/a10.map')]), /no tag profiles/);
});

test('tag profiles: the names the game keeps', () => {
  assert.ok(profiles.plainName('Brute Carbine.weap.tagref', 127));
  assert.ok(!profiles.plainName('.hidden', 127));
  assert.ok(!profiles.plainName('..', 127));
  assert.ok(!profiles.plainName('trailing ', 127));
  assert.ok(!profiles.plainName('a/b', 127));
  assert.ok(!profiles.plainName('x'.repeat(64), 63));
  assert.strictEqual(profiles.tomlString('name = "Shot\\"gun"\nprofile = "Brutes"\n', 'profile'), 'Brutes');
  assert.strictEqual(profiles.tomlString('name = "Shot\\"gun"\n', 'name'), 'Shot"gun');
});

test('a zip of maps: put under custom_maps, its folder a category or custom_maps itself', () => {
  const paths = (list) => custom.zipFolderFiles(list.map((path) => ({ webkitRelativePath: path })))
    .map((file) => file.webkitRelativePath);
  assert.deepStrictEqual(paths(['custom_maps/a.map', 'custom_maps/Firefight/b.map']),
    ['custom_maps/a.map', 'custom_maps/Firefight/b.map']);
  assert.deepStrictEqual(paths(['a.map', 'Firefight/b.map']), ['custom_maps/a.map', 'custom_maps/Firefight/b.map']);
  assert.deepStrictEqual(paths(['Firefight/a.map', 'Firefight/b.map']),
    ['custom_maps/Firefight/a.map', 'custom_maps/Firefight/b.map']);
  assert.deepStrictEqual(paths(['ce/bitmaps.map', 'ce/a.map']), ['custom_maps/bitmaps.map', 'custom_maps/a.map']);
  assert.deepStrictEqual(paths(['ce/a.map', 'ce/Firefight/b.map']), ['custom_maps/a.map', 'custom_maps/Firefight/b.map']);
});
