# RFC: DNS Encoded Format

**Version:** 1.2 (Draft)

## 1. Purpose

This document defines the **DNS Encoded Format (DEF)** used by idr.to to transform arbitrary Unicode strings into a DNS-friendly ASCII representation suitable for use as a single DNS label.

The transformation is intended for identifier components such as entity IDs, host names, service names, labels, and similar fields. A primary use case is encoding a URI (or URI-like string) into one valid DNS label while preserving UTF-8 text through encoding and decoding.

The encoding is deterministic and simple to parse.

## 2. Design Goals

* DNS-friendly representation as a single label.
* Simple implementation.
* Context-free parsing.
* Uniform algorithm for all identifier components.
* Supports arbitrary UTF-8 input within the DNS label length limit, using a hash fallback when reversible DEF encoding does not fit.
* Canonicalizes ASCII letters to lowercase.
* Escapes all characters except lowercase letters and decimal digits.

## 3. DNS Label Length Limit

A DNS label MUST NOT exceed **63 octets** ([RFC 1035](https://www.rfc-editor.org/rfc/rfc1035)).

Because DEF output is ASCII, one output character equals one octet. An encoded string MUST NOT exceed 63 characters.

Implementations MUST enforce this limit:

* **Encoding:** The final encoded label (including its encoding-type prefix; see §3.1) MUST NOT exceed 63 characters.
* **Decoding:** If the encoded input exceeds 63 characters, the decoder MUST reject the input.

This limit applies to the encoded form, not necessarily to the decoded Unicode string length.

### 3.1 Encoding-Type Prefix

The first character of every encoded label identifies the encoding scheme used for the remainder of the label. Additional schemes MAY be defined in future revisions; the first character SHALL always identify the scheme.

| Prefix | Scheme | Reversible |
| ------ | ------ | ---------- |
| `d` | DEF byte escape (§6–§7) | Yes |
| `h` | SHA-256 hash (§3.2) | No |

The prefix consumes one character of the 63-character label limit. Reversible DEF content therefore MUST NOT exceed **62** characters after the `d` prefix.

### 3.2 Hash Encoding (`h`)

When the DEF byte-escape body (§9, steps 1–4, without the prefix) would exceed 62 characters, the encoder MUST use hash encoding instead of rejecting the input.

Hash encoding is:

```text
h<base36-sha256>
```

where:

* `h` is the hash-encoding prefix.
* `base36-sha256` is the Base36 representation of the SHA-256 digest (32 octets) of the **DEF body** produced by §9 steps 3–4—the full byte-escape string *before* the `d` or `h` prefix is applied. The hash input is the UTF-8 encoding of that ASCII DEF body string.
* Base36 output MUST use the lowercase alphabet `0123456789abcdefghijklmnopqrstuvwxyz`.
* The digest SHALL be interpreted as a big-endian unsigned integer, converted to Base36 by repeated division by 36, and left-padded with `0` to exactly **50** characters (no other padding characters).

A SHA-256 digest encodes to 50 Base36 characters, so the full label is 51 characters (`h` + 50) and always fits within the DNS limit.

Hash encoding is **one-way**. Implementations MUST NOT attempt to recover the original input from an `h`-prefixed label. Decoders MUST reject `h`-prefixed input with a `not_decodable` error (§10).

Hash labels are suitable for identifiers whose DEF body exceeds 62 characters but must still map deterministically to a single DNS label—for example, very long hostnames or URI payloads. The hash is computed **after** regular DEF byte escaping, not over the raw input. Consumers that need the original value MUST retain it out of band or use a registry; the DNS label alone identifies the resource by digest of its DEF body.

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
PREFIX = %x64 / %x68
        ; d = DEF byte escape, h = SHA-256 hash

HEXDIG = %x30-39 / %x61-66
        ; 0-9 or a-f

LITERAL = %x61-7A / %x30-39
        ; a-z or 0-9

ESCAPE = "-" HEXDIG HEXDIG

DEF_BODY = *( LITERAL / ESCAPE )
          ; length MUST NOT exceed 62 characters

BASE36 = %x30-39 / %x61-7A
        ; 0-9, a-z

HASH_BODY = 50BASE36

ENCODED = "d" DEF_BODY
        / "h" HASH_BODY
        ; total length MUST NOT exceed 63 characters
```

## 9. Encoding Algorithm

Given an input Unicode string:

1. Convert every ASCII uppercase letter (`A`–`Z`) to lowercase.
2. Encode the resulting string as UTF-8.
3. For each UTF-8 byte:
   * If the byte represents ASCII `a`–`z` or `0`–`9`, emit it literally.
   * Otherwise emit `-` followed by exactly two lowercase hexadecimal digits.
4. Concatenate the emitted tokens into a **DEF body** (without a prefix).
5. If the DEF body length is 62 characters or fewer, emit `d` followed by the DEF body.
6. Otherwise compute SHA-256 over the UTF-8 octets of the DEF body from step 4, encode the digest with lowercase Base36 (left-padded to 50 characters), and emit `h` followed by that string.
7. If the result exceeds 63 characters, reject the input. (Step 6 always produces a 51-character label for hash encoding; step 5 is bounded by the 62-character DEF body limit.)

## 10. Decoding Algorithm

Given an encoded string:

1. If the input length exceeds 63 characters, reject the input.
2. If the input is empty or the first character is not a recognized prefix (`d` or `h`), reject the input (`invalid_encoding`).
3. If the first character is `h`, reject the input (`not_decodable`). Hash-encoded labels cannot be decoded.
4. If the first character is `d`, treat the remainder as the DEF body and:
   * Initialize an empty byte buffer.
   * Scan the DEF body left to right.
   * If the next character is `a`–`z` or `0`–`9`, append its ASCII byte.
   * If the next character is `-`, read exactly the next two hexadecimal digits, convert them into one byte, and append it.
   * Continue until the end of the DEF body.
   * Decode the accumulated bytes as UTF-8.

If:

* the input exceeds 63 characters,
* the encoding prefix is missing or unrecognized,
* the input uses hash encoding (`h` prefix),
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

Because of lowercase canonicalization, for all inputs where `encode(x)` uses DEF encoding (`d` prefix):

```
decode(encode(x)) == lowercase_ascii(x)
```

where `lowercase_ascii(x)` denotes `x` after converting only ASCII `A`–`Z` to `a`–`z`.

This property does **not** apply to hash-encoded (`h` prefix) labels.

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

The gateway zone implies the URI scheme. For `idr.to`, the scheme is `idrto` and the `idrto:` prefix is **not** part of the encoded input.

Under **idr.to URI grammar v1**, the DEF payload is the **identity locator** only: `<host>~<entity-id>`. Service, port, path, and query are **not** encoded in the DNS label; they appear in the HTTPS URL path after the hostname (see [idr.to URI v1](https://github.com/idrto/api/blob/main/openspec/URI.md)).

A logical URI such as `idrto:laptop.us-east~user@example.com/accounts-db` is carried by:

1. DEF-encoding the payload `laptop.us-east~user@example.com` into the leftmost DNS label, and
2. Placing the service segment (`accounts-db`) in the HTTPS path: `https://<def-label>.idr.to/accounts-db`.

The result is a valid hostname suitable for:

* DNS resolution (A/AAAA records),
* TLS connections with a correct SNI value, and
* HTTPS URLs of the form `https://<encoded-label>.idr.to/...`.

The gateway at `idr.to` receives the TLS connection, reads the SNI hostname, extracts and decodes the embedded label to recover the URI payload, and routes the request (prepending the implied scheme where needed).

DEF does **not** parse URI structure beyond treating the payload as opaque text. For idr.to v1, publishers MUST encode only the identity locator (`host~entity-id`); decoders recover that locator and combine it with the HTTPS path to reconstruct the full idrto URI.

### 12.3 Worked Example (idr.to v1)

Consider an idrto resource with a named service. The scheme is implied by the `idr.to` zone and is not encoded:

```text
Identity locator (DEF payload):
  laptop.us-east~user@example.com
```

Apply DEF to the payload:

| Step | Value |
| ---- | ----- |
| Canonical form | `laptop.us-east~user@example.com` |
| Encoded label (38 characters) | `dlaptop-2eus-2deast-7euser-40example-2ecom` |
| FQDN | `dlaptop-2eus-2deast-7euser-40example-2ecom.idr.to` |
| HTTPS URL | `https://dlaptop-2eus-2deast-7euser-40example-2ecom.idr.to/accounts-db` |

Encoding the payload by segment:

| Segment | Characters | Encoded fragment |
| ------- | ---------- | ---------------- |
| Host | `laptop.us-east` | `laptop-2eus-2deast` |
| Delimiter | `~` | `-7e` |
| Entity | `user@example.com` | `user-40example-2ecom` |

The full logical URI `idrto:laptop.us-east~user@example.com/accounts-db` is recovered by the gateway from the decoded identity locator, the zone-implied `idrto:` scheme, and the HTTPS path segment `accounts-db`.

A client that wishes to access this resource over HTTPS:

1. Takes the identity locator (`host~entity-id`, everything between `idrto:` and the first `/` after the locator) and encodes it with DEF.
2. Forms the FQDN `<encoded>.idr.to`.
3. Opens a TLS connection to that hostname, sending the same hostname in SNI.
4. Issues an HTTP request whose path carries the service-or-port and optional path suffix (for example `/accounts-db`).
5. The server decodes the label to `laptop.us-east~user@example.com`, parses the path for the service segment, and routes using the implied `idrto:` scheme.

### 12.4 Length Constraint

If the DEF body exceeds 62 characters, the encoder uses hash encoding (`h` prefix) instead of reversible DEF. Hash-encoded labels cannot be decoded to recover the original payload; applications that require the plaintext MUST store or transmit it separately.

If a future encoding scheme cannot fit within 63 characters, the encoder MUST reject the input.

### 12.5 Publishing a URI in DNS (Publisher)

Any party MAY take a URI payload, DEF-encode it, and publish the result as a DNS name under a DEF-enabled zone (for example `idr.to`):

1. **Choose the payload.** Strip the scheme prefix if the zone implies it. For `idr.to` v1, encode the identity locator only (`laptop.us-east~user@example.com`), not the service or path segments.
2. **DEF-encode the payload.** Apply §9. Short payloads produce a reversible `d…` label; long payloads produce a deterministic `h…` label.
3. **Publish DNS.** Create a record for `<encoded-label>.idr.to`. The record type depends on how clients reach the service—commonly `A`, `AAAA`, or `CNAME`. A **wildcard** such as `*.idr.to` MAY point many encoded names at one or more gateway addresses without per-label provisioning.

Example publication:

```text
URI:           idrto:laptop.us-east~user@example.com/accounts-db
Payload:       laptop.us-east~user@example.com
DEF label:     dlaptop-2eus-2deast-7euser-40example-2ecom
DNS name:      dlaptop-2eus-2deast-7euser-40example-2ecom.idr.to
HTTPS path:    /accounts-db
DNS record:    *.idr.to.  IN  A     203.0.113.10
               *.idr.to.  IN  AAAA  2001:db8::10
```

The publisher does not need a separate record for each encoded label when a wildcard covers the zone.

### 12.6 Client Experience (Consumer)

A client that wishes to use a published DEF name:

1. **Encode** the identity locator with DEF (same algorithm as the publisher).
2. **Form the hostname** `<encoded-label>.<zone>` (for example `…idr.to`).
3. **Resolve DNS.** Query the hostname (or rely on the zone wildcard). The resolver MAY return multiple `A` and/or `AAAA` addresses.
4. **Choose an address.** The client MAY connect to any returned address; all gateways behind the wildcard are expected to accept any valid DEF hostname for that zone.
5. **Open TLS/HTTP** to that address while sending the full DEF hostname:
   * **SNI** (TLS): the complete hostname (for example `dlaptop-2eus-2deast-7e…idr.to`).
   * **Host header** (HTTP): the same hostname once the connection is established.
   * **Path** (HTTP): the service-or-port and optional path suffix from the idrto URI (for example `/accounts-db` or `/443/v1/health`).

The receiving server (for example **nginx**) terminates TLS and routes using the `Host` header (or SNI during the handshake). It extracts the leftmost label, DEF-decodes it when the prefix is `d`, and recovers the identity locator. It parses the HTTP path for the service segment and reconstructs the full idrto URI. For `h` labels the server cannot decode the payload from DNS alone; it MUST match the label by digest or consult out-of-band state.

Worked client flow for the example above:

```text
1. Payload:     laptop.us-east~user@example.com
2. DEF encode:  dlaptop-2eus-2deast-7euser-40example-2ecom
3. Hostname:    dlaptop-2eus-2deast-7euser-40example-2ecom.idr.to
4. DNS answer:  203.0.113.10, 2001:db8::10        (from *.idr.to)
5. Client picks: 203.0.113.10
6. TLS SNI:     dlaptop-2eus-2deast-7euser-40example-2ecom.idr.to
7. HTTP Host:   dlaptop-2eus-2deast-7euser-40example-2ecom.idr.to
8. HTTP path:   /accounts-db
9. Server decodes label → laptop.us-east~user@example.com
10. Logical URI: idrto:laptop.us-east~user@example.com/accounts-db
```

Wildcard DNS (`*.idr.to`) therefore separates **name discovery** (the unique DEF label in SNI/Host) from **endpoint selection** (which IP address the client connects to).

## 13. Examples

| Original           | Encoded                |
| ------------------ | ---------------------- |
| `alice`            | `dalice`               |
| `Alice`            | `dalice`               |
| `USER@example.COM` | `duser-40example-2ecom` |
| `Laptop.US-East`   | `dlaptop-2eus-2deast`   |
| `alice-1`          | `dalice-2d1`            |
| `ssh`              | `dssh`                  |
| `postgres`         | `dpostgres`             |
| `laptop.us-east~user@example.com` | `dlaptop-2eus-2deast-7euser-40example-2ecom` |
| `用户`               | `d-e7-94-a8-e6-88-b7`   |
| `😊`               | `d-f0-9f-98-8a`         |

Maximum-length reversible DEF example (63 characters including `d` prefix):

```text
Input:  a × 62
Output: d + (a × 62)
```

Hash encoding example (DEF body would exceed 62 characters):

```text
Input:  a × 63
DEF body (before prefix):  a × 63
SHA-256 input:            the DEF body string above (not the raw input)
Output: hfmz7982xfprnqkjav7p0cp7ak3hz0vqeswbb9hqzybd4azew5wt0
```

The `h`-prefixed label is deterministic but not decodable. SHA-256 is always computed over the DEF body produced by byte escaping, never over the raw canonical input alone.

## 14. Conformance

Implementations MUST pass all test vectors in [`vectors/test-vectors.json`](../vectors/test-vectors.json).

## 15. Security Considerations

Decoders MUST reject invalid UTF-8 rather than substitute replacement characters. Encoders and decoders MUST enforce the 63-character label limit to avoid producing or accepting labels invalid on the wire. Decoders MUST reject hash-encoded (`h`) labels rather than returning partial or guessed plaintext.
