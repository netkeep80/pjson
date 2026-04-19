## Summary

<!-- Briefly describe the change. -->

## Change Contract

<!-- Keep this block in the PR body so repo-guard can validate the change. -->

```repo-guard-yaml
change_type: feature
scope:
  - "**"
budgets:
  max_new_files: 6
  max_new_docs: 1
  max_net_added_lines: 800
# Add requirement IDs such as FR-001 when this PR affects, implements,
# or verifies them.
anchors:
  affects: []
  implements: []
  verifies: []
must_touch: []
must_not_touch: []
expected_effects:
  - Describe the expected behavior change
```

## Test Plan

- [ ] `node scripts/validate-requirements.js`
- [ ] `node scripts/validate-docs-headings.js`
