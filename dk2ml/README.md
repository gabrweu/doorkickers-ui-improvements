# dk2ml/

The plugin API of the **Door Kickers 2 Native Mod Loader** (dk2ml), copied from the loader's `include/` (the loader's
template zip ships the same files in its `dk2ml\` folder). Don't edit them: they describe the loader's ABI.
- `dk2ml.h`: the C API. This plugin uses the loader's events (GUI_LOADED, FRAME, WINDOW_RESIZED), the GUI kit, the
  options on the Native mods screen (AddOption), GetConfigDir and GetGameWindow.
- `dk2ml.hpp`: the header-only C++ layer. `src/core/game.h`/`game.cpp` declare every game name as a `dk2ml::Fn`/`Field`/
  `Global`/`TypeSize`/`Enum` binding, `dk2ml::ResolveAll` resolves them, hooks use `dk2ml::Hook` with typed arguments
  (`dk2ml::Arg<T>`), and `src/core/gui.cpp` forwards to `dk2ml::gui::`.

To update, copy both files from the loader's `include/` over these, rebuild and dry-run the plugin with
`.\build.ps1 -SymTest` (see CLAUDE.md). The manifest's first argument is the `DK2ML_API_VERSION` the plugin needs.
