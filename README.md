# Interaction Combinators

**The Void Maiz showcase.** Lafont's interaction combinators (γ constructor,
δ duplicator, ε eraser, and their six rules) live on a node canvas. Every
gesture *and every rewrite* is a dispatched, logged, undoable Void Core command,
and two people can reduce one net at the same time.

Built on [Void Maiz](https://github.com/migriv24/VoidMaiz) (the canvas,
gestures, inspector, command bar, and the `maiz::reduce` executor, a
conformance-proven C++20 port of
[Void Core](https://github.com/migriv24/void-core)'s portable reduce contract).

## Download

From [Releases](https://github.com/migriv24/InteractionCombinators/releases):

- **Windows (x64):** unzip and run `interaction_combinators.exe`. Keep the DLLs
  beside it. Windows 10 or later.
- **Linux (x86_64, arm64):** unpack and run it — nothing to install.

  ```sh
  tar -xzf InteractionCombinators-<version>-linux-x86_64.tar.gz
  cd InteractionCombinators-<version>-linux-x86_64
  ./interaction_combinators
  ```

  It needs glibc 2.31 or newer (Ubuntu 20.04, Debian 11 and anything later),
  OpenGL, and X11 — on a Wayland desktop it runs through XWayland, which every
  Wayland desktop ships. A minimal or server install may be missing the X
  client libraries; on Debian and Ubuntu that is
  `sudo apt install libgl1 libx11-6 libxrandr2 libxinerama1 libxcursor1 libxi6`.
  `libvoidcore.so` travels in the folder and is found there, so the app can live
  anywhere. `run.sh` is there for habit; it does nothing the binary does not do
  by itself.

- **Android (arm64):** install the APK (allow installs from your browser or
  file manager when asked). It is signed with a local key rather than a Play
  Store key, so Android treats it as a sideloaded app.

**Updates.** From 0.3.0 the app asks once whether it may check for new versions
when it starts. If you say yes, a new version appears as an *update* button that
lists what changes; nothing downloads or installs until you choose it. On Windows
and Linux the new version opens from a folder beside the old one, which is left
untouched. On Android the system's own installer asks you to confirm. (0.2.0 has no updater,
so moving to 0.3.0 is by hand, once.)

**On a phone** the layout follows the screen: upright, the actions sit under the
canvas and the inspector is a sheet you drag up; sideways, the inspector moves
beside the canvas. Everything that does not fit waits behind a **⋮** button.

## What to try

- **Orange halo = live redex**: an active pair, two principals joined by a wire
  whose glyphs have a rule.
- **step** fires the first redex. The rewrite compiles to ONE `batch` command
  (visible in the log strip): remove the pair, mint the copies, relink the
  boundary. **`undo` literally un-rewrites the net.**
- **reduce all** steps to normal form, each step its own undo frame.
- Drag ports to rewire (principal↔principal makes new redexes), Shift+A to add
  agents, and the command bar to type any core command directly.
- Tag an agent `+red`, then `+yellow`: it turns orange. The colours are mixed by
  a second interaction net running invisibly behind the UI.
- On a phone: double-tap anywhere to fire the nearest pair; pinch to zoom.

## Two people, one net

`interaction_combinators_duo.exe` opens **two windows on one machine**: Ana
hosts, Bo joins. Each is a complete copy of the app with its own document, and
they sync through [Void Palabra](https://github.com/migriv24/VoidPalabra)'s real
sync session. Only the network between them is simulated, and Ana's window has a
panel to add latency or loss, or to cut the link entirely.

Cut the link, fire a different pair in each window, and reconnect: both windows
settle into one net, and each one's changes animate into the other. This works
because wires are stored as *wire runes* joined by fusion links (Palabra SPEC
§5.11) rather than as plain edges, so two rewrites that share a wire compose
instead of losing it. The design is in Void Maiz's
[collaborative canvas](https://github.com/migriv24/VoidMaiz/blob/main/okf/concepts/collaborative-canvas.md)
notes.

**On real devices** (a PC and a phone on the same Wi-Fi), the **Share this net** /
**Join a net** buttons do the same over the network. Since 0.6.0 that network is
[Reticulum](https://reticulum.network), through Void Palabra: the link is
encrypted, the host allows each device that asks, and a device let in once is
known again next time by its proven identity. Nets on the Wi-Fi appear in a list;
there is no join code any more. 0.6.0 cannot share with 0.5.x, so update every
device.

## Build

The project builds against its siblings, checked out side by side:

```
Projects/
  InteractionCombinators/   (this repository)
  VoidMaiz/                 required
  VoidCore/                 required (Void Maiz finds it; build it first)
  VoidAllomone/             required (Void Maiz finds it)
  VoidPalabra/              optional: adds Host/Join and the duo bench
```

```
cmake -S . -B build -G Ninja
cmake --build build
build/bin/interaction_combinators.exe      # or run InteractionCombinators.bat
build/bin/interaction_combinators_duo.exe  # needs VoidPalabra
```

On Windows the toolchain is MSYS2 UCRT64 (GCC). The Android build is
`powershell -File android\build_apk.ps1`, which needs an Android SDK with the NDK
and uses no Gradle and no Java source.

**A release** is `powershell -File tools\package_release.ps1`. It makes an
optimized desktop build, stages it with its DLLs, runs the duo selftest from the
staged folder with no toolchain on `PATH` (to prove the folder is
self-contained), zips it, and builds the APK. The version comes from `VERSION`.

## The mathematics

[okf/interaction-combinators.md](okf/interaction-combinators.md) is the ground
truth: agents and the six rules, the properties (locality, strong confluence,
universality), vicious circles, and the table mapping the maths to the model.
When in doubt whether the app is right, check it against that.

## Why this app exists

It is Void Maiz's exit-test vehicle and the proof that the founding commitment
survives contact with real mathematics: humans, scripts and the reduction engine
all speak the same dispatcher language, and the transcript replays.

## License

MIT. See [LICENSE](LICENSE).
