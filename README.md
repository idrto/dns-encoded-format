# DNS Encoded Format (DEF)

Transform arbitrary Unicode strings into DNS-friendly ASCII labels (max **63 characters**) and back.

**[Try it in the browser →](https://idrto.github.io/dns-encoded-format-demo/)** — encode an idr.to identity locator to a profile label or FQHN (`idr.to` by default).

The specification lives in [docs/rfc.md](docs/rfc.md). Conformance vectors are in [vectors/test-vectors.json](vectors/test-vectors.json).

## API layers

| Layer | Purpose |
| ----- | ------- |
| **DEF body** | Context-free byte escape for opaque strings (`encodeBody` / `decodeBody`) |
| **Service Provider Profile** | Structured locator `<host>--<entity>` with hash fallback (`encodeProfile` / `decodeProfile`) |
| **idr.to shortcuts** | Profile with marker `idrto-h1--` (`encode` / `decode`) |

Global host constraints (§5.1): host MUST NOT be `xn` (reserved for IDNA A-label prefix) or `idrto-h1` (the marker host prefix for `idrto-h1--`).

Reversible profile labels preserve the first literal `--` separator. Labels longer than 63 characters, or labels beginning with `xn--`, use profile hash encoding:

```text
idrto-h1--<50-base36-sha256>
```

Hash labels are deterministic but not decodable from DNS alone.

## Why DEF exists

Many resources are identified by URIs whose schemes are not HTTP or HTTPS—`idrto:`, `postgres:`, `ssh:`, and other application-specific schemes. These URIs contain characters (`:`, `@`, `/`, `.`, and more) that cannot appear in a DNS hostname.

DEF lets you take an **identity locator**, encode it into a single DNS label, and publish it under a zone such as `idr.to`. Clients resolve that name, connect over HTTPS, and the gateway recovers the original locator from the hostname.

## Publishing a URI in DNS

1. Take the **identity locator** (`host--entity-id`; for `idr.to`, omit `idrto:`).
2. **Profile-encode** the locator (`encode` / `encodeProfile`).
3. **Publish** `<profile-label>.idr.to` in DNS, or rely on a zone wildcard.

```text
URI:           idrto:laptop.us-east--user@example.com/accounts-db
Payload:       laptop.us-east--user@example.com
Profile label: laptop-2eus-2deast--user-40example-2ecom
DNS name:      laptop-2eus-2deast--user-40example-2ecom.idr.to

Zone wildcard (example):
  *.idr.to.  IN  A     203.0.113.10
  *.idr.to.  IN  AAAA  2001:db8::10
```

## Client experience

```text
1. Encode payload:  laptop.us-east--user@example.com
                    → laptop-2eus-2deast--user-40example-2ecom
2. Hostname:        laptop-2eus-2deast--user-40example-2ecom.idr.to
3. DNS lookup:      returns 203.0.113.10, 2001:db8::10  (from *.idr.to)
4. Connect to:      any returned A or AAAA address
5. TLS SNI:         laptop-2eus-2deast--user-40example-2ecom.idr.to
6. HTTP Host:       laptop-2eus-2deast--user-40example-2ecom.idr.to
7. Server (nginx):  reads Host, decodes label → laptop.us-east--user@example.com
```

## Quick example

```text
encodeBody("USER@example.COM")  →  user-40example-2ecom
decodeBody("user-40example-2ecom")  →  user@example.com

encode("s--acme")  →  s--acme
decode("s--acme")  →  s--acme
```

JavaScript `encode` / `encodeProfile` are **async** (Web Crypto for profile hash labels):

```js
import { encode, decode, encodeBody, decodeBody } from "@idrto/dns-encoded-format";

const label = await encode("laptop.us-east--user@example.com");
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
