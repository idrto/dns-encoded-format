# RFC: DNS Encoded Format

**Version:** 1.3 (Draft)

## 1. Purpose

This document defines the **DNS Encoded Format (DEF)** used by idr.to to transform arbitrary Unicode strings into a DNS-friendly ASCII representation suitable for use as a single DNS label. It also defines an optional Service Provider Profile for providers that need to retain one structural separator in a DEF-derived label.

The transformation is intended for identifier components such as entity IDs, host names, service names, labels, and similar fields. A primary use case is encoding a URI (or URI-like string) into one valid DNS label while preserving UTF-8 text through encoding and decoding.

The encoding is deterministic and simple to parse.

## 2. Design Goals

* DNS-friendly representation as a single label.
* Simple implementation.
* Context-free parsing.
* Uniform algorithm for all identifier components.
* Supports arbitrary UTF-8 input within the DNS label length limit, using a profile hash fallback when a reversible profile label does not fit.
* Canonicalizes ASCII letters to lowercase.
* Escapes all characters except lowercase letters and decimal digits.

## 3. DNS Label Length Limit

A DNS label MUST NOT exceed **63 octets** ([RFC 1035](https://www.rfc-editor.org/rfc/rfc1035)).

Because DEF output is ASCII, one output character equals one octet. An encoded string MUST NOT exceed 63 characters.

Implementations MUST enforce this limit:

* **Encoding:** The final encoded label (including any configured profile marker) MUST NOT exceed 63 characters.
* **Decoding:** If the encoded input exceeds 63 characters, the decoder MUST reject the input.

This limit applies to the encoded form, not necessarily to the decoded Unicode string length.

### 3.1 Base36 Digest Encoding

The Service Provider Profile (§12) uses SHA-256 digests encoded as lowercase Base36 strings. A digest is:

* the SHA-256 hash of a specified ASCII input string, interpreted as UTF-8 octets;
* converted to Base36 using the lowercase alphabet `0123456789abcdefghijklmnopqrstuvwxyz`;
* interpreted as a big-endian unsigned integer, converted by repeated division by 36; and
* left-padded with `0` to exactly **50** characters (no other padding characters).

A SHA-256 digest always encodes to 50 Base36 characters. Profile hash labels append this digest to a configured `provider_hash_marker` (§12.3).

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

### 5.1 Host Component Constraints

Some DEF uses treat one component as a **host** joined to an entity with the literal sequence `--`. A host value MUST satisfy these global constraints:

* It MUST NOT be `xn`. A host of `xn` would produce a profile label beginning with `xn--`, which is reserved for IDNA A-labels.
* It MUST NOT equal the configured `provider_hash_marker` with its terminal `--` removed. Each service provider defines its own hash marker (for example `idrto-h1--`); the corresponding host value `idrto-h1` is reserved so reversible locators cannot be confused with profile hash labels.

These constraints apply to canonical host values before DEF byte escaping. Encoders MUST reject locators whose host violates either rule. Decoders MUST reject recovered locators whose decoded host violates either rule.

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

DEF_BODY = *( LITERAL / ESCAPE )
```

## 9. Encoding Algorithm

Given an input Unicode string:

1. Convert every ASCII uppercase letter (`A`–`Z`) to lowercase.
2. Encode the resulting string as UTF-8.
3. For each UTF-8 byte:
   * If the byte represents ASCII `a`–`z` or `0`–`9`, emit it literally.
   * Otherwise emit `-` followed by exactly two lowercase hexadecimal digits.
4. Concatenate the emitted tokens into a **DEF body** and return it.

## 10. Decoding Algorithm

Given a DEF body:

1. If the input is empty, return the empty string.
2. Initialize an empty byte buffer.
3. Scan the DEF body left to right.
   * If the next character is `a`–`z` or `0`–`9`, append its ASCII byte.
   * If the next character is `-`, read exactly the next two hexadecimal digits, convert them into one byte, and append it.
   * Continue until the end of the DEF body.
4. Decode the accumulated bytes as UTF-8.

If:

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
* DNS-friendly as profile labels of at most 63 octets, and
* canonical with respect to ASCII letter case.

Because of lowercase canonicalization:

```
decode(encode(x)) == lowercase_ascii(x)
```

where `lowercase_ascii(x)` denotes `x` after converting only ASCII `A`–`Z` to `a`–`z`.

## 12. Service Provider Profile

### 12.1 Purpose and Configuration

Base DEF (§§4–10) is context-free: it encodes one opaque Unicode string and does not interpret its contents. A service provider that needs a label with two structured components MAY use the **Service Provider Profile** defined in this section. This profile is parameterized by a `provider_hash_marker`; it does not change the base DEF algorithm.

`provider_hash_marker` is an explicit wire-format discriminator for the profile's hash labels. It MUST:

* contain only lowercase ASCII letters, digits, and hyphens;
* end in the literal sequence `--`;
* be at least 3 and at most 13 characters long;
* NOT begin with `xn--`;
* be unique within the provider's DNS namespace; and
* reserve its host prefix: a profile host MUST NOT equal the marker with its terminal `--` removed (for `idrto-h1--`, the reserved host is `idrto-h1`).

The 13-character maximum leaves room for the fixed 50-character Base36 SHA-256 digest within the 63-octet DNS label limit. A provider SHOULD include a format or algorithm version in the marker. This document uses `idrto-h1--` as the idr.to marker; its reserved host prefix is `idrto-h1`.

### 12.2 Profile Input and Reversible Encoding

The profile input is a locator that is canonicalized before encoding:

```text
<host>--<entity>
```

`host` and `entity` MUST both be non-empty. After canonicalization, `host` MUST begin with ASCII `a-z` or `0-9` so the resulting DNS label does not begin with a hyphen. `host` MUST NOT contain `--`. `host` MUST satisfy the global host constraints in §5.1 (`xn` and the configured marker host prefix are forbidden). `entity` MAY contain `--`. The first occurrence of `--` is the sole structural separator.

To encode a locator reversibly:

1. Canonicalize the entire locator according to §4.
2. Split the canonical locator at its first `--` into `host` and `entity`. Reject the input if either component is empty or if `host` violates §5.1.
3. Apply DEF byte escaping (§§6–7) independently to `host` and `entity`, producing `host-body` and `entity-body`.
4. Emit `host-body + "--" + entity-body`.
5. If this label is 63 characters or fewer, return it. Otherwise use profile hash encoding (§12.3).

The profile preserves only the first structural `--`. Every hyphen inside either component is escaped by base DEF. Therefore a later `--` in `entity` is emitted as `-2d-2d`.

For example:

```text
Input:   api-v2--acme--production
Output:  api-2dv2--acme-2d-2dproduction
```

### 12.3 Profile Hash Encoding

When the reversible profile label would exceed 63 characters, the encoder MUST emit:

```text
<provider_hash_marker><base36-sha256>
```

The digest is SHA-256 of the UTF-8 bytes of the ordinary full DEF body of the canonical locator. In that ordinary DEF body, the structural `--` is represented as `-2d-2d`; the preserved profile separator is not used as hash input. Convert the digest to exactly 50 lowercase Base36 characters as specified in §3.1.

For the idr.to marker, a profile hash label is:

```text
idrto-h1--<50-base36-sha256>
```

It is 60 characters long. Profile hash labels are one-way and MUST be resolved through provider state or a registry; they cannot recover the host or entity from DNS alone.

For example, a locator comprising 31 `a` characters, `--`, and 31 `b` characters has a 68-character ordinary DEF body:

```text
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-2d-2dbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
```

Its idr.to profile hash label is:

```text
idrto-h1--3mug7fre5lope7l4qxk6cb7wrekhkhk0ey25dr5d9z3bg7tbqa
```

### 12.4 Profile Decoding and Dispatch

Given a configured `provider_hash_marker` and an input label:

1. Reject the label if it exceeds 63 characters.
2. If the label starts with the exact `provider_hash_marker`, require exactly 50 following lowercase Base36 characters. If valid, reject it as `not_decodable`; otherwise reject it as `invalid_encoding`.
3. Otherwise, reject the label if it begins with `xn--`.
4. Find the first literal `--`. Reject the label if none exists.
5. Treat the text before that separator as `host-body` and all text after it as `entity-body`. Decode each body independently according to §10.
6. Reject the label if either body is empty, invalid, or decodes to a host that violates §5.1 or contains `--`.
7. Reconstruct the canonical locator as `host + "--" + entity`.

The first literal `--` is unambiguous: a base DEF escape is always `-` followed by two hexadecimal digits and cannot contain `--`.

### 12.5 Profile Grammar (ABNF)

```text
PROFILE_BODY = 1*( LITERAL / ESCAPE )
HOST_BODY = LITERAL *( LITERAL / ESCAPE )
PROFILE_LABEL = HOST_BODY "--" PROFILE_BODY
PROFILE_HASH = PROVIDER_HASH_MARKER 50BASE36

; Configured out of band. It meets the constraints in §12.1.
PROVIDER_HASH_MARKER = LITERAL *10( LITERAL / "-" ) "--"
```

`PROFILE_LABEL` is additionally constrained by §12.2 and §5.1: its decoded host MUST NOT be `xn`, MUST NOT equal the configured marker host prefix, and MUST NOT contain `--`.

### 12.6 Security and Interoperability

Providers MUST configure the same `provider_hash_marker` at all encoders, gateways, and registries for a DNS namespace. Parsers MUST test the marker before attempting reversible profile parsing, and MUST NOT fall back to a reversible interpretation after a malformed marker-prefixed label.

Providers MUST reserve their marker host prefix to prevent a direct locator from being confused with a profile hash label. Host `xn` is globally forbidden (§5.1) so valid profile labels never begin with `xn--`. Profile hash labels are identifiers, not authenticated assertions: applications requiring integrity or authorization MUST enforce those properties separately. As with base DEF, implementations MUST enforce the DNS length limit and reject malformed escapes and invalid UTF-8.

## 13. URI Use Case: FQDNs for HTTPS and SNI

### 13.1 Problem

Many systems identify resources with URIs whose schemes are not HTTP or HTTPS—for example `idrto:`, `postgres:`, `ssh:`, or custom application schemes. These URIs can contain characters (`:`, `@`, `/`, `.`, `-`, and others) that are not valid in a DNS hostname.

HTTPS, however, is built on DNS and TLS. A client connecting over TLS sends the target hostname in the **Server Name Indication (SNI)** extension during the handshake ([RFC 6066](https://www.rfc-editor.org/rfc/rfc6066)). The server uses that hostname to select the correct certificate and route the request.

To reach a gateway or proxy over HTTPS while carrying a non-HTTP URI, the URI must be represented as a hostname the client can resolve, connect to, and advertise in SNI.

### 13.2 Solution

DEF, or a configured Service Provider Profile built on DEF, maps a URI **payload** (the URI body without its scheme prefix) to a single DNS label. That label is placed as the leftmost label of a fully qualified domain name (FQDN) under a controlled zone (for example `idr.to`):

```text
<encoded-label>.idr.to
```

The gateway zone implies the URI scheme. For `idr.to`, the scheme is `idrto` and the `idrto:` prefix is **not** part of the encoded input.

Under **idr.to URI grammar v1**, the payload is the **identity locator** only: `<host>--<entity-id>`. idr.to uses the Service Provider Profile (§12) with `provider_hash_marker` set to `idrto-h1--`. Service, port, path, and query are **not** encoded in the DNS label; they appear in the HTTPS URL path after the hostname (see [idr.to URI v1](https://github.com/idrto/api/blob/main/openspec/URI.md)).

A logical URI such as `idrto:laptop.us-east--user@example.com/accounts-db` is carried by:

1. Applying the idr.to Service Provider Profile to `laptop.us-east--user@example.com` and placing the resulting label in the leftmost DNS position, and
2. Placing the service segment (`accounts-db`) in the HTTPS path: `https://<def-label>.idr.to/accounts-db`.

The result is a valid hostname suitable for:

* DNS resolution (A/AAAA records),
* TLS connections with a correct SNI value, and
* HTTPS URLs of the form `https://<encoded-label>.idr.to/...`.

The gateway at `idr.to` receives the TLS connection, reads the SNI hostname, extracts and decodes the embedded label to recover the URI payload, and routes the request (prepending the implied scheme where needed).

Base DEF does **not** parse URI structure beyond treating its input as opaque text. The idr.to Service Provider Profile interprets its own first `--` separator. For idr.to v1, publishers MUST encode only the identity locator (`host--entity-id`); decoders recover that locator and combine it with the HTTPS path to reconstruct the full idrto URI.

### 13.3 Worked Example (idr.to v1)

Consider an idrto resource with a named service. The scheme is implied by the `idr.to` zone and is not encoded:

```text
Identity locator (profile input):
  laptop.us-east--user@example.com
```

Apply the idr.to Service Provider Profile:

| Step | Value |
| ---- | ----- |
| Canonical form | `laptop.us-east--user@example.com` |
| Encoded label (40 characters) | `laptop-2eus-2deast--user-40example-2ecom` |
| FQDN | `laptop-2eus-2deast--user-40example-2ecom.idr.to` |
| HTTPS URL | `https://laptop-2eus-2deast--user-40example-2ecom.idr.to/accounts-db` |

Encoding the payload by segment:

| Segment | Characters | Encoded fragment |
| ------- | ---------- | ---------------- |
| Host | `laptop.us-east` | `laptop-2eus-2deast` |
| Delimiter | first `--` | `--` |
| Entity | `user@example.com` | `user-40example-2ecom` |

For an entity such as `user--ops`, its internal separator is ordinary DEF data and encodes as `user-2d-2dops`.

The full logical URI `idrto:laptop.us-east--user@example.com/accounts-db` is recovered by the gateway from the decoded identity locator, the zone-implied `idrto:` scheme, and the HTTPS path segment `accounts-db`.

A client that wishes to access this resource over HTTPS:

1. Takes the identity locator (`host--entity-id`, everything between `idrto:` and the first `/` after the locator) and encodes it with the idr.to Service Provider Profile.
2. Forms the FQDN `<encoded>.idr.to`.
3. Opens a TLS connection to that hostname, sending the same hostname in SNI.
4. Issues an HTTP request whose path carries the service-or-port and optional path suffix (for example `/accounts-db`).
5. The server decodes the label to `laptop.us-east--user@example.com`, parses the path for the service segment, and routes using the implied `idrto:` scheme.

### 13.4 Length Constraint

If the reversible profile label exceeds 63 characters, the encoder uses profile hash encoding with the `idrto-h1--` marker. Hash-encoded labels cannot be decoded to recover the original payload; applications that require the plaintext MUST store or transmit it separately.

If a future encoding scheme cannot fit within 63 characters, the encoder MUST reject the input.

### 13.5 Publishing a URI in DNS (Publisher)

Any party MAY take a URI payload, apply its configured Service Provider Profile, and publish the result as a DNS name under a DEF-enabled zone (for example `idr.to`):

1. **Choose the payload.** Strip the scheme prefix if the zone implies it. For `idr.to` v1, encode the identity locator only (`laptop.us-east--user@example.com`), not the service or path segments.
2. **Encode the payload.** Apply §12 with the `idrto-h1--` marker. Short payloads produce a reversible profile label; long payloads produce a deterministic `idrto-h1--…` label.
3. **Publish DNS.** Create a record for `<encoded-label>.idr.to`. The record type depends on how clients reach the service—commonly `A`, `AAAA`, or `CNAME`. A **wildcard** such as `*.idr.to` MAY point many encoded names at one or more gateway addresses without per-label provisioning.

Example publication:

```text
URI:           idrto:laptop.us-east--user@example.com/accounts-db
Payload:       laptop.us-east--user@example.com
Profile label: laptop-2eus-2deast--user-40example-2ecom
DNS name:      laptop-2eus-2deast--user-40example-2ecom.idr.to
HTTPS path:    /accounts-db
DNS record:    *.idr.to.  IN  A     203.0.113.10
               *.idr.to.  IN  AAAA  2001:db8::10
```

The publisher does not need a separate record for each encoded label when a wildcard covers the zone.

### 13.6 Client Experience (Consumer)

A client that wishes to use a published idr.to name:

1. **Encode** the identity locator with the configured idr.to Service Provider Profile (the same configuration as the publisher).
2. **Form the hostname** `<encoded-label>.<zone>` (for example `…idr.to`).
3. **Resolve DNS.** Query the hostname (or rely on the zone wildcard). The resolver MAY return multiple `A` and/or `AAAA` addresses.
4. **Choose an address.** The client MAY connect to any returned address; all gateways behind the wildcard are expected to accept any valid profile hostname for that zone.
5. **Open TLS/HTTP** to that address while sending the full DEF hostname:
   * **SNI** (TLS): the complete hostname (for example `laptop-2eus-2deast--user-40…idr.to`).
   * **Host header** (HTTP): the same hostname once the connection is established.
   * **Path** (HTTP): the service-or-port and optional path suffix from the idrto URI (for example `/accounts-db` or `/443/v1/health`).

The receiving server (for example **nginx**) terminates TLS and routes using the `Host` header (or SNI during the handshake). It extracts the leftmost label and applies the idr.to profile decoder to recover the identity locator. It parses the HTTP path for the service segment and reconstructs the full idrto URI. For `idrto-h1--` labels the server cannot decode the payload from DNS alone; it MUST match the label by digest or consult out-of-band state.

Worked client flow for the example above:

```text
1. Payload:     laptop.us-east--user@example.com
2. Profile encode: laptop-2eus-2deast--user-40example-2ecom
3. Hostname:    laptop-2eus-2deast--user-40example-2ecom.idr.to
4. DNS answer:  203.0.113.10, 2001:db8::10        (from *.idr.to)
5. Client picks: 203.0.113.10
6. TLS SNI:     laptop-2eus-2deast--user-40example-2ecom.idr.to
7. HTTP Host:   laptop-2eus-2deast--user-40example-2ecom.idr.to
8. HTTP path:   /accounts-db
9. Server decodes label → laptop.us-east--user@example.com
10. Logical URI: idrto:laptop.us-east--user@example.com/accounts-db
```

Wildcard DNS (`*.idr.to`) therefore separates **name discovery** (the unique profile label in SNI/Host) from **endpoint selection** (which IP address the client connects to).

## 14. Examples

DEF body examples:

| Original           | DEF body               |
| ------------------ | ---------------------- |
| `alice`            | `alice`                |
| `Alice`            | `alice`                |
| `USER@example.COM` | `user-40example-2ecom` |
| `Laptop.US-East`   | `laptop-2eus-2deast`   |
| `alice-1`          | `alice-2d1`            |
| `ssh`              | `ssh`                  |
| `postgres`         | `postgres`             |
| `用户`               | `-e7-94-a8-e6-88-b7`   |
| `😊`               | `-f0-9f-98-8a`         |

idr.to Service Provider Profile examples:

| Locator | Profile label |
| ------- | ------------- |
| `s--acme` | `s--acme` |
| `laptop.us-east--user@example.com` | `laptop-2eus-2deast--user-40example-2ecom` |
| `api-v2--acme--production` | `api-2dv2--acme-2d-2dproduction` |

Profile hash example (reversible label exceeds 63 characters):

```text
Locator:    a × 31 + "--" + b × 31
DEF body:   a × 31 + "-2d-2d" + b × 31
Profile label: idrto-h1--3mug7fre5lope7l4qxk6cb7wrekhkhk0ey25dr5d9z3bg7tbqa
```

Profile hash labels are deterministic but not decodable. SHA-256 is always computed over the ordinary full DEF body of the canonical locator, never over the raw input alone.

## 15. Conformance

Implementations MUST pass all test vectors in [`vectors/test-vectors.json`](../vectors/test-vectors.json) for core DEF body encoding/decoding and Service Provider Profile encoding/decoding.

## 16. Security Considerations

Decoders MUST reject invalid UTF-8 rather than substitute replacement characters. Encoders and decoders MUST enforce the 63-character label limit to avoid producing or accepting labels invalid on the wire. Decoders MUST reject profile hash labels rather than returning partial or guessed plaintext.
