# DNS Encoded Format (DEF)

Transform arbitrary Unicode strings into DNS-friendly ASCII labels (max **63 characters**) and back.

The specification lives in [docs/rfc.md](docs/rfc.md). Conformance vectors are in [vectors/test-vectors.json](vectors/test-vectors.json).

A primary use case is encoding a URI (or URI-like string) into a single valid DNS label while preserving UTF-8 through encode/decode. DEF does not parse URI structure; it encodes the full string.

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
