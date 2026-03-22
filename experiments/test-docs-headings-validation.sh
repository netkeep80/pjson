#!/bin/bash
# Experiment: Test docs headings validation script edge cases
# This script creates temporary markdown files to test heading traceability validation,
# then removes them after each test.

set -e
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
PASS=0
FAIL=0

# Save original files
cp "$PROJECT_DIR/docs/architecture.md" /tmp/architecture.md.bak
cp "$PROJECT_DIR/docs/pmm_requirements.md" /tmp/pmm_requirements.md.bak

restore_files() {
  cp /tmp/architecture.md.bak "$PROJECT_DIR/docs/architecture.md"
  cp /tmp/pmm_requirements.md.bak "$PROJECT_DIR/docs/pmm_requirements.md"
}

run_test() {
  local name="$1"
  local expected="$2"  # "pass" or "fail"
  local output
  output=$(node "$PROJECT_DIR/scripts/validate-docs-headings.js" 2>&1) || true
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

echo "=== Test 1: Baseline passes ==="
run_test "baseline" "pass"

echo ""
echo "=== Test 2: Heading without requirement link in architecture.md ==="
# Add a heading without a requirement link
echo "" >> "$PROJECT_DIR/docs/architecture.md"
echo "## Новый раздел без ссылки" >> "$PROJECT_DIR/docs/architecture.md"
run_test "heading without link in architecture.md" "fail"
restore_files

echo ""
echo "=== Test 3: Heading without requirement link in pmm_requirements.md ==="
echo "" >> "$PROJECT_DIR/docs/pmm_requirements.md"
echo "### Подраздел без трассировки" >> "$PROJECT_DIR/docs/pmm_requirements.md"
run_test "heading without link in pmm_requirements.md" "fail"
restore_files

echo ""
echo "=== Test 4: All heading levels without links ==="
cat > "$PROJECT_DIR/docs/pmm_requirements.md" << 'EOF'
# Уровень 1 без ссылки
## Уровень 2 без ссылки
### Уровень 3 без ссылки
#### Уровень 4 без ссылки
EOF
run_test "all heading levels without links" "fail"
restore_files

echo ""
echo "=== Test 5: All heading levels with links ==="
cat > "$PROJECT_DIR/docs/pmm_requirements.md" << 'EOF'
# Уровень 1 [BR-001](../requirements/business/BR-001.json)
## Уровень 2 [SR-001](../requirements/stakeholder/SR-001.json)
### Уровень 3 [FR-001](../requirements/functional/FR-001.json)
#### Уровень 4 [NFR-001](../requirements/nonfunctional/NFR-001.json)
EOF
run_test "all heading levels with links" "pass"
restore_files

echo ""
echo "=== Test 6: Mixed - some headings with links, some without ==="
cat > "$PROJECT_DIR/docs/pmm_requirements.md" << 'EOF'
# С ссылкой [BR-001](../requirements/business/BR-001.json)
## Без ссылки
### С ссылкой [FR-001](../requirements/functional/FR-001.json)
EOF
run_test "mixed headings" "fail"
restore_files

echo ""
echo "=== Test 7: Heading with requirement ID but without brackets ==="
cat > "$PROJECT_DIR/docs/pmm_requirements.md" << 'EOF'
# Раздел FR-001
EOF
run_test "requirement ID without brackets" "fail"
restore_files

echo ""
echo "=== Results ==="
echo "Passed: $PASS"
echo "Failed: $FAIL"

# Verify clean state
echo ""
echo "=== Verifying clean state ==="
node "$PROJECT_DIR/scripts/validate-docs-headings.js" 2>&1 | tail -5
