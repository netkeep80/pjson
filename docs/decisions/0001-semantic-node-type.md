# ADR-0001: Semantic pjson type lives in PMM application NodeType metadata

Status: accepted for implementation by issues #47 and #30.

Related requirements: FR-002, FR-009, FR-014.

## Context

The original pjson draft conflated three different concepts:

1. the semantic JSON value type (`null`, boolean, integer, string, array, object, ...);
2. the physical PMM storage primitive used by a value (`pstring`, `parray`, `pmap`, `pstringview`);
3. optional database indexes/forests that make it possible to enumerate values by type.

It also assigned pjson values directly to PMM `NodeType` values `1..5`. Those values are already owned by the PMM kernel (`ManagerHeader`, `Generic`, `ReadOnlyLocked`, `PStringView`, `PString`) and therefore cannot be reused by pjson.

Keeping a second `node_tag` in every pjson payload would avoid the collision but would violate the intended single-discriminator architecture and permanently duplicate persistent metadata.

## Decision

PMM remains the storage kernel and reserves its low `NodeType` range for kernel/container semantics. PMM exposes an application-defined range beginning at raw value `32`.

pjson owns the subrange `32..63` and currently assigns:

| Raw NodeType | pjson semantic type |
| ---: | --- |
| 32 | null |
| 33 | boolean |
| 34 | signed integer (`int64`) |
| 35 | unsigned integer (`uint64`) |
| 36 | real (`double`) |
| 37 | string |
| 38 | binary |
| 39 | array |
| 40 | object |
| 41 | ref |
| 42..63 | reserved for future pjson semantics |

The outer persistent pjson node has exactly one semantic discriminator: its PMM block `NodeType`. Its payload contains only the scalar value or a persistent offset to backing storage.

Internal backing blocks keep their PMM-native types. For example, a pjson `string` node has semantic NodeType `37`, while the referenced backing `pstring` block keeps PMM's built-in `PString` NodeType. The same separation applies to `parray`, `pmap` and `pstringview`.

`$base64` is a serialization form for the binary semantic type, not a separate in-memory type. `$ref` serializes/deserializes the ref semantic type.

`pptr<T>` remains PMM's compile-time typed persistent pointer. pjson runtime polymorphism is implemented by a common pjson payload/reference type plus semantic NodeType inspection; pjson does not turn arbitrary PMM pointers into untyped pointers.

## Consequences

- `pjson_node` does not contain a persistent `node_tag`.
- Runtime semantic type lookup is O(1) from PMM block metadata.
- PMM stays JSON-agnostic; it only knows that values `>=32` are application-defined allocated node types.
- pjson can evolve semantic types within its reserved subrange without consuming PMM kernel identifiers.
- Physical container representation can change without changing the semantic NodeType contract.
- A future pjson_db may build secondary indexes/forests by semantic type, but such indexes are not implicit in the runtime discriminator and are not required by pjson core.

## Rejected alternatives

### Reuse PMM values 1..5

Rejected because those identifiers already have kernel meanings and are validated by PMM.

### Store JSON semantic type only in pstring/parray/pmap NodeType

Rejected because physical storage type and JSON semantic type are different concepts. It also cannot distinguish null/boolean/integer/real and makes representation changes observable as semantic changes.

### Keep a payload `node_tag`

Rejected because it duplicates PMM metadata, increases every persistent node, and creates two fields that can disagree after corruption or a bug.

### Force one AVL forest per pjson semantic type

Rejected as a core requirement. Type-wide enumeration is an indexing/database concern. pjson_db may add it when query/recovery requirements justify the additional structures and maintenance cost.
