/*
ARCHIVE.JS

Zip files, out of the site's storage and into it: the page's exports (the
saved games, the tag profiles, the Custom Edition maps) are zips this makes,
and its imports take zips as well as folders. It is loaded by the page
(index.html) and by the workers that copy files (importScripts), as
self.HaloArchive.

- zip(entries, onProgress) makes a zip of { name, data } (data a Blob or a
  Uint8Array), its files stored as they are (the maps and the profiles' packs
  are compressed already). A Blob is read only for its CRC: the zip is a Blob
  of the entries' own Blobs, so an export of gigabytes of maps does not hold
  them in memory. Past 4 GB, or 65535 files, it is a zip64.
- open(file) reads a zip's directory, and gives its files as File-like
  objects: { name, webkitRelativePath (its path in the zip), size,
  slice(start, end), stream() }, as a chosen folder's files are, so that the
  importers take either. A file stored is a slice of the zip; a file
  deflated is inflated as it is read (DecompressionStream). stream() checks
  the file's CRC and size as it ends; a zip's paths are the player's, so
  the importers check them as a folder's.
*/

'use strict';

(function (scope) {
  const LOCAL_SIGNATURE = 0x04034b50;
  const CENTRAL_SIGNATURE = 0x02014b50;
  const END_SIGNATURE = 0x06054b50;
  const END64_SIGNATURE = 0x06064b50;
  const LOCATOR64_SIGNATURE = 0x07064b50;
  const ZIP64_EXTRA = 0x0001;
  const MAXIMUM_32 = 0xFFFFFFFF;
  const MAXIMUM_16 = 0xFFFF;
  const UTF8_FLAG = 0x0800;
  const ENCRYPTED_FLAG = 0x0001;
  const STORED = 0;
  const DEFLATED = 8;
  const READ_CHUNK = 4 * 1024 * 1024;
  // (the end record's comment is 65535 bytes at most)
  const END_SEARCH = 22 + 65535;
  const MAXIMUM_DIRECTORY_SIZE = 256 * 1024 * 1024;

  const crcTable = (() => {
    const table = new Uint32Array(256);
    for (let n = 0; n < 256; n++) {
      let c = n;
      for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
      table[n] = c >>> 0;
    }
    return table;
  })();

  // a CRC-32 carried on over more bytes (crc32(bytes) for the first)
  function crc32(bytes, crc = 0) {
    let c = (crc ^ MAXIMUM_32) >>> 0;
    for (let i = 0; i < bytes.length; i++) c = crcTable[(c ^ bytes[i]) & 0xFF] ^ (c >>> 8);
    return (c ^ MAXIMUM_32) >>> 0;
  }

  async function blobCrc(blob, onChunk) {
    let crc = 0;
    for (let position = 0; position < blob.size; position += READ_CHUNK) {
      const bytes = new Uint8Array(await blob.slice(position, position + READ_CHUNK).arrayBuffer());
      crc = crc32(bytes, crc);
      onChunk(bytes.length);
    }
    return crc;
  }

  // MS-DOS's time and date, as a zip keeps them (1980 at the earliest)
  function dosTime(milliseconds) {
    const date = new Date(milliseconds || Date.now());
    if (date.getFullYear() < 1980) return { time: 0, date: 0x21 };
    return {
      time: (date.getHours() << 11) | (date.getMinutes() << 5) | (date.getSeconds() >> 1),
      date: ((date.getFullYear() - 1980) << 9) | ((date.getMonth() + 1) << 5) | date.getDate(),
    };
  }

  function setUint64(view, offset, value) {
    view.setBigUint64(offset, BigInt(value), true);
  }

  function getUint64(view, offset) {
    return Number(view.getBigUint64(offset, true));
  }

  async function zip(entries, onProgress = () => {}) {
    const encoder = new TextEncoder();
    const total = entries.reduce((sum, entry) => sum + (entry.data.size ?? entry.data.length), 0);
    const parts = [];
    const central = [];
    let done = 0;
    let offset = 0;
    for (const { name, data } of entries) {
      const nameBytes = encoder.encode(name);
      const size = data.size ?? data.length;
      const crc = data instanceof Uint8Array ? crc32(data) : await blobCrc(data, (count) => {
        done += count;
        onProgress(name, done, total);
      });
      if (data instanceof Uint8Array) {
        done += size;
        onProgress(name, done, total);
      }
      const { time, date } = dosTime(data.lastModified);
      const large = size >= MAXIMUM_32;
      const far = offset >= MAXIMUM_32;
      const version = large || far ? 45 : 20;

      // (the zip64 fields follow the name: in a buffer of their own)
      const localExtra = new DataView(new ArrayBuffer(large ? 20 : 0));
      const local = new DataView(new ArrayBuffer(30));
      local.setUint32(0, LOCAL_SIGNATURE, true);
      local.setUint16(4, version, true);
      local.setUint16(6, UTF8_FLAG, true);
      local.setUint16(10, time, true);
      local.setUint16(12, date, true);
      local.setUint32(14, crc, true);
      local.setUint32(18, large ? MAXIMUM_32 : size, true);
      local.setUint32(22, large ? MAXIMUM_32 : size, true);
      local.setUint16(26, nameBytes.length, true);
      local.setUint16(28, localExtra.byteLength, true);
      if (large) {
        localExtra.setUint16(0, ZIP64_EXTRA, true);
        localExtra.setUint16(2, 16, true);
        setUint64(localExtra, 4, size);
        setUint64(localExtra, 12, size);
      }

      // (the central record's zip64 fields: those that do not fit, in order)
      const wide = [...(large ? [size, size] : []), ...(far ? [offset] : [])];
      const centralExtra = new DataView(new ArrayBuffer(wide.length ? 4 + 8 * wide.length : 0));
      const header = new DataView(new ArrayBuffer(46));
      header.setUint32(0, CENTRAL_SIGNATURE, true);
      header.setUint16(4, version, true);
      header.setUint16(6, version, true);
      header.setUint16(8, UTF8_FLAG, true);
      header.setUint16(12, time, true);
      header.setUint16(14, date, true);
      header.setUint32(16, crc, true);
      header.setUint32(20, large ? MAXIMUM_32 : size, true);
      header.setUint32(24, large ? MAXIMUM_32 : size, true);
      header.setUint16(28, nameBytes.length, true);
      header.setUint16(30, centralExtra.byteLength, true);
      header.setUint32(42, far ? MAXIMUM_32 : offset, true);
      if (wide.length) {
        centralExtra.setUint16(0, ZIP64_EXTRA, true);
        centralExtra.setUint16(2, 8 * wide.length, true);
        wide.forEach((value, index) => setUint64(centralExtra, 4 + 8 * index, value));
      }

      parts.push(local.buffer, nameBytes, localExtra.buffer, data);
      central.push(header.buffer, nameBytes, centralExtra.buffer);
      offset += local.byteLength + nameBytes.length + localExtra.byteLength + size;
    }
    const centralSize = central.reduce((sum, part) => sum + part.byteLength, 0);
    const tail = [];
    const zip64 = entries.length > MAXIMUM_16 || offset >= MAXIMUM_32 || centralSize >= MAXIMUM_32;
    if (zip64) {
      const record = new DataView(new ArrayBuffer(56 + 20));
      record.setUint32(0, END64_SIGNATURE, true);
      setUint64(record, 4, 44);
      record.setUint16(12, 45, true);
      record.setUint16(14, 45, true);
      setUint64(record, 24, entries.length);
      setUint64(record, 32, entries.length);
      setUint64(record, 40, centralSize);
      setUint64(record, 48, offset);
      record.setUint32(56, LOCATOR64_SIGNATURE, true);
      setUint64(record, 64, offset + centralSize);
      record.setUint32(72, 1, true);
      tail.push(record.buffer);
    }
    const end = new DataView(new ArrayBuffer(22));
    end.setUint32(0, END_SIGNATURE, true);
    end.setUint16(8, Math.min(entries.length, MAXIMUM_16), true);
    end.setUint16(10, Math.min(entries.length, MAXIMUM_16), true);
    end.setUint32(12, zip64 ? MAXIMUM_32 : centralSize, true);
    end.setUint32(16, zip64 ? MAXIMUM_32 : offset, true);
    tail.push(end.buffer);
    return new Blob([...parts, ...central, ...tail], { type: 'application/zip' });
  }

  async function readView(file, start, end) {
    const buffer = await file.slice(start, end).arrayBuffer();
    if (buffer.byteLength !== end - start) throw new Error(`${file.name} is cut short: it is not a whole zip.`);
    return new DataView(buffer);
  }

  // where a zip's central directory is, and how many files it has
  async function directoryOf(file) {
    const start = Math.max(0, file.size - END_SEARCH);
    const view = await readView(file, start, file.size);
    let at = -1;
    for (let i = view.byteLength - 22; i >= 0; i--) {
      if (view.getUint32(i, true) === END_SIGNATURE) {
        at = i;
        break;
      }
    }
    if (at < 0) throw new Error(`${file.name} is not a zip.`);
    let count = view.getUint16(at + 10, true);
    let size = view.getUint32(at + 12, true);
    let offset = view.getUint32(at + 16, true);
    if (count === MAXIMUM_16 || size === MAXIMUM_32 || offset === MAXIMUM_32) {
      const locator = at - 20;
      if (locator >= 0 && view.getUint32(locator, true) === LOCATOR64_SIGNATURE) {
        const recordOffset = getUint64(view, locator + 8);
        const record = await readView(file, recordOffset, recordOffset + 56);
        if (record.getUint32(0, true) !== END64_SIGNATURE) throw new Error(`${file.name}'s zip64 directory is damaged.`);
        count = getUint64(record, 32);
        size = getUint64(record, 40);
        offset = getUint64(record, 48);
      }
    }
    if (offset + size > file.size || size > MAXIMUM_DIRECTORY_SIZE) throw new Error(`${file.name}'s directory is damaged.`);
    return { count, size, offset };
  }

  function decodeName(bytes, utf8) {
    if (utf8) return new TextDecoder().decode(bytes);
    let text = '';
    for (const byte of bytes) text += String.fromCharCode(byte);
    return text;
  }

  // a zip's files: { path, method, crc, compressedSize, size, local }, its
  // folders left out
  async function entriesOf(file) {
    const directory = await directoryOf(file);
    const view = await readView(file, directory.offset, directory.offset + directory.size);
    const entries = [];
    let at = 0;
    for (let index = 0; index < directory.count; index++) {
      if (at + 46 > view.byteLength || view.getUint32(at, true) !== CENTRAL_SIGNATURE) {
        throw new Error(`${file.name}'s directory is damaged.`);
      }
      const flags = view.getUint16(at + 8, true);
      const method = view.getUint16(at + 10, true);
      const crc = view.getUint32(at + 16, true);
      let compressedSize = view.getUint32(at + 20, true);
      let size = view.getUint32(at + 24, true);
      const nameLength = view.getUint16(at + 28, true);
      const extraLength = view.getUint16(at + 30, true);
      const commentLength = view.getUint16(at + 32, true);
      let local = view.getUint32(at + 42, true);
      const nameStart = at + 46;
      if (nameStart + nameLength + extraLength > view.byteLength) throw new Error(`${file.name}'s directory is damaged.`);
      const path = decodeName(new Uint8Array(view.buffer, nameStart, nameLength), flags & UTF8_FLAG)
        .replace(/\\/g, '/');
      // (the zip64 fields, of those 32 bits did not hold, in order)
      for (let extra = nameStart + nameLength; extra + 4 <= nameStart + nameLength + extraLength;) {
        const id = view.getUint16(extra, true);
        const length = view.getUint16(extra + 2, true);
        if (id === ZIP64_EXTRA) {
          let field = extra + 4;
          if (size === MAXIMUM_32) { size = getUint64(view, field); field += 8; }
          if (compressedSize === MAXIMUM_32) { compressedSize = getUint64(view, field); field += 8; }
          if (local === MAXIMUM_32) local = getUint64(view, field);
        }
        extra += 4 + length;
      }
      at = nameStart + nameLength + extraLength + commentLength;
      if (path.endsWith('/')) continue;
      if (flags & ENCRYPTED_FLAG) throw new Error(`${path} in ${file.name} is encrypted: zip it without a password.`);
      if (method !== STORED && method !== DEFLATED) {
        throw new Error(`${path} in ${file.name} is compressed in a way the browser cannot read: zip it again ` +
          'with "deflate", or without compressing.');
      }
      if (local + compressedSize > file.size) throw new Error(`${file.name} is cut short: it is not a whole zip.`);
      entries.push({ path, method, crc, compressedSize, size, local });
    }
    return entries;
  }

  // checks a file's bytes as they pass: its size and CRC, at its end
  function checked(entry, zipName) {
    let crc = 0;
    let count = 0;
    return new TransformStream({
      transform(chunk, controller) {
        crc = crc32(chunk, crc);
        count += chunk.length;
        controller.enqueue(chunk);
      },
      flush(controller) {
        if (count !== entry.size || crc !== entry.crc) {
          controller.error(new Error(`${entry.path} in ${zipName} is damaged.`));
        }
      },
    });
  }

  // a zip's file, as a chosen folder's are
  function zipFile(file, entry) {
    let start = null;
    const dataStart = async () => {
      if (start === null) {
        const local = await readView(file, entry.local, entry.local + 30);
        if (local.getUint32(0, true) !== LOCAL_SIGNATURE) throw new Error(`${entry.path} in ${file.name} is damaged.`);
        start = entry.local + 30 + local.getUint16(26, true) + local.getUint16(28, true);
      }
      return start;
    };
    const stream = () => {
      const { readable, writable } = new TransformStream();
      (async () => {
        const from = await dataStart();
        let source = file.slice(from, from + entry.compressedSize).stream();
        if (entry.method === DEFLATED) source = source.pipeThrough(new DecompressionStream('deflate-raw'));
        await source.pipeThrough(checked(entry, file.name)).pipeTo(writable);
      })().catch((error) => writable.abort(error).catch(() => {}));
      return readable;
    };
    const parts = entry.path.split('/');
    return {
      name: parts[parts.length - 1],
      webkitRelativePath: entry.path,
      size: entry.size,
      zip: file.name,
      stream,
      // (a part of the file: a stored file's is the zip's own; a deflated
      // file's is inflated from its start, which the importers ask only for
      // its header)
      slice: (sliceStart = 0, sliceEnd = entry.size) => ({
        arrayBuffer: async () => {
          const end = Math.min(sliceEnd, entry.size);
          if (entry.method === STORED) {
            const from = await dataStart();
            return file.slice(from + sliceStart, from + end).arrayBuffer();
          }
          const bytes = new Uint8Array(Math.max(0, end - sliceStart));
          const reader = stream().getReader();
          let position = 0;
          while (position < end) {
            const { done, value } = await reader.read();
            if (done) break;
            const from = Math.max(sliceStart - position, 0);
            const to = Math.min(end - position, value.length);
            if (to > from) bytes.set(value.subarray(from, to), position + from - sliceStart);
            position += value.length;
          }
          reader.cancel().catch(() => {});
          return bytes.buffer;
        },
      }),
    };
  }

  // what macOS's Finder adds to a zip it makes, which is no one's file
  function finderNoise(path) {
    return path.split('/').some((part) => part === '__MACOSX' || part === '.DS_Store' || part.startsWith('._'));
  }

  async function open(file) {
    return (await entriesOf(file)).filter((entry) => !finderNoise(entry.path)).map((entry) => zipFile(file, entry));
  }

  function isZip(file) {
    return /\.zip$/i.test(file.name);
  }

  scope.HaloArchive = { crc32, zip, open, isZip };
  if (typeof module !== 'undefined') module.exports = scope.HaloArchive;
})(typeof self !== 'undefined' ? self : globalThis);
