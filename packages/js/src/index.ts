import { createHash } from "node:crypto";

export const MAX_LABEL_LENGTH = 63;
export const MAX_DEF_BODY_LENGTH = 62;
export const DEF_PREFIX = "d";
export const HASH_PREFIX = "h";

const CROCKFORD_ALPHABET = "0123456789abcdefghjkmnpqrstvwxyz";

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

function canonicalize(input: string): string {
  let out = "";
  for (let i = 0; i < input.length; i++) {
    const code = input.charCodeAt(i);
    if (code >= 0x41 && code <= 0x5a) {
      out += String.fromCharCode(code + 32);
    } else {
      out += input[i]!;
    }
  }
  return out;
}

function encodeDefBody(bytes: Uint8Array): string {
  let out = "";
  for (const byte of bytes) {
    if (isLiteralByte(byte)) {
      out += String.fromCharCode(byte);
    } else {
      out += `-${byte.toString(16).padStart(2, "0")}`;
    }
  }
  return out;
}

function crockfordBase32(data: Uint8Array): string {
  let bits = 0;
  let value = 0;
  let out = "";

  for (const byte of data) {
    value = (value << 8) | byte;
    bits += 8;
    while (bits >= 5) {
      out += CROCKFORD_ALPHABET[(value >>> (bits - 5)) & 0x1f]!;
      bits -= 5;
    }
  }

  if (bits > 0) {
    out += CROCKFORD_ALPHABET[(value << (5 - bits)) & 0x1f]!;
  }

  return out;
}

function encodeHash(canonicalBytes: Uint8Array): string {
  const digest = createHash("sha256").update(canonicalBytes).digest();
  const encoded = HASH_PREFIX + crockfordBase32(digest);
  if (encoded.length > MAX_LABEL_LENGTH) {
    throw new DefError(
      "encoded label exceeds 63 characters",
      "label_too_long",
    );
  }
  return encoded;
}

export function encode(input: string): string {
  const canonical = canonicalize(input);
  const bytes = new TextEncoder().encode(canonical);
  const body = encodeDefBody(bytes);

  if (body.length <= MAX_DEF_BODY_LENGTH) {
    const encoded = DEF_PREFIX + body;
    if (encoded.length > MAX_LABEL_LENGTH) {
      throw new DefError(
        "encoded label exceeds 63 characters",
        "label_too_long",
      );
    }
    return encoded;
  }

  return encodeHash(new TextEncoder().encode(body));
}

export function decode(encoded: string): string {
  if (encoded.length > MAX_LABEL_LENGTH) {
    throw new DefError(
      "encoded label exceeds 63 characters",
      "label_too_long",
    );
  }

  if (encoded.length === 0) {
    throw new DefError("missing encoding prefix", "invalid_encoding");
  }

  const prefix = encoded[0];
  if (prefix === HASH_PREFIX) {
    throw new DefError("hash-encoded label is not decodable", "not_decodable");
  }
  if (prefix !== DEF_PREFIX) {
    throw new DefError("unrecognized encoding prefix", "invalid_encoding");
  }

  const body = encoded.slice(1);
  const bytes: number[] = [];

  for (let i = 0; i < body.length; ) {
    const code = body.charCodeAt(i);

    if ((code >= 0x61 && code <= 0x7a) || (code >= 0x30 && code <= 0x39)) {
      bytes.push(code);
      i += 1;
      continue;
    }

    if (code !== 0x2d) {
      throw new DefError("invalid character in encoded input", "invalid_escape");
    }

    if (i + 3 > body.length) {
      throw new DefError("truncated escape sequence", "invalid_escape");
    }

    const hex = body.slice(i + 1, i + 3);
    if (!/^[0-9a-f]{2}$/.test(hex)) {
      throw new DefError("invalid escape sequence", "invalid_escape");
    }

    bytes.push(Number.parseInt(hex, 16));
    i += 3;
  }

  try {
    return new TextDecoder("utf-8", { fatal: true }).decode(
      new Uint8Array(bytes),
    );
  } catch {
    throw new DefError("invalid utf-8 byte sequence", "invalid_utf8");
  }
}
