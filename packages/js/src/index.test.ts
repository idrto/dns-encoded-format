import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { decode, encode } from "./index.js";

const vectorsPath = join(
  dirname(fileURLToPath(import.meta.url)),
  "..",
  "..",
  "..",
  "vectors",
  "test-vectors.json",
);
const vectors = JSON.parse(readFileSync(vectorsPath, "utf8")) as {
  encode: Array<{ input: string; encoded: string }>;
  encode_hash: Array<{ input: string; encoded: string }>;
  encode_errors: Array<{ input: string; reason: string }>;
  decode: Array<{ input: string; decoded: string }>;
  decode_errors: Array<{ input: string; reason: string }>;
};

describe("encode", () => {
  for (const { input, encoded } of vectors.encode) {
    it(`encodes ${JSON.stringify(input)}`, async () => {
      await expect(encode(input)).resolves.toBe(encoded);
    });
  }

  for (const { input, encoded } of vectors.encode_hash) {
    it(`hash-encodes ${JSON.stringify(input)}`, async () => {
      await expect(encode(input)).resolves.toBe(encoded);
    });
  }

  for (const { input, reason } of vectors.encode_errors) {
    it(`rejects encode ${JSON.stringify(input)} (${reason})`, async () => {
      await expect(encode(input)).rejects.toMatchObject({ code: reason });
    });
  }
});

describe("decode", () => {
  for (const { input, decoded } of vectors.decode) {
    it(`decodes ${JSON.stringify(input)}`, () => {
      expect(decode(input)).toBe(decoded);
    });
  }

  for (const { input, reason } of vectors.decode_errors) {
    it(`rejects decode ${JSON.stringify(input)} (${reason})`, () => {
      expect(() => decode(input)).toThrowError(
        expect.objectContaining({ code: reason }),
      );
    });
  }
});

describe("round trip", () => {
  for (const { input } of vectors.encode) {
    it(`round trips ${JSON.stringify(input)}`, async () => {
      expect(decode(await encode(input))).toBe(
        input.replace(/[A-Z]/g, (ch) => ch.toLowerCase()),
      );
    });
  }
});
