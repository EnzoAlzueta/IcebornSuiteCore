# IcebornSuiteCore

Shared loader, settings menu and DX11 HUD renderer for Iceborn Suite.

Development source for the shared in-game runtime. It checks the supported World executable before game data is read and provides a common settings menu for the optional modules.

## Development

Run `build-core.cmd` with the Visual Studio C++ x64 build tools installed. The optional graphics fixture accepts a built Monster Info module. See [the SDK notes](CONTRACT-STATUS.md) for the versioned contracts.

## Iceborn Suite projects

- [IcebornDamageMeter](https://github.com/EnzoAlzueta/IcebornDamageMeter)
- [IcebornMonsterInfo](https://github.com/EnzoAlzueta/IcebornMonsterInfo)
- [IcebornBuffTracker](https://github.com/EnzoAlzueta/IcebornBuffTracker)
- [IcebornCrownTracker](https://github.com/EnzoAlzueta/IcebornCrownTracker)
- [IcebornHuntAnalyzer](https://github.com/EnzoAlzueta/IcebornHuntAnalyzer) (planning)
- [IcebornToolTracker](https://github.com/EnzoAlzueta/IcebornToolTracker) (planning)
