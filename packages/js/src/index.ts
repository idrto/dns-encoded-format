export const MAX_LABEL_LENGTH = 63;

export type DefErrorCode =
  | "label_too_long"
  | "invalid_escape"
  | "invalid_utf8";

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

function ensureFits(currentLength: number, addLength: number): void {
  if (currentLength + addLength > MAX_LABEL_LENGTH) {
    throw new DefError(
      "encoded label exceeds 63 characters",
      "label_too_long",
    );
  }
}

export function encode(input: string): string {
  const bytes = new TextEncoder().encode(canonicalize(input));
  let out = "";

  for (const byte of bytes) {
    if (isLiteralByte(byte)) {
      ensureFits(out.length, 1);
      out += String.fromCharCode(byte);
    } else {
      const escape = `-${byte.toString(16).padStart(2, "0")}`;
      ensureFits(out.length, escape.length);
      out += escape;
    }
  }

  return out;
}

export function decode(encoded: string): string {
  if (encoded.length > MAX_LABEL_LENGTH) {
    throw new DefError(
      "encoded label exceeds 63 characters",
      "label_too_long",
    );
  }

  const bytes: number[] = [];

  for (let i = 0; i < encoded.length; ) {
    const code = encoded.charCodeAt(i);

    if ((code >= 0x61 && code <= 0x7a) || (code >= 0x30 && code <= 0x39)) {
      bytes.push(code);
      i += 1;
      continue;
    }

    if (code !== 0x2d) {
      throw new DefError("invalid character in encoded input", "invalid_escape");
    }

    if (i + 3 > encoded.length) {
      throw new DefError("truncated escape sequence", "invalid_escape");
    }

    const hex = encoded.slice(i + 1, i + 3);
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
