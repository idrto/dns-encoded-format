# DNS Encoded Format (DEF)

DEF maps Unicode strings to deterministic ASCII while preserving a generic,
visible structural boundary:

```text
input:  alpha--beta
DEF:    alpha--beta
```

The exact ASCII sequence `--` is the **DEF Structural Separator**. DEF
preserves it but does not know what `alpha` or `beta` mean. Higher-level
protocols define component meaning, count, and validity.

The authoritative specification is [docs/rfc.md](docs/rfc.md). All language
implementations consume the shared
[conformance vectors](vectors/test-vectors.json).

## Layering

```text
Application grammar       assigns component meaning
Carrier/profile rules     apply DNS, URI, or other constraints
DEF Core                  encodes payload and preserves "--"
```

Core supports zero, one, or many separators:

```text
alpha
alpha--beta
alpha--beta--v1
```

Empty Core components are valid. Applications and carrier profiles can impose
stricter rules.

## Payload versus structure

An unescaped `--` is always structural in a DEF representation. To place the
literal characters `--` inside one component, use the component encoder:

```text
encodeComponent("alpha--beta")  →  alpha-2d-2dbeta
```

To construct a structured value safely, encode each component and join with
`--`:

```text
encodeComponent("alpha") + "--" + encodeComponent("beta")
→ alpha--beta
```

Core encoding follows the same rule:

```text
DEF(A--B--C) =
  DEF_COMPONENT(A) + "--" +
  DEF_COMPONENT(B) + "--" +
  DEF_COMPONENT(C)
```

Decoders identify separators first, decode each component exactly once, and
then reassemble the value. Escaped payload is never recursively reinterpreted
as structural syntax.

## Generic API

Names follow each language's conventions:

```text
encodeBody / encode_body
decodeBody / decode_body
encodeComponent / encode_component
decodeComponent / decode_component
```

`encodeBody` and `decodeBody` operate on complete DEF values and preserve
structural separators. Component helpers operate on payload only and therefore
escape every hyphen.

The existing `encodeProfile` / `decodeProfile` APIs apply DNS carrier
constraints and retain the optional deterministic hash fallback for labels
that exceed 63 characters. Existing `encode` / `decode` aliases remain for
source compatibility. Profile APIs do not assign application semantics.

JavaScript profile encoding remains asynchronous because hash fallback uses
Web Crypto:

```js
import {
  decodeBody,
  encodeBody,
  encodeComponent,
} from "@idrto/dns-encoded-format";

const encoded = encodeBody("alpha--user@example.com");
// alpha--user-40example-2ecom

const literalPayload = encodeComponent("alpha--beta");
// alpha-2d-2dbeta

const decoded = decodeBody(encoded);
// alpha--user@example.com
```

## DNS and IDNA

DEF Core does not perform DNS validation, IDNA processing, or Punycode.
The DNS profile enforces its own label length and edge-hyphen constraints.

IDNA-aware applications need additional care. RFC 5890 reserves LDH labels
whose third and fourth characters are `--`; only valid `xn--` A-labels are
usable as IDNA labels in that reserved class. Thus `s1--acme`,
`ab--example`, and `xn--example` cannot be approved merely by checking that
they contain DNS-looking ASCII. See the RFC's
[DNS and IDNA considerations](docs/rfc.md#13-dns-and-idna-considerations).

## Non-normative application examples

Different protocols can independently define:

```text
Protocol A: <left>--<right>
Protocol B: <namespace>--<object>--<version>
idr.to:     <host>--<entity>
```

Those names and arity rules belong to the application grammar, not DEF.

## Compatibility

Version 2 changes Core handling of logical `--`:

```text
Version 1 opaque encoding:      foo-2d-2dbar
Version 2 structural encoding:  foo--bar
Version 2 literal component:    foo-2d-2dbar
```

Canonical version 1 representations remain decodable as single-component
payload. Applications that relied on the previous profile's first-separator
host/entity behavior must move that rule into their application grammar and
component-encode later literal double hyphens.

## Packages

| Language | Path |
| --- | --- |
| JavaScript/TypeScript | [packages/js](packages/js) |
| Rust | [packages/rust](packages/rust) |
| Go | [packages/go](packages/go) |
| Python | [packages/python](packages/python) |
| C | [packages/c](packages/c) |
| C++ | [packages/cpp](packages/cpp) |
| Java | [packages/java](packages/java) |
| .NET | [packages/dotnet](packages/dotnet) |

## Development

```bash
cd packages/js && npm test
cd packages/rust && cargo test
cd packages/go && go test ./...
cd packages/python && python -m pytest
cd packages/c && cmake -S . -B build && cmake --build build && ctest --test-dir build
cd packages/java && mvn test
cd packages/dotnet && dotnet test Idrto.DnsEncodedFormat.Tests/Idrto.DnsEncodedFormat.Tests.csproj
```

## License

MIT — see [LICENSE](LICENSE).
