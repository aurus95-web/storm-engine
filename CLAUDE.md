# Порт Storm Engine на macOS / iPad

Цель: запустить «Корсары 2» — **Beyond New Horizons** (BNH, бесплатная standalone-версия
мода New Horizons для Pirates of the Caribbean, https://cmdrhammie.itch.io/beyond-new-horizons)
на iPad владельца репозитория. Сборка только для себя (sideload через Xcode), не App Store:
движок под GPL-3.0, ассеты игры защищены авторским правом и **никогда не коммитятся** в репозиторий.

Рабочая ветка: `claude/confident-franklin-7779pa`. Основа — форк движка Pirates Ahoy
(https://github.com/PiratesAhoy/storm-engine, `main`), на нём работает BNH.

## Архитектура порта

- Рендер: движок пишет в D3D9. На Apple: **DXVK** (D3D9 → Vulkan) + **MoltenVK** (Vulkan → Metal).
  Тот же стек, что в порте C&C Generals на iPad: https://github.com/ammaarreshi/Generals-Mac-iOS-iPad
  (см. `docs/port/PORTING_PLAYBOOK.md` там — много готовых решений iOS-проблем).
- DXVK: форк `fbraz3/dxvk`, ветка `generalsx-macos-v2.6` (коммит закреплён в `cmake/apple.cmake`),
  с нашим патчем `cmake/dxvk-apple.patch` (поиск MoltenVK/SDL2 в бандле, swapchain в пикселях).
- DXVK загружает SDL2 через `dlopen`, поэтому на Apple SDL2 — **динамическая** библиотека.
- Окно/ввод: SDL2. Звук: **miniaudio** (общественное достояние, FetchContent) за API FMOD, которое использует
  `sound_service`: прослойка `src/libs/sound_service/fmod_miniaudio` (свои `fmod.hpp`/`fmod_errors.h`, каналы
  переиспользуются как в FMOD, вызовы у пустого канала — ошибка, ось z зеркальна: FMOD левосторонний),
  `STORM_SOUND_MINIAUDIO` (по умолчанию, если не задан `STORM_FMOD_DIR`). Каналы прослойки переиспользуются чаще, чем у FMOD: `SoundService::FreeSound`
  не освобождает ячейку дважды (иначе два звука делили ячейку и первый играл вечно). Проверено владельцем на iPad (17.10.2026).
  Сборки с ним: `build/macos-ma`, `build/ios-ma` (`-DSTORM_SOUND_MINIAUDIO=ON -DSTORM_FMOD_DIR=`). FMOD по-прежнему
  можно подключить (`STORM_FMOD_DIR`, SDK с fmod.com; у владельца 2.02.35 в `~/Documents/Корсары/FMOD Programmers API mac|ios`,
  ими собраны `build/macos`, `build/ios`).
- SSE → NEON через `src/libs/math/include/third_party/sse2neon.h` (подключается из `storm_simd.h`).
- Зависимости без Conan: `cmake/dependencies.cmake` (FetchContent из git). `STORM_USE_CONAN=OFF`
  по умолчанию на Apple. Цели называются так же, как в Conan-сборке.
- Sentry и mimalloc на Apple выключены (`STORM_USE_SENTRY`, `STORM_USE_MIMALLOC`).

## Сборка

Инструменты: `brew install cmake meson ninja glslang pkg-config molten-vk`
(или LunarG Vulkan SDK с переменной `VULKAN_SDK` — нужен для iOS: MoltenVK.xcframework).

```bash
# macOS (Apple Silicon)
cmake --preset macos [-DSTORM_FMOD_DIR=~/FMOD/macos]
cmake --build --preset macos-debug          # → build/macos/Debug/engine + dylib рядом

# iOS / iPadOS (Xcode-генератор)
cmake --preset ios -DSTORM_IOS_BUNDLE_ID=com.<you>.storm \
      -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=<TEAM_ID> \
      [-DSTORM_MOLTENVK_PATH=<.../ios-arm64/MoltenVK.framework>] [-DSTORM_FMOD_DIR=~/FMOD/ios]
cmake --build --preset ios-debug
xcrun devicectl list devices
xcrun devicectl device install app --device <UUID> build/ios/Debug/engine.app   # путь проверить: find build/ios -name engine.app
xcrun devicectl device process launch --console --device <UUID> <bundle-id>
```

Linux (проверка общего не-Windows кода, так же собирает CI):
```bash
cmake -B build -G Ninja -DSTORM_USE_CONAN=OFF -DSTORM_FETCH_SDL=ON -DCMAKE_BUILD_TYPE=Debug
ninja -C build dependencies && ninja -C build
```

## Запуск игры

- macOS: распаковать BNH, положить собранный `engine` и все `*.dylib` из `build/macos/Debug/`
  в папку игры (рядом с `engine.ini`), запускать из этой папки (`cd <BNH> && ./engine`).
  Поиск файлов игры регистронезависимый (сканирование рабочей папки, `FILE_SERVICE::ScanResourcePaths`).
  Папку `resource/` из `build/macos/Debug/` (техники и шейдеры движка) тоже скопировать в папку игры
  (слить с её `RESOURCE/`).
- iPad: через Finder (iPad → Файлы) или приложение «Файлы» скопировать содержимое папки BNH
  в Documents приложения. На iOS движок делает `chdir` в Documents и сам копирует `resource/`
  из бандла в папку игры (`SetupApplePlatform` в `main.cpp`).
- Логи: `~/Library/Application Support/Akella/Sea Dogs/Logs` (или `stash`/`logs_folder` из `engine.ini`),
  лог DXVK — `*_d3d9.log` рядом с бинарником/в рабочей папке.
- **Осторожно**: `xcrun devicectl device copy to ... --remove-existing-content true` стирает весь
  контейнер приложения (включая данные игры) — не использовать.

## Проверка изменений рендера

Делай скриншоты (`screencapture -x shot.png` на macOS) и смотри на них: ошибки рендера видны только глазами.
Сравнивай с той же сценой на Windows-версии BNH, если владелец может прислать скриншот.

## Состояние и план

1. [x] Слияние с PiratesAhoy, режим без Conan, sse2neon, CMake для macOS/iOS.
   Проверено только в Linux-контейнере: Linux-сборка, все исходники проходят `clang --target=aarch64`,
   движок совместим с заголовками DXVK-форка, патч DXVK применяется и собирается.
   На macOS собирается (`macos-debug`, Apple clang 21, CMake 4, хватает Command Line Tools без Xcode);
   без данных игры движок стартует, грузит DXVK/MoltenVK/SDL2 из своей папки и показывает окно ошибки.
2. [x] Запустить на macOS с данными BNH, получить главное меню — работает (фон, кнопки, текст).
   Можно не копировать `engine` в папку игры: `cd <BNH> && <repo>/build/macos/Debug/engine`
   (dylib ищутся рядом с бинарником). Из `resource/` движка в игру нужны только `*.vso`/`*.pso`:
   остальные файлы у BNH те же (отличаются переводами строк). Картинки `.png/.jpg/.tga` вне Windows
   грузит stb_image (`TextureLoadUsingD3DX`). Язык выбирается в игре (Options → Language), `language.ini` не влияет.
   Море, отражения, облака карты мира проверены по официальным скриншотам BNH (itch.io).
   Ролики: на Apple `CAviPlayer` играет `.mp4` рядом с `.wmv` через AVFoundation (`aviplayer/apple_video.mm`,
   компилируется как C++ с `-xobjective-c++`); перекодировать ролики игры один раз: `tools/video/convert-videos.sh <папка игры>`
   (нужен `brew install ffmpeg`).
   Исправлены ошибки самого движка PiratesAhoy (есть и в Windows-версии BNH): вылет по assert стека вызовов
   скриптов при заходе в порт, свечение парусов ночью (цвета `r,g,b,a` в `rigging.ini`), пропадание солнца
   при повороте камеры (обрезка спрайта по горизонту). Для игры использовать Release-сборку.
   Управление: меню F1, быстрое сохранение/загрузка F5/F6 (на Mac — fn+F…), F10 — отладочное меню.
   Ошибки рендера, найденные в игре, и их причины:
   - DXVK-форк без spec-констант типа сэмплера (MoltenVK) объявляет все сэмплеры ps_1_x как 2D.
     В `cmake/dxvk-apple.patch` для `texm3x3spec/vspec/tex` сэмплер объявляется кубическим
     (отражения моря), а объёмная bump-текстура моря на Apple отключена (`SEA::Init`, рябь).
     Новые ps_1_x шейдеры с 3D/кубическими текстурами через `tex` будут читаться как 2D.
   - Самописные `.vso/.pso` сверять с HLSL построчно: `worldmap_main.vso` терял `normalize`
     (облака карты), теперь генерируется `tools/shader-bins/worldmap_shaders.py`.
   - Скриншот окна игры: `screencapture -x -o -l <CGWindowID>`; ID окна 1024×768 процесса `engine`
     через `CGWindowListCopyWindowInfo` (у процесса несколько окон, нужно взять окно игры).
3. [x] Шейдеры для не-Windows. На Windows `.fx` компилируются D3DX Effects, на остальных платформах
   `CTechnique` сам разбирает `.fx` и грузит предкомпилированный байткод `<fx>_<функция>.vso/.pso`
   (`GetShaderBinPath`), константы C++ передаёт напрямую в регистры (`#else` ветки рядом с `#ifdef _WIN32 // Effects`).
   Не хватало `worldmap_main.vso` (облака) и `font_ps_font*.pso` (BmFont): добавлены, байткод шрифтов
   генерирует `tools/shader-bins/font_shaders.py`, проверено `tools/shader-bins/test_shaders.cpp`
   через DXVK на lavapipe. При новых HLSL-шейдерах в `.fx` нужно добавлять такие же бинарники.
4. [ ] iPad: подпись, запуск, пути, производительность.
   iPad владельца на iOS 15 — минимальная версия в пресете `ios` и `apple.cmake` 15.0 (MoltenVK 1.4.1 требует 14.0).
   MoltenVK для iOS (Homebrew собирает только macOS): релиз v1.4.1 `MoltenVK-ios.tar` распакован в
   `~/Developer/moltenvk-ios`, путь для `-DSTORM_MOLTENVK_PATH`:
   `~/Developer/moltenvk-ios/MoltenVK/MoltenVK/dynamic/MoltenVK.xcframework/ios-arm64/MoltenVK.framework`.
   Xcode 27 установлен (минимальная iOS в его SDK — ровно 15.0). Сборка без подписи проходит за ~2 мин:
   `cmake --preset ios -DSTORM_IOS_BUNDLE_ID=com.aurus95.storm -DSTORM_MOLTENVK_PATH=<путь выше>
   -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO && cmake --build --preset ios-debug`; все dylib в бандле
   собраны под iOS 15, предупреждений о недоступных API нет.
   **Работает на iPad Pro 12.9 M1, iPadOS 15.3.1 с джейлбрейком Dopamine (AppSync): главное меню.**
   Xcode 27 iPad на iOS 15 не видит (ни `devicectl`, ни `xcdevice`) и не делает ad-hoc подпись для iOS, поэтому:
   сборка без подписи → `tools/ios/package-adhoc.sh build/ios/Release/engine.app engine.ipa [icon.png ["Корсары 2"]]`
   (иконка — квадратная картинка вне репозитория, у владельца `~/Documents/Корсары/ipad/icon.png`; ad-hoc +
   entitlements с повышенным лимитом памяти) → `ideviceinstaller upgrade engine.ipa` (не `install`: тот
   пересоздаёт контейнер; UUID контейнера меняется и при upgrade — искать по bundle id).
   **Полный IPA с игрой** (PotC 2,4 ГБ → IPA 1,3 ГБ): `tools/ios/package-adhoc.sh --data <папка игры> --ini <engine.ini для iPad>
   build/ios/Release/engine.app <out.ipa> [icon] [name]` кладёт игру в `engine.app/GameData` (без `.wmv`, журналов, SAVE,
   Windows-файлов); при первом запуске каждой новой версии данных (`GameData/.version` ≠ `Documents/.gamedata_version`)
   `InstallBundledGameData` (`main.cpp`) переносит её в Documents клонами APFS (`clonefile`: мгновенно, без места),
   не трогая `engine.ini`, `options` и сохранения. У владельца: `~/Documents/Корсары/ipad/Корсары2-full.ipa`, `engine.ini` iPad —
   `~/Documents/Корсары/ipad/engine.ini`.
   Иначе данные (16 ГБ, включая `modules/` — это данные игры, не Windows) копируются по SSH: OpenSSH+rsync из
   Procursus, `iproxy 2222 22` (с подключённым iPhone — `iproxy -u <UDID iPad> 2222 22`, UDID из `idevice_id -l`), ключ `~/.ssh/ipad_bnh`, `rsync --rsync-path=/var/jb/usr/bin/rsync` в
   `/var/mobile/Containers/Data/Application/<UUID>/Documents` (house_arrest/afcclient для ad-hoc приложений
   даёт Permission denied). Отладка: на iOS `stderr` пишется в `Documents/stderr.log` (DXVK, MoltenVK, Metal);
   отчёты о падениях — `idevicecrashreport` (у iOS дневной лимит отчётов: свежие могут не сохраняться),
   журнал — `idevicesyslog`. Свой обработчик падений (Apple, `CrashSignalHandler` в `main.cpp`) пишет стек движка в
   `Documents/crash.log` (pc/lr, адрес сбоя и регистры; адрес загрузки для `atos -o <engine> -l <адрес>`; копии бинарников iPad-сборок —
   `~/Documents/Корсары/ipad/builds/`) и стек скриптов в `script_stack.log`. Скриншоты с Mac невозможны (нет образа разработчика iOS 15 в Xcode 27).
   Ограничения iPadOS < 16.4 / iOS < 17 и обходы: Metal не умеет BC/DXT — движок распаковывает DXT в A8R8G8B8
   при загрузке (`bDecompressDxt`, по версии ОС: MoltenVK врёт, что поддерживает); нет динамического шага
   вершин — в `dxvk-apple.patch` `dynamicVertexStridesSupported()` (шаг в состоянии конвейера).
   Разрешение на iPad: `screen_x/screen_y` — размер кадра D3D9, свопчейн всегда в пикселях экрана (2732×2048), DXVK
   растягивает кадр; у владельца 1366×1024 (ровно 2×). Меню (`XINTERFACE`, виртуальный экран 720×540) и тач-кнопки
   не зависят от разрешения, боевой интерфейс PotC задан в пикселях кадра и с ростом разрешения мельчает.
   Сглаживание: `engine.ini msaa = 4` (у владельца на Mac и iPad) — без него канаты тоньше пикселя рассыпаются в пунктир.
   На iOS `max_fps` по умолчанию 120 (частота ProMotion; iPad рисовал до 250 кадров/с и грелся), ожидание кадра —
   сон (`main.cpp`).
   **iPhone владельца** (14 Pro Max, iOS 26, без джейлбрейка, бесплатный Apple ID: подпись на 7 дней, команда
   `<TEAM_ID>`, сертификат `Apple Development: <Apple ID> (<ID>)`; значения — в локальной памяти Claude, не в репозитории): отдельная папка `build/iphone`
   (`cmake --preset ios -B build/iphone -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=<TEAM_ID> ...`), сборка схемой с устройством,
   чтобы Xcode зарегистрировал телефон в профиле: `xcodebuild -project build/iphone/storm-engine.xcodeproj -scheme engine
   -configuration Release -destination id=<UDID> -allowProvisioningUpdates -allowProvisioningDeviceRegistration build`;
   затем `tools/ios/package-adhoc.sh --data <игра> --ini ~/Documents/Корсары/iphone/engine.ini --sign "<сертификат>"
   build/iphone/Release/engine.app ~/Documents/Корсары/iphone/engine.app` и `xcrun devicectl device install app`.
   Файлы в Documents: `xcrun devicectl device copy from/to --domain-type appDataContainer --domain-identifier com.aurus95.storm`
   (`engine.ini` в Documents при установке не заменяется). DXVK этой папки собран и с SDL3 — движок задаёт
   `DXVK_WSI_DRIVER=SDL2` (`main.cpp`). Кадр рисуется в безопасной зоне без боковых отступов Dynamic Island
   (`OSWindow::GetContentArea`, `DX9RENDER::Present` с прямоугольником, касания — `Input::SetTouchArea`); разрешение
   под неё 1222×645.
   DXVK отключает ступень fixed function без текстуры, если она её читает, и все следующие: в PotC нет `ships\emptygerald.tga`
   (ступень 1 паруса), из-за этого не рисовались дыры (ступень 2) — `SAIL::SetAllSails` создаёт прозрачную 1×1.
   Карта PotC: встречи держат 16 ячеек скрипта (`MapEncounters`), ячейку освобождает событие `WorldMap_DeleteShipEncounter`
   (в PiratesAhoy было закомментировано — через время корабли переставали появляться), движок шлёт его, когда корабль исчезает
   (`WdmEnemyShip::Update`, у пары сражающихся — и за соперника). Корабли карты сохраняются только для игры, сохранённой на
   карте (атрибут скриптов `worldMap.isLoaded` = "true" при создании карты), исчезнувшие (`killMe`) не загружаются; удалённый
   корабль из пары отвязывается от соперника (`~WdmWarringShip`), иначе тот писал в освобождённую память.
   Анимации PotC: нет `hit_fire` и `HitNoFight` (лог «not set fight fire hit action») — для PotC реакция на выстрел — первый
   удар (`hit_1`), вне боя — `hit_stand` (`Character`, чтение `actions`); удары PotC в `actions.hit` (у обезьяны «hit»).
   Звук попадания («man_hit» и т. п., `Event_ChrSnd_Hit`) владелец попросил убрать: `return` в `LAi_ChrSnd_Hit` (`PROGRAM/loc_ai/LAi_sounds.c`, копия `.orig`).
   Отличия от Windows, найденные в PotC: сохранение глобальной `aref` на удалённые атрибуты Windows ловит `__except`,
   вне Windows — учёт живых атрибутов (`ATTRIBUTES::IsAlive`, `COMPILER::TraceARoot`); камера сцены (`storm::Scene`)
   после удаления активной (камера палубы после абордажа) возвращается к предыдущей.
   Вне Windows нельзя использовать `std::format` и вычисления с float в `<charconv>` (нужны macOS 13.3 / iOS 16.3).
5. [~] Тач-управление (сделано для PotC, проверяется владельцем): `SDLInput::ProcessTouch` (только прямые касания,
   `SDL_HINT_TOUCH_MOUSE_EVENTS=0`): тап = клик там, где палец (`CONTROLS::GetPointerPosition` → курсор
   `XINTERFACE::MouseMove`), удержание > 0,15 с = зажатая левая кнопка (в PotC на суше — идти; в море, на палубе и на карте касания левую кнопку не нажимают, `Input::SetTouchPress`: там она — зум/ход вперёд; меню команд берёт тапы из `CONTROLS::GetTouchTap`), палец, отведённый от точки касания = стик геймпада (камера крутится со скоростью, пропорциональной отклонению до ⅙ высоты экрана,
   `PCS_CONTROLS::Update`, `Input::TouchStick`),
   тап двумя пальцами = правый клик, щипок = колесо. Экранные кнопки (у всех одна тонкая рамка цвета выделенных команд, рамки картинок обрезаются на 5%): `PCS_CONTROLS::UpdateTouchButtons` выбирает набор
   по текущей группе управления скриптов (переменная `curKeyGroupName`: суша/бой/море/палуба/карта/команды), `DrawOverlay` рисует их
   из `DX9RENDER::RunEnd`; `engine.ini [controls] touch_buttons = 1` показывает их на Mac (для скриншотов).
   На тач-экране значки меню команд и портреты/корабли размером с экранные кнопки (10% высоты кадра,
   `BIUtils::GetTouchIconScale` в конвертерах PotC; значок действия в левом верхнем углу — `ILogAndActions::ScaleIconForTouch`, журнал сдвигается вправо; навигатор в море — `BATTLE_NAVIGATOR::Init`, масштаб высота/768). Радиус стиков 0,085 высоты. Стики — картинки `touch_stick_base/knob.tga` в стиле компаса общей карты (кольца с румбами, ручка-медальон с морем; с прозрачностью).
   На суше и в бою левая половина экрана — невидимая зона стика ходьбы: стик появляется под пальцем
   (`TouchButton::stick`, `Input::MoveStick/MoveStickArea`; кнопки проверяются раньше стика, значки меню команд
   каждый кадр отмечаются `CONTROLS::AddTouchPointerArea` — там касание остаётся тапом): вверх/вниз — клавиши ChrForward/
   ChrBackward, до упора — ChrRun, в стороны — поворот героя; на карте и в море тот же стик — WMapShip*/Ship_SailUp/Down/TurnLeft/Right
   (`TouchButton::leftKey/rightKey`).
   Диалог PotC (`LegacyDialog`): тап по ответу выделяет его, второй тап выбирает (`DlgLinkDescribe::GetLinkAt`);
   кнопки Вверх/Вниз/Выбрать. «Выполнить» = быстрая команда F3 (`BIFastCommand`: подобрать, говорить, сундук, дверь).
   Меню команд (портреты на суше, корабли в море) на тач-экране открыто всегда (`CONTROLS::IsTouchScreen`,
   `BICommandList::SetTouchMode`, `BIShipIcon/BIManSign::TouchControl`): тап по команде — выбрать, тап мимо — назад,
   красный крест «назад» только в подменю.
   Если команд больше, чем видно (`maxShowQuantity` скриптов, 5), справа от них стрелки прокрутки
   (`BIUtils::SetPotcCommandArrows`, картинка `touch_lr_arrows.tga` из `tools/touch-icons`): тап листает.
   Свои иконки `tools/touch-icons` нарисованы на фоне иконок предметов (`items1.tga`, функция `plate`). У картинок игры фон
   нарисован в самой картинке: подложка под ними видна только тёмной рамкой (пробовали и убрали).
   `tools/touch-icons/game_icons.py <папка игры>` (только читает текстуры игры): в текстурах команд и действий
   (`LandCommands`, `LandTarget4` — таверна, магазин, порт, губернатор; `land_fast_commands` — замки; `cicons_*`, паруса,
   способности моря, корабли `ships_*`, значки карты `WorldMap/Interfaces/BigIcons`; у портретов `PORTRAITS/64/face_*`
   меняется только синий фон, связанный с верхним краем, рамка своя) вырезает однотонный фон обычных картинок и рисует золотую рамку предметов (левая половина; выделенные с
   оранжевым свечением и картинки во всю клетку — меньше 35% края цвета фона: губы, глаз — не трогает) и кладёт их в `RESOURCE/Textures/touch/<тот же путь>`:
   `DX9RENDER::TextureCreate` на Apple берёт их вместо оригинала. «Залп» — `INTERFACES/touch_pic_broadside.tga`.
   Над кораблями в море три полосы `indicators.tga` (команда — `Crew.quantity`/`MaxCrew`, атрибуты
   `shipcrew*`), паруса, корпус — на тёмных дорожках во всю ширину (`shipstatebackuv`). Порядок сверху: паруса (белая),
   команда (синяя), корпус (красная). Такие же полосы над кораблями в меню команд (цели «Доплыть», атаки…): свои — всегда,
   чужие — если скрипт разрешает по подзорной трубе (`BI_CheckSSShow`, `BIShipCommandList::AddStateToIcon`, атрибут
   `CommandList.ShipStateTexture`).
   В опасности (PotC `data.alarm` > 0 или `data.riskAlarm`) портрет героя плавно мигает красным (`BIManSign`, `alarmfacecolor`).
   «Приказ спутнику» PotC: скрипт шлёт `BI_IN_SET_COMMAND_MODE` с персонажем (`BI_SetSpecCommandMode`) — движок PiratesAhoy
   его игнорировал, теперь `BIShipIcon::SetCommandCharacter` показывает приказы этого корабля; тап по значку своего корабля тоже.
   Пока экран затемняется (сущность `Fader`), меню не открывается и тапы не принимает (иначе второй тап по «Карта»
   запускал `Sea_MapLoad` дважды); команды «Заряд» в меню нет (заряды — отдельные кнопки).
   В море «Ближе»/«Дальше» (`ShipCamera_Forward/Backward`, на тач-экране зум в 10 раз медленнее, `ship_camera.cpp`).
   На карте стик на всей левой половине (меню команд там нет); палец, поставленный во время загрузки, становится стиком,
   когда стик появляется (`SDLInput::SetTouchButtons`). На «В море» — картинка из `BigIcons.tga`, как значок карты
   (атрибут карты `sea_icon`: встреча, шторм, остров или открытое море).
   На «Выполнить» — картинка текущего действия (`ILogAndActions` пишет атрибут `CurrentAction`).
   Картинки способностей: «Оружие» — `perks\128\SwordplayProfessional`, «Убрать» и кнопки без картинок в игре (зум, обзор, пропуск, диалог, паруса, Esc/Меню/Герой без подписей; материалы с объёмом и светом) — `interfaces\touch_*.tga`, рисует `tools/touch-icons/touch_icons.py <папка игры>`; «Труба» — `items6.tga` №16, «Отскок» — сапоги (`LandCommands.tga`, №5), «Выстрел» — `GunMan`, «Блок» — `BasicDefense`,
   «Удар» — `CriticalHit`, «Лечение» — зелье (`items1.tga`, №7). Кнопки «Бег» нет (бег — стик до упора). На «Выполнить»,
   когда действия нет, — закрытый замок (`land_fast_commands.tga`, №0); тогда на суше вне боя кнопка — `ChrAction` (пробел ПК:
   переход абордажа `LAi_ActivateReload`). «Reload», вытесненный другим действием (сундук), возвращается после него
   (`ILogAndActions::SetAction`), «Talk» из `Player::Update` возвращает последнее действие скриптов (атрибут `ScriptAction`, а не то, что было до «Talk»).
   В море — 4 маленькие кнопки зарядов (картинки `cicons_charge.tga`, `hk_charge1..4`, текущий по `GetCurrentCharge`).
   Прежний план: тап = клик, долгое нажатие = правый клик, щипок = колесо
   (см. раздел Touch input в playbook Generals), ввод текста, разрешение интерфейса.
   Важно: движок включает `SDL_SetRelativeMouseMode` (`src/libs/input/src/sdl_input.cpp`) и сам ведёт
   курсор по приращениям мыши — синтезированный из касания клик придётся на текущую позицию курсора,
   а не под палец. Тач-слой должен сначала ставить курсор в точку касания (абсолютные координаты).
   Мышь/трекпад iPadOS работают без доработок.
6. [~] Геймпад и настройки порта в меню игры (PotC, проверяется владельцем; у владельца Xbox Wireless Controller).
   Геймпад: SDL на iOS по умолчанию делает акселерометр джойстиком №0 — `SDL_HINT_ACCELEROMETER_AS_JOYSTICK=0`
   (`main.cpp`), `SDLInput::OpenController` берёт первое устройство с `SDL_IsGameController`. Кнопки геймпада, как
   экранные кнопки, нажимают клавиши действий текущей группы управления (`PCS_CONTROLS::UpdateTouchButtons`,
   `m_aGamepadKeys`, `IsKeyPressed`); назначения по группам — `storm::GetGamepadControl` (`core/.../port_options`,
   раскладка по умолчанию по `init_xbox.c`, свои — `engine.ini [gamepad_<группа>]`). Левый стик = стик ходьбы группы
   (или стрелки в меню), правый — камера (`pad_camera_speed`). Кнопка, зажатая при смене группы, ждёт отпускания
   (иначе A, закрывшая диалог, начинала его снова). Пока играют геймпадом (`CONTROLS::IsGamepadActive`: геймпад
   после последнего касания), экранные кнопки скрыты, а меню команд работает как на ПК (`LeaveTouchMode`: Y открывает).
   Настройки порта — в `option_screen` игры (данные игры, только у владельца, копии оригиналов `*.orig`:
   `PROGRAM/interface/option_screen.c`, `utilite.c`, `RESOURCE/INI/INTERFACES/option_screen.ini`, строки в
   `RESOURCE/INI/TEXTS/RUSSIAN/common.ini`, `PROGRAM/battle_interface/LogInterface.c`; `PROGRAM/worldmap/worldmap_encgen.c` —
   преследующие корабли на карте на 20–50% быстрее игрока; `PROGRAM/interface/shipyard.c` — стрелки
   клавиатуры/геймпада листают корабли верфи: `com:activate` кнопок-стрелок, а сценарий ждал только `click`). Функции скриптов `XI_*EngineOption*`,
   `XI_Gamepad*` (`XI_GamepadActionText/CaptureAction/CaptureDone/StopCapture`), `XI_SetTouchPreview`, `XI_RestartGame` — `xinterface/.../str_service.cpp`. «Изображение»: ползунки
   разрешения (доли экрана), частоты кадров (на лету, `main.cpp`), MSAA, «Перезапустить игру» (на iOS — выход).
   «Управление» → «Сенсорное управление» (`engine.ini [touch]` в процентах: `button_scale` — и значки игры через
   `BIUtils::GetTouchIconScale`, `stick_scale`, `camera_speed`, `zoom_speed`; на странице рисуются образцы кнопок и
   стика) и «Геймпад» (скорости стиков, назначение кнопок по режимам: список действий режима, тап по действию — ждёт кнопку геймпада,
   `storm::StartGamepadCapture`, кнопку отдаёт `PCS_CONTROLS::UpdateTouchButtons`, у других кнопок это действие снимается; второй тап — без кнопки). В меню (`MainInterface`) палец двигает ползунки,
   стика камеры нет (`Input::SetTouchDrag`). Картинки `.tga` вне Windows грузятся с mip-уровнями (`TextureLoadUsingD3DX`).
   Дальше: перестановка экранных кнопок пальцем.

Известные риски: D3D9 на MoltenVK (нет geometry shaders — в форке fbraz3 уже обработано),
видео-ролики на Linux отключены (на Apple — AVFoundation), Linux-путь DXVK (`cmake/linux.cmake`)
использует старый dxvk-native.

## Правила

- Код пишется в стиле окружающего кода, правки минимальные и по делу.
- Платформенный код — под `#ifdef __APPLE__` / `TARGET_OS_IPHONE`, Windows-сборка не должна ломаться.
- Перед пушем: Linux-сборка без Conan (или CI Linux) должна проходить.
