#!/bin/bash
# Experiment: exercise requirements-strict repo-guard rules that replaced
# bespoke requirement trace reference checks.

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

WORK_PARENT=$(mktemp -d /tmp/pjson-requirements-strict-tests.XXXXXX)
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
  git -C "$WORK_DIR" checkout -- requirements/functional/FR-003.json repo-policy.json scripts/validate-repo-guard-workflow.js experiments/test-validation.sh
  rm -f "$WORK_DIR/contract.json"
}

write_contract() {
  local field="$1"
  cat > "$WORK_DIR/contract.json" << EOF
{
  "change_type": "requirements-policy",
  "scope": ["repo-policy.json"],
  "budgets": {},
  "anchors": {
    "$field": ["FR-006"]
  },
  "must_touch": [],
  "must_not_touch": [],
  "expected_effects": ["exercise requirements-strict declared anchor evidence"]
}
EOF
}

run_test() {
  local name="$1"
  local expected="$2"
  shift 2
  local output
  local exit_code

  set +e
  output=$(node "$REPO_GUARD_CLI" --repo-root "$WORK_DIR" --enforcement blocking check-diff "$@" --format summary 2>&1)
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
    echo "$output" | head -60
    FAIL=$((FAIL + 1))
  fi
}

echo "=== Test 1: Baseline passes ==="
run_test "baseline" "pass"

echo ""
echo "=== Test 2: Requirement JSON trace ref must resolve ==="
node - "$WORK_DIR/requirements/functional/FR-003.json" << 'EOF'
const fs = require('fs');
const file = process.argv[2];
const req = JSON.parse(fs.readFileSync(file, 'utf8'));
req.traces_to = ['FR-999'];
fs.writeFileSync(file, JSON.stringify(req, null, 2) + '\n');
EOF
run_test "unresolved requirement JSON ref" "fail"
restore_files

echo ""
echo "=== Test 3: anchors.implements requires implementation evidence ==="
write_contract "implements"
printf '\n' >> "$WORK_DIR/repo-policy.json"
run_test "implements without implementation evidence" "fail" --contract contract.json
restore_files

echo ""
echo "=== Test 4: anchors.implements passes with script evidence ==="
write_contract "implements"
printf '\n' >> "$WORK_DIR/scripts/validate-repo-guard-workflow.js"
run_test "implements with script evidence" "pass" --contract contract.json
restore_files

echo ""
echo "=== Test 5: anchors.verifies requires verification evidence ==="
write_contract "verifies"
printf '\n' >> "$WORK_DIR/repo-policy.json"
run_test "verifies without verification evidence" "fail" --contract contract.json
restore_files

echo ""
echo "=== Test 6: anchors.verifies passes with experiment evidence ==="
write_contract "verifies"
printf '\n' >> "$WORK_DIR/experiments/test-validation.sh"
run_test "verifies with experiment evidence" "pass" --contract contract.json
restore_files

echo ""
echo "=== Results ==="
echo "Passed: $PASS"
echo "Failed: $FAIL"

exit "$FAIL"
