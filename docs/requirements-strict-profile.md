# Профиль requirements-strict [FR-006](../requirements/functional/FR-006.json)

`requirements-strict` - строгий downstream-профиль repo-guard для репозиториев,
где требования являются каноническим источником архитектурных и проверочных
решений. pjson использует этот профиль как развернутую конфигурацию в
`repo-policy.json`: текущая schema repo-guard `0.3.0` не принимает отдельное
верхнеуровневое поле `profile`, поэтому профиль выражен через поддерживаемые
`anchors` и `trace_rules` плюс локальные overrides pjson.

## Базовый профиль [FR-006](../requirements/functional/FR-006.json)

Минимальный `requirements-strict` включает следующие проверки:

| Capability | Правило pjson |
| --- | --- |
| Канонические ID требований читаются из JSON-полей `id` | `requirement_id` |
| Все ID внутри JSON-файлов требований должны разрешаться | `requirement-json-req-refs-must-resolve` |
| Ссылки `@req` в коде, тестах, примерах и скриптах должны разрешаться | `code-req-refs-must-resolve` |
| Ссылки на требования в Markdown должны разрешаться | `doc-req-refs-must-resolve` |
| Заголовки строгих документов должны иметь bracketed requirement link | `doc-headings-must-have-req-ref` |
| Измененные требования требуют evidence-файл | `changed-requirements-need-evidence` |
| `anchors.affects`, `anchors.implements`, `anchors.verifies` требуют evidence-файл | `declared-*-anchors-need-evidence` |

Проверка JSON trace refs перенесена в repo-guard через anchor type
`requirement_json_req_ref`. Это убирает дублирование из
`scripts/validate-requirements.js`: legacy validator больше не проверяет
существование `traces_from` и `traces_to`, а отвечает за те graph-проверки,
которых пока нет в repo-guard.

## Локальные overrides pjson [FR-006](../requirements/functional/FR-006.json)

pjson усиливает базовый профиль локальными правилами:

| Override | Причина |
| --- | --- |
| Строгие heading refs только в `docs/architecture.md` и `docs/pmm_requirements.md` | Эти документы являются canonical architecture/PMM trace docs |
| Evidence для измененных требований включает `include/**`, `src/**`, `tests/**`, `examples/**`, `docs/**`, `README.md`, `requirements/README.md`, `scripts/**`, `.github/workflows/**` | Требование не должно меняться без реализации, проверки, документации или CI/script следа |
| `anchors.implements` принимают implementation evidence в `include/**`, `src/**`, `scripts/**`, `.github/workflows/**` | В текущей фазе проекта часть реализации требований является policy/script/CI инфраструктурой |
| `anchors.verifies` принимают verification evidence в `tests/**`, `experiments/**`, `scripts/**`, `.github/workflows/**` | До появления C++ test tree исполняемые проверки живут в scripts и experiments |
| Запрещены временные файлы, build outputs, logs и `node_modules/**` | Строгий профиль не должен пропускать operational мусор в PR diff |

## Оставшиеся bespoke проверки [FR-005](../requirements/functional/FR-005.json)

`scripts/validate-requirements.js` остается нужным для проверок, которые
repo-guard пока не моделирует как first-class graph rules:

| Проверка | Статус |
| --- | --- |
| JSON-схема, обязательные поля, enum-значения | Оставить в legacy validator |
| Имя файла, каталог и тип требования | Оставить в legacy validator |
| Запрет ссылки требования на себя | Оставить в legacy validator |
| Directionality `traces_to` и `traces_from` | Оставить в legacy validator |
| Отсутствие циклов в requirement graph | Оставить в legacy validator |
| Reverse trace consistency | Оставить как предупреждения в legacy validator |
| Существование implementation/test files | Оставить как предупреждения в legacy validator |
| Разрешение ID внутри JSON trace refs | Перенесено в repo-guard |
| Heading traceability docs | Перенесено в repo-guard в PR #22 |

## Миграция на first-class preset [FR-006](../requirements/functional/FR-006.json)

Когда repo-guard добавит runtime support для профилей, pjson должен заменить
развернутый блок common profile на declarative preset и оставить только
локальные overrides. Целевая форма:

```json
{
  "policy_format_version": "0.3.0",
  "repository_kind": "library",
  "profile": "requirements-strict",
  "profile_overrides": {
    "strict_heading_docs": [
      "docs/architecture.md",
      "docs/pmm_requirements.md"
    ],
    "evidence_surfaces": [
      "include/**",
      "src/**",
      "tests/**",
      "examples/**",
      "docs/**",
      "README.md",
      "requirements/README.md",
      "scripts/**",
      ".github/workflows/**"
    ]
  }
}
```

Эта форма является migration target, а не текущим валидным
`repo-policy.json`: до изменения schema repo-guard pjson должен хранить
эквивалентный expanded profile в поддерживаемых полях policy.
