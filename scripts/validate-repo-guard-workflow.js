#!/usr/bin/env node

/**
 * @req FR-006 — Валидация трассировки требований в CI
 *
 * Проверяет, что repo-guard подключён как PR-gate с requirements-aware policy
 * и что шаблон PR/документация описывают контракт изменения.
 *
 * Использование: node scripts/validate-repo-guard-workflow.js
 * Код возврата: 0 если все проверки пройдены, 1 если есть ошибки.
 */

const fs = require('fs');
const path = require('path');

const PROJECT_ROOT = path.join(__dirname, '..');
const EXPECTED_ACTION = 'netkeep80/repo-guard@88fdc275cbc9bd835cc20c638d83d832027182c7';
const EXPECTED_ENFORCEMENT = 'blocking';

let errors = 0;

function error(msg) {
  console.error(`ОШИБКА: ${msg}`);
  errors++;
}

function info(msg) {
  console.log(`ИНФО: ${msg}`);
}

function readText(relPath) {
  const fullPath = path.join(PROJECT_ROOT, relPath);
  if (!fs.existsSync(fullPath)) {
    error(`${relPath}: файл не найден`);
    return null;
  }
  return fs.readFileSync(fullPath, 'utf8');
}

function readJson(relPath) {
  const text = readText(relPath);
  if (text === null) return null;

  try {
    return JSON.parse(text);
  } catch (e) {
    error(`${relPath}: невалидный JSON - ${e.message}`);
    return null;
  }
}

function requireContains(label, text, needle, message) {
  if (!text.includes(needle)) {
    error(`${label}: ${message}`);
  }
}

function requireNotContains(label, text, needle, message) {
  if (text.includes(needle)) {
    error(`${label}: ${message}`);
  }
}

function requireArrayIncludes(label, values, expected) {
  if (!Array.isArray(values) || !values.includes(expected)) {
    error(`${label}: отсутствует '${expected}'`);
  }
}

info('Проверка repo-guard workflow...');

const workflow = readText('.github/workflows/repo-guard.yml');
if (workflow !== null) {
  requireContains(
    '.github/workflows/repo-guard.yml',
    workflow,
    'pull_request:',
    'workflow должен запускаться в pull_request контексте'
  );
  requireContains(
    '.github/workflows/repo-guard.yml',
    workflow,
    'types: [opened, synchronize, reopened, ready_for_review, edited]',
    'workflow должен перезапускаться при изменении PR body/контракта'
  );
  requireContains(
    '.github/workflows/repo-guard.yml',
    workflow,
    'contents: read',
    'workflow должен задавать минимальные permissions.contents'
  );
  requireContains(
    '.github/workflows/repo-guard.yml',
    workflow,
    'issues: read',
    'workflow должен разрешать чтение linked issue для fallback-контракта'
  );
  requireContains(
    '.github/workflows/repo-guard.yml',
    workflow,
    'pull-requests: read',
    'workflow должен разрешать чтение PR body'
  );
  requireContains(
    '.github/workflows/repo-guard.yml',
    workflow,
    'fetch-depth: 0',
    'repo-guard нужен полный git history для diff base...head'
  );
  requireContains(
    '.github/workflows/repo-guard.yml',
    workflow,
    `uses: ${EXPECTED_ACTION}`,
    `workflow должен использовать pinned Action ${EXPECTED_ACTION}`
  );
  requireContains(
    '.github/workflows/repo-guard.yml',
    workflow,
    'mode: check-pr',
    'workflow должен запускать repo-guard в режиме check-pr'
  );
  requireContains(
    '.github/workflows/repo-guard.yml',
    workflow,
    `enforcement: ${EXPECTED_ENFORCEMENT}`,
    `workflow должен использовать режим ${EXPECTED_ENFORCEMENT}`
  );
  requireContains(
    '.github/workflows/repo-guard.yml',
    workflow,
    'GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}',
    'workflow должен передавать GH_TOKEN для чтения PR/issue контекста'
  );
  requireContains(
    '.github/workflows/repo-guard.yml',
    workflow,
    'if: always()',
    'workflow должен публиковать summary даже при нарушениях policy'
  );
  requireContains(
    '.github/workflows/repo-guard.yml',
    workflow,
    '$GITHUB_STEP_SUMMARY',
    'workflow должен выводить readable diagnostics в job summary'
  );
  requireNotContains(
    '.github/workflows/repo-guard.yml',
    workflow,
    'continue-on-error',
    'workflow не должен маскировать runtime/configuration failures'
  );
  requireNotContains(
    '.github/workflows/repo-guard.yml',
    workflow,
    'git clone https://github.com/netkeep80/repo-guard.git',
    'workflow должен использовать reusable Action, а не ручной clone'
  );
  requireNotContains(
    '.github/workflows/repo-guard.yml',
    workflow,
    'node /tmp/repo-guard/src/repo-guard.mjs',
    'workflow не должен запускать временно клонированный CLI напрямую'
  );
}

info('Проверка repo-policy.json...');

const policy = readJson('repo-policy.json');
if (policy !== null) {
  const policyMode = policy.enforcement && policy.enforcement.mode;
  if (policyMode !== EXPECTED_ENFORCEMENT) {
    error(`repo-policy.json: enforcement.mode должен быть '${EXPECTED_ENFORCEMENT}', получено '${policyMode}'`);
  }

  const governancePaths = policy.paths && policy.paths.governance_paths;
  requireArrayIncludes('repo-policy.json paths.governance_paths', governancePaths, 'repo-policy.json');
  requireArrayIncludes('repo-policy.json paths.governance_paths', governancePaths, '.github/PULL_REQUEST_TEMPLATE.md');
  requireArrayIncludes('repo-policy.json paths.governance_paths', governancePaths, '.github/workflows/');
  requireArrayIncludes('repo-policy.json paths.governance_paths', governancePaths, 'scripts/validate-repo-guard-workflow.js');

  const traceRules = Array.isArray(policy.trace_rules) ? policy.trace_rules : [];
  const ruleIds = traceRules.map(rule => rule.id);
  requireArrayIncludes('repo-policy.json trace_rules', ruleIds, 'code-req-refs-must-resolve');
  requireArrayIncludes('repo-policy.json trace_rules', ruleIds, 'doc-req-refs-must-resolve');
  requireArrayIncludes('repo-policy.json trace_rules', ruleIds, 'declared-affected-anchors-need-evidence');

  const declaredAnchorsRule = traceRules.find(rule => rule.id === 'declared-affected-anchors-need-evidence');
  if (!declaredAnchorsRule || declaredAnchorsRule.contract_field !== 'anchors.affects') {
    error("repo-policy.json: правило declared-affected-anchors-need-evidence должно читать contract_field 'anchors.affects'");
  }
}

info('Проверка PR template и README...');

const prTemplate = readText('.github/PULL_REQUEST_TEMPLATE.md');
if (prTemplate !== null) {
  requireContains('.github/PULL_REQUEST_TEMPLATE.md', prTemplate, '```repo-guard-yaml', 'шаблон должен содержать repo-guard YAML block');
  requireContains('.github/PULL_REQUEST_TEMPLATE.md', prTemplate, 'anchors:', 'шаблон должен содержать секцию anchors');
  requireContains('.github/PULL_REQUEST_TEMPLATE.md', prTemplate, 'affects:', 'шаблон должен позволять объявлять affected anchors');
  requireContains('.github/PULL_REQUEST_TEMPLATE.md', prTemplate, 'implements:', 'шаблон должен позволять объявлять implemented anchors');
  requireContains('.github/PULL_REQUEST_TEMPLATE.md', prTemplate, 'verifies:', 'шаблон должен позволять объявлять verified anchors');
}

const readme = readText('README.md');
if (readme !== null) {
  requireContains('README.md', readme, '.github/workflows/repo-guard.yml', 'документация должна упоминать PR workflow repo-guard');
  requireContains('README.md', readme, 'anchors.affects', 'документация должна объяснять affected anchors в PR body');
  requireContains('README.md', readme, 'anchors.implements', 'документация должна объяснять implemented anchors в PR body');
  requireContains('README.md', readme, 'anchors.verifies', 'документация должна объяснять verified anchors в PR body');
}

info('Проверка включения validator в CI...');

const requirementsWorkflow = readText('.github/workflows/requirements.yml');
if (requirementsWorkflow !== null) {
  requireContains(
    '.github/workflows/requirements.yml',
    requirementsWorkflow,
    'node scripts/validate-repo-guard-workflow.js',
    'CI должен запускать validate-repo-guard-workflow.js'
  );
  requireContains(
    '.github/workflows/requirements.yml',
    requirementsWorkflow,
    '.github/workflows/**',
    'CI должен перезапускаться при изменении workflows'
  );
  requireContains(
    '.github/workflows/requirements.yml',
    requirementsWorkflow,
    '.github/PULL_REQUEST_TEMPLATE.md',
    'CI должен перезапускаться при изменении PR template'
  );
  requireContains(
    '.github/workflows/requirements.yml',
    requirementsWorkflow,
    'repo-policy.json',
    'CI должен перезапускаться при изменении repo-policy.json'
  );
}

console.log('');
console.log('=== Итоги валидации repo-guard workflow ===');
console.log(`Ошибок: ${errors}`);

if (errors > 0) {
  console.log('\nВалидация НЕ ПРОЙДЕНА');
  process.exit(1);
}

console.log('\nВалидация ПРОЙДЕНА');
