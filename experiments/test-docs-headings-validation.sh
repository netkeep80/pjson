#!/bin/bash
# Experiment: exercise documentation heading traceability through repo-guard.

set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
REPO_GUARD_CLI=${REPO_GUARD_CLI:-/tmp/repo-guard/src/repo-guard.mjs}
PASS=0
FAIL=0

if [ ! -f "$REPO_GUARD_CLI" ]; then
  echo "repo-guard CLI not found: $REPO_GUARD_CLI"
  echo "Set REPO_GUARD_CLI=/path/to/repo-guard/src/repo-guard.mjs"
  exit 2
fi

WORK_PARENT=$(mktemp -d /tmp/pjson-doc-heading-tests.XXXXXX)
WORK_DIR="$WORK_PARENT/repo"

cleanup() {
  rm -rf "$WORK_PARENT"
}
trap cleanup EXIT

cp -R "$PROJECT_DIR" "$WORK_DIR"
rm -rf "$WORK_DIR/.git"
git -C "$WORK_DIR" init -q
git -C "$WORK_DIR" config user.email "repo-guard@example.invalid"
git -C "$WORK_DIR" config user.name "repo-guard experiment"
git -C "$WORK_DIR" add .
git -C "$WORK_DIR" commit -qm baseline

restore_files() {
  git -C "$WORK_DIR" checkout -- docs/architecture.md docs/pmm_requirements.md
}

run_test() {
  local name="$1"
  local expected="$2"
  local output
  local exit_code

  set +e
  output=$(node "$REPO_GUARD_CLI" --repo-root "$WORK_DIR" --enforcement blocking check-diff --format summary 2>&1)
  exit_code=$?
  set -e

  if [ "$expected" = "pass" ] && [ "$exit_code" -eq 0 ]; then
    echo "  PASS: $name"
    PASS=$((PASS + 1))
  elif [ "$expected" = "fail" ] && [ "$exit_code" -ne 0 ]; then
    echo "  PASS: $name (correctly detected error)"
    PASS=$((PASS + 1))
  else
    echo "  FAIL: $name (expected $expected, exit $exit_code)"
    echo "$output" | head -40
    FAIL=$((FAIL + 1))
  fi
}

echo "=== Test 1: Baseline passes ==="
run_test "baseline" "pass"

echo ""
echo "=== Test 2: Heading without requirement link in architecture.md ==="
printf '\n## New untraced section\n' >> "$WORK_DIR/docs/architecture.md"
run_test "heading without link in architecture.md" "fail"
restore_files

echo ""
echo "=== Test 3: Heading without requirement link in pmm_requirements.md ==="
printf '\n### Untraced subsection\n' >> "$WORK_DIR/docs/pmm_requirements.md"
run_test "heading without link in pmm_requirements.md" "fail"
restore_files

echo ""
echo "=== Test 4: All heading levels without links ==="
cat > "$WORK_DIR/docs/pmm_requirements.md" << 'EOF'
# Level 1 without link
## Level 2 without link
### Level 3 without link
#### Level 4 without link
EOF
run_test "all heading levels without links" "fail"
restore_files

echo ""
echo "=== Test 5: All heading levels with links ==="
cat > "$WORK_DIR/docs/pmm_requirements.md" << 'EOF'
# Level 1 [BR-001](../requirements/business/BR-001.json)
## Level 2 [SR-001](../requirements/stakeholder/SR-001.json)
### Level 3 [FR-001](../requirements/functional/FR-001.json)
#### Level 4 [NFR-001](../requirements/nonfunctional/NFR-001.json)
EOF
run_test "all heading levels with links" "pass"
restore_files

echo ""
echo "=== Test 6: Mixed headings ==="
cat > "$WORK_DIR/docs/pmm_requirements.md" << 'EOF'
# Linked [BR-001](../requirements/business/BR-001.json)
## Missing link
### Linked [FR-001](../requirements/functional/FR-001.json)
EOF
run_test "mixed headings" "fail"
restore_files

echo ""
echo "=== Test 7: Heading with requirement ID but without brackets ==="
cat > "$WORK_DIR/docs/pmm_requirements.md" << 'EOF'
# Section FR-001
EOF
run_test "requirement ID without brackets" "fail"
restore_files

echo ""
echo "=== Test 8: Heading with unresolved requirement link ==="
cat > "$WORK_DIR/docs/pmm_requirements.md" << 'EOF'
# Section [FR-999](../requirements/functional/FR-999.json)
EOF
run_test "unresolved requirement link" "fail"
restore_files

echo ""
echo "=== Results ==="
echo "Passed: $PASS"
echo "Failed: $FAIL"

exit "$FAIL"
