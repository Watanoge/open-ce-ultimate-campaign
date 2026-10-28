/*
TAG-PROFILES-WORKER.JS

Exports the Tag Injector's profiles out of the site's Origin Private File
System, and imports them into it (/data/tag_injector: port_tag_profiles.c,
port_tag_mp_profiles.c, docs/tag_injector.md "Profiles"). A profile is a
folder of tag_injector, with its profile.toml, its tags' files and its pack,
and is played without the maps its tags came from, so it is copied whole;
the multiplayer and campaign profiles are files of tag_injector/multiplayer
and tag_injector/campaign, each naming the folder of the profile it plays
(an earlier build's .multiplayer and .campaign are moved there first, as the
game moves them: port_tag_mp_profiles.c's folder_move).
The shares downloaded in network games (tag_injector/.shared) are not
exported: each was copied into the profiles as "<name> (online)" already.

An export is a zip (archive.js) whose paths start with tag_injector/: a
profile's folder, and the multiplayer and campaign profiles that play it (all
of them, for all the profiles). An import takes such a zip, a zip of a
profile's folder or of its files, or a chosen folder (tag_injector, or a
profile's), and copies the profiles in it and the multiplayer and campaign
profiles beside them. The paths are the player's, so they are checked as the
game names its files (port_tag_profiles.c's file_name_clean: letters, digits,
spaces, dots, - and _); a folder without profile.toml is not a profile. A
profile of the same folder's name (whatever its case, as the game compares
them) is replaced whole, and so is a multiplayer or campaign profile of the
same file, only once the page has asked (replace).

Messages from the page: { type: 'export', folder } (one profile's folder, or
null for all), or { type: 'import', files, replace }. To the page:
{ type: 'progress', file, done, total, verb }, then { type: 'done', blob,
name } (an export), { type: 'done', profiles, others, files, bytes } (an
import), { type: 'conflicts', profiles, others } (an import that would replace
those, not yet copied), or { type: 'error', message }.
*/

'use strict';

if (typeof importScripts === 'function') importScripts('archive.js');

const FOLDER = 'tag_injector';
// the folders of the multiplayer and campaign profiles, and an earlier
// build's (which a zip of one may have)
const KIND_FOLDERS = ['multiplayer', 'campaign'];
const OLD_KIND_FOLDERS = { multiplayer: '.multiplayer', campaign: '.campaign' };
// (port_tag_profiles.c's MAXIMUM_TAG_PROFILES, MAXIMUM_FOLDER_LENGTH and
// MAXIMUM_FILE_NAME_LENGTH, with their terminators; port_tag_mp_profiles.c's
// MAXIMUM_MP_PROFILES and MAXIMUM_FILE_LENGTH, without .toml; and
// MAXIMUM_PROFILE_TAGS's tags, with a few more files)
const MAXIMUM_PROFILES = 64;
const MAXIMUM_FOLDER_LENGTH = 63;
const MAXIMUM_FILE_LENGTH = 127;
const MAXIMUM_KIND_PROFILES = 64;
const MAXIMUM_KIND_FILE_LENGTH = 63;
const MAXIMUM_PROFILE_FILES = 256 + 16;

function asciiLower(text) {
  return text.replace(/[A-Z]/g, (letter) => String.fromCharCode(letter.charCodeAt(0) + 32));
}

// a name the game keeps as it is: file_name_clean leaves it so
function plainName(name, maximum) {
  return /^[A-Za-z0-9_-][A-Za-z0-9 ._-]*$/.test(name) && !/[ .]$/.test(name) && name.length <= maximum;
}

// what the game calls a profile: profile.toml's name, else its folder's
function tomlString(text, key) {
  const match = new RegExp(`^\\s*${key}\\s*=\\s*"((?:[^"\\\\\\n]|\\\\.)*)"`, 'm').exec(text);
  return match ? match[1].replace(/\\(.)/g, '$1') : null;
}

// The profiles of a chosen folder's or a zip's files ([{ path, file }],
// path '/'-separated), and what of them is left out:
// { profiles: [{ folder, files: [{ name, file }] }],
//   others: [{ kind, name, file }], skipped: [{ path, why }] }.
// zipName names a profile whose files are a zip's own, at its top.
function profileFiles(list, zipName) {
  let paths = list.map(({ path, file }) => ({ parts: path.split('/').filter((part) => part !== ''), file }));
  // (a zip or a folder of tag_injector: what is in it)
  if (paths.length && paths.every(({ parts }) => parts.length > 1 && asciiLower(parts[0]) === FOLDER)) {
    paths = paths.map(({ parts, file }) => ({ parts: parts.slice(1), file }));
  }
  // (a zip of a profile's files: its name the zip's)
  if (paths.some(({ parts }) => parts.length === 1 && parts[0] === 'profile.toml')) {
    const folder = (zipName || 'Imported').replace(/\.zip$/i, '').replace(/[^A-Za-z0-9 ._-]/g, '').trim();
    paths = paths.map(({ parts, file }) => ({ parts: parts.length === 1 ? [folder, ...parts] : parts, file }));
  }
  const groups = new Map();
  const skipped = [];
  for (const entry of paths) {
    if (entry.parts.length < 2) continue;
    const top = entry.parts[0];
    if (!groups.has(top)) groups.set(top, []);
    groups.get(top).push(entry);
  }
  const profiles = [];
  const others = [];
  for (const [top, entries] of groups) {
    const kind = KIND_FOLDERS.find((folder) => asciiLower(folder) === asciiLower(top) ||
      OLD_KIND_FOLDERS[folder] === asciiLower(top));
    if (kind) {
      for (const { parts, file } of entries) {
        const name = parts[1];
        const base = name.replace(/\.toml$/i, '');
        if (parts.length !== 2 || !/\.toml$/i.test(name)) continue;
        if (!plainName(base, MAXIMUM_KIND_FILE_LENGTH)) {
          skipped.push({ path: parts.join('/'), why: 'the game would not name a profile so' });
          continue;
        }
        others.push({ kind, name: base + '.toml', file });
      }
      continue;
    }
    if (top.startsWith('.') || KIND_FOLDERS.includes(asciiLower(top))) continue;
    if (!entries.some(({ parts }) => parts.length === 2 && parts[1] === 'profile.toml')) {
      skipped.push({ path: top, why: 'it has no profile.toml: it is not a profile' });
      continue;
    }
    if (!plainName(top, MAXIMUM_FOLDER_LENGTH)) {
      skipped.push({ path: top, why: 'the game would not name a profile\'s folder so' });
      continue;
    }
    const files = [];
    for (const { parts, file } of entries) {
      // (a profile's files are beside its profile.toml, never in a folder)
      if (parts.length !== 2) continue;
      if (!plainName(parts[1], MAXIMUM_FILE_LENGTH)) {
        skipped.push({ path: parts.join('/'), why: 'the game would not name a profile\'s file so' });
        continue;
      }
      if (files.some(({ name }) => asciiLower(name) === asciiLower(parts[1]))) continue;
      files.push({ name: parts[1], file });
    }
    if (files.length > MAXIMUM_PROFILE_FILES) {
      skipped.push({ path: top, why: 'it has more files than a profile has' });
      continue;
    }
    if (profiles.some(({ folder }) => asciiLower(folder) === asciiLower(top))) continue;
    profiles.push({ folder: top, files });
  }
  if (!profiles.length && !others.length) {
    const why = skipped.length ? ` (${skipped.slice(0, 3).map((entry) => `${entry.path}: ${entry.why}`).join('; ')})` : '';
    throw new Error(`There are no tag profiles in what was chosen${why}.`);
  }
  return { profiles, others, skipped };
}

// the files chosen as [{ path, file }]: a zip's files, or a folder's; and
// the name of a zip chosen
async function chosenFiles(list) {
  const files = [];
  let zipName = null;
  for (const file of list) {
    if (!file.webkitRelativePath && HaloArchive.isZip(file)) {
      zipName = zipName || file.name;
      for (const inner of await HaloArchive.open(file)) files.push({ path: inner.webkitRelativePath, file: inner });
    } else {
      files.push({ path: file.webkitRelativePath || file.name, file });
    }
  }
  return { files, zipName };
}

async function directoryNamed(parent, name) {
  try {
    return await parent.getDirectoryHandle(name);
  } catch {
    return null;
  }
}

// the names of a folder's folders or files, by their lower case
async function namesIn(directory, kind) {
  const names = new Map();
  if (!directory) return names;
  for await (const [name, handle] of directory.entries()) {
    if (handle.kind === kind) names.set(asciiLower(name), name);
  }
  return names;
}

async function writeFile(directory, name, file, onChunk) {
  const handle = await directory.getFileHandle(name, { create: true });
  const access = await handle.createSyncAccessHandle();
  try {
    access.truncate(0);
    const reader = file.stream().getReader();
    let position = 0;
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      let written = 0;
      while (written < value.length) {
        const result = access.write(value.subarray(written), { at: position + written });
        if (!result) throw new Error(`Could not write ${name}: is the device full?`);
        written += result;
      }
      position += value.length;
      onChunk(value.length);
    }
    if (position !== file.size) throw new Error(`Could not read ${name}.`);
    access.flush();
  } finally {
    access.close();
  }
}

// an earlier build's .multiplayer and .campaign: their files moved to
// multiplayer and campaign, if those are not there yet (as the game does)
async function moveOldKinds(root) {
  const folders = await namesIn(root, 'directory');
  for (const kind of KIND_FOLDERS) {
    const old = await directoryNamed(root, OLD_KIND_FOLDERS[kind]);
    if (!old || folders.has(kind)) continue;
    const directory = await root.getDirectoryHandle(kind, { create: true });
    let left = 0;
    for await (const [name, handle] of old.entries()) {
      if (handle.kind !== 'file') {
        left++;
        continue;
      }
      try {
        const file = await handle.getFile();
        const access = await (await directory.getFileHandle(name, { create: true })).createSyncAccessHandle();
        try {
          access.truncate(0);
          access.write(new Uint8Array(await file.arrayBuffer()), { at: 0 });
          access.flush();
        } finally {
          access.close();
        }
        await old.removeEntry(name);
      } catch {
        left++;
      }
    }
    if (!left) await root.removeEntry(OLD_KIND_FOLDERS[kind], { recursive: true }).catch(() => {});
  }
}

async function importProfiles(list, replace) {
  const { files, zipName } = await chosenFiles(list);
  const found = profileFiles(files, zipName);
  const storage = await navigator.storage.getDirectory();
  const root = await storage.getDirectoryHandle(FOLDER, { create: true });
  await moveOldKinds(root);
  const folders = await namesIn(root, 'directory');
  const kinds = {};
  for (const kind of KIND_FOLDERS) kinds[kind] = await namesIn(await directoryNamed(root, kind), 'file');
  const replaced = found.profiles.filter(({ folder }) => folders.has(asciiLower(folder))).map(({ folder }) => folder);
  const replacedOthers = found.others.filter(({ kind, name }) => kinds[kind].has(asciiLower(name)))
    .map(({ kind, name }) => `${kind}/${name}`);
  if (!replace && (replaced.length || replacedOthers.length)) {
    return { type: 'conflicts', profiles: replaced, others: replacedOthers };
  }
  const existing = [...folders.keys()].filter((name) => !name.startsWith('.')).length;
  if (existing + found.profiles.length - replaced.length > MAXIMUM_PROFILES) {
    throw new Error(`The game lists ${MAXIMUM_PROFILES} profiles at most, and this browser has ${existing}.`);
  }
  for (const kind of KIND_FOLDERS) {
    const adding = found.others.filter((other) => other.kind === kind && !kinds[kind].has(asciiLower(other.name)));
    if (kinds[kind].size + adding.length > MAXIMUM_KIND_PROFILES) {
      throw new Error(`The game lists ${MAXIMUM_KIND_PROFILES} ${kind} profiles at most.`);
    }
  }

  const all = [...found.profiles.flatMap((profile) => profile.files), ...found.others];
  const total = all.reduce((sum, { file }) => sum + file.size, 0);
  if (navigator.storage.estimate) {
    const { quota, usage } = await navigator.storage.estimate();
    if (quota && quota - usage < total) {
      throw new Error(`The browser lets this site keep ${Math.floor((quota - usage) / 1e6)} MB more, ` +
        `and the profiles take ${Math.ceil(total / 1e6)} MB.`);
    }
  }
  let done = 0;
  let lastReport = 0;
  const progress = (file) => (count) => {
    done += count;
    const now = Date.now();
    if (now - lastReport > 100 || done === total) {
      lastReport = now;
      postMessage({ type: 'progress', file, done, total, verb: 'Copying' });
    }
  };
  for (const profile of found.profiles) {
    // (replaced whole, under the name it came with)
    const old = folders.get(asciiLower(profile.folder));
    if (old) await root.removeEntry(old, { recursive: true });
    const directory = await root.getDirectoryHandle(profile.folder, { create: true });
    try {
      for (const { name, file } of profile.files) {
        await writeFile(directory, name, file, progress(`${profile.folder}/${name}`));
      }
    } catch (error) {
      // (no half of a profile: the game would play what was copied)
      await root.removeEntry(profile.folder, { recursive: true }).catch(() => {});
      throw error;
    }
  }
  for (const { kind, name, file } of found.others) {
    const directory = await root.getDirectoryHandle(kind, { create: true });
    const old = kinds[kind].get(asciiLower(name));
    if (old && old !== name) await directory.removeEntry(old).catch(() => {});
    try {
      await writeFile(directory, name, file, progress(`${kind}/${name}`));
    } catch (error) {
      await directory.removeEntry(name).catch(() => {});
      throw error;
    }
  }
  return {
    type: 'done',
    profiles: found.profiles.map(({ folder }) => folder),
    others: found.others.map(({ kind, name }) => `${kind}/${name}`),
    skipped: found.skipped,
    files: all.length,
    bytes: total,
  };
}

// the multiplayer and campaign profiles that play a profile's folder (all,
// for null)
async function kindEntries(root, folder) {
  const entries = [];
  for (const kind of KIND_FOLDERS) {
    const directory = await directoryNamed(root, kind);
    if (!directory) continue;
    for await (const [name, handle] of directory.entries()) {
      if (handle.kind !== 'file' || !/\.toml$/i.test(name) || name.startsWith('.')) continue;
      const file = await handle.getFile();
      if (folder !== null && asciiLower(tomlString(await file.text(), 'profile') || '') !== asciiLower(folder)) continue;
      entries.push({ name: `${FOLDER}/${kind}/${name}`, data: file });
    }
  }
  return entries;
}

async function exportProfiles(folder) {
  const storage = await navigator.storage.getDirectory();
  const root = await directoryNamed(storage, FOLDER);
  if (!root) throw new Error('There are no tag profiles yet.');
  await moveOldKinds(root);
  const entries = [];
  let name = 'opence-tag-profiles.zip';
  for await (const [profile, handle] of root.entries()) {
    if (handle.kind !== 'directory' || profile.startsWith('.') || KIND_FOLDERS.includes(asciiLower(profile))) continue;
    if (folder !== null && profile !== folder) continue;
    const files = [];
    for await (const [file, fileHandle] of handle.entries()) {
      if (fileHandle.kind === 'file' && !file.startsWith('.')) {
        files.push({ name: `${FOLDER}/${profile}/${file}`, data: await fileHandle.getFile() });
      }
    }
    if (!files.some((entry) => entry.name.endsWith('/profile.toml'))) continue;
    entries.push(...files);
    if (folder !== null) name = `${profile}.zip`;
  }
  if (!entries.length) throw new Error(folder === null ? 'There are no tag profiles yet.' : `There is no profile ${folder}.`);
  entries.push(...await kindEntries(root, folder));
  let lastReport = 0;
  const blob = await HaloArchive.zip(entries, (file, done, total) => {
    const now = Date.now();
    if (now - lastReport > 100 || done === total) {
      lastReport = now;
      postMessage({ type: 'progress', file, done, total, verb: 'Packing' });
    }
  });
  return { type: 'done', blob, name };
}

if (typeof onmessage !== 'undefined' || typeof WorkerGlobalScope !== 'undefined') {
  onmessage = async (event) => {
    try {
      const data = event.data;
      postMessage(data.type === 'export' ? await exportProfiles(data.folder ?? null) :
        await importProfiles(data.files, !!data.replace));
    } catch (error) {
      postMessage({ type: 'error', message: error && error.message ? error.message : String(error) });
    }
  };
}

// (for the tests: port/web/tests)
if (typeof module !== 'undefined') {
  module.exports = { profileFiles, plainName, tomlString };
}
