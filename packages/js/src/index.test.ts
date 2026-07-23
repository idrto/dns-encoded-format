import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import {
  decode,
  decodeBody,
  decodeProfile,
  encode,
  encodeBody,
  encodeProfile,
  IDRTO_HASH_MARKER,
} from "./index.js";

const vectorsPath = join(
  dirname(fileURLToPath(import.meta.url)),
  "..",
  "..",
  "..",
  "vectors",
  "test-vectors.json",
);
const vectors = JSON.parse(readFileSync(vectorsPath, "utf8")) as {
  encode_body: Array<{ input: string; encoded: string }>;
  decode_body: Array<{ input: string; decoded: string }>;
  decode_body_errors: Array<{ input: string; reason: string }>;
  encode_profile: Array<{ input: string; encoded: string }>;
  encode_profile_hash: Array<{ input: string; encoded: string }>;
  encode_profile_errors: Array<{ input: string; reason: string }>;
  decode_profile: Array<{ input: string; decoded: string }>;
  decode_profile_errors: Array<{ input: string; reason: string }>;
};

describe("encodeBody", () => {
  for (const { input, encoded } of vectors.encode_body) {
    it(`encodes ${JSON.stringify(input)}`, () => {
      expect(encodeBody(input)).toBe(encoded);
    });
  }
});

describe("decodeBody", () => {
  for (const { input, decoded } of vectors.decode_body) {
    it(`decodes ${JSON.stringify(input)}`, () => {
      expect(decodeBody(input)).toBe(decoded);
    });
  }

  for (const { input, reason } of vectors.decode_body_errors) {
    it(`rejects decodeBody ${JSON.stringify(input)} (${reason})`, () => {
      expect(() => decodeBody(input)).toThrowError(
        expect.objectContaining({ code: reason }),
      );
    });
  }
});

describe("encodeProfile", () => {
  for (const { input, encoded } of vectors.encode_profile) {
    it(`encodes ${JSON.stringify(input)}`, async () => {
      await expect(encodeProfile(input, IDRTO_HASH_MARKER)).resolves.toBe(
        encoded,
      );
    });
  }

  for (const { input, encoded } of vectors.encode_profile_hash) {
    it(`hash-encodes ${JSON.stringify(input)}`, async () => {
      await expect(encodeProfile(input, IDRTO_HASH_MARKER)).resolves.toBe(
        encoded,
      );
    });
  }

  for (const { input, reason } of vectors.encode_profile_errors) {
    it(`rejects encodeProfile ${JSON.stringify(input)} (${reason})`, async () => {
      await expect(encodeProfile(input, IDRTO_HASH_MARKER)).rejects.toMatchObject(
        { code: reason },
      );
    });
  }
});

describe("decodeProfile", () => {
  for (const { input, decoded } of vectors.decode_profile) {
    it(`decodes ${JSON.stringify(input)}`, () => {
      expect(decodeProfile(input, IDRTO_HASH_MARKER)).toBe(decoded);
    });
  }

  for (const { input, reason } of vectors.decode_profile_errors) {
    it(`rejects decodeProfile ${JSON.stringify(input)} (${reason})`, () => {
      expect(() => decodeProfile(input, IDRTO_HASH_MARKER)).toThrowError(
        expect.objectContaining({ code: reason }),
      );
    });
  }
});

describe("idr.to shortcuts", () => {
  for (const { input, encoded } of vectors.encode_profile) {
    it(`encode ${JSON.stringify(input)}`, async () => {
      await expect(encode(input)).resolves.toBe(encoded);
    });
  }

  for (const { input, decoded } of vectors.decode_profile) {
    it(`decode ${JSON.stringify(input)}`, () => {
      expect(decode(input)).toBe(decoded);
    });
  }
});

describe("round trip", () => {
  for (const { input } of vectors.encode_profile) {
    it(`round trips ${JSON.stringify(input)}`, async () => {
      expect(decode(await encode(input))).toBe(
        input.replace(/[A-Z]/g, (ch) => ch.toLowerCase()),
      );
    });
  }
});
