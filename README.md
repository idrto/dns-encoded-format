# DNS Encoded Format (DEF)

Transform arbitrary Unicode strings into DNS-friendly ASCII labels (max **63 characters**) and back.

The specification lives in [docs/rfc.md](docs/rfc.md). Conformance vectors are in [vectors/test-vectors.json](vectors/test-vectors.json).

Every encoded label begins with an **encoding-type prefix**:

| Prefix | Meaning |
| ------ | ------- |
| `d` | Reversible DEF byte-escape encoding |
| `h` | SHA-256 hash of the DEF body (Crockford Base32); **not decodable** |

When the DEF body would exceed 62 characters, the encoder hashes the **DEF body** (after byte escaping, before any prefix) and emits `h` + Crockford Base32(SHA-256).

## Why DEF exists

Many resources are identified by URIs whose schemes are not HTTP or HTTPS—`idrto:`, `postgres:`, `ssh:`, and other application-specific schemes. These URIs contain characters (`:`, `@`, `/`, `.`, and more) that cannot appear in a DNS hostname.

DEF lets you take **any URI payload**, encode it into a single DNS label, and publish it under a zone such as `idr.to`. Clients resolve that name, connect over HTTPS, and the gateway recovers the original payload from the hostname.

## Publishing a URI in DNS

1. Take the URI **payload** (without the scheme—for `idr.to`, omit `idrto:`).
2. **DEF-encode** the payload (`d…` if it fits, `h…` if the DEF body is longer than 62 characters).
3. **Publish** `<encoded-label>.idr.to` in DNS, or rely on a zone wildcard.

```text
URI:           idrto:user@example.com/db1.us-east/accounts-db
Payload:       user@example.com/db1.us-east/accounts-db
DEF label:     duser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb
DNS name:      duser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb.idr.to

Zone wildcard (example):
  *.idr.to.  IN  A     203.0.113.10
  *.idr.to.  IN  AAAA  2001:db8::10
```

A wildcard covers every encoded label without creating a separate record per URI.

## Client experience

```text
1. Encode payload:  user@example.com/db1.us-east/accounts-db
                    → duser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb
2. Hostname:        duser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb.idr.to
3. DNS lookup:      returns 203.0.113.10, 2001:db8::10  (from *.idr.to)
4. Connect to:      any returned A or AAAA address
5. TLS SNI:         duser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb.idr.to
6. HTTP Host:       duser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb.idr.to
7. Server (nginx):  reads Host, DEF-decodes label → user@example.com/db1.us-east/accounts-db
```

The DEF label in **SNI** and the **Host** header identifies *which* URI the client wants. The client may connect to *any* IP returned for `*.idr.to`; the gateway uses the hostname to route, not the chosen address alone.

## URI example with path slashes

```text
URI payload (encoded):
  user@example.com/db1.us-east/accounts-db

Encoded label:
  duser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb

FQDN:
  duser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb.idr.to

HTTPS URL:
  https://duser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb.idr.to
```

The logical URI `idrto:user@example.com/db1.us-east/accounts-db` is identified by encoding only the payload above. The `idrto:` scheme is implied by the `idr.to` zone, not included in the DEF input.

## Quick example

```text
encode("USER@example.COM")  →  duser-40example-2ecom
decode("duser-40example-2ecom")  →  user@example.com
```

Encoded output uses only `a-z`, `0-9`, and `-`, begins with `d` (reversible) or `h` (hash of DEF body), and MUST NOT exceed 63 characters. Labels starting with `h` cannot be decoded.

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
