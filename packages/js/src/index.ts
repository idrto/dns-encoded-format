export const MAX_LABEL_LENGTH = 63;
export const IDRTO_HASH_MARKER = "idrto-h1--";
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

/** Encode one DEF payload component. */
export function encodeComponent(input: string): string {
  return encodeBytes(canonicalizeToBytes(input));
}

/** Encode a DEF body, preserving structural separators. */
export function encodeBody(input: string): string {
  return input
    .split(STRUCTURAL_SEPARATOR)
    .map(encodeComponent)
    .join(STRUCTURAL_SEPARATOR);
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

/** Decode one DEF payload component. */
export function decodeComponent(component: string): string {
  if (component.includes(STRUCTURAL_SEPARATOR)) {
    throw new DefError("structural separator in component", "invalid_escape");
  }

  const bytes = new Uint8Array(component.length);
  let len = 0;

  for (let i = 0; i < component.length; ) {
    const code = component.charCodeAt(i);

    if ((code >= 0x61 && code <= 0x7a) || (code >= 0x30 && code <= 0x39)) {
      bytes[len++] = code;
      i += 1;
      continue;
    }

    if (code !== 0x2d) {
      throw new DefError("invalid character in encoded input", "invalid_escape");
    }

    if (i + 3 > component.length) {
      throw new DefError("truncated escape sequence", "invalid_escape");
    }

    const value = parseHexPair(
      component.charCodeAt(i + 1),
      component.charCodeAt(i + 2),
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

/** Decode a DEF body without reinterpreting decoded payload. */
export function decodeBody(body: string): string {
  return body
    .split(STRUCTURAL_SEPARATOR)
    .map(decodeComponent)
    .join(STRUCTURAL_SEPARATOR);
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
    marker.startsWith("xn--") ||
    !/^[a-z0-9-]+$/.test(marker)
  ) {
    throw new DefError("invalid provider hash marker", "invalid_encoding");
  }
}

/** Encode a value as a profile label with the configured hash marker. */
export async function encodeProfile(
  value: string,
  marker: string = IDRTO_HASH_MARKER,
): Promise<string> {
  validateMarker(marker);

  const label = encodeBody(value);
  if (label.length === 0 || label.startsWith("-") || label.endsWith("-")) {
    throw new DefError("invalid profile encoding", "invalid_encoding");
  }

  if (label.length <= MAX_LABEL_LENGTH && !label.startsWith(marker)) {
    return label;
  }

  const hashInput = encodeComponent(value);
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

  if (label.length === 0 || label.startsWith("-") || label.endsWith("-")) {
    throw new DefError("invalid profile encoding", "invalid_encoding");
  }

  if (label.length > MAX_LABEL_LENGTH) {
    throw new DefError(
      "encoded label exceeds 63 characters",
      "label_too_long",
    );
  }

  return decodeBody(label);
}

/** Encode a value with the default profile. */
export function encode(value: string): Promise<string> {
  return encodeProfile(value, IDRTO_HASH_MARKER);
}

/** Decode a label with the default profile. */
export function decode(label: string): string {
  return decodeProfile(label, IDRTO_HASH_MARKER);
}
