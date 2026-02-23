# AI Load Tester Plugin

The AI Load Tester plugin exposes a headless bot controller that connects accounts, runs scripted movement/combat loops, and reports metrics useful for client and server load tests. It registers runtime commands so you can start or stop scenarios without restarting the host.

## Building

1. Ensure the plugin is included in the build by configuring CMake as usual:

   ```bash
   cmake -S . -B build -G "Ninja"
   cmake --build build
   ```

2. The compiled module is emitted to `plugin/ai_load_tester/` alongside its `plugin.json` manifest.

3. The legacy Visual Studio solution (`src/build/win32/swg.sln`) also includes an `AiLoadTesterPlugin` project for Visual Studio 2013 builds. Building that project outputs `AiLoadTesterPlugin.dll` to `compile/win32/AiLoadTesterPlugin/<Configuration>/` alongside its import library and PDB.

## Configuring scenarios

Scenarios are JSON documents describing accounts, spawn positions, and scripted behaviors. The loader expects the following shape:

```json
{
  "connectRatePerSecond": 4.0,
  "pingIntervalSeconds": 5.0,
  "accounts": [
    { "username": "bot_01", "password": "secret", "character": "Load Bot 01" }
  ],
  "spawns": [
    { "planet": "tatooine", "x": 0.0, "y": 0.0, "z": 0.0 }
  ],
  "behaviors": [
    "move:north:10",
    "attack:nearest"
  ]
}
```

Notes:

- `connectRatePerSecond` throttles new connection attempts so you can avoid stampeding the login server.
- `pingIntervalSeconds` controls how often headless agents record a simulated latency sample.
- `accounts` is required. Blank passwords will be treated as login failures, which is useful for negative testing.
- `spawns` and `behaviors` fall back to a placeholder when omitted. Multiple entries are cycled across agents.
- A ready-made `scenario.sample.json` lives in `plugin/ai_load_tester/` for quick experimentation. If no custom scenario is found, the plugin automatically falls back to this sample so new setups can run without extra configuration.
- The default scenario path is `plugin/ai_load_tester/Swg+ai.cfg`. Drop a JSON document there (for example, the included 20-account autologin profile) to have the load test start automatically when the client boots.
- Override the scenario path with the `SWG_AI_LOAD_SCENARIO` environment variable when launching the host (for example, point it at `plugin/ai_load_tester/scenario.json` if you prefer to keep the legacy filename).

### Shipping configuration

The repository ships with a ready-to-run scenario at `plugin/ai_load_tester/Swg+ai.cfg` that connects twenty test accounts using the shared `Oliver123` password and cycles through a simple idle/walk/wander/emote:wave loop in Mos Espa (Tatooine). Pair the scenario with the following launcher overrides written into your `swg+setup` file (the client only reads this config file at startup) to auto-connect to the SWG+ login cluster and authenticate each bot. Add these values inside the `[ClientGame]` section so they sit alongside any other client options you already ship:

```ini
launcherAvatarName="Avatar Name"
loginClientIndex=1
autoConnectToLoginServer=true
loginClientID=username
loginClientPassword=password
loginServerPort0=44453
loginServerAddress0=login.swgplus.com
```

Replace `username`/`password` with the values from the scenario (the plugin plugs in each bot's values as it logs in) and set `launcherAvatarName` to a label that helps you identify the session in logs. You can also provide multiple comma- or semicolon-separated values (for example, `"Avatar One, Avatar Two"`) so that separate client instances can auto-select different characters without editing the config between launches. When a `Swg+ai.cfg` scenario exists (or you point `SWG_AI_LOAD_SCENARIO` at another JSON file), the client automatically appends the listed `character` names to the autoload list, letting each instance pick a unique bot without touching the launcher config.

When you provide numbered login credentials (for example, `loginClientID2`, `loginClientPassword2`, and `launcherAvatarName2`), set `loginClientIndex` to the slot you want the current client to use. The matching `launcherAvatarName#` is automatically promoted to the front of the autoload list so auto-connect picks the correct avatar instead of always selecting the first configured name.

For context, a minimal `swg+setup` with these overrides might resemble the following (keep your existing search trees and other sections intact):

```ini
[SharedFile]
    maxSearchPriority=7
    searchTree_00_7=polaris_patch_10.5.tre
    searchTree_00_6=ILM_animation.tre
    searchTree_00_5=ILM_visuals.tre
    searchTree_00_4=ILM_music.tre
    searchTree_00_3=ILM_maps.tre
    searchTree_00_2=ILM_sound.tre
    searchTree_00_1=patch_1.tre
    searchTOC_00_0=sku0_client.toc
[HavelonClient]
    allowMultipleInstances=true
    forceHighDetailLevels=TRUE
    crossFadeEnabled=TRUE
    fadeInEnabled=TRUE
    staticNonCollidableFloraDistance=1024
    dynamicNearFloraDistance=288
    cameraFarPlane=8192
[ClientUserInterface]
    messageOfTheDayTable=live_motd
    debugExamine=0
    debugClipboardExamine=0
    allowTargetAnything=0
    drawNetworkIds=0
[SwgClientUserInterface/SwgCuiService]
    knownIssuesArticle=10424
[SharedFoundation]
    memoryBlockManagerForceAllNonShared=1
[Station]
    subscriptionFeatures=1
    gameFeatures=65535

[ClientGame]
    freeChaseCameraMaximumZoom=5
    skipIntro=1
    disableCutScenes=1
    skipSplash=1
    launcherAvatarName="Avatar Name"
    autoConnectToLoginServer=true
    loginClientID=username
    loginClientPassword=password
    loginServerPort0=44453
    loginServerAddress0=login.swgplus.com
[ClientSkeletalAnimation]
    lodManagerEnable=0
[PreloadedAssets]
    # Default Texture, ShaderTemplate and AppearanceTemplate
    texture=texture/defaulttexture.dds
    shaderTemplate=shader/defaultshader.sht
    appearanceTemplate=appearance/defaultappearance.apt
    # Preload player object templates.
    objectTemplate=object/creature/player/shared_bothan_female.iff
    objectTemplate=object/creature/player/shared_bothan_male.iff
    objectTemplate=object/creature/player/shared_human_female.iff
    objectTemplate=object/creature/player/shared_human_male.iff
    objectTemplate=object/creature/player/shared_moncal_female.iff
    objectTemplate=object/creature/player/shared_moncal_male.iff
    objectTemplate=object/creature/player/shared_rodian_female.iff
    objectTemplate=object/creature/player/shared_rodian_male.iff
    objectTemplate=object/creature/player/shared_trandoshan_female.iff
    objectTemplate=object/creature/player/shared_trandoshan_male.iff
    objectTemplate=object/creature/player/shared_twilek_female.iff
    objectTemplate=object/creature/player/shared_twilek_male.iff
    objectTemplate=object/creature/player/shared_wookiee_female.iff
    objectTemplate=object/creature/player/shared_wookiee_male.iff
    objectTemplate=object/creature/player/shared_zabrak_female.iff
    objectTemplate=object/creature/player/shared_zabrak_male.iff
    # UI cursors
    texture=texture/ui_cursor_activate.dds
    texture=texture/ui_cursor_attack.dds
    texture=texture/ui_cursor_big.dds
    texture=texture/ui_cursor_crafting.dds
    texture=texture/ui_cursor_default.dds
    texture=texture/ui_cursor_eat.dds
    texture=texture/ui_cursor_info.dds
    texture=texture/ui_cursor_loot.dds
    texture=texture/ui_cursor_move.dds
    texture=texture/ui_cursor_resize_hor.dds
    texture=texture/ui_cursor_resize_se.dds
    texture=texture/ui_cursor_resize_sw.dds
    texture=texture/ui_cursor_resize_vert.dds
    texture=texture/ui_cursor_selection_small.dds
    texture=texture/ui_cursor_talk.dds
    texture=texture/ui_cursor_throw.dds
    texture=texture/ui_cursor_trade_start.dds
    texture=texture/ui_cursor_use.dds
```

## Running with `SwgClient_r.exe`

1. Copy the built plugin binary (`AiLoadTesterPlugin.dll`) and the `plugin.json` manifest into the `plugin/ai_load_tester/` directory beside `SwgClient_r.exe`.
2. Place your scenario file at `plugin/ai_load_tester/Swg+ai.cfg` (or point `SWG_AI_LOAD_SCENARIO` at another path if you prefer a different filename such as `plugin/ai_load_tester/scenario.json`).
3. Start `SwgClient_r.exe` with plugin loading enabled. The client logs an `AI Load Tester plugin loaded` message if initialization succeeds and will immediately start the scenario when `Swg+ai.cfg` exists.

## Runtime commands

The plugin registers host commands via `HostDispatch::registerCommand`:

- `ai_load_start` — queues a task to parse the scenario file and spawn agents. Metrics begin streaming to the plugin log every few seconds.
- `ai_load_stop` — requests a graceful shutdown of all simulated agents and emits a final metrics snapshot.
- `ai_load_status` — logs the current metrics snapshot (or indicates the system is idle) without starting or stopping anything.

Use the host's command palette or scripting hooks to run these commands at any time. The worker tasks are dispatched through `HostDispatch::enqueueTask` to avoid blocking the render or UI thread.

## Metrics

Every five seconds the plugin reports structured metrics via the log callback:

- **Connections per second** based on attempts and elapsed scenario time.
- **Successful logins** vs. **login failures** (blank passwords or rejected accounts).
- **Average simulated latency** across all agents.
- **Active agent count** to track how many bots are currently executing scripts.

These values are formatted in a single log line prefixed with `[ai_load_tester]` for easy filtering.
