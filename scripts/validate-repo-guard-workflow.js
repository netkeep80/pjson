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
const EXPECTED_ACTION_REF = '99bf716da62c5d01070aa0d7e4d4f8031b43a351';
const EXPECTED_ACTION = `netkeep80/repo-guard@${EXPECTED_ACTION_REF}`;
const EXPECTED_ENFORCEMENT = 'blocking';

let errors = 0;
let repoGuardWorkflowRefs = [];
let requirementsWorkflowRefs = [];

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

function requireArrayNotIncludes(label, values, forbidden) {
  if (Array.isArray(values) && values.includes(forbidden)) {
    error(`${label}: не должен содержать '${forbidden}'`);
  }
}

function extractRepoGuardActionRefs(text) {
  const refs = [];
  const pattern = /uses:\s+netkeep80\/repo-guard@([^\s#]+)/g;
  let match;
  while ((match = pattern.exec(text)) !== null) {
    refs.push(match[1].replace(/^['"]|['"]$/g, ''));
  }
  return refs;
}

function requireSingleExpectedRepoGuardAction(label, text) {
  const refs = extractRepoGuardActionRefs(text);
  if (refs.length !== 1) {
    error(`${label}: должен быть ровно один uses: ${EXPECTED_ACTION}, найдено ${refs.length}`);
    return refs;
  }
  const ref = refs[0];
  if (!/^[0-9a-f]{40}$/.test(ref)) {
    error(`${label}: repo-guard Action должен быть pinned на commit SHA, получено '${ref}'`);
  }
  if (ref !== EXPECTED_ACTION_REF) {
    error(`${label}: repo-guard Action должен использовать ${EXPECTED_ACTION_REF}, получено '${ref}'`);
  }
  return refs;
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
  repoGuardWorkflowRefs = requireSingleExpectedRepoGuardAction('.github/workflows/repo-guard.yml', workflow);
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
  requireArrayIncludes('repo-policy.json paths.governance_paths', governancePaths, '.github/ISSUE_TEMPLATE/change-contract.yml');
  requireArrayIncludes('repo-policy.json paths.governance_paths', governancePaths, '.github/workflows/');
  requireArrayIncludes('repo-policy.json paths.governance_paths', governancePaths, 'requirements/README.md');
  requireArrayIncludes('repo-policy.json paths.governance_paths', governancePaths, 'requirements/schemas/');
  requireArrayIncludes('repo-policy.json paths.governance_paths', governancePaths, 'scripts/validate-repo-guard-workflow.js');
  requireArrayIncludes('repo-policy.json paths.governance_paths', governancePaths, 'docs/requirements-strict-profile.md');
  requireArrayNotIncludes('repo-policy.json paths.governance_paths', governancePaths, 'scripts/validate-docs-headings.js');

  const anchorTypes = policy.anchors && policy.anchors.types ? policy.anchors.types : {};
  const anchorTypeIds = Object.keys(anchorTypes);
  requireArrayIncludes('repo-policy.json anchors.types', anchorTypeIds, 'requirement_json_req_ref');
  requireArrayIncludes('repo-policy.json anchors.types', anchorTypeIds, 'doc_heading_req_ref');
  requireArrayIncludes('repo-policy.json anchors.types', anchorTypeIds, 'doc_heading_without_req_ref');

  const requirementJsonSources = anchorTypes.requirement_json_req_ref && anchorTypes.requirement_json_req_ref.sources;
  const requirementJsonGlobs = Array.isArray(requirementJsonSources) ? requirementJsonSources.map(source => source.glob) : [];
  requireArrayIncludes('repo-policy.json requirement_json_req_ref globs', requirementJsonGlobs, 'requirements/business/*.json');
  requireArrayIncludes('repo-policy.json requirement_json_req_ref globs', requirementJsonGlobs, 'requirements/functional/*.json');

  const headingRefSources = anchorTypes.doc_heading_req_ref && anchorTypes.doc_heading_req_ref.sources;
  const missingHeadingRefSources = anchorTypes.doc_heading_without_req_ref && anchorTypes.doc_heading_without_req_ref.sources;
  const headingRefGlobs = Array.isArray(headingRefSources) ? headingRefSources.map(source => source.glob) : [];
  const missingHeadingRefGlobs = Array.isArray(missingHeadingRefSources) ? missingHeadingRefSources.map(source => source.glob) : [];
  requireArrayIncludes('repo-policy.json doc_heading_req_ref globs', headingRefGlobs, 'docs/architecture.md');
  requireArrayIncludes('repo-policy.json doc_heading_req_ref globs', headingRefGlobs, 'docs/pmm_requirements.md');
  requireArrayIncludes('repo-policy.json doc_heading_without_req_ref globs', missingHeadingRefGlobs, 'docs/architecture.md');
  requireArrayIncludes('repo-policy.json doc_heading_without_req_ref globs', missingHeadingRefGlobs, 'docs/pmm_requirements.md');

  const traceRules = Array.isArray(policy.trace_rules) ? policy.trace_rules : [];
  const ruleIds = traceRules.map(rule => rule.id);
  requireArrayIncludes('repo-policy.json trace_rules', ruleIds, 'requirement-json-req-refs-must-resolve');
  requireArrayIncludes('repo-policy.json trace_rules', ruleIds, 'code-req-refs-must-resolve');
  requireArrayIncludes('repo-policy.json trace_rules', ruleIds, 'doc-req-refs-must-resolve');
  requireArrayIncludes('repo-policy.json trace_rules', ruleIds, 'doc-heading-req-refs-must-resolve');
  requireArrayIncludes('repo-policy.json trace_rules', ruleIds, 'doc-headings-must-have-req-ref');
  requireArrayIncludes('repo-policy.json trace_rules', ruleIds, 'declared-affected-anchors-need-evidence');
  requireArrayIncludes('repo-policy.json trace_rules', ruleIds, 'declared-implemented-anchors-need-evidence');
  requireArrayIncludes('repo-policy.json trace_rules', ruleIds, 'declared-verified-anchors-need-evidence');

  const requirementJsonRule = traceRules.find(rule => rule.id === 'requirement-json-req-refs-must-resolve');
  if (!requirementJsonRule || requirementJsonRule.from_anchor_type !== 'requirement_json_req_ref' || requirementJsonRule.to_anchor_type !== 'requirement_id') {
    error("repo-policy.json: правило requirement-json-req-refs-must-resolve должно разрешать requirement_json_req_ref -> requirement_id");
  }

  const headingRefsRule = traceRules.find(rule => rule.id === 'doc-heading-req-refs-must-resolve');
  if (!headingRefsRule || headingRefsRule.from_anchor_type !== 'doc_heading_req_ref' || headingRefsRule.to_anchor_type !== 'requirement_id') {
    error("repo-policy.json: правило doc-heading-req-refs-must-resolve должно разрешать doc_heading_req_ref -> requirement_id");
  }

  const missingHeadingRefsRule = traceRules.find(rule => rule.id === 'doc-headings-must-have-req-ref');
  if (!missingHeadingRefsRule || missingHeadingRefsRule.from_anchor_type !== 'doc_heading_without_req_ref' || missingHeadingRefsRule.to_anchor_type !== 'requirement_id') {
    error("repo-policy.json: правило doc-headings-must-have-req-ref должно разрешать doc_heading_without_req_ref -> requirement_id");
  }

  const declaredAnchorRules = [
    ['declared-affected-anchors-need-evidence', 'anchors.affects'],
    ['declared-implemented-anchors-need-evidence', 'anchors.implements'],
    ['declared-verified-anchors-need-evidence', 'anchors.verifies'],
  ];
  for (const [ruleId, contractField] of declaredAnchorRules) {
    const declaredAnchorsRule = traceRules.find(rule => rule.id === ruleId);
    if (!declaredAnchorsRule || declaredAnchorsRule.contract_field !== contractField) {
      error(`repo-policy.json: правило ${ruleId} должно читать contract_field '${contractField}'`);
    }
  }
}

info('Проверка PR template и README...');

const prTemplate = readText('.github/PULL_REQUEST_TEMPLATE.md');
if (prTemplate !== null) {
  requireContains('.github/PULL_REQUEST_TEMPLATE.md', prTemplate, '```repo-guard-yaml', 'шаблон должен содержать repo-guard YAML block');
  requireContains('.github/PULL_REQUEST_TEMPLATE.md', prTemplate, 'change intent', 'шаблон должен объяснять, что PR body хранит только change intent');
  requireContains('.github/PULL_REQUEST_TEMPLATE.md', prTemplate, 'authorized_governance_paths in this PR body is not trusted', 'шаблон должен запрещать доверять governance authorization из PR body');
  requireContains('.github/PULL_REQUEST_TEMPLATE.md', prTemplate, 'linked issue body', 'шаблон должен направлять governance authorization в linked issue body');
  requireContains('.github/PULL_REQUEST_TEMPLATE.md', prTemplate, 'anchors:', 'шаблон должен содержать секцию anchors');
  requireContains('.github/PULL_REQUEST_TEMPLATE.md', prTemplate, 'affects:', 'шаблон должен позволять объявлять affected anchors');
  requireContains('.github/PULL_REQUEST_TEMPLATE.md', prTemplate, 'implements:', 'шаблон должен позволять объявлять implemented anchors');
  requireContains('.github/PULL_REQUEST_TEMPLATE.md', prTemplate, 'verifies:', 'шаблон должен позволять объявлять verified anchors');
}

const issueTemplate = readText('.github/ISSUE_TEMPLATE/change-contract.yml');
if (issueTemplate !== null) {
  requireContains('.github/ISSUE_TEMPLATE/change-contract.yml', issueTemplate, '```repo-guard-yaml', 'issue form должен содержать repo-guard YAML block');
  requireContains('.github/ISSUE_TEMPLATE/change-contract.yml', issueTemplate, 'authorized_governance_paths', 'issue form должен документировать privileged authorization field');
  requireContains('.github/ISSUE_TEMPLATE/change-contract.yml', issueTemplate, 'privileged field', 'issue form должен явно называть authorized_governance_paths privileged field');
  requireContains('.github/ISSUE_TEMPLATE/change-contract.yml', issueTemplate, 'Only the contract in THIS issue body can unlock policy.paths.governance_paths', 'issue form должен фиксировать trusted issue channel');
}

const readme = readText('README.md');
if (readme !== null) {
  requireContains('README.md', readme, '.github/workflows/repo-guard.yml', 'документация должна упоминать PR workflow repo-guard');
  requireContains('README.md', readme, 'requirements-strict', 'документация должна описывать строгий профиль requirements-strict');
  requireContains('README.md', readme, 'doc_heading_req_ref', 'документация должна описывать heading anchor types');
  requireContains('README.md', readme, 'repo-policy.json` является источником истины для трассировки заголовков', 'документация должна называть repo-policy source of truth для заголовков');
  requireContains('README.md', readme, 'anchors.affects', 'документация должна объяснять affected anchors в PR body');
  requireContains('README.md', readme, 'anchors.implements', 'документация должна объяснять implemented anchors в PR body');
  requireContains('README.md', readme, 'anchors.verifies', 'документация должна объяснять verified anchors в PR body');
  requireContains('README.md', readme, 'authorized_governance_paths', 'документация должна описывать issue-sourced governance authorization');
  requireContains('README.md', readme, 'linked issue body', 'документация должна фиксировать trusted issue channel');
  requireContains('README.md', readme, 'base branch repo-policy.json', 'документация должна фиксировать trusted base policy boundary');
  requireContains('README.md', readme, 'fail closed', 'документация должна описывать fail-closed поведение при недоступной trusted boundary');
}

const strictProfileDoc = readText('docs/requirements-strict-profile.md');
if (strictProfileDoc !== null) {
  requireContains('docs/requirements-strict-profile.md', strictProfileDoc, 'requirements-strict', 'документ должен называть strict profile');
  requireContains('docs/requirements-strict-profile.md', strictProfileDoc, 'requirement-json-req-refs-must-resolve', 'документ должен описывать перенос JSON trace ref resolution в repo-guard');
  requireContains('docs/requirements-strict-profile.md', strictProfileDoc, 'scripts/validate-requirements.js', 'документ должен описывать оставшийся legacy validator');
  requireContains('docs/requirements-strict-profile.md', strictProfileDoc, 'authorized_governance_paths', 'документ должен описывать issue-sourced governance authorization');
  requireContains('docs/requirements-strict-profile.md', strictProfileDoc, 'trusted base policy', 'документ должен описывать trusted base branch policy boundary');
  requireContains('docs/requirements-strict-profile.md', strictProfileDoc, 'fail closed', 'документ должен описывать fail-closed behavior');
}

const requirementsValidator = readText('scripts/validate-requirements.js');
if (requirementsValidator !== null) {
  requireNotContains(
    'scripts/validate-requirements.js',
    requirementsValidator,
    'traces_from ссылается на несуществующее требование',
    'разрешение JSON trace refs должно выполняться repo-guard, а не legacy validator'
  );
  requireNotContains(
    'scripts/validate-requirements.js',
    requirementsValidator,
    'traces_to ссылается на несуществующее требование',
    'разрешение JSON trace refs должно выполняться repo-guard, а не legacy validator'
  );
}

info('Проверка включения repo-guard checks в CI...');

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
    `uses: ${EXPECTED_ACTION}`,
    `CI должен запускать pinned Action ${EXPECTED_ACTION} для repo-guard policy`
  );
  requirementsWorkflowRefs = requireSingleExpectedRepoGuardAction('.github/workflows/requirements.yml', requirementsWorkflow);
  requireContains(
    '.github/workflows/requirements.yml',
    requirementsWorkflow,
    'mode: check-diff',
    'CI должен запускать repo-guard в режиме check-diff для repository-wide anchor checks'
  );
  requireNotContains(
    '.github/workflows/requirements.yml',
    requirementsWorkflow,
    'node scripts/validate-docs-headings.js',
    'CI не должен зависеть от legacy validator заголовков'
  );
  requireNotContains(
    '.github/workflows/requirements.yml',
    requirementsWorkflow,
    'scripts/validate-docs-headings.js',
    'CI paths не должны зависеть от удалённого legacy validator заголовков'
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
    '.github/ISSUE_TEMPLATE/**',
    'CI должен перезапускаться при изменении issue template'
  );
  requireContains(
    '.github/workflows/requirements.yml',
    requirementsWorkflow,
    'repo-policy.json',
    'CI должен перезапускаться при изменении repo-policy.json'
  );
  requireNotContains(
    '.github/workflows/requirements.yml',
    requirementsWorkflow,
    'git clone https://github.com/netkeep80/repo-guard.git',
    'CI должен использовать pinned Action, а не ручной clone'
  );
  requireNotContains(
    '.github/workflows/requirements.yml',
    requirementsWorkflow,
    'node /tmp/repo-guard/src/repo-guard.mjs',
    'CI не должен запускать временно клонированный CLI напрямую'
  );
}

if (
  repoGuardWorkflowRefs.length === 1 &&
  requirementsWorkflowRefs.length === 1 &&
  repoGuardWorkflowRefs[0] !== requirementsWorkflowRefs[0]
) {
  error(
    `.github/workflows/repo-guard.yml и .github/workflows/requirements.yml должны использовать один и тот же repo-guard pin: ${repoGuardWorkflowRefs[0]} != ${requirementsWorkflowRefs[0]}`
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
