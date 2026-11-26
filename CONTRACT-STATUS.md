# Panel SDK 1.0.0-dev.1

The exact development snapshot is sdk/1.0.0-dev.1/iceborn_panel.h. Its SHA-256 is e68c65e4c99ee0d3b82ba8d79a6e0af92fd69d2d60bb5fa8c1ed4d2671f159e1. Copy and pin this file in the consuming project. Package ABI identifier 0x00010001 requires an exact match in this cohort. The host calls IcebornQueryPanel, validates struct size, ABI, expected module ID, the 200 ms period and sample callback. A missing or incompatible module is skipped. Version selection is fixed to 0.1.0-dev.1 for this private cohort. General package compatibility across upgrades is pending.

Core verifies the executable digest before installing graphics callbacks or reading game fields. A single sampling worker obtains quest context before collection and rechecks it afterward for all consumers. Modules use the borrowed frame and copy up to the supplied capacity of UTF-8 panel rows. They must finish synchronously and must not retain buffers or throw across the ABI. Core caps combined output at 128 rows. The graphics path takes a nonblocking snapshot update, retains the prior scene during contention, and composites cached pixels for an unchanged scene. Only Core owns the suite DXGI callbacks. Module selection and placement use the optional startup INI documented in Monster Info's field procedure. The public 0.6.17 meter retains its existing renderer, sampling and keyboard handling.

The runtime and clean-install bootstrap are built with build-core.cmd. Pass an absolute path to Monster Info's build/monster_info.dll to run the isolated D3D11 pixel tests. tests/render_dx11.cpp supplies the Kulu and Tzitzi inputs from the recorded 69-damage hit, plus a second synthetic consumer. It tests admission, independent removal, resizing, stable pixels and inactive quest state. Pass meter-first or core-first and an absolute path to an isolated copy of the public DLL to exercise coexistence. The public DLL writes logs and user data beside itself. tests/loader_host.cpp verifies clean-root and combined entry routes in temporary directories. Never run a fixture named MonsterHunterWorld.exe inside the real game directory.

Graphics sources dx11_surface, dxgi_bridge, hooks, process_com_ptr and the XInput bootstrap derive from the published Damage Meter 0.6.17 source archive (commit 9edde329a7f6b4bd3fb6bea7d1899f2c94d13092). Core adapted the bootstrap destination, DX11 discovery selection and window-class names. The published MIT notice is licenses/Damage-Meter-MIT.txt; MinHook includes its original upstream record and license in third_party/minhook. Core's hash helper uses the local native HP diagnostic implementation.

The build directory contains iceborn_suite_core.dll, iceborn_suite_adapter.dll, xinput9_1_0.dll, loader_host.exe and render_dx11.exe. Monster Info's dist directory contains both field-test ZIPs with cohort manifests, SDK hash and licenses. Shared keyboard handling, localization, DX12, independent package upgrade selection and combined performance measurements are pending.

## Timer SDK timer-dev.1

Core 0.1.0-dev.11 accepts the optional `IcebornQueryTimerModule` export from
the versioned Buff Tracker DLL. The separate header in `sdk/timer-dev.1`
uses the unchanged borrowed panel frame. The serialized 200 ms worker
collects up to three timer rows with whole seconds and active/recharge state.
These rows have an independent placement group and share the DX11 render pass
with the HP panel. The `[timers]` section in the existing Core INI controls
position, scale and visibility at startup.

## Crown SDK crown-dev.1

Header `sdk/crown-dev.1/iceborn_crown.h`, SHA-256
`b6a7ba9ddf7694c41c78366af6813763b710306378b72aaa2c695c0e1c73540b`.
Core negotiates `IcebornQueryCrownModule` with ABI `0x00010000` and
samples at 200 ms. Valid snapshots contain only detected crowns. A negative
sample leaves presence tracking intact while current notices still expire.
The callback and buffers share the existing serialized worker lifetime.

Core owns the optional Coronas settings provider, the notice lifetime,
and DX11 compositing. The independent `[crowns]` settings group uses
`Position`, `Seconds`, `Visible`, `Fixed`, `ScalePercent`,
`OpacityPercent`, `BackgroundPercent`, `MiniGold`, `LargeGold` and
`LargeSilver`.
The default is a fixed list at the bottom left
with both gold kinds enabled. Temporary mode defaults to twelve seconds
and can be configured from one to 120 seconds. Settings already saved in
the INI are read as overrides. The startup module switch is
`[modules] crown-tracker`. The Core settings and worker both block duration
edits in fixed mode. The health panel has separate `OpacityPercent` and
`BackgroundPercent` keys under `[panels]`. Background opacity is independent
of foreground opacity. Until a percentage is saved, the legacy switch and
panel opacity supply the effective background percentage.
