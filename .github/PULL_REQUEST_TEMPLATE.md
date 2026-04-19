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
# Put requirement IDs such as FR-001 in anchors.affects when the diff changes
# behavior, docs, tests, scripts, CI, or policy tied to those requirements.
# Use anchors.implements for new implementation work and anchors.verifies for
# new tests/checks that verify a requirement.
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
- [ ] `node scripts/validate-repo-guard-workflow.js`
- [ ] `repo-guard --repo-root . check-diff --format summary`
