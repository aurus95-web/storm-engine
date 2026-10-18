# ТЗ: оригинальные «Корсары 2» (Pirates of the Caribbean, 2003) на нашем движке

Сначала прочитай `CLAUDE.md` в корне репозитория: там архитектура порта (DXVK + MoltenVK + SDL2),
сборка для macOS/iOS, установка на iPad и правила. Это ТЗ — журнал и план работы по оригиналу.

## Цель

Владелец хочет играть в **оригинальные «Корсары 2: Пираты Карибского моря»** (русская версия 1.3 от 1С,
сборка «ReBuild by Foxx») на Mac и затем на iPad. Не в мод Beyond New Horizons (BNH): он уже работает,
а именно в оригинал 2003 года — со старыми скриптами, интерфейсом и балансом.

Владелец общается по-русски, сам играет и сообщает о проблемах. «снимай» = сделай скриншот окна игры
и посмотри на него (`devtools/shot.sh`, см. ниже).

## Главный принцип

Наш движок — форк PiratesAhoy, выросший из движка «Корсаров: Города потерянных кораблей» (КВЛ, 2.8).
Скрипты PotC (2003) общаются с ним почти тем же протоколом, но многое переименовано, расширено или
стало обязательным. Почти каждая ошибка — одно из:
- скрипт PotC не задаёт атрибут, который КВЛ-код ждёт (результат: значение по умолчанию 0 → серые/белые
  квадраты без текстуры, ничего не летит, не светится и т. п.);
- другой формат сообщения/события (строка вместо числа, меньше параметров, другой порядок);
- другие имена (классов, функций, действий анимации, событий анимации) или нумерация констант;
- ресурсы, которых в данных PotC нет (берём из BNH — только локально, в git не кладём).

Правило: всё, что меняет поведение, — только при `target_version = potc`
(`core.GetTargetEngineVersion() <= storm::ENGINE_VERSION::PIRATES_OF_THE_CARIBBEAN`).
Исключения — явные ошибки движка, которые затрагивают и BNH (список ниже, их надо проверить на BNH).
Отладка: временная запись в лог (`core.Trace("TEMP DEBUG ...")`), владелец играет и выходит через меню,
читаем `system.log`; диагностику удаляем до коммита. Не гадать — сначала лог.

## Данные игры (не коммитить никогда — авторское право)

Папка: `~/Documents/Корсары/potc`.
Сделанные изменения данных (нужно будет повторить при переносе на iPad):
- `RESOURCE/` слит с ресурсами движка (`build/macos/Release/resource/`: `techniques/` с `.fx/.vso/.pso`).
  Изменённые нами `.fx` копировать вручную: копия `resource/` в build обновляется только при пересборке engine
  (`game_interface.fx` с `DialogFont`, `worldmap.fx` с `WdmDrawIcon`).
- `engine.ini`: `[compatibility] target_version = potc`, окно 1280×720, `tracefilesoff = 0`, `logs_folder = .`,
  `[controls] mouse_move_scale = 2` (оригинал — `engine.ini.orig`).
- 825 текстовых файлов (`PROGRAM/**/*.c,*.h`, `RESOURCE/INI/**`, `RESOURCE/QUESTSTEXT/**`) перекодированы
  cp1251 → UTF-8. Оригиналы: `~/Documents/Корсары/potc-texts-cp1251-backup.tar.gz`.
- Из BNH скопировано: `RESOURCE/Sea` (кадры волн), `RESOURCE/Particles` (`.xps`), `Textures/Particles_list*`,
  `Textures/sealight`, `SeaTrash`, 20 текстур `Textures/WorldMap/Interfaces|Sea`, `CloudsLight`, `StormLight`, `Lightnings`, `rain` (облака шторма на карте),
  `Textures/LocEfx/state_bars.tga.tx`. Копии частиц под именами PotC: `cancloud` ← `cancloud_stock`,
  `fire_mix` ← `Fire`, `waterfog_fall` ← `waterfog`, `stars` ← `signstar`.
- Небеса PotC сконвертированы `tools/potc-skybox-converter` (`sky_side` → `sky_fr/lf/bk/rt`) во всех 8 папках.
- `RESOURCE/Textures/INTERFACES/touch_*.tga` — картинки тач-кнопок, рисует `tools/touch-icons/touch_icons.py <папка игры>`.
- Ошибка данных оригинала: ночной патч порта Исла Муэльи (`MuPort_pn`) не пропускает через место тележки на причале,
  хотя ночью рынка (`MuPort_e1`) нет. В `PROGRAM/locations/locations_loader.c` (перед загрузкой дневных/ночных моделей)
  и в `PROGRAM/Locations/init/IslaMuelle.c` ночью рынок остаётся (`models.night.rinok`, техника `DLightModel` —
  у модели только дневные цвета вершин, иначе светится) с дневным патчем `MuPort_pd`. Скопировано и на iPad.

## Как запускать и отлаживать на Mac

```bash
cmake --build --preset macos-release       # FMOD: -DSTORM_FMOD_DIR="~/Documents/Корсары/FMOD Programmers API mac"
cd ~/Documents/Корсары/potc && ~/storm-engine/build/macos/Release/engine
```
- Логи пишутся в папку игры (`system.log`, `compile.log` — трассировка скриптов, `error.log` — ошибки скриптов),
  полностью — только при выходе через меню (F1 → Выход). Падения: `~/Library/Logs/DiagnosticReports/engine-1-*.ips`
  (стек из JSON: `threads[faultingThread].frames`). Зависание: `sample <pid>`; если главный поток в `nanosleep`
  из `main`, игра просто на паузе (окно не в фокусе).
- Помощники (вне репозитория) `~/Documents/Корсары/devtools`:
  `shot.sh <имя>` (снимок окна 1280×720 в `$SHOT_DIR/<имя>s.png`, окно снимается в 2× Retina),
  `winid.swift <pid>`, `mouse.swift move dx dy | click | rclick` (курсор игры относительный),
  `click.swift x y`, `key.swift <код>` (клавиатура через CGEvent до игры НЕ доходит),
  `tx2png.py <.tx> <.png> [масштаб]` (форматы 21/23/25/26), `potc-run.sh`. Терминалу дан «Универсальный доступ».
- Управление PotC: E — достать оружие, пробел — удар (на суше) / залп (в море), Ctrl — блок,
  Enter — меню команд, F5/F6 (fn+F…) — быстрое сохранение/загрузка, F1 — меню.

## Сделано (всё НЕ закоммичено, ветка `claude/confident-franklin-7779pa`, 54 файла, ~+1200 строк)

### Скрипты и ядро (`src/libs/core`)
- `internal_functions.cpp`: слои по имени (`storm::GetLayerIndexByName` в `storm/layers.hpp`), `LayerSetRealize/
  Execute` с 2 аргументами, `LayerCreate/LayerDelete`; в конце таблицы функций (номера не сдвигать!):
  `GetEntityNext`, `GetEntityName`, `FindClass`, `FindClassNext`; `GetEntity(слой, &ref)` перебирает слой;
  подмена классов `GetEntityClassName`: `dialog` → `LegacyDialog`, `PEOPLE_ON_SHIP` → `Sailors`.
- `compiler.cpp`: в режиме potc лишние `native`-функции библиотек не ошибка.
- `data.cpp`: `int += / -= SendMessage(...)` (результат типа VAR_PTR) — раньше молча не прибавлялось
  (ломало раскладку клавиш в Опциях). Общее исправление.
- `entity_manager.cpp`: в режиме potc `EraseAll` не сбрасывает счётчик id (иначе после `DeleteEntities()`
  новые объекты получали id удалённых — пропадала музыка и пр.).
- `file_service.cpp`: `_GetFileSize` без исключения (вылет на пустом имени шрифта). Общее.
- `core_impl.cpp`: сброс логов перед окном «Failed to compile program».

### Рендер, погода, звук, диалоги, ввод
- `s_device.cpp`: `GetRenderTargetAsTexture` через `StretchRect` (общее); в potc ненайденная текстура ищется
  в папке языка (`loading\sea.tga` → `loading\Russian\sea.tga`, язык из `INI/TEXTS/language.ini`).
- `sky.cpp`: `Sky.Dir` строкой (PotC). `sea.cpp`: значения `Sea2` по умолчанию (BNH `WhrUtils.c`), цвета из
  `Sea.GF3`. `island.cpp`: остров без `FogDensity` — туман погоды.
- `sound_service.cpp`: псевдонимы из `ALIASES/*.lng` (голоса), поиск псевдонима без учёта регистра. Общее (регистр).
- `legacy_dialog.cpp`: имя в верхний регистр с UTF-8 (общее), незагруженный звук не повторяется каждый кадр;
  `game_interface.fx`: техника `DialogFont`.
- `pcs_controls`: `engine.ini [controls] mouse_move_scale` (на Apple по умолчанию 2) — влияет и на BNH.
- `player.cpp` / `location_camera.cpp`: игрок поворачивается мышью сразу (×0.35), наклон камеры ×2;
  значок «говорить» (`Talk`), когда рядом собеседник; пробел (`ChrAttack`) = быстрый удар.
- `rigging/sail_config.cpp`: цвета материала парусов массивом чисел — паруса больше не светятся ночью.
  Общее (BNH, вероятно, тоже светился).

### Интерфейс на суше и в море (`battle_interface`)
- Наземный (`land/battle_land.cpp`, `battle_man_sign.cpp`): `Parameters/CommandShowParam` PotC → `ManSign/
  CommandList`; портреты в ряд внизу слева, полосы здоровья (красная) и заряда (синяя) над портретом;
  меню команд справа от ряда (`commandlistrightgap`); без `ManSign` блок не создаётся (был серый квадрат);
  `BI_IN_SET_COMMAND_MODE` 0/−1 закрывает/обновляет меню.
- Морской (`sea/i_battle.cpp`, `battle_ship_sign.cpp`, `ships_list.cpp`, `island_descr.cpp`): `CommandShowParam` →
  `ShipIcon/CommandList`, корабли эскадры в ряд, полосы корпуса/парусов над значком, без надписей;
  главный корабль по атрибуту `MainCharacter = 1` (PotC не задаёт `MainChrIndex`) — это включило «Карта» и
  «Плыть к»; картинка корабля в формате PotC `[pic, tex, dead]`; `evntGetLandData` из 4 значений.
- `battle_command.cpp`: текстуры команд PotC — сетка обычных значков + такая же выделенных справа
  (все `AddTexture` и `Init` удваивают столбцы; номер выделенного = обычный + столбцы/2), `texNum` по умолчанию.
- `battle_navigator.cpp`: `GetCurrentCharge` одним числом. `bi_utils.cpp`: рамка боя без атрибутов не рисуется.

### Море (`sea_ai`, `ship`, `sea_cameras`, `sailors`)
- Камера: сообщение `SeaCameras` без кода (`"ia"`). Корабль: слои строками (`AI_MESSAGE_SET_LAYERS`), путь
  текстур `Ships\` в `SHIP::Mount`.
- **Отношения**: PotC friend 0 / neutral 1 / enemy 2, движок 1/2/3 — перевод в `ai_helper.cpp` (из скрипта) и
  `sea_ai.cpp` (`AI_MESSAGE_GET_RELATION`, в скрипт). Без этого никто не стрелял и ИИ не нападал.
- Ядра: `HeightMultiply/SizeMultiply/TimeSpeedMultiply` по умолчанию 1, `MaxFireDistance` — без ограничения.
- Форт: тип орудий из `Ship.Cannons.Type`, без `ShipLights` (создание и выстрел). Борта орудий
  `Ship.Cannons.Borts.*` создаются при отсутствии. «Плыть к» месту: угол необязателен.
- Матросы: `Sailors` принимает маршруты PotC `"lileee"` (`SailorsPoints::ReadFromScript`: точки, типы
  0 палуба / 1 пушка / 2..5 мачты, связи `a*256+b`).

### Карта мира (`worldmap`)
- Острова — одна модель `islands.gm` (и коллизии кораблей с ней), зоны островов по `worldMap.islands.*.position/
  radius` → `WorldMap_inside/outside_islandzone`, точки торговцев = `position.rx/rz`.
- Подписи островов и открытых мест с флагом нации; модели открытых мест (`town`, `leaf`), открываются скриптом
  `wdmOpenIslandLocation` при посещении.
- Календарь `WdmCounter` и `WindRose` PotC вместо `WdmWindUI`; значки событий `WdmIcon` (+ техника `WdmDrawIcon`).
- Встречи в формате PotC (`ProcessPotcEncounterMessage`: тип, модель, остров, скорость, время);
  встречи не сохраняются между заходами на карту (побеждённые не возвращаются).
- Вылет при выходе с карты (`patch` не обнулялся) — исправлен.

### Сборка
- `cmake/apple.cmake` копирует `libfmod.dylib`; FMOD 2.02.35: `~/Documents/Корсары/FMOD Programmers API mac|ios`.
- `tools/potc-skybox-converter/sky_convert.py`: `nmips = 1`.

## Работает (проверено владельцем)
Главное меню, новая игра, каюта, диалоги с голосом, музыка и звуки, город и порт, корабли в порту, вода,
небо, сохранение и загрузка, меню команд на суше и в море (раскладка), карта мира (острова, подписи,
календарь, роза ветров, встречи, столкновения с островами), выход в море и обратно на карту, паруса ночью.

## Осталось (по приоритету)

1. **Бой на саблях — исправлено, ждёт проверки владельцем** (`character.cpp`): в potc без `attack_fast` все виды
   ударов берутся из `actions.attack`, удары по персонажу — из `actions.hit` (и `hit_feint/parry/round`), парирование =
   `block`, `blockbreak` = `blockhit`; события анимации `Attack_slash`/`Attack_stub` = `Attack`; урон — событиями PotC
   `Location_CharacterAttack`/`Location_CharacterBlock` с параметрами `iiff` (атакующий, цель, 1.0, 1.0).
   `Resact` в PotC нет — проверить, что серия ударов не залипает. `ChrRecoil` (отскок PotC) читается в `Player::Update` (в остальных версиях отскок — «назад» в бою).
   Удар не начинался: `Character::Attack` ждёт ответ скрипта `ChrAttackAction` (энергия КВЛ), в PotC
   обработчика нет — в potc без ответа удар разрешён.
   Оглушение при каждом ударе (нет `NPC_Event_StunChance`) — в potc шанс 30% (`np_character.cpp`).
   **Белый многоугольник в небе** = свечение солнца без текстуры: PotC пишет `SunGlow.Glow.Texture`, движок
   читал только `GlowTexture` (`sun_glow.cpp`) — исправлено. Сильный туман утром — погода PotC (плотность 0.2).
   Абордаж пропал после перевода отношений: `SeaAI.Update.Ships/Forts.*.relation` тоже в числах PotC
   (`AIShip::SetSeaAIAttributes`). Трава без тумана: у острова нет `FogDensity` → в potc туман локации (`grass.cpp`).
   Флаги кораблей в меню — над значком, подпись меню в море — над флагами.
   Параметры `ShipIcon/ManSign/CommandList` в potc пересчитываются из параметров PotC при каждом создании
   интерфейса: в сохранениях лежат атрибуты старых версий конвертера (из-за этого полоски корабля оставались
   вертикальными). Над персонажем урон «нанесено/осталось» (`Location`, `%.0f/%.0f` в potc).
   «Чёрный квадрат» при загрузках = картинка загрузки 4:3 поверх последнего кадра (у PotC нет полноэкранного
   фона `back0`): в potc `ProgressView` очищает экран чёрным. Текстуры в potc сначала ищутся в папке языка
   (`Loading\Russian\...` раньше английской `Loading\...`).
   Полоски над головами противников (`Location::DrawEnemyBars`, из КВЛ) в potc не рисуются.
   «Чёрный квадрат» остался после правки `ProgressView` — на самом деле это `Fader`: картинка 4:3 проявлялась
   вместе с затемнением сцены; в potc она рисуется только на полностью чёрном экране.
   `ATTRIBUTES::CreateAttribute` всегда добавляет новый атрибут (дубликат) — конвертеры удаляют старые
   `ShipIcon/ManSign/CommandList` (из сохранений) перед созданием, иначе читался старый `CommandList`.
   Карта: значок острова (WdmIcon, внизу слева) — только ближе `radius * kradius`; все города и бухты
   (модели и подписи) показываются всегда, не только посещённые (по просьбе владельца; в PotC они открывались
   при посещении); при выходе из моря на карту — `playerShipUpdateDisp`: корабль ставится в точку подхода
   острова (`position.rx/rz`) + `playerShipDispX/Z` (раньше оставался там, где вышел в море).
   Пропуск встречи (`WMapSkipEncounter`, Enter; при значке пропуска — перк «SailingProfessional», 50%) движок не
   обрабатывал: в potc догоняющий корабль удаляется с карты (`WdmPlayerShip::Update`).
   Падение с высоты: в PotC нет анимаций `fall`/`fall_land`, падение играет `jump` (пауза «Jump pause» в воздухе,
   доигрывание при приземлении) — `fall` = `jump` в potc (`character.cpp`).
   Вылет при выходе из моря: удалённый корабль оставался в `ShipLights` (в potc огни живут в `SEA_AI` и удаляются
   позже кораблей) — `SHIP::~SHIP` вызывает `ShipLights::Release`, которая удаляет его огни.
   Вылет при заходе в порт ночью (найден по crash.log): корабли у причала (`LocLoadShips`) монтируются без
   `ShipLights`, а огни по погоде включены — `SHIP::Mount` разыменовывал null; теперь проверка.
   Огни кораблей: `SEA_AI` в potc создаёт `ShipLights` с параметрами `ShipLights.c` поздних версий (вспышки выстрелов
   ночью), огни по погоде `bWeatherIsLight`; но в моделях кораблей PotC нет локаторов `lights/flares` —
   фонарей на кораблях в оригинальном ship.dll нет; по просьбе владельца в potc ночью фонарь на корме
   (`ShipLights::AddLights`, верх кормы по габаритам корпуса). Текстуры `ShipsFlares` скопированы из BNH.
   Ускорение времени (`TimeScale`, R) в PotC только в море; в potc и на суше (`Player::Update`, ×3).
   Выход из игры на iOS: `exit()` после главного цикла (иначе чёрный экран).
   Корабль игрока на карте в potc поворачивает быстрее (`maxTurnSpd` 1.0 вместо 0.65, разгон поворота ×2).
   Подписи островов просвечивали сквозь компас (подписи пишут глубину): перед компасом в potc очищается Z
   (`WdmWindRose::LRender`).
   Штормов на карте не было: `MSG_WORLDMAP_CREATESTORM` в PotC без параметра «торнадо» — сообщение отклонялось.
   **Ролики**: на Apple `CAviPlayer` → AVFoundation (`aviplayer/apple_video.mm`), файлы `.mp4` рядом с `.wmv`
   (`tools/video/convert-videos.sh <папка игры>`, ffmpeg из Homebrew). PotC перекодирован (13 роликов),
   проверено: ролик играет со звуком, по окончании игра продолжается. Ролики BNH владельцу не нужны.
2. **Полоски над значками** горизонтальные в potc (атрибут `horizontalbars` из конвертеров) — владелец говорит,
   что направление не изменилось; нужен скрин с неполным здоровьем. На суше: здоровье над портретом, заряд
   пистолета над ним, выше ряд пуль `guncharge.tga` (атрибуты `gunchargebullets/gunchargebulletspace`,
   `GunShootHeight/GunShootSpace` PotC). Подпись меню команд — над значками (смещение PotC от верха значка).
   Флаги наций у кораблей в списке команд: `BI_GetData(BIDT_GERALD_DATA)` (отношение + флаг из `small_nations`);
   удвоение сетки текстур PotC только если ячейка вдвое шире высоты (`HasPotcSelectedPictures`).
   **Заряд на навигаторе**: `chargePos` PotC считается от левого верхнего угла навигатора (движок — от центра,
   иконка уходила за экран) — исправлено в `battle_navigator.cpp`, ждёт проверки.
3. Проверить после последних сборок: стрельба с корабля и форта (отношения), ИИ кораблей, «Плыть к», матросы,
   значки событий на карте, города/бухты на карте, раскладка клавиш в Опциях, картинки загрузки, туман у островов.
4. **Подзорная труба — сделана, ждёт проверки** (`battle_interface/src/spyglass`): класс `TELESCOPE` (наследник
   `ISPYGLASS`, режим `PotcMode::telescope`): Ctrl в виде с палубы (`TelescopeIn/Out`) — приближение, линза
   (текстура из `MSG_TELESCOPE_SET_TYPE` "lsflll"), поиск корабля → события `SetTelescopeInfo`/`ReleaseTelescopeInfo`.
   `ispyglass` с атрибутами `parameters` — режим панели PotC (`InitPotcPanel`: подложка, флаг, иконки, тексты,
   `MSG_ISG_UPDATE` "lsslllflll", `MSG_ISG_VISIBLE`, плавное появление `FadeTime`).
5. Быстрое перемещение по городу на суше (команда `BI_FastReload` в меню) — проверить, почему нет.
6. Мелочи: чёрный квадрат/экран на долю секунды при быстром перемещении и перед картинкой загрузки;
   компас в море в стиле КВЛ (PotC — свой); облака на карте; текстура `INTERFACES\INT_BORDER.TGA`,
   `LOCEFX\STATE_BARS` (скопирована), `SHIPS\EMPTYGERALD.TGA`; номер «6» на значке корабля (класс?).
7. Сквозной сценарий: торговля, верфь, таверна, квесты, абордаж, захват форта, смерть/загрузка.
8. Перед коммитом: убрать временную диагностику (`grep -rn "TEMP DEBUG" src/`), Linux CI, проверить BNH на Mac
   (меню, порт, море, паруса ночью, мышь ×2, Опции) — особенно «общие» исправления выше.
9. **Перенесено на iPad** (только PotC, BNH удалён; данные: rsync без `*.wmv`, `engine.ini` с 1366×1024 — половина экрана 2732×2048, `msaa = 4`; было 1024×768, копия `engine.ini.1024x768`).
   Тач-управление и экранные кнопки — см. CLAUDE.md п.5. «Вертолёт» (герой крутился сам после отпускания
   стика): мгновенный поворот PotC в `Player::Rotate` копил угол без нормализации, `Character::Turn`
   раскручивал лишние обороты — угол теперь в −π…π. На iPad игра идёт 120–250 кадров/с. Было: Перенос на iPad (iPadOS 15.3.1, процедура в `CLAUDE.md` п.4): данные со всеми изменениями из раздела
   «Данные игры»; договориться о переключении BNH ↔ PotC (подпапки или отдельный bundle id). Звук на iPad.

## Правила

- Правки минимальные, в стиле окружающего кода. Поведение для старых скриптов — за проверкой версии potc.
- Платформенный код — под `#ifdef __APPLE__` / `TARGET_OS_IPHONE`; Windows-сборка не должна ломаться.
- Перед пушем — Linux CI: коммит во временную ветку `ci/macos-build-linux-check`, дождаться `CI Linux`
  (`gh run list -R aurus95-web/storm-engine --branch ci/macos-build-linux-check`), затем пушить в рабочую ветку
  и удалить временную. Windows CI сломан независимо от нас (Conan 1 → 2 на раннере).
- Ассеты игр не коммитить. Временную диагностику (`TEMP DEBUG`) убирать до коммита.
