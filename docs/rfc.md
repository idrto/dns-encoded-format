# RFC: DNS Encoded Format

**Version:** 1.0 (Draft)

## 1. Purpose

This document defines the **DNS Encoded Format (DEF)** used by idr.to to transform arbitrary Unicode strings into a DNS-friendly ASCII representation suitable for use as a single DNS label.

The transformation is intended for identifier components such as user IDs, host names, service names, labels, and similar fields. A primary use case is encoding a URI (or URI-like string) into one valid DNS label while preserving UTF-8 text through encoding and decoding.

The encoding is deterministic and simple to parse.

## 2. Design Goals

* DNS-friendly representation as a single label.
* Simple implementation.
* Context-free parsing.
* Uniform algorithm for all identifier components.
* Supports arbitrary UTF-8 input subject to the DNS label length limit.
* Canonicalizes ASCII letters to lowercase.
* Escapes all characters except lowercase letters and decimal digits.

## 3. DNS Label Length Limit

A DNS label MUST NOT exceed **63 octets** ([RFC 1035](https://www.rfc-editor.org/rfc/rfc1035)).

Because DEF output is ASCII, one output character equals one octet. An encoded string MUST NOT exceed 63 characters.

Implementations MUST enforce this limit:

* **Encoding:** If the encoded output would exceed 63 characters, the encoder MUST reject the input.
* **Decoding:** If the encoded input exceeds 63 characters, the decoder MUST reject the input.

This limit applies to the encoded form, not necessarily to the decoded Unicode string length.

## 4. Canonicalization

Before encoding, every ASCII uppercase letter (`A`–`Z`) SHALL be converted to its lowercase equivalent (`a`–`z`).

Only ASCII uppercase letters are affected. Other Unicode case mappings are not applied.

For example:

| Input              | Canonical Form     |
| ------------------ | ------------------ |
| `ABC`              | `abc`              |
| `User@Example.Com` | `user@example.com` |
| `Laptop.US-East`   | `laptop.us-east`   |

This transformation is irreversible. Decoding SHALL produce the canonical lowercase representation rather than the original input.

## 5. Encoding Unit

After canonicalization, the string SHALL be encoded as UTF-8.

Encoding operates independently on each UTF-8 byte.

## 6. Literal Characters

The following ASCII characters SHALL be emitted unchanged:

```text
a-z
0-9
```

Equivalently:

```text
0x61-0x7a
0x30-0x39
```

All other bytes MUST be escaped.

## 7. Escape Format

Each escaped byte SHALL be represented as:

```text
-hh
```

where:

* `-` is the escape introducer.
* `hh` are exactly two lowercase hexadecimal digits representing one UTF-8 byte.

Examples:

| Character | UTF-8 | Encoded |
| --------- | ----- | ------- |
| `-`       | `2d`  | `-2d`   |
| `.`       | `2e`  | `-2e`   |
| `@`       | `40`  | `-40`   |
| `_`       | `5f`  | `-5f`   |
| `+`       | `2b`  | `-2b`   |
| `:`       | `3a`  | `-3a`   |
| `/`       | `2f`  | `-2f`   |
| space     | `20`  | `-20`   |

Unicode example:

```text
é
UTF-8: c3 a9
Encoded: -c3-a9
```

Emoji example:

```text
😊
UTF-8: f0 9f 98 8a
Encoded: -f0-9f-98-8a
```

## 8. Formal Grammar (ABNF)

```
HEXDIG = %x30-39 / %x61-66
        ; 0-9 or a-f

LITERAL = %x61-7A / %x30-39
        ; a-z or 0-9

ESCAPE = "-" HEXDIG HEXDIG

ENCODED = *( LITERAL / ESCAPE )
        ; length MUST NOT exceed 63 characters
```

## 9. Encoding Algorithm

Given an input Unicode string:

1. Convert every ASCII uppercase letter (`A`–`Z`) to lowercase.
2. Encode the resulting string as UTF-8.
3. For each UTF-8 byte:
   * If the byte represents ASCII `a`–`z` or `0`–`9`, emit it literally.
   * Otherwise emit `-` followed by exactly two lowercase hexadecimal digits.
4. Concatenate the emitted tokens.
5. If the result exceeds 63 characters, reject the input.

## 10. Decoding Algorithm

Given an encoded string:

1. If the input length exceeds 63 characters, reject the input.
2. Initialize an empty byte buffer.
3. Scan left to right.
4. If the next character is `a`–`z` or `0`–`9`, append its ASCII byte.
5. If the next character is `-`, read exactly the next two hexadecimal digits.
6. Convert those digits into one byte and append it.
7. Continue until the end of input.
8. Decode the accumulated bytes as UTF-8.

If:

* the input exceeds 63 characters,
* `-` is not followed by exactly two hexadecimal digits,
* the resulting byte stream is not valid UTF-8, or
* any other invalid input is encountered,

the decoder MUST reject the input.

## 11. Properties

The DNS Encoded Format is:

* deterministic,
* context-free,
* independent of identifier semantics,
* UTF-8 compatible,
* DNS-friendly as a single label of at most 63 octets, and
* canonical with respect to ASCII letter case.

Because of lowercase canonicalization:

```
decode(encode(x)) == lowercase_ascii(x)
```

where `lowercase_ascii(x)` denotes `x` after converting only ASCII `A`–`Z` to `a`–`z`, for all inputs where `encode(x)` succeeds.

## 12. URI Use Case: FQDNs for HTTPS and SNI

### 12.1 Problem

Many systems identify resources with URIs whose schemes are not HTTP or HTTPS—for example `idrto:`, `postgres:`, `ssh:`, or custom application schemes. These URIs can contain characters (`:`, `@`, `/`, `.`, `-`, and others) that are not valid in a DNS hostname.

HTTPS, however, is built on DNS and TLS. A client connecting over TLS sends the target hostname in the **Server Name Indication (SNI)** extension during the handshake ([RFC 6066](https://www.rfc-editor.org/rfc/rfc6066)). The server uses that hostname to select the correct certificate and route the request.

To reach a gateway or proxy over HTTPS while carrying a non-HTTP URI, the URI must be represented as a hostname the client can resolve, connect to, and advertise in SNI.

### 12.2 Solution

DEF maps a URI **payload** (the URI body without its scheme prefix) to a single DNS label. That label is placed as the leftmost label of a fully qualified domain name (FQDN) under a controlled zone (for example `idr.to`):

```text
<encoded-label>.idr.to
```

The gateway zone implies the URI scheme. For `idr.to`, the scheme is `idrto` and the `idrto:` prefix is **not** part of the encoded input. A logical URI such as `idrto:user@example.com/db1.us-east/accounts-db` is carried by encoding only `user@example.com/db1.us-east/accounts-db`.

The result is a valid hostname suitable for:

* DNS resolution (A/AAAA records),
* TLS connections with a correct SNI value, and
* HTTPS URLs of the form `https://<encoded-label>.idr.to/...`.

The gateway at `idr.to` receives the TLS connection, reads the SNI hostname, extracts and decodes the embedded label to recover the URI payload, and routes the request (prepending the implied scheme where needed).

DEF does **not** parse URI structure. The payload—including credentials, host, path segments, and slashes—is encoded as opaque text. Decoding restores the canonical lowercase ASCII form of that payload.

### 12.3 Worked Example

Consider an idrto resource with path segments (slashes). The scheme is implied by the `idr.to` zone and is not encoded:

```text
URI payload (input to DEF):
  user@example.com/db1.us-east/accounts-db
```

Apply DEF to the payload:

| Step | Value |
| ---- | ----- |
| Canonical form | `user@example.com/db1.us-east/accounts-db` |
| Encoded label (54 characters) | `user-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb` |
| FQDN | `user-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb.idr.to` |
| HTTPS URL | `https://user-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb.idr.to` |

Encoding the payload by segment:

| Segment | Characters | Encoded fragment |
| ------- | ---------- | ---------------- |
| User | `user@` | `user-40` |
| Host | `example.com` | `example-2ecom` |
| Path | `/db1.us-east/accounts-db` | `-2fdb1-2eus-2deast-2faccounts-2ddb` |

The full logical URI `idrto:user@example.com/db1.us-east/accounts-db` is recovered by the gateway from the decoded payload and the zone-implied `idrto:` scheme.

A client that wishes to access this resource over HTTPS:

1. Takes the URI payload (everything after `idrto:`) and encodes it with DEF.
2. Forms the FQDN `<encoded>.idr.to`.
3. Opens a TLS connection to that hostname, sending the same hostname in SNI.
4. The server decodes the label to `user@example.com/db1.us-east/accounts-db` and routes using the implied `idrto:` scheme.

### 12.4 Length Constraint

If the encoded label exceeds 63 characters, the URI cannot be represented as a single DNS label using DEF. Applications MUST handle this error or split the identifier across multiple labels using a separate convention outside this specification.

## 13. Examples

| Original           | Encoded                |
| ------------------ | ---------------------- |
| `alice`            | `alice`                |
| `Alice`            | `alice`                |
| `USER@example.COM` | `user-40example-2ecom` |
| `Laptop.US-East`   | `laptop-2eus-2deast`   |
| `alice-1`          | `alice-2d1`            |
| `ssh`              | `ssh`                  |
| `postgres`         | `postgres`             |
| `user@example.com/db1.us-east/accounts-db` | `user-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb` |
| `用户`               | `-e7-94-a8-e6-88-b7`   |
| `😊`               | `-f0-9f-98-8a`         |

Maximum-length literal example (63 characters):

```text
Input:  a × 63
Output: a × 63
```

## 14. Conformance

Implementations MUST pass all test vectors in [`vectors/test-vectors.json`](../vectors/test-vectors.json).

## 15. Security Considerations

Decoders MUST reject invalid UTF-8 rather than substitute replacement characters. Encoders and decoders MUST enforce the 63-character label limit to avoid producing or accepting labels invalid on the wire.
