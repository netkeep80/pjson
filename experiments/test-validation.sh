#!/bin/bash
# Experiment: Test validation script edge cases
# This script creates temporary requirement files to test validation rules,
# then removes them after each test.

set -e
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
PASS=0
FAIL=0

run_test() {
  local name="$1"
  local expected="$2"  # "pass" or "fail"
  local output
  output=$(node "$PROJECT_DIR/scripts/validate-requirements.js" 2>&1) || true
  local has_errors
  has_errors=$(echo "$output" | grep -c "Ошибок: 0" || true)

  if [ "$expected" = "pass" ] && [ "$has_errors" -eq 1 ]; then
    echo "  PASS: $name"
    PASS=$((PASS + 1))
  elif [ "$expected" = "fail" ] && [ "$has_errors" -eq 0 ]; then
    echo "  PASS: $name (correctly detected error)"
    PASS=$((PASS + 1))
  else
    echo "  FAIL: $name (expected $expected)"
    echo "$output" | head -20
    FAIL=$((FAIL + 1))
  fi
}

echo "=== Test: Baseline passes ==="
run_test "baseline" "pass"

echo ""
echo "=== Test: Extra file in requirement directory ==="
touch "$PROJECT_DIR/requirements/business/README.md"
run_test "extra file detection" "fail"
rm -f "$PROJECT_DIR/requirements/business/README.md"

echo ""
echo "=== Test: Wrong filename pattern ==="
cp "$PROJECT_DIR/requirements/business/BR-001.json" "$PROJECT_DIR/requirements/business/business-req1.json"
run_test "wrong filename pattern" "fail"
rm -f "$PROJECT_DIR/requirements/business/business-req1.json"

echo ""
echo "=== Test: Self-reference in traces_to ==="
# Create a temp file with self-reference
cat > /tmp/test-self-ref.json << 'EOF'
{"id":"FR-099","type":"functional","title":"Test","description":"Test","status":"draft","priority":"low","traces_to":["FR-099"]}
EOF
cp /tmp/test-self-ref.json "$PROJECT_DIR/requirements/functional/FR-099.json"
run_test "self-reference detection" "fail"
rm -f "$PROJECT_DIR/requirements/functional/FR-099.json"

echo ""
echo "=== Test: Circular reference (A->B->A) ==="
cat > "$PROJECT_DIR/requirements/functional/FR-098.json" << 'EOF'
{"id":"FR-098","type":"functional","title":"Test A","description":"Test","status":"draft","priority":"low","traces_to":["FR-099"]}
EOF
cat > "$PROJECT_DIR/requirements/functional/FR-099.json" << 'EOF'
{"id":"FR-099","type":"functional","title":"Test B","description":"Test","status":"draft","priority":"low","traces_to":["FR-098"]}
EOF
run_test "circular reference detection" "fail"
rm -f "$PROJECT_DIR/requirements/functional/FR-098.json" "$PROJECT_DIR/requirements/functional/FR-099.json"

echo ""
echo "=== Test: Upward traces_to (FR -> BR) ==="
cat > "$PROJECT_DIR/requirements/functional/FR-099.json" << 'EOF'
{"id":"FR-099","type":"functional","title":"Test","description":"Test","status":"draft","priority":"low","traces_to":["BR-001"]}
EOF
run_test "upward traces_to detection" "fail"
rm -f "$PROJECT_DIR/requirements/functional/FR-099.json"

echo ""
echo "=== Test: Downward traces_from (BR -> FR) ==="
cat > "$PROJECT_DIR/requirements/business/BR-099.json" << 'EOF'
{"id":"BR-099","type":"business","title":"Test","description":"Test","status":"draft","priority":"low","traces_from":["FR-001"]}
EOF
run_test "downward traces_from detection" "fail"
rm -f "$PROJECT_DIR/requirements/business/BR-099.json"

echo ""
echo "=== Test: File in root of requirements ==="
touch "$PROJECT_DIR/requirements/temp.txt"
run_test "extra file in root" "fail"
rm -f "$PROJECT_DIR/requirements/temp.txt"

echo ""
echo "=== Test: Subdirectory in requirement folder ==="
mkdir -p "$PROJECT_DIR/requirements/business/subfolder"
run_test "subdirectory detection" "fail"
rm -rf "$PROJECT_DIR/requirements/business/subfolder"

echo ""
echo "=== Results ==="
echo "Passed: $PASS"
echo "Failed: $FAIL"

# Verify clean state
node "$PROJECT_DIR/scripts/validate-requirements.js" 2>&1 | tail -5
