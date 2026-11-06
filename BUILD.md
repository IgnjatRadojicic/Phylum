# Building Resurrection Capsule Server

* [Docker](#docker)
* [Linux](#linux)

## Docker
The process of building the ReCap server with a Docker container can be executed from any system, as long as you have Docker installed.
1. Build the Docker image:
   ```bash
   git clone https://github.com/vitor251093/recap_server
   cd recap_server/
   docker-compose up
   ```
2. Run the Docker container (will build and run the server locally):
   ```bash
   docker run -v $(pwd)/darkspore_server:/recap -P --network=host -it recap-server
   ```

## Linux
1. Install dependencies:
   * Ubuntu: `sudo apt install g++-13 cmake libfindbin-libs-perl libstdc++-13-dev build-essential`
2. Download the Resurrection Capsule Server repository and prepare:
   ```bash
   git clone https://github.com/vitor251093/recap_server
   cd recap_server/darkspore_server
   ```
3. Compile:
   ```bash
   ./build.sh
   ```

## Tests
`build.ps1` builds and runs the unit tests in `darkspore_server/tests`, and stops if one fails.

To check that game scripts have not got worse, run this with the game installed:
```powershell
.\check-scripts.ps1 -GameBin "C:\Program Files (x86)\Steam\steamapps\common\Darkspore\DarksporeBin"
```

## Windows
Needs no Visual Studio install. From PowerShell, in `scripts/windows`:
1. Download the tools. This accepts the Visual Studio license, and uses about 3 GB in `.tools`, or the folder given with `-ToolsDir`:
   ```powershell
   .\setup-tools.ps1 -AcceptMsvcLicense
   ```
2. Build:
   ```powershell
   .\build.ps1
   ```
3. To also install it into the game, close the server and pass the game's `DarksporeBin` folder. The old server is kept as `recap_server.previous.exe`:
   ```powershell
   .\build.ps1 -GameBin "C:\Program Files (x86)\Steam\steamapps\common\Darkspore\DarksporeBin"
   ```
