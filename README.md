# pjson — Персистентный JSON

C++20 header-only библиотека, предоставляющая JSON-подобный API для структур данных, хранящихся в персистентном адресном пространстве, управляемом [PersistMemoryManager (pmm)](https://github.com/netkeep80/PersistMemoryManager).

## Что такое pjson?

pjson — это аналог [nlohmann::json](https://github.com/nlohmann/json) для персистентной памяти. Вместо работы с объектами в оперативной памяти, требующими явной сериализации, pjson работает непосредственно с образом персистентной памяти — все данные сохраняются между перезапусками процесса без преобразования.

Ключевые характеристики:
- **Персистентность по замыслу** — JSON-данные хранятся в файле с отображением в память или в персистентной куче; нет накладных расходов на сериализацию/десериализацию
- **Привычный API** — вдохновлён nlohmann::json с доступом по пути, итерацией и типобезопасностью
- **Единая архитектура** — использует парадигму леса AVL-деревьев pmm как для управления памятью, так и для хранимых объектов
- **Header-only** — подключите и компилируйте, отдельная сборка библиотеки не нужна
- **C++20** — использует concepts, ranges и современные возможности C++

## Архитектура

pjson — это тонкая JSON-обёртка вокруг pmm. Всё управление памятью, персистентные указатели и структуры данных предоставляются pmm:

| Тип JSON   | Контейнер pmm | Доступ     |
|------------|---------------|------------|
| object     | `pmap`        | O(log n)   |
| array      | `parray`      | O(1)       |
| string     | `pstring`     | O(1)       |
| string key | `pstringview` | O(1) cmp   |
| number     | inline        | O(1)       |
| boolean    | inline        | O(1)       |
| null       | (только тег)  | O(1)       |

См. [docs/architecture.md](docs/architecture.md) для полного описания архитектуры и [docs/development-plan.md](docs/development-plan.md) для дорожной карты разработки.

## Статус проекта

**Фаза 1: Инфраструктура** — Фреймворк требований, документация и CI-пайплайн созданы.

Прототип pjson в настоящее время разрабатывается в [BinDiffSynchronizer](https://github.com/netkeep80/BinDiffSynchronizer). Миграция в этот репозиторий (Фаза 2) заменит пул-аллокатор прототипа и сортированный массив-карту на нативные контейнеры pmm на основе AVL-деревьев.

## Разработка на основе требований

Проект следует методологии разработки на основе требований (по Карлу Вигерсу). Все требования хранятся в виде JSON-файлов с полной трассировкой:

```
requirements/
├── schemas/          # JSON-схема для валидации требований
├── business/         # Бизнес-требования (BR-xxx)
├── stakeholder/      # Требования заинтересованных сторон (SR-xxx)
├── functional/       # Функциональные требования (FR-xxx)
├── nonfunctional/    # Нефункциональные требования (NFR-xxx)
├── constraints/      # Ограничения (CR-xxx)
└── interface/        # Требования к интерфейсам (IR-xxx)
```

Каждый файл требования включает:
- Уникальный ID с префиксом типа (например, `FR-001`)
- Ссылки трассировки вверх/вниз (`traces_from` / `traces_to`)
- Ссылки на файлы реализации и файлы тестов
- Критерии приёмки
- Статус и приоритет

См. [requirements/README.md](requirements/README.md) для полного руководства по требованиям.

### Валидация требований

```bash
node scripts/validate-requirements.js
```

Проверяет соответствие схеме, согласованность трассировки и ссылки на файлы.

### Repo-guard policy

В корне репозитория находится `repo-policy.json` — pilot-конфиг для
[`repo-guard`](https://github.com/netkeep80/repo-guard). Он описывает:

- anchor types `requirement_id`, `code_req_ref`, `doc_req_ref`,
  `doc_heading_req_ref`, `doc_heading_without_req_ref`;
- правила `must_resolve` для ссылок `@req`, документных ссылок на требования
  и обязательных ссылок на требования в заголовках
  `docs/architecture.md` / `docs/pmm_requirements.md`;
- правило evidence: изменения файлов требований должны сопровождаться
  изменениями в коде, тестах, документации, примерах, скриптах или CI;
- базовые file-level guardrails для временных файлов, build artifacts и
  co-change правил.

Проверка policy через локальную копию `repo-guard`:

```bash
node /path/to/repo-guard/src/repo-guard.mjs --repo-root . check-diff --format summary
```

`repo-policy.json` является источником истины для трассировки заголовков:
каждый Markdown-заголовок в `docs/architecture.md` и
`docs/pmm_requirements.md` должен содержать ссылку вида
`[FR-001](../requirements/functional/FR-001.json)`, а найденный ID должен
разрешаться в JSON-файл требования.

### PR workflow repo-guard

PR-gate находится в [`.github/workflows/repo-guard.yml`](.github/workflows/repo-guard.yml).
Workflow запускает `repo-guard` в `check-pr` режиме для событий
`pull_request`, использует `fetch-depth: 0` для корректного diff
`base...head` и передаёт `GH_TOKEN` для чтения PR body или связанной issue.
Начальный режим enforcement — `blocking`, потому что `repo-policy.json` уже
содержит requirements-aware rules, diff budgets и file-level guardrails для
governance, requirements, docs, scripts и CI изменений.
Существующий workflow валидации требований остаётся параллельным контролем и
запускает `repo-guard` в `check-diff` режиме для repository-wide проверки
якорей, включая трассировку заголовков документации.

Для PR подготовлен пример change contract в
[`.github/PULL_REQUEST_TEMPLATE.md`](.github/PULL_REQUEST_TEMPLATE.md). В PR body
нужно сохранять блок `repo-guard-yaml` и обновлять его вместе с diff:

- `anchors.affects` — требования, на которые влияет изменение поведения,
  документации, тестов, скриптов, CI или policy;
- `anchors.implements` — требования, для которых PR добавляет реализацию;
- `anchors.verifies` — требования, для которых PR добавляет тест или другой
  исполняемый check.

Заявленные в `anchors.affects` требования должны сопровождаться evidence-файлом
из списков `must_touch_any` в `repo-policy.json`; иначе repo-guard покажет
диагностику в GitHub Actions summary и заблокирует PR.

## Формат комментариев в исходном коде

Исходные файлы ссылаются на ID требований с помощью тега `@req`:

```cpp
// @req FR-001 — Хранение JSON-узлов через лес AVL-деревьев pmm
```

## Зависимости

| Зависимость | Назначение |
|-------------|------------|
| [pmm](https://github.com/netkeep80/PersistMemoryManager) | Управление персистентной памятью |
| Компилятор C++20 | GCC 12+ / Clang 15+ / MSVC 19.30+ |
| CMake 3.20+ | Система сборки |
| Node.js 20+ | Скрипты валидации требований |

## Лицензия

[The Unlicense](LICENSE) — общественное достояние.
