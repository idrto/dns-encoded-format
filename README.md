# DNS Encoded Format (DEF)

Transform arbitrary Unicode strings into DNS-friendly ASCII labels (max **63 characters**) and back.

The specification lives in [docs/rfc.md](docs/rfc.md). Conformance vectors are in [vectors/test-vectors.json](vectors/test-vectors.json).

## Why DEF exists

Many resources are identified by URIs whose schemes are not HTTP or HTTPS—`idrto:`, `postgres:`, `ssh:`, and other application-specific schemes. These URIs contain characters (`:`, `@`, `/`, `.`, and more) that cannot appear in a DNS hostname.

To reach such resources through HTTPS infrastructure, the URI must be carried as a **fully qualified domain name (FQDN)** that a TLS client can resolve and advertise in the **Server Name Indication (SNI)** header during the handshake. SNI tells the server which hostname the client intends to reach, allowing it to present the right certificate and route the connection.

DEF solves this by encoding the entire URI string into a single DNS label. That label becomes a subdomain of a gateway zone (for example `idr.to`), producing a valid hostname and HTTPS URL:

```text
<encoded-label>.idr.to
```

A gateway at `idr.to` accepts the TLS connection, reads SNI, decodes the label, and recovers the original URI for routing—regardless of whether the original scheme was `idrto:`, `postgres:`, or anything else.

## URI example with path slashes

```text
Original URI:
  idrto:user@example.com/db1.us-east/accounts-db

Encoded label:
  idrto-3auser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb

FQDN:
  idrto-3auser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb.idr.to

HTTPS URL:
  https://idrto-3auser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb.idr.to
```

The client encodes the URI, connects to the FQDN over TLS with matching SNI, and the server decodes the label back to `idrto:user@example.com/db1.us-east/accounts-db`.

DEF treats the URI as opaque text—it does not parse scheme, host, or path separately. Slashes, dots, and at-signs are escaped as `-2f`, `-2e`, and `-40` respectively.

## Quick example

```text
encode("USER@example.COM")  →  user-40example-2ecom
decode("user-40example-2ecom")  →  user@example.com
```

Encoded output uses only `a-z`, `0-9`, and `-`, and MUST NOT exceed 63 characters.

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
