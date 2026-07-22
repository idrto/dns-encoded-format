# DNS Encoded Format (DEF)

Transform arbitrary Unicode strings into DNS-friendly ASCII labels (max **63 characters**) and back.

**[Try it in the browser →](https://idrto.github.io/dns-encoded-format-demo/)** — encode a string to a DEF label or FQHN (`idr.to` by default).

The specification lives in [docs/rfc.md](docs/rfc.md). Conformance vectors are in [vectors/test-vectors.json](vectors/test-vectors.json).

Every encoded label begins with an **encoding-type prefix**:

| Prefix | Meaning |
| ------ | ------- |
| `d` | Reversible DEF byte-escape encoding |
| `h` | SHA-256 hash of the DEF body (Base36); **not decodable** |

When the DEF body would exceed 62 characters, the encoder hashes the **DEF body** (after byte escaping, before any prefix) and emits `h` + Base36(SHA-256) (50 digits, left-padded with `0`).

## Why DEF exists

Many resources are identified by URIs whose schemes are not HTTP or HTTPS—`idrto:`, `postgres:`, `ssh:`, and other application-specific schemes. These URIs contain characters (`:`, `@`, `/`, `.`, and more) that cannot appear in a DNS hostname.

DEF lets you take **any URI payload**, encode it into a single DNS label, and publish it under a zone such as `idr.to`. Clients resolve that name, connect over HTTPS, and the gateway recovers the original payload from the hostname.

## Publishing a URI in DNS

1. Take the URI **payload** (without the scheme—for `idr.to`, omit `idrto:`).
2. **DEF-encode** the payload (`d…` if it fits, `h…` if the DEF body is longer than 62 characters).
3. **Publish** `<encoded-label>.idr.to` in DNS, or rely on a zone wildcard.

```text
URI:           idrto:laptop.us-east~user@example.com/accounts-db
Payload:       laptop.us-east~user@example.com
DEF label:     dlaptop-2eus-2deast-7euser-40example-2ecom
DNS name:      dlaptop-2eus-2deast-7euser-40example-2ecom.idr.to

Zone wildcard (example):
  *.idr.to.  IN  A     203.0.113.10
  *.idr.to.  IN  AAAA  2001:db8::10
```

A wildcard covers every encoded label without creating a separate record per URI.

## Client experience

```text
1. Encode payload:  laptop.us-east~user@example.com
                    → dlaptop-2eus-2deast-7euser-40example-2ecom
2. Hostname:        dlaptop-2eus-2deast-7euser-40example-2ecom.idr.to
3. DNS lookup:      returns 203.0.113.10, 2001:db8::10  (from *.idr.to)
4. Connect to:      any returned A or AAAA address
5. TLS SNI:         dlaptop-2eus-2deast-7euser-40example-2ecom.idr.to
6. HTTP Host:       dlaptop-2eus-2deast-7euser-40example-2ecom.idr.to
7. Server (nginx):  reads Host, DEF-decodes label → laptop.us-east~user@example.com
```

The DEF label in **SNI** and the **Host** header identifies *which* URI the client wants. The client may connect to *any* IP returned for `*.idr.to`; the gateway uses the hostname to route, not the chosen address alone.

## URI example (v1 identity locator)

For idr.to relay ingress, the DEF payload is the **identity locator** only (`host~entity-id`). Service and path are carried in the HTTPS URL path.

```text
Identity locator (DEF payload):
  laptop.us-east~user@example.com

Encoded label:
  dlaptop-2eus-2deast-7euser-40example-2ecom

FQDN:
  dlaptop-2eus-2deast-7euser-40example-2ecom.idr.to

HTTPS URL (service in path):
  https://dlaptop-2eus-2deast-7euser-40example-2ecom.idr.to/accounts-db
```

Full idrto URI: `idrto:laptop.us-east~user@example.com/accounts-db` — scheme implied by zone; service/path not encoded in the label.

## Quick example

```text
encode("USER@example.COM")  →  duser-40example-2ecom
decode("duser-40example-2ecom")  →  user@example.com
```

Encoded output uses only `a-z`, `0-9`, and `-`, begins with `d` (reversible) or `h` (hash of DEF body), and MUST NOT exceed 63 characters. Labels starting with `h` cannot be decoded.

JavaScript `encode` is **async** (uses the Web Crypto API for `h…` labels; no Node crypto polyfill in browsers):

```js
import { encode, decode } from "@idrto/dns-encoded-format";

const label = await encode("laptop.us-east~user@example.com");
const host = `${label}.idr.to`;
```

## Packages

| Language | Path | Install |
|----------|------|---------|
| JavaScript/TypeScript | [packages/js](packages/js) | `npm install @idrto/dns-encoded-format` |
| Rust | [packages/rust](packages/rust) | `cargo add dns-encoded-format` |
| Go | [packages/go](packages/go) | `go get github.com/idrto/dns-encoded-format/go/def` |
| Python | [packages/python](packages/python) | `pip install dns-encoded-format` |
| C | [packages/c](packages/c) | vendored library |
| C++ | [packages/cpp](packages/cpp) | header wrapper over C |
| Java | [packages/java](packages/java) | `io.idrto:dns-encoded-format` |
| .NET | [packages/dotnet](packages/dotnet) | `dotnet add package Idrto.DnsEncodedFormat` |

## Development

Run tests per package:

```bash
cd packages/js && npm install && npm test
cd packages/rust && cargo test
cd packages/go/def && go test
cd packages/python && pip install -e ".[dev]" && pytest
cd packages/c && cmake -S . -B build && cmake --build build && ctest --test-dir build
cd packages/java && mvn test
cd packages/dotnet && dotnet test
```

## License

MIT — see [LICENSE](LICENSE).
