# RFC: DNS Encoded Format

**Version:** 2.0 (Draft)

## 1. Status and Requirements Language

This document defines the DNS Encoded Format (DEF). The key words **MUST**,
**MUST NOT**, **REQUIRED**, **SHALL**, **SHALL NOT**, **SHOULD**, **SHOULD
NOT**, **RECOMMENDED**, **NOT RECOMMENDED**, **MAY**, and **OPTIONAL** in this
document are to be interpreted as described in BCP 14
([RFC 2119](https://www.rfc-editor.org/rfc/rfc2119) and
[RFC 8174](https://www.rfc-editor.org/rfc/rfc8174)) when, and only when, they
appear in all capitals.

## 2. Scope and Architecture

DEF is a deterministic mapping between Unicode strings and an ASCII
representation. It canonicalizes ASCII uppercase letters, preserves structural
boundaries, and escapes component payload as UTF-8 bytes.

DEF is layered:

```text
Application Grammar
        |
Carrier or Usage Profile
        |
DEF Core
```

**DEF Core** defines encoding, decoding, reserved syntax, and the semantically
neutral `--` structural token. A **DEF Profile** applies Core output to a
carrier or environment, such as a DNS label. An **Application Grammar** assigns
meaning and arity to DEF Components.

DEF Core does not know whether a component identifies a host, entity, tenant,
namespace, package, resource, version, service, organization, user, device, or
any other application concept.

The defining structural property is:

```text
DEF(A -- B) = DEF_COMPONENT(A) -- DEF_COMPONENT(B)

DEF(A -- B -- C) =
    DEF_COMPONENT(A) -- DEF_COMPONENT(B) -- DEF_COMPONENT(C)
```

where `A`, `B`, and `C` are component payloads, not semantic types.

## 3. Terminology

### 3.1 DEF Core

The protocol-independent canonicalization, component encoding, structural
serialization, and decoding rules in Sections 4 through 10.

### 3.2 DEF Structural Separator

The exact two-character ASCII sequence `--`. It is reserved DEF syntax. DEF
preserves it but assigns no meaning to the fields on either side.

### 3.3 DEF Component

A contiguous portion of a DEF value bounded by the beginning of the value, the
end of the value, or a DEF Structural Separator.

For example, `alpha--beta--gamma` has the three DEF Components `alpha`,
`beta`, and `gamma`.

### 3.4 DEF Payload

The data contained in one DEF Component. Payload is encoded independently of
payload in every other component.

### 3.5 DEF Reserved Syntax

Literal syntax interpreted by DEF itself. In this version, an unescaped `--`
is reserved as the DEF Structural Separator. An escape token is also DEF
syntax, but the byte represented by that token is payload.

### 3.6 DEF Profile

A specification that applies DEF Core to a carrier or environment. A profile
can impose limits that Core does not, such as the DNS 63-octet label limit.
A profile MUST NOT silently assign application semantics to DEF Components.

### 3.7 Application Grammar

A higher-level grammar that assigns component meanings, count, order, and
validity rules. Application grammar is outside DEF Core.

## 4. Canonicalization and Encoding Units

Before encoding, every ASCII uppercase letter (`A` through `Z`) SHALL be
converted to its lowercase equivalent (`a` through `z`). No other Unicode case
mapping or normalization is performed.

Each component's canonical form SHALL be encoded as UTF-8. Component encoding
operates independently on each UTF-8 byte.

Canonicalization is irreversible. Decoding produces the ASCII-lowercased form:

```text
decode(encode(x)) == lowercase_ascii(x)
```

## 5. Component Payload Encoding

The ASCII bytes for `a` through `z` and `0` through `9` SHALL be emitted
unchanged. Every other byte MUST be emitted as:

```text
-hh
```

where `hh` is exactly two lowercase hexadecimal digits.

Examples:

| Payload | Component representation |
| --- | --- |
| `abc` | `abc` |
| `a-b` | `a-2db` |
| `@` | `-40` |
| `é` | `-c3-a9` |
| `😊` | `-f0-9f-98-8a` |
| `--` | `-2d-2d` |

An encoder intended specifically for one component (called
`encodeComponent` in the reference APIs) MUST encode every hyphen as `-2d`.
It MUST NOT emit an unescaped `--`.

## 6. DEF Structural Separator

The ASCII sequence `--` is reserved by DEF as the **DEF Structural
Separator**.

1. A DEF encoder MUST recognize each exact ASCII `--` in a structural input
   value as a DEF Structural Separator.
2. A DEF encoder MUST preserve each DEF Structural Separator verbatim.
3. A DEF decoder MUST preserve the position and occurrence of every structural
   separator.
4. DEF Core MUST NOT assign application-specific meaning to components.
5. DEF Core MUST permit zero, one, or multiple separators.
6. Component count, ordering, arity, and semantics MUST be defined by
   higher-level specifications, not by DEF Core.

Recognition is greedy from left to right. Therefore:

```text
a---b   = a -- -b
a----b  = a -- "" -- b
a-----b = a -- "" -- -b
```

The hyphens that are payload are escaped during encoding.

### 6.1 Empty Components

DEF Core permits empty components. Consequently, all of the following are
valid Core values:

```text
--a       components: "", "a"
a--       components: "a", ""
--        components: "", ""
a----b    components: "a", "", "b"
----      components: "", "", ""
```

This keeps Core semantically neutral. A profile or application grammar MAY
prohibit empty components.

### 6.2 Literal `--` in Payload

An unescaped literal `--` in a valid DEF representation always denotes a DEF
Structural Separator and MUST NOT denote ordinary component payload.

An application that needs the literal characters `--` inside one component
MUST component-encode that payload. The existing byte escape is sufficient:

```text
payload:                  --
component representation: -2d-2d
```

For example, the one-component payload `alpha--beta` is represented as
`alpha-2d-2dbeta`. In contrast, the two-component structural value is
represented as `alpha--beta`.

Because an unannotated input string cannot distinguish those two intentions,
callers constructing structure SHOULD component-encode each payload and join
the resulting representations with `--`. The reference libraries expose
generic component helpers for this purpose.

## 7. Formal Grammar

The following ABNF uses the notation of
[RFC 5234](https://www.rfc-editor.org/rfc/rfc5234):

```text
HEXDIG       = %x30-39 / %x61-66
LITERAL      = %x61-7A / %x30-39
ESCAPE       = "-" HEXDIG HEXDIG
COMPONENT    = *(LITERAL / ESCAPE)
SEPARATOR    = "--"
DEF-VALUE    = COMPONENT *(SEPARATOR COMPONENT)
```

Uppercase hexadecimal digits are not canonical and MUST be rejected.

## 8. Encoding Algorithm

Given a structural Unicode input value:

1. Canonicalize ASCII uppercase letters as specified in Section 4.
2. Scan from left to right.
3. If the next two characters are exactly `--`, emit `--` and advance by two.
4. Otherwise, collect component payload up to the next separator, encode that
   payload as UTF-8, and apply Section 5 byte escaping.
5. Continue through the end of input, preserving empty components.

The encoder MUST NOT create a visible `--` while encoding component payload.
Its complexity SHOULD remain linear in the input and output sizes.

## 9. Decoding Algorithm

Decoding MUST use this processing order:

```text
recognize DEF structural syntax
    -> identify encoded components
    -> decode each component payload exactly once
    -> reassemble while preserving separators
```

Given a DEF representation:

1. Scan left to right and identify every exact literal `--` as a separator.
2. Treat the intervening substrings, including empty substrings, as encoded
   components.
3. Decode each non-structural component independently:
   * append literal `a` through `z` and `0` through `9` as ASCII bytes;
   * for `-hh`, append the represented byte;
   * reject any other character or malformed escape.
4. Decode each resulting byte sequence as strict UTF-8.
5. Reassemble decoded components with one `--` for every structural separator.

Bytes produced by decoding an escape MUST NOT be rescanned as DEF syntax.
Thus `-2d-2d` decodes to payload `--`; it does not create a structural
separator during that decoding operation.

A truncated or malformed escape adjacent to a separator remains an error. A
separator cannot hide, terminate, or repair malformed component payload.

## 10. Core Invariants

Conforming implementations MUST maintain these invariants:

1. **Separator preservation:** the number of intentionally structural
   separators before encoding equals the number visible in the representation.
2. **Separator ordering:** structural separators retain their relative order.
3. **Component independence:** each component is encoded as if by
   `encodeComponent`, then joined with `--`.
4. **Semantic neutrality:** output does not depend on application meaning.
5. **No accidental separator creation:** component encoding never emits a raw
   `--`.
6. **Deterministic round trip:**
   `decode(encode(x)) == lowercase_ascii(x)`.

## 11. Separator Transparency

DEF Structural Separators remain directly observable in an encoded
representation without decoding surrounding components:

```text
encoded-component-a--encoded-component-b--encoded-component-c
```

This **Separator Transparency** is intentional. A router, resolver, registry,
proxy, cache, sharding system, or access-control system can locate boundaries
while treating component payload as opaque. Such a system MUST NOT infer
component meaning from DEF itself.

## 12. DNS Profile

This section defines the reference DNS carrier profile. It does not change Core
syntax or component semantics.

### 12.1 DNS Constraints

A reversible profile output:

* MUST be non-empty;
* MUST NOT begin or end with a hyphen;
* MUST contain only the Core ASCII syntax; and
* MUST NOT exceed 63 octets, as required for a DNS label by
  [RFC 1035](https://www.rfc-editor.org/rfc/rfc1035).

Because DEF output is ASCII, characters and octets have equal counts.
The profile permits any Core separator count and permits internal empty
components when the resulting label meets these carrier constraints.

### 12.2 Optional Hash Fallback

The reference APIs retain a configured hash marker for compatibility with
previous releases. The marker MUST contain lowercase ASCII letters, digits,
and hyphens; MUST end in `--`; MUST be 3 through 13 characters; and MUST NOT
begin with `xn--`.

If reversible output exceeds 63 characters, or would begin with the configured
marker, the profile emits:

```text
<marker><base36-sha256>
```

The digest is SHA-256 over the UTF-8 bytes of the component-encoded canonical
full logical input. This deliberately escapes every hyphen in the digest input
and preserves the version 1 hash calculation. The digest is encoded as exactly
50 lowercase Base36 characters using `0123456789abcdefghijklmnopqrstuvwxyz`.

Hash labels are one-way. A decoder MUST dispatch and validate the configured
marker before Core parsing and MUST return `not_decodable` for a valid hash
label. It MUST NOT fall back to reversible parsing after malformed
marker-prefixed input.

The historical default marker is `idrto-h1--`. Its name and application use
are not part of DEF Core.

## 13. DNS and IDNA Considerations

DEF Core syntax, DNS label syntax, and IDNA processing are separate concerns.
Core performs no IDNA or Punycode processing.

Traditional hostname-style LDH labels permit internal hyphens, subject to
carrier length and edge restrictions; see RFC 1035 and
[RFC 1123](https://www.rfc-editor.org/rfc/rfc1123). IDNA adds another
classification. Section 2.3.2.3 of
[RFC 5890](https://www.rfc-editor.org/rfc/rfc5890) classifies an LDH label
having `--` in its third and fourth character positions as a Reserved LDH
(R-LDH) label. In IDNA-aware applications, only the subset beginning with
`xn--` and satisfying all A-label requirements is usable as an A-label.

Section 4.2.3.1 and Section 5.4 of
[RFC 5891](https://www.rfc-editor.org/rfc/rfc5891) specify corresponding
hyphen restrictions for IDNA registration and lookup. Merely checking or
excluding strings beginning with `xn--` is therefore insufficient.

Examples:

* `s1--acme` and `ab--example` place `--` in positions 3 and 4. They are
  R-LDH labels and are not A-labels merely because their remaining characters
  look DNS-compatible.
* `xn--example` has the ACE prefix but is valid for IDNA only if the complete
  label satisfies Punycode and all other A-label requirements.
* `server--acme` does not place the separator in positions 3 and 4, although
  normal DNS profile constraints still apply.

A system that sends DEF profile output through an IDNA-aware API MUST apply the
applicable IDNA policy at the profile boundary. Implementations MUST NOT feed a
DEF ASCII representation through Punycode and assume the structural separator
will remain usable. Deployments needing separator transparency SHOULD use a
DNS path that treats the already-ASCII DEF value as the intended label and
whose policy permits that label.

## 14. Application Grammars (Non-Normative)

One protocol can define:

```text
<component-a>--<component-b>
```

Another can define:

```text
<namespace>--<object>--<version>
```

As an application-specific example, an idr.to naming grammar can define:

```text
<host>--<entity>
```

and use `s1--acme`. The meanings of `host` and `entity`, including whether they
may be empty or contain literal double hyphens, are defined by idr.to and are
not part of DEF.

## 15. Error Handling

A decoder MUST reject:

* truncated or non-lowercase hexadecimal escapes;
* characters outside literals, escapes, and structural separators;
* invalid UTF-8 in any component; and
* profile values violating the selected carrier constraints.

Each component MUST be validated independently. Implementations SHOULD preserve
their existing error types. Profiles MAY impose input, output, component-count,
or resource limits, but those limits MUST be documented as profile rules.

## 16. Security Considerations

All participants interpreting one representation MUST agree that raw `--` is
structural. Treating it as payload in one parser and structure in another can
cause routing, authorization, cache-key, or signature differentials.

Implementations MUST split structural syntax before component decoding and MUST
decode each escape exactly once. Recursive decoding, decoding before splitting,
or rescanning decoded payload can turn escaped data into unintended structure.
Normalization MUST NOT add, remove, or move separators; DEF performs only ASCII
uppercase canonicalization, which cannot affect `--`.

Implementations SHOULD bound total input length, output length, component
count, and per-component work as appropriate to their environment. They MUST
reject malformed escapes and invalid UTF-8 rather than substitute data. DNS
deployments MUST separately enforce the DNS and IDNA policy they claim to use.
Hash profile labels are identifiers, not authenticated assertions.

## 17. Compatibility and Migration

Version 2 reserves raw `--` in Core. This is an encoding-output and parsing
change and is potentially breaking:

* Version 1 Core encoded every payload hyphen, so logical `foo--bar` became
  `foo-2d-2dbar`.
* Version 2 structural encoding of `foo--bar` is `foo--bar`.
* Version 1 profile encoding preserved only its first application-specific
  separator. Version 2 Core preserves every structural separator.

Existing canonical version 1 Core representations remain valid version 2
single-component representations. For example, `foo-2d-2dbar` still decodes
once to payload `foo--bar` and is not reinterpreted as structure.

Applications migrating a literal in-component `--` MUST use component
encoding:

```text
logical component payload: foo--bar
DEF component:             foo-2d-2dbar
```

Applications relying on the old first-separator profile rule MUST define that
arity in their own grammar and explicitly component-encode any later literal
double hyphens. Mixed version parsers can disagree on structure; deployments
SHOULD version or coordinate upgrades at trust boundaries. The change warrants
a major semantic-version increment for libraries that previously exposed
version 1 behavior.

## 18. Reference Examples and Test Vectors

Core examples:

| Logical input | DEF representation |
| --- | --- |
| `abc` | `abc` |
| `abc--xyz` | `abc--xyz` |
| `abc--xyz--123` | `abc--xyz--123` |
| `--abc` | `--abc` |
| `abc--` | `abc--` |
| `--` | `--` |
| `a----b` | `a----b` |
| `a-b` | `a-2db` |
| `a---b` | `a---2db` |
| `a-----b` | `a-----2db` |
| `é--用户` | `-c3-a9---e7-94-a8-e6-88-b7` |

The shared, machine-readable conformance vectors are in
[`vectors/test-vectors.json`](../vectors/test-vectors.json). Every reference
implementation MUST consume those vectors and produce byte-for-byte identical
results and defined errors.

## 19. Conformance

A conforming DEF Core implementation:

* implements Sections 4 through 10;
* treats every raw `--` in a representation as reserved structural syntax;
* provides a way to component-encode literal payload `--`;
* does not assign application semantics or separator arity; and
* passes the shared Core conformance vectors.

A conforming profile implementation additionally documents and enforces its
carrier-specific constraints without changing Core semantics.
