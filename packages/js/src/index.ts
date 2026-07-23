export const MAX_LABEL_LENGTH = 63;
export const IDRTO_HASH_MARKER = "idrto-h1--";
export const IDRTO_MARKER_HOST = "idrto-h1";
export const RESERVED_HOST_XN = "xn";
export const HASH_BODY_LENGTH = 50;
export const STRUCTURAL_SEPARATOR = "--";
export const STRUCTURAL_SEPARATOR_ESCAPED = "-2d-2d";

const BASE36_ALPHABET = "0123456789abcdefghijklmnopqrstuvwxyz";
const HEX = "0123456789abcdef";

export type DefErrorCode =
  | "label_too_long"
  | "invalid_escape"
  | "invalid_utf8"
  | "invalid_encoding"
  | "invalid_locator"
  | "not_decodable";

export class DefError extends Error {
  readonly code: DefErrorCode;

  constructor(message: string, code: DefErrorCode) {
    super(message);
    this.name = "DefError";
    this.code = code;
  }
}

function isLiteralByte(byte: number): boolean {
  return (byte >= 0x61 && byte <= 0x7a) || (byte >= 0x30 && byte <= 0x39);
}

function isHostStart(byte: number): boolean {
  return isLiteralByte(byte);
}

function encodeBytes(bytes: Uint8Array): string {
  let out = "";
  for (const byte of bytes) {
    if (isLiteralByte(byte)) {
      out += String.fromCharCode(byte);
    } else {
      out += "-" + HEX[byte >> 4]! + HEX[byte & 0x0f]!;
    }
  }
  return out;
}

function canonicalizeToBytes(input: string): Uint8Array {
  const raw = new TextEncoder().encode(input);
  const bytes = new Uint8Array(raw.length);
  for (let i = 0; i < raw.length; i++) {
    const byte = raw[i]!;
    bytes[i] = byte >= 0x41 && byte <= 0x5a ? byte + 32 : byte;
  }
  return bytes;
}

/** Encode input to a DEF body (§9). */
export function encodeBody(input: string): string {
  return encodeBytes(canonicalizeToBytes(input));
}

function parseHexPair(h1: number, h2: number): number | null {
  const n1 =
    h1 >= 0x30 && h1 <= 0x39
      ? h1 - 0x30
      : h1 >= 0x61 && h1 <= 0x66
        ? h1 - 0x61 + 10
        : -1;
  const n2 =
    h2 >= 0x30 && h2 <= 0x39
      ? h2 - 0x30
      : h2 >= 0x61 && h2 <= 0x66
        ? h2 - 0x61 + 10
        : -1;
  if (n1 < 0 || n2 < 0) {
    return null;
  }
  return n1 * 16 + n2;
}

/** Decode a DEF body (§10). */
export function decodeBody(body: string): string {
  const bytes = new Uint8Array(body.length);
  let len = 0;

  for (let i = 0; i < body.length; ) {
    const code = body.charCodeAt(i);

    if ((code >= 0x61 && code <= 0x7a) || (code >= 0x30 && code <= 0x39)) {
      bytes[len++] = code;
      i += 1;
      continue;
    }

    if (code !== 0x2d) {
      throw new DefError("invalid character in encoded input", "invalid_escape");
    }

    if (i + 3 > body.length) {
      throw new DefError("truncated escape sequence", "invalid_escape");
    }

    const value = parseHexPair(
      body.charCodeAt(i + 1),
      body.charCodeAt(i + 2),
    );
    if (value === null) {
      throw new DefError("invalid escape sequence", "invalid_escape");
    }

    bytes[len++] = value;
    i += 3;
  }

  try {
    return new TextDecoder("utf-8", { fatal: true }).decode(
      bytes.subarray(0, len),
    );
  } catch {
    throw new DefError("invalid utf-8 byte sequence", "invalid_utf8");
  }
}

function markerHostPrefix(marker: string): string {
  if (!marker.endsWith(STRUCTURAL_SEPARATOR)) {
    throw new DefError("invalid provider hash marker", "invalid_encoding");
  }
  return marker.slice(0, -STRUCTURAL_SEPARATOR.length);
}

function validateHost(host: string, marker: string): void {
  if (host === RESERVED_HOST_XN || host === markerHostPrefix(marker)) {
    throw new DefError("invalid profile host", "invalid_locator");
  }
}

function splitLocator(
  locator: string,
  marker: string,
): { host: string; entity: string } {
  const sep = locator.indexOf(STRUCTURAL_SEPARATOR);
  if (sep <= 0 || sep + 2 >= locator.length) {
    throw new DefError("invalid profile locator", "invalid_locator");
  }

  const host = locator.slice(0, sep);
  const entity = locator.slice(sep + 2);
  if (host.includes(STRUCTURAL_SEPARATOR) || entity.length === 0) {
    throw new DefError("invalid profile locator", "invalid_locator");
  }

  const hostBytes = new TextEncoder().encode(host);
  if (hostBytes.length === 0 || !isHostStart(hostBytes[0]!)) {
    throw new DefError("invalid profile host", "invalid_locator");
  }

  validateHost(host, marker);
  return { host, entity };
}

function base36(data: Uint8Array): string {
  let n = 0n;
  for (const byte of data) {
    n = (n << 8n) | BigInt(byte);
  }

  if (n === 0n) {
    return "0".repeat(HASH_BODY_LENGTH);
  }

  let out = "";
  while (n > 0n) {
    out = BASE36_ALPHABET[Number(n % 36n)]! + out;
    n /= 36n;
  }

  return out.padStart(HASH_BODY_LENGTH, "0");
}

async function sha256(data: Uint8Array): Promise<Uint8Array> {
  const subtle = globalThis.crypto?.subtle;
  if (!subtle) {
    throw new DefError(
      "Web Crypto API (crypto.subtle) is required for profile hash encoding",
      "invalid_encoding",
    );
  }
  return new Uint8Array(await subtle.digest("SHA-256", data as BufferSource));
}

function isBase36Char(code: number): boolean {
  return (
    (code >= 0x30 && code <= 0x39) || (code >= 0x61 && code <= 0x7a)
  );
}

function validateMarker(marker: string): void {
  if (
    marker.length < 3 ||
    marker.length > 13 ||
    !marker.endsWith(STRUCTURAL_SEPARATOR) ||
    marker.startsWith("xn--")
  ) {
    throw new DefError("invalid provider hash marker", "invalid_encoding");
  }
}

/** Encode a profile locator with the configured hash marker (§12). */
export async function encodeProfile(
  locator: string,
  marker: string = IDRTO_HASH_MARKER,
): Promise<string> {
  validateMarker(marker);

  const canonical = canonicalizeToBytes(locator);
  const canonicalText = new TextDecoder().decode(canonical);
  const { host, entity } = splitLocator(canonicalText, marker);

  const hostBody = encodeBytes(new TextEncoder().encode(host));
  const entityBody = encodeBytes(new TextEncoder().encode(entity));
  const label = hostBody + STRUCTURAL_SEPARATOR + entityBody;

  if (label.length <= MAX_LABEL_LENGTH) {
    return label;
  }

  const hashInput =
    hostBody + STRUCTURAL_SEPARATOR_ESCAPED + entityBody;
  const digest = await sha256(new TextEncoder().encode(hashInput));
  const encoded = marker + base36(digest);
  if (encoded.length > MAX_LABEL_LENGTH) {
    throw new DefError(
      "encoded label exceeds 63 characters",
      "label_too_long",
    );
  }
  return encoded;
}

/** Decode a profile label with the configured hash marker (§12). */
export function decodeProfile(
  label: string,
  marker: string = IDRTO_HASH_MARKER,
): string {
  validateMarker(marker);

  if (label.length > MAX_LABEL_LENGTH) {
    throw new DefError(
      "encoded label exceeds 63 characters",
      "label_too_long",
    );
  }

  if (label.startsWith(marker)) {
    const digest = label.slice(marker.length);
    if (digest.length !== HASH_BODY_LENGTH) {
      throw new DefError("invalid profile hash label", "invalid_encoding");
    }
    for (let i = 0; i < digest.length; i++) {
      if (!isBase36Char(digest.charCodeAt(i))) {
        throw new DefError("invalid profile hash label", "invalid_encoding");
      }
    }
    throw new DefError("profile hash label is not decodable", "not_decodable");
  }

  if (label.startsWith("xn--")) {
    throw new DefError("invalid profile label", "invalid_encoding");
  }

  const sep = label.indexOf(STRUCTURAL_SEPARATOR);
  if (sep <= 0 || sep + 2 > label.length) {
    throw new DefError("missing profile separator", "invalid_encoding");
  }

  const host = decodeBody(label.slice(0, sep));
  const entity = decodeBody(label.slice(sep + 2));

  if (host.length === 0 || entity.length === 0 || host.includes(STRUCTURAL_SEPARATOR)) {
    throw new DefError("invalid decoded profile locator", "invalid_locator");
  }

  validateHost(host, marker);

  return host + STRUCTURAL_SEPARATOR + entity;
}

/** Encode an idr.to identity locator. */
export function encode(locator: string): Promise<string> {
  return encodeProfile(locator, IDRTO_HASH_MARKER);
}

/** Decode an idr.to profile label. */
export function decode(label: string): string {
  return decodeProfile(label, IDRTO_HASH_MARKER);
}
