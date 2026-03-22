#!/usr/bin/env node

/**
 * @req FR-005, FR-006 — Скрипт валидации требований
 *
 * Валидирует все JSON-файлы требований по схеме и проверяет
 * согласованность трассировки, направление ссылок, отсутствие циклов,
 * именование файлов и содержание каталогов.
 *
 * Использование: node scripts/validate-requirements.js
 * Код возврата: 0 если все проверки пройдены, 1 если есть ошибки.
 */

const fs = require('fs');
const path = require('path');

const REQUIREMENTS_DIR = path.join(__dirname, '..', 'requirements');
const SCHEMA_PATH = path.join(REQUIREMENTS_DIR, 'schemas', 'requirement.schema.json');

const SUBDIRS = ['business', 'stakeholder', 'functional', 'nonfunctional', 'constraints', 'interface'];

const TYPE_PREFIX_MAP = {
  business: 'BR',
  stakeholder: 'SR',
  functional: 'FR',
  nonfunctional: 'NFR',
  constraint: 'CR',
  interface: 'IR',
};

// Иерархия уровней требований (меньше = выше уровень)
const TYPE_LEVEL = {
  business: 0,
  stakeholder: 1,
  functional: 2,
  nonfunctional: 2,
  constraint: 2,
  interface: 2,
};

// Маппинг префикса ID к типу
const PREFIX_TYPE_MAP = {
  BR: 'business',
  SR: 'stakeholder',
  FR: 'functional',
  NFR: 'nonfunctional',
  CR: 'constraint',
  IR: 'interface',
};

// Маппинг типа к каталогу
const TYPE_SUBDIR_MAP = {
  business: 'business',
  stakeholder: 'stakeholder',
  functional: 'functional',
  nonfunctional: 'nonfunctional',
  constraint: 'constraints',
  interface: 'interface',
};

// Маппинг каталога к допустимым префиксам
const SUBDIR_PREFIXES = {
  business: ['BR'],
  stakeholder: ['SR'],
  functional: ['FR'],
  nonfunctional: ['NFR'],
  constraints: ['CR'],
  interface: ['IR'],
};

let errors = 0;
let warnings = 0;

function error(msg) {
  console.error(`ОШИБКА: ${msg}`);
  errors++;
}

function warn(msg) {
  console.warn(`ПРЕДУПРЕЖДЕНИЕ: ${msg}`);
  warnings++;
}

function info(msg) {
  console.log(`ИНФО: ${msg}`);
}

// Загрузка схемы
let schema;
try {
  schema = JSON.parse(fs.readFileSync(SCHEMA_PATH, 'utf8'));
} catch (e) {
  error(`Не удалось загрузить схему: ${e.message}`);
  process.exit(1);
}

// Фаза 1: Проверка содержания каталогов (лишние файлы)
info('Проверка содержания каталогов с требованиями...');

for (const subdir of SUBDIRS) {
  const dirPath = path.join(REQUIREMENTS_DIR, subdir);
  if (!fs.existsSync(dirPath)) continue;

  const allowedPrefixes = SUBDIR_PREFIXES[subdir];
  const allEntries = fs.readdirSync(dirPath);

  for (const entry of allEntries) {
    const entryPath = path.join(dirPath, entry);
    const stat = fs.statSync(entryPath);

    if (stat.isDirectory()) {
      error(`${subdir}/${entry}: В каталоге с требованиями не должно быть подкаталогов`);
      continue;
    }

    // Проверка имени файла по паттерну {PREFIX}-{NNN}.json
    const filePattern = new RegExp(`^(${allowedPrefixes.join('|')})-[0-9]{3}\\.json$`);
    if (!filePattern.test(entry)) {
      error(`${subdir}/${entry}: Имя файла не соответствует паттерну '${allowedPrefixes.join('|')}-NNN.json'`);
    }
  }
}

// Проверка каталога schemas
const schemasDir = path.join(REQUIREMENTS_DIR, 'schemas');
if (fs.existsSync(schemasDir)) {
  const schemasEntries = fs.readdirSync(schemasDir);
  for (const entry of schemasEntries) {
    if (entry !== 'requirement.schema.json') {
      error(`schemas/${entry}: В каталоге schemas допускается только файл 'requirement.schema.json'`);
    }
  }
}

// Проверка корневого каталога requirements (только допустимые подкаталоги и README.md)
const allowedRootEntries = new Set([...SUBDIRS, 'schemas', 'README.md']);
const rootEntries = fs.readdirSync(REQUIREMENTS_DIR);
for (const entry of rootEntries) {
  if (!allowedRootEntries.has(entry)) {
    error(`requirements/${entry}: Лишний файл или каталог в корне requirements`);
  }
}

// Сбор всех файлов требований
const allRequirements = new Map();
const allFiles = [];

for (const subdir of SUBDIRS) {
  const dirPath = path.join(REQUIREMENTS_DIR, subdir);
  if (!fs.existsSync(dirPath)) continue;

  const files = fs.readdirSync(dirPath).filter(f => f.endsWith('.json'));
  for (const file of files) {
    const filePath = path.join(dirPath, file);
    allFiles.push({ filePath, subdir, file });
  }
}

info(`Найдено ${allFiles.length} файл(ов) требований`);

// Фаза 2: Валидация по схеме (базовая, без внешнего валидатора)
info('Валидация по схеме...');

for (const { filePath, subdir, file } of allFiles) {
  let req;
  try {
    req = JSON.parse(fs.readFileSync(filePath, 'utf8'));
  } catch (e) {
    error(`${file}: Невалидный JSON - ${e.message}`);
    continue;
  }

  // Проверка обязательных полей
  const requiredFields = schema.required || [];
  for (const field of requiredFields) {
    if (!(field in req)) {
      error(`${file}: Отсутствует обязательное поле '${field}'`);
    }
  }

  // Валидация формата ID
  if (req.id) {
    const idPattern = /^(BR|SR|FR|NFR|CR|IR)-[0-9]{3}$/;
    if (!idPattern.test(req.id)) {
      error(`${file}: Невалидный формат ID '${req.id}' (ожидается паттерн: XX-NNN)`);
    }

    // Проверка соответствия имени файла и ID
    const expectedFileName = `${req.id}.json`;
    if (file !== expectedFileName) {
      error(`${file}: Имя файла не совпадает с ID требования '${req.id}' (ожидается '${expectedFileName}')`);
    }

    // Проверка соответствия префикса ID типу
    if (req.type) {
      const expectedPrefix = TYPE_PREFIX_MAP[req.type];
      if (expectedPrefix && !req.id.startsWith(expectedPrefix + '-')) {
        error(`${file}: ID '${req.id}' не соответствует типу '${req.type}' (ожидается префикс '${expectedPrefix}-')`);
      }
    }

    // Проверка соответствия каталога и типа
    if (req.type) {
      const expectedSubdir = TYPE_SUBDIR_MAP[req.type];
      if (expectedSubdir && subdir !== expectedSubdir) {
        error(`${file}: Файл находится в каталоге '${subdir}', а тип требования '${req.type}' (ожидается каталог '${expectedSubdir}')`);
      }
    }

    // Проверка на дубликаты ID
    if (allRequirements.has(req.id)) {
      error(`${file}: Дублирующийся ID '${req.id}' (также в ${allRequirements.get(req.id)._file})`);
    } else {
      req._file = file;
      req._filePath = filePath;
      req._subdir = subdir;
      allRequirements.set(req.id, req);
    }
  }

  // Валидация перечисления type
  if (req.type) {
    const validTypes = schema.properties.type.enum;
    if (!validTypes.includes(req.type)) {
      error(`${file}: Невалидный тип '${req.type}' (допустимые: ${validTypes.join(', ')})`);
    }
  }

  // Валидация перечисления status
  if (req.status) {
    const validStatuses = schema.properties.status.enum;
    if (!validStatuses.includes(req.status)) {
      error(`${file}: Невалидный статус '${req.status}' (допустимые: ${validStatuses.join(', ')})`);
    }
  }

  // Валидация перечисления priority
  if (req.priority) {
    const validPriorities = schema.properties.priority.enum;
    if (!validPriorities.includes(req.priority)) {
      error(`${file}: Невалидный приоритет '${req.priority}' (допустимые: ${validPriorities.join(', ')})`);
    }
  }

  // Проверка на неизвестные свойства
  const knownProperties = new Set(Object.keys(schema.properties));
  for (const key of Object.keys(req)) {
    if (key.startsWith('_')) continue; // внутренние поля
    if (!knownProperties.has(key)) {
      error(`${file}: Неизвестное свойство '${key}'`);
    }
  }
}

// Фаза 3: Валидация трассировки — существование ссылок
info('Проверка ссылок трассировки...');

for (const [id, req] of allRequirements) {
  if (Array.isArray(req.traces_from)) {
    for (const refId of req.traces_from) {
      if (!allRequirements.has(refId)) {
        error(`${req._file}: traces_from ссылается на несуществующее требование '${refId}'`);
      }
    }
  }

  if (Array.isArray(req.traces_to)) {
    for (const refId of req.traces_to) {
      if (!allRequirements.has(refId)) {
        error(`${req._file}: traces_to ссылается на несуществующее требование '${refId}'`);
      }
    }
  }
}

// Фаза 4: Запрет ссылки на себя
info('Проверка отсутствия ссылок на себя...');

for (const [id, req] of allRequirements) {
  if (Array.isArray(req.traces_from) && req.traces_from.includes(id)) {
    error(`${req._file}: '${id}' содержит ссылку на себя в traces_from`);
  }
  if (Array.isArray(req.traces_to) && req.traces_to.includes(id)) {
    error(`${req._file}: '${id}' содержит ссылку на себя в traces_to`);
  }
}

// Фаза 5: Проверка направления трассировки
info('Проверка направления трассировки...');

function getTypeFromId(reqId) {
  const prefix = reqId.replace(/-[0-9]{3}$/, '');
  return PREFIX_TYPE_MAP[prefix];
}

function getLevelFromId(reqId) {
  const type = getTypeFromId(reqId);
  return type !== undefined ? TYPE_LEVEL[type] : undefined;
}

for (const [id, req] of allRequirements) {
  const sourceLevel = getLevelFromId(id);
  if (sourceLevel === undefined) continue;

  // traces_to: прямая ссылка допускается только на тот же или более низкий уровень
  if (Array.isArray(req.traces_to)) {
    for (const refId of req.traces_to) {
      const targetLevel = getLevelFromId(refId);
      if (targetLevel === undefined) continue;
      if (targetLevel < sourceLevel) {
        error(`${req._file}: '${id}' (уровень ${sourceLevel}) содержит traces_to на вышестоящее требование '${refId}' (уровень ${targetLevel}). Прямая ссылка на вышестоящие требования запрещена`);
      }
    }
  }

  // traces_from: обратная ссылка допускается только на тот же или более высокий уровень
  if (Array.isArray(req.traces_from)) {
    for (const refId of req.traces_from) {
      const targetLevel = getLevelFromId(refId);
      if (targetLevel === undefined) continue;
      if (targetLevel > sourceLevel) {
        error(`${req._file}: '${id}' (уровень ${sourceLevel}) содержит traces_from на нижестоящее требование '${refId}' (уровень ${targetLevel}). Обратная ссылка на нижестоящие требования запрещена`);
      }
    }
  }
}

// Фаза 6: Обнаружение циклических ссылок
info('Проверка отсутствия циклических ссылок...');

function detectCycles(requirements) {
  const WHITE = 0; // не посещён
  const GRAY = 1;  // в процессе обхода
  const BLACK = 2; // полностью обработан

  const color = new Map();
  const parent = new Map();
  const cycles = [];

  for (const id of requirements.keys()) {
    color.set(id, WHITE);
  }

  function dfs(nodeId, pathStack) {
    color.set(nodeId, GRAY);
    pathStack.push(nodeId);

    const req = requirements.get(nodeId);
    const neighbors = Array.isArray(req.traces_to) ? req.traces_to : [];

    for (const neighborId of neighbors) {
      if (!requirements.has(neighborId)) continue;

      if (color.get(neighborId) === GRAY) {
        // Найден цикл — извлечь путь
        const cycleStart = pathStack.indexOf(neighborId);
        const cyclePath = pathStack.slice(cycleStart).concat(neighborId);
        cycles.push(cyclePath);
      } else if (color.get(neighborId) === WHITE) {
        dfs(neighborId, pathStack);
      }
    }

    pathStack.pop();
    color.set(nodeId, BLACK);
  }

  for (const id of requirements.keys()) {
    if (color.get(id) === WHITE) {
      dfs(id, []);
    }
  }

  return cycles;
}

const cycles = detectCycles(allRequirements);
for (const cycle of cycles) {
  error(`Обнаружена циклическая ссылка: ${cycle.join(' → ')}`);
}

// Фаза 7: Согласованность двунаправленной трассировки
info('Проверка согласованности двунаправленной трассировки...');

for (const [id, req] of allRequirements) {
  if (Array.isArray(req.traces_to)) {
    for (const refId of req.traces_to) {
      const target = allRequirements.get(refId);
      if (target && Array.isArray(target.traces_from)) {
        if (!target.traces_from.includes(id)) {
          warn(`${req._file}: '${id}' traces_to '${refId}', но '${refId}' не содержит traces_from '${id}'`);
        }
      }
    }
  }

  if (Array.isArray(req.traces_from)) {
    for (const refId of req.traces_from) {
      const source = allRequirements.get(refId);
      if (source && Array.isArray(source.traces_to)) {
        if (!source.traces_to.includes(id)) {
          warn(`${req._file}: '${id}' traces_from '${refId}', но '${refId}' не содержит traces_to '${id}'`);
        }
      }
    }
  }
}

// Фаза 8: Проверка существования файлов (только предупреждения)
info('Проверка ссылок на файлы реализации и тестов...');

const projectRoot = path.join(__dirname, '..');

for (const [id, req] of allRequirements) {
  if (Array.isArray(req.implementation_files)) {
    for (const file of req.implementation_files) {
      const absPath = path.join(projectRoot, file);
      if (!fs.existsSync(absPath)) {
        warn(`${req._file}: Файл реализации '${file}' не существует`);
      }
    }
  }

  if (Array.isArray(req.test_files)) {
    for (const file of req.test_files) {
      const absPath = path.join(projectRoot, file);
      if (!fs.existsSync(absPath)) {
        warn(`${req._file}: Файл тестов '${file}' не существует`);
      }
    }
  }
}

// Итоги
console.log('');
console.log('=== Итоги валидации ===');
console.log(`Требований: ${allRequirements.size}`);
console.log(`Ошибок: ${errors}`);
console.log(`Предупреждений: ${warnings}`);

if (errors > 0) {
  console.log('\nВалидация НЕ ПРОЙДЕНА');
  process.exit(1);
} else {
  console.log('\nВалидация ПРОЙДЕНА');
  process.exit(0);
}
