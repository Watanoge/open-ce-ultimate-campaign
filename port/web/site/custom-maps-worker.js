/*
CUSTOM-MAPS-WORKER.JS

Copies Halo Custom Edition maps into the site's Origin Private File System,
where the game finds them (/data/custom_maps: d:\custom_maps\ in
port/linux/game/custom_edition_maps.c, docs/custom_edition_caches.md), from
a custom_maps folder the player chose: its maps, their pictures (.bmp) and
descriptions (.txt), and Custom Edition's bitmaps.map, sounds.map and
loc.map, in the folder itself and in one level of subfolders, as the game
lists them (each subfolder a category), or from maps chosen alone and zips
(archive.js) of them or of a custom_maps folder, as its export makes. It
runs in a Worker, which can write with synchronous OPFS access handles. The
files are added to those already there, replacing those of the same names,
so that the maps map sharing downloaded stay. It exports them too: a zip of
custom_maps, whose paths start with custom_maps/.

The folder is the player's, so its paths are checked as the mods importer
checks them (mods-worker.js): no part of one empty, hidden or with a
separator in it. Each map's header is checked as the game checks it first: a
Custom Edition cache (head and foot signatures, version 609), or a resource
map named for its kind (its first word 1, 2 or 3). Other maps are left out
and named in the result. The game cannot run a Custom Edition map without
the three resource maps, so a folder that brings none of them when the
browser has none either is refused.

Messages from the page: { type: 'add', files } (the chosen folder's File
objects, each with its webkitRelativePath, or the files chosen: maps and
zips), { type: 'remove' } (all of custom_maps) or { type: 'export' }. To
the page: { type: 'progress', file, done, total, verb }, then { type: 'done',
files, bytes, skipped, blob (an export's zip) } or { type: 'error',
message }.
*/

'use strict';

if (typeof importScripts === 'function') importScripts('archive.js');

const FOLDER = 'custom_maps';
// (custom_edition_maps.h's CUSTOM_EDITION_MAP_CATEGORY_LENGTH, with its
// terminator, and CUSTOM_EDITION_MAPS_MAXIMUM's maps with a picture and a
// description each)
const MAXIMUM_CATEGORY_LENGTH = 63;
const MAXIMUM_FILES = 3 * 8192;
const MAP_HEADER_SIZE = 0x800;
const CUSTOM_EDITION_CACHE_VERSION = 609;
const XBOX_CACHE_VERSION = 5;
// a resource map's kind, its first word, by its name
const RESOURCE_MAPS = { 'bitmaps.map': 1, 'sounds.map': 2, 'loc.map': 3 };
const KEPT_EXTENSIONS = ['.map', '.bmp', '.txt'];

function asciiLower(text) {
  return text.replace(/[A-Z]/g, (letter) => String.fromCharCode(letter.charCodeAt(0) + 32));
}

// a part of a path a file may have: not hidden, no separator
function plainPart(part) {
  return part.length > 0 && part.length <= 255 && !part.startsWith('.') && !/[\\/:\u0000-\u001f]/.test(part);
}

function latin1(bytes) {
  let text = '';
  for (const byte of bytes) text += String.fromCharCode(byte);
  return text;
}

// Why the game would not take a map whose name and first bytes these are,
// or null.
function mapProblem(name, header) {
  const view = new DataView(header.buffer, header.byteOffset, header.byteLength);
  const kind = RESOURCE_MAPS[asciiLower(name)];
  if (kind !== undefined) {
    if (header.length < 16) return 'it is too short';
    if (view.getUint32(0, true) !== kind) return `it is not Custom Edition's ${asciiLower(name)}`;
    return null;
  }
  if (header.length < MAP_HEADER_SIZE) return 'it is too short';
  if (latin1(header.subarray(0, 4)) !== 'daeh' || latin1(header.subarray(0x7FC, 0x800)) !== 'toof') {
    return 'it is not a map';
  }
  const version = view.getUint32(4, true);
  if (version === XBOX_CACHE_VERSION) return 'it is an Xbox map (those go in the maps folder: Game data)';
  if (version !== CUSTOM_EDITION_CACHE_VERSION) return `it is not a Custom Edition map (its cache version is ${version})`;
  return null;
}

// A zip's files as a custom_maps folder's: its paths put under custom_maps/.
// A zip of one folder is of custom_maps itself when the folder is named so,
// has the resource maps or has subfolders; else the folder is a category.
function zipFolderFiles(files) {
  const split = files.map((file) => file.webkitRelativePath.split('/'));
  const tops = new Set(split.map((parts) => (parts.length > 1 ? parts[0] : null)));
  let root = false;
  if (tops.size === 1 && !tops.has(null)) {
    const top = [...tops][0];
    root = asciiLower(top) === FOLDER || split.some((parts) => parts.length > 2) ||
      split.some((parts) => parts.length === 2 && RESOURCE_MAPS[asciiLower(parts[1])] !== undefined);
  }
  files.forEach((file, index) => {
    file.webkitRelativePath = `${FOLDER}/${(root ? split[index].slice(1) : split[index]).join('/')}`;
  });
  return files;
}

// the files chosen, each zip among those chosen alone as its files
async function expandZips(list) {
  const files = [];
  for (const file of list) {
    if (!file.webkitRelativePath && HaloArchive.isZip(file)) files.push(...zipFolderFiles(await HaloArchive.open(file)));
    else files.push(file);
  }
  return files;
}

// The files of a chosen folder to copy: [{ parts, file }], parts the path
// under custom_maps (a name, or a subfolder and a name), and the paths left
// out, with why. The chosen folder's own name is not kept: custom_maps is
// the game's.
function customMapFiles(list) {
  const files = [];
  const skipped = [];
  let top = null;
  for (const file of list) {
    const parts = (file.webkitRelativePath || file.name).split('/');
    // (a file chosen alone has no folder: it goes in custom_maps itself)
    const below = parts.length > 1 ? parts.slice(1) : parts;
    if (parts.length > 1) {
      if (top === null) top = parts[0];
      if (parts[0] !== top) throw new Error('Choose one custom_maps folder.');
    }
    if (!below.every(plainPart)) continue;
    const name = below[below.length - 1];
    const lower = asciiLower(name);
    if (!KEPT_EXTENSIONS.some((extension) => lower.endsWith(extension))) continue;
    if (below.length > 2) {
      skipped.push({ path: below.join('/'), why: 'the game lists a folder and its subfolders, not theirs' });
      continue;
    }
    if (below.length === 2 && new TextEncoder().encode(below[0]).length > MAXIMUM_CATEGORY_LENGTH) {
      skipped.push({ path: below.join('/'), why: `its folder's name is longer than ${MAXIMUM_CATEGORY_LENGTH} bytes` });
      continue;
    }
    // (the first of names that differ only in case)
    const key = asciiLower(below.join('/'));
    if (files.some((entry) => asciiLower(entry.parts.join('/')) === key)) continue;
    files.push({ parts: below, file });
  }
  if (!files.length && !skipped.length) throw new Error('The folder has no maps.');
  if (files.length > MAXIMUM_FILES) throw new Error('The folder has more files than the game lists.');
  return { files, skipped };
}

async function readBytes(file, start, end) {
  return new Uint8Array(await file.slice(start, end).arrayBuffer());
}

// The files whose maps the game would take, and the rest added to skipped.
async function checkedFiles(found) {
  const kept = [];
  for (const entry of found.files) {
    const name = entry.parts[entry.parts.length - 1];
    if (asciiLower(name).endsWith('.map')) {
      // (the resource maps are Custom Edition's own, in custom_maps itself)
      if (entry.parts.length > 1 && RESOURCE_MAPS[asciiLower(name)] !== undefined) {
        found.skipped.push({ path: entry.parts.join('/'), why: 'the game reads it only in custom_maps itself' });
        continue;
      }
      const problem = mapProblem(name, await readBytes(entry.file, 0, MAP_HEADER_SIZE));
      if (problem) {
        found.skipped.push({ path: entry.parts.join('/'), why: problem });
        continue;
      }
    }
    kept.push(entry);
  }
  return kept;
}

// the resource maps a list of files under custom_maps names
function resourceMapsAmong(names) {
  const lower = names.map(asciiLower);
  return Object.keys(RESOURCE_MAPS).filter((name) => lower.includes(name));
}

async function storedResourceMaps(folder) {
  const names = [];
  for await (const [name, handle] of folder.entries()) {
    if (handle.kind === 'file') names.push(name);
  }
  return resourceMapsAmong(names);
}

async function directoryOf(root, parts) {
  let directory = root;
  for (const part of parts) directory = await directory.getDirectoryHandle(part, { create: true });
  return directory;
}

async function writeFile(directory, name, file, onChunk) {
  const handle = await directory.getFileHandle(name, { create: true });
  const access = await handle.createSyncAccessHandle();
  let complete = false;
  try {
    access.truncate(0);
    // (a stream: a zip's deflated file is read once, from its start)
    const reader = file.stream().getReader();
    let position = 0;
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      let written = 0;
      while (written < value.length) {
        const result = access.write(value.subarray(written), { at: position + written });
        if (!result) throw new Error(`Could not write ${file.name}: is the device full?`);
        written += result;
      }
      position += value.length;
      onChunk(value.length);
    }
    if (position !== file.size) throw new Error(`Could not read ${file.name}.`);
    access.flush();
    complete = true;
  } finally {
    access.close();
    // (no half of a map: the game would list it and fail to load it)
    if (!complete) await directory.removeEntry(name).catch(() => {});
  }
}

async function add(list) {
  const found = customMapFiles(await expandZips(list));
  const files = await checkedFiles(found);
  const storage = await navigator.storage.getDirectory();
  const folder = await storage.getDirectoryHandle(FOLDER, { create: true });
  const brought = resourceMapsAmong(files.filter((entry) => entry.parts.length === 1).map((entry) => entry.parts[0]));
  const stored = await storedResourceMaps(folder);
  const absent = Object.keys(RESOURCE_MAPS).filter((name) => !brought.includes(name) && !stored.includes(name));
  const levels = files.filter((entry) => {
    const name = asciiLower(entry.parts[entry.parts.length - 1]);
    return name.endsWith('.map') && RESOURCE_MAPS[name] === undefined;
  });
  if (levels.length && absent.length) {
    throw new Error(`The folder has no ${absent.join(', ')}: Custom Edition maps need bitmaps.map, sounds.map ` +
      'and loc.map from Halo Custom Edition\'s maps folder, in the custom_maps folder.');
  }
  if (!files.length) {
    const why = found.skipped.slice(0, 3).map((entry) => `${entry.path}: ${entry.why}`).join('; ');
    throw new Error(`None of the folder's maps can be played (${why}).`);
  }
  const total = files.reduce((sum, entry) => sum + entry.file.size, 0);
  if (navigator.storage.estimate) {
    const { quota, usage } = await navigator.storage.estimate();
    if (quota && quota - usage < total) {
      throw new Error(`The browser lets this site keep ${Math.floor((quota - usage) / 1e6)} MB more, ` +
        `and the maps take ${Math.ceil(total / 1e6)} MB.`);
    }
  }
  let done = 0;
  let lastReport = 0;
  for (const entry of files) {
    const directory = await directoryOf(folder, entry.parts.slice(0, -1));
    await writeFile(directory, entry.parts[entry.parts.length - 1], entry.file, (count) => {
      done += count;
      const now = Date.now();
      if (now - lastReport > 100 || done === total) {
        lastReport = now;
        postMessage({ type: 'progress', file: entry.parts.join('/'), done, total });
      }
    });
  }
  return { files: files.length, bytes: total, skipped: found.skipped };
}

async function remove() {
  const storage = await navigator.storage.getDirectory();
  await storage.removeEntry(FOLDER, { recursive: true }).catch(() => {});
  return { files: 0, bytes: 0, skipped: [] };
}

// custom_maps as a zip: the folder's maps, pictures and descriptions and its
// subfolders', as the game lists them
async function exportMaps() {
  const storage = await navigator.storage.getDirectory();
  const entries = [];
  const collect = async (directory, prefix, top) => {
    for await (const [name, handle] of directory.entries()) {
      if (!plainPart(name)) continue;
      if (handle.kind === 'directory') {
        if (top) await collect(handle, `${prefix}${name}/`, false);
      } else if (KEPT_EXTENSIONS.some((extension) => asciiLower(name).endsWith(extension))) {
        entries.push({ name: prefix + name, data: await handle.getFile() });
      }
    }
  };
  try {
    await collect(await storage.getDirectoryHandle(FOLDER), `${FOLDER}/`, true);
  } catch {
    // none yet
  }
  if (!entries.length) throw new Error('There are no Custom Edition maps to export.');
  let lastReport = 0;
  const blob = await HaloArchive.zip(entries, (file, done, total) => {
    const now = Date.now();
    if (now - lastReport > 100 || done === total) {
      lastReport = now;
      postMessage({ type: 'progress', file, done, total, verb: 'Packing' });
    }
  });
  const bytes = entries.reduce((sum, entry) => sum + entry.data.size, 0);
  return { files: entries.length, bytes, skipped: [], blob };
}

if (typeof onmessage !== 'undefined' || typeof WorkerGlobalScope !== 'undefined') {
  onmessage = async (event) => {
    try {
      const type = event.data.type;
      const result = type === 'remove' ? await remove() : type === 'export' ? await exportMaps() :
        await add(event.data.files);
      postMessage({ type: 'done', ...result });
    } catch (error) {
      postMessage({ type: 'error', message: error && error.message ? error.message : String(error) });
    }
  };
}

// (for the tests: port/web/tests)
if (typeof module !== 'undefined') {
  module.exports = { customMapFiles, checkedFiles, mapProblem, resourceMapsAmong, plainPart, zipFolderFiles };
}
