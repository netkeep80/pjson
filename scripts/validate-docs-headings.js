#!/usr/bin/env node

/**
 * @req FR-006 — Валидация трассировки заголовков документации
 *
 * Проверяет, что все заголовки (строки начинающиеся с # ) в документации
 * содержат ссылку на требование в формате [XX-NNN].
 *
 * Использование: node scripts/validate-docs-headings.js
 * Код возврата: 0 если все проверки пройдены, 1 если есть ошибки.
 */

const fs = require('fs');
const path = require('path');

// Документы для валидации (относительно корня проекта)
const DOCS_TO_VALIDATE = [
  'docs/architecture.md',
  'docs/pmm_requirements.md',
];

// Паттерн ссылки на требование: [XX-NNN] где XX — префикс типа требования
const REQ_LINK_PATTERN = /\[(BR|SR|FR|NFR|CR|IR)-[0-9]{3}\]/;

const projectRoot = path.join(__dirname, '..');

let errors = 0;

function error(msg) {
  console.error(`ОШИБКА: ${msg}`);
  errors++;
}

function info(msg) {
  console.log(`ИНФО: ${msg}`);
}

info('Проверка трассировки заголовков документации...');

for (const docPath of DOCS_TO_VALIDATE) {
  const fullPath = path.join(projectRoot, docPath);

  if (!fs.existsSync(fullPath)) {
    error(`${docPath}: Файл не найден`);
    continue;
  }

  info(`Проверка ${docPath}...`);

  const content = fs.readFileSync(fullPath, 'utf8');
  const lines = content.split('\n');

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const lineNum = i + 1;

    // Проверяем только строки-заголовки (начинаются с # )
    if (/^#{1,6}\s/.test(line)) {
      if (!REQ_LINK_PATTERN.test(line)) {
        error(`${docPath}:${lineNum}: Заголовок не содержит ссылку на требование: "${line.trim()}"`);
      }
    }
  }
}

// Итоги
console.log('');
console.log('=== Итоги валидации заголовков ===');
console.log(`Документов проверено: ${DOCS_TO_VALIDATE.length}`);
console.log(`Ошибок: ${errors}`);

if (errors > 0) {
  console.log('\nВалидация НЕ ПРОЙДЕНА');
  process.exit(1);
} else {
  console.log('\nВалидация ПРОЙДЕНА');
  process.exit(0);
}
