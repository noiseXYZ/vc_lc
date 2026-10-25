# Vice City & Liberty City for SA-MP / open.mp

Everything you need to run the **Vice City** and **Liberty City** maps on a SA-MP 0.3.DL or open.mp server, with the coordinates measured and fixed:

| Part | Where | What it does |
|---|---|---|
| Maps (objects + models) | `filterscripts/`, `models/` | The VC2SA / LC2SA 4 SAMP conversions (GTA:United 1.1) as custom models. |
| ColAndreas collision | `filterscripts/` | Builds ColAndreas collision for both cities from their own DFFs, so `CA_FindZ_For2DCoord`, `CA_RayCastLine` and similar functions work there. |
| Zones + coordinates | `include/vc_lc.inc` | Every district of both cities, the city offsets, and helpers to convert between game and world coordinates. |
| GPS road networks | `scriptfiles/GPS_VC.dat`, `GPS_LC.dat` | The car paths of both games in `GPS.dat` format, moved to the right place. |
| Radar + pause-menu maps | `vc_lc.asi` ([Releases](../../releases)), `client/` | A client ASI: inside VC or LC, the HUD radar and the pause-menu map show that city instead of San Andreas. |
| Map images | `maps/vc.png`, `maps/lc.png` | 1024 x 1024 city maps built from the original radar tiles. |

---

## Coordinates

Both cities sit far outside San Andreas. A position from the original game converts to the SA-MP world like this:

```
world = original + offset
```

| City | X offset | Y offset | Z offset | World area (X) | World area (Y) |
|---|---|---|---|---|---|
| Vice City | +6000.0 | +1050.0 | -5.5 | 4000 .. 8000 | -950 .. 3050 |
| Liberty City | -5350.0 | -200.0 | 0.0 | -7350 .. -3350 | -2200 .. 1800 |

- Each original map covers -2000 .. 2000 on both axes, so each city is a 4000 x 4000 square around its offset.
- Objects between Z 1000 and Z 1700 are interiors.
- **The original filterscript headers were wrong.** They said "+4000 X +2000 Y" (VC) and "-4000 X -2000 Y" (LC), but GTA:United had already moved the cities before that. The offsets above were measured by matching hundreds of objects against the original IPL files, and they are exact on every island. The headers in this repository have been corrected.

`include/vc_lc.inc` has the offsets as defines (`VCLC_VC_OFFSET_X`, ...) and does the conversion for you (see [Zones and helpers](#zones-and-helpers)).

---

## Server setup

### Requirements

- An **open.mp** server (or SA-MP 0.3.DL) with custom models enabled.
- [Streamer](https://github.com/samp-incognito/samp-streamer-plugin).
- [ColAndreas](https://github.com/Pottus/ColAndreas) with `CA_LoadFromDff`. Check that your `colandreas.inc` has `native CA_LoadFromDff(newid, const dffFileName[]);`.
- Players need a client that downloads custom models (SA-MP 0.3.DL, or open.mp's launcher).

### 1. Copy the files

```
models/vc2sa/          -> <server>/models/vc2sa/
models/lc2sa/          -> <server>/models/lc2sa/
filterscripts/*.pwn    -> <server>/filterscripts/
include/gl_common.inc  -> <server>/include/gl_common.inc
include/vc_lc.inc      -> your compiler's include folder (optional, for your gamemode)
scriptfiles/*.dat      -> <server>/scriptfiles/ (optional, only if you use the road networks)
```

The filterscripts include `"../include/gl_common.inc"` (just `strtok`), so `include/` must sit next to `filterscripts/`. The model paths in the scripts are relative to the models folder (`vc2sa/dff/...`, `vc2sa/txd/...`).

### 2. Enable custom models

open.mp `config.json`:

```json
"artwork": {
    "enable": true,
    "models_path": "models"
}
```

(SA-MP 0.3.DL: `useartwork 1` and `artpath models` in `server.cfg`.)

### 3. Compile and load the filterscripts

Compile `vc2sa4samp.pwn` and `lc2sa4samp.pwn` (they need `open.mp`, `streamer` and `colandreas` includes), then load them, for example in `config.json`:

```json
"pawn": {
    "side_scripts": ["filterscripts/vc2sa4samp", "filterscripts/lc2sa4samp"]
}
```

You can load one city or both.

### 4. ColAndreas in the gamemode

Call `CA_Init()` in your gamemode's `OnGameModeInit`, like you would for San Andreas. The filterscripts build the city collision **3 seconds after they start**, on top of what `CA_Init` loaded (see [ColAndreas collision](#colandreas-collision)).

### 5. Recommended streamer settings

These maps are big: thousands of objects close together. Without these settings, players can be kicked or IP-blocked when they arrive in a city.

- The filterscripts set `Streamer_VisibleItems(STREAMER_TYPE_OBJECT, 1000)`. The original scripts used 2000, but the client cannot show more than 1000 objects, and the extra objects only flood it.
- Turn on chunk streaming in your gamemode, so objects are sent in small batches instead of all at once:

  ```pawn
  Streamer_ToggleChunkStream(1);
  Streamer_SetChunkSize(STREAMER_TYPE_OBJECT, 50);
  ```

  Without it, arriving in a city sends around 1000 object RPCs in one go. The client's acknowledgements can exceed open.mp's `network.acks_limit`, and the server then blocks the player's IP for `network.limits_ban_time`. To the player, this looks like a random ban.
- When you teleport a player into a city, load the objects around the destination first so they don't fall through the ground:

  ```pawn
  Streamer_UpdateEx(playerid, x, y, z, GetPlayerVirtualWorld(playerid), GetPlayerInterior(playerid), STREAMER_TYPE_OBJECT, 1500, 1);
  SetPlayerPos(playerid, x, y, z);
  ```

### Filterscript commands

These come from the original scripts:

| Command | What it does |
|---|---|
| `/drawD <distance>` | Sets the draw distance of every map object. |
| `/streamD <distance>` | Sets the stream distance of every map object. |
| `/vcoinfo` | Shows the current distances and the object count. |

They are open to every player. Remove them or put an admin check on them before you go live.

---

## ColAndreas collision

By default, ColAndreas only knows San Andreas, so the cities have no collision on the server side: `CA_FindZ_For2DCoord` returns nothing there, and raycasts go straight through buildings.

The filterscripts fix this. Each script runs its model and object lists twice:

1. **On start:** the normal run. `AddSimpleModel` registers each model, and `CreateDynamicObject` places each object.
2. **3 seconds later** (`VC2SA_BuildCollision` / `LC2SA_BuildCollision`): the same lists, but in collision mode:
   - every model: `CA_LoadFromDff(modelid, "vc2sa/dff/<name>.dff")` reads the collision out of the model's DFF.
   - every object: `CA_CreateObject(modelid, x, y, z, rx, ry, rz)` places that collision in the ColAndreas world.

The 3-second delay makes sure the gamemode's `CA_Init` has run first. When the collision is ready, the server console shows:

```
[VC2SA] ColAndreas: <n> models with collision (<n> without), <n> objects
```

"Without" counts DFFs that have no collision data. This is normal for decorations such as signs and plants.

After that, the usual functions work in both cities:

```pawn
new Float:z;
CA_FindZ_For2DCoord(6431.0, -268.4, z); // Ocean Beach, Vice City
SetPlayerPos(playerid, 6431.0, -268.4, z + 1.0);
```

The collision objects are static: they are not tied to virtual worlds or interiors.

---

## Zones and helpers

`include/vc_lc.inc` holds every district of both cities as 3D boxes, taken from the original zone files (`navig.zon` for Vice City, `gta3.zon` for Liberty City) and moved by the offsets. The rows are ordered so that the first match wins.

```pawn
#include <vc_lc>

new name[32];
GetPlayerPos(playerid, x, y, z);
if(VCLC_GetZoneName(x, y, z, name))  // "Ocean Beach", "Chinatown", ...
{
    // outside a district box it falls back to "Vice City" / "Liberty City"
}
```

| Function | Returns |
|---|---|
| `VCLC_GetCity(Float:x, Float:y)` | `VCLC_CITY_VC`, `VCLC_CITY_LC` or `VCLC_CITY_NONE` |
| `VCLC_GetCityName(city, name[], len)` | `"Vice City"`, `"Liberty City"` or `"San Andreas"` |
| `VCLC_GetZoneName(Float:x, Float:y, Float:z, name[], len)` | 1 and the district name, or 0 outside both cities |
| `VCLC_FromOriginal(city, ox, oy, oz, &x, &y, &z)` | Converts a position from the original game to the world |
| `VCLC_ToOriginal(x, y, z, &ox, &oy, &oz)` | Converts a world position to the original game; 0 outside both cities |

If you already have a San Andreas zone table (`SAZones` style), you can copy the rows of `VCLC_Zones` into it. They use the same `{min x, min y, min z, max x, max y, max z}` layout.

Two small Liberty City zones have only internal names in `gta3.zon` (`SUB_ZO2` and `SUB_ZO3`). They are listed as "Shoreside Vale" here.

---

## GPS road networks

`scriptfiles/GPS_VC.dat` and `scriptfiles/GPS_LC.dat` are the car-path road networks of both cities, in the same format as the `GPS.dat` of the SA-MP GPS plugin (RouteConnector):

```
0 x y z -1 id      a node (a point on a road)
1 from to 0        a connection from node `from` to node `to`
```

| File | Node IDs | Nodes | Connections |
|---|---|---|---|
| `GPS_VC.dat` | 32768 and up | 3699 | 7578 |
| `GPS_LC.dat` | 40960 and up | 3218 | 6658 |

- Every road is listed in both directions, like in San Andreas' `GPS.dat`.
- The IDs start above San Andreas' own node IDs (up to 30586), so you can append both files to the end of an existing `GPS.dat` without conflicts. The cities are not connected to San Andreas or to each other.
- Positions are already in world coordinates (offsets applied), at road level.
- Small separate pieces under 100 nodes are left out (for example the race track inside the Vice City stadium).

You can load these files with your own pathfinding code (A*, Dijkstra) or with a GPS plugin that reads `GPS.dat`.

### How they were made

`tools/city_roads.pl` builds them from the original game files:

- **Vice City:** `data/maps/paths.ipl`, the car groups (12 nodes each, absolute positions in 1/16 units).
- **GTA III:** the `path` sections of the map IDE files. Their positions are relative to every IPL instance of that model, so they are rotated by the instance's heading (`-2 * atan2(qz, qw)`; III stores the rotation inverted) and moved to it. The generic road pieces are in `data/maps/generic.ide`. Without them, Liberty City splits into 92 pieces.

External nodes from different groups that are on the same spot (within 1 m) are joined into one node, the same way the games do it.

```
perl tools/city_roads.pl vc <paths.ipl> scriptfiles/GPS_VC.dat
perl tools/city_roads.pl lc scriptfiles/GPS_LC.dat <all GTA III map .ide and .ipl files>
```

The original files can be read at [noclip.website](https://noclip.website) (`GrandTheftAutoViceCity/data/maps/paths.ipl`, `GrandTheftAuto3/data/maps/<area>/<area>.ide/.ipl`).

---

## vc_lc.asi (radar and pause-menu maps)

Without this plugin, the radar and the pause-menu map always show San Andreas: in Vice City or Liberty City, the player walks on empty sea. **vc_lc.asi** fixes this on the player's side.

While the player is inside one of the two cities:

- The **HUD radar** shows the city map around the player.
- The **pause-menu map** (Esc > Map) shows the whole city.
- **Blips, the player arrow, gang zones and checkpoints** are in the right place on both.
- **The zone name** under the menu crosshair is the city's own district ("Ocean Beach", "Chinatown").
- **Placing a waypoint** on the menu map puts it in the right place in the world. `OnPlayerClickMap` gets the correct world coordinates too.

Anywhere else, the game's own San Andreas maps are drawn as usual.

### Install

1. Download `vc_lc.asi` from [Releases](../../releases).
2. Put it in the GTA San Andreas folder (next to `gta_sa.exe`).
3. You need an ASI loader. If you already use other `.asi` mods (or CLEO), you have one. If not, install [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader).

It needs **gta_sa.exe 1.0 US**, the version SA-MP itself needs. It does not depend on the SA-MP version. It does not talk to the server either: it only checks where the player is.

The plugin writes `vc_lc.log` next to itself. If something does not work, look there first:

| Log line | Meaning |
|---|---|
| `vc_lc 1.0.0: city maps on` | Everything is hooked. |
| `Vice City map ready` / `Liberty City map ready` | The city map was loaded the first time it was needed. |
| `this is not gta_sa.exe 1.0 US` | Wrong game version: the plugin stays off. |
| `... changed by another mod, city maps are off` | Another radar or map mod got there first. The plugin stays off rather than crash. |

### How it works

The original city map (4000 x 4000 units) is stretched over San Andreas' map grid (6000 x 6000, 12 x 12 tiles of 500 units):

```
map position = (world position - city offset) * 1.5
```

When the player is in a city, the radar runs in this "map space". Every world position that goes into the radar is converted on the way in:

| gta_sa.exe 1.0 US | What is hooked and why |
|---|---|
| `0x583530` `CRadar::TransformRealWorldPointToRadarSpace` | Every blip position is converted. |
| `0x56E400` `FindPlayerCoors` (inside `CRadar::DrawBlips` `0x588050`) | The player arrow (DrawBlips computes it inline). |
| `0x5853D0` `CRadar::DrawAreaOnRadar` | Gang zones (also inline); zones outside the city are skipped. |
| calls at `0x588157` / `0x588203` | The north marker and the menu crosshair are already in map space, so they skip the conversion. |
| `0x575130` `CMenuManager::PrintMap` | Switches the conversion on while the pause-menu map is drawn. |
| `0x586520` `CRadar::DrawRadarSectionMap` | Draws the city tiles in the pause menu. |
| `0x571D90` | The zone name under the crosshair. |
| call at `0x5775D2` (`CRadar::SetCoordBlip`) | Converts the clicked map position back to the world (waypoint, `OnPlayerClickMap`). |
| call at `0x58A38A` (`CRadar::DrawMap`), `0x586880` `CRadar::DrawRadarMap` | Moves the HUD radar's origin into map space and scales its range by 1.5, so the radar shows the same area as usual. |
| `0x586110` `CRadar::DrawRadarSection` | Draws the HUD radar with the city tiles, plus an extra ring of tiles, because the city map is 1.5x denser. |

The map pictures are built into the ASI. On first use, they are decoded (Windows Imaging Component) and cut into 144 tiles of 128 px, like San Andreas' `radar00` .. `radar143`.

Before it hooks anything, the plugin checks the game version and the code it is about to change. If another mod has already changed one of those places, the plugin turns itself off and writes a log line instead of crashing. If another mod has redirected the `DrawMap` or `SetCoordBlip` calls, the plugin passes the calls on to that mod.

### Build

You need Visual Studio 2022 or newer (the "Desktop development with C++" workload, which includes CMake and Ninja). The build is 32-bit only.

```bat
:: from an "x86 Native Tools Command Prompt for VS"
client\build.bat
```

Output: `client\build\vc_lc.asi`. CMake downloads [kthook](https://github.com/kin4stat/kthook), the hooking library, by itself. The C runtime is linked statically, so players don't need the VC++ redistributable.

---

## Map images

`maps/vc.png` and `maps/lc.png` (1024 x 1024) are the radar maps of the original games, put together from their `radar00.txd` .. `radar63.txd` tiles (8 x 8 tiles of 128 px; the top-left tile is the north-west corner; the image covers original -2000 .. 2000).

- Vice City's tiles are DXT1 and are used as they are.
- GTA III's tiles store the map in the alpha channel, so they are drawn over a dark blue background (28, 36, 48). The placeholder town names on the unused mainland at the top are painted out.

You can also use these images for websites, textdraws or minimaps of your own. To convert a world position to a pixel on the image:

```
px = (world_x - offset_x + 2000) / 4000 * 1024
py = (2000 - (world_y - offset_y)) / 4000 * 1024
```

To rebuild them (and the ASI's built-in copy):

```
perl tools/radar2png.pl <folder with VC's radarNN.txd> maps/vc.png
BG_R=28 BG_G=36 BG_B=48 CLEAN_Y=62 perl tools/radar2png.pl <folder with III's radarNN.txd> maps/lc.png
perl tools/bin2hpp.pl > client/src/map_data.hpp
```

`radar2png.pl` needs Perl with `Compress::Zlib`. The `radarNN.txd` files are in the game's `models/gta3.img`.

---

## Known issues

From the original conversions:

- Some objects did not convert (a few jumps and barrels).
- Some road signs have the wrong rotation, because only Z rotation was converted.
- Some objects appear twice (Vice City: Ocean Beach, Malibu, parts of the mansion).
- Parts of Liberty City are flooded: they are below San Andreas' sea level, and the water cannot be controlled from a script.
- There are no LOD models. Objects that had one get a longer draw distance instead.

From this release:

- The pause-menu map shows the city the player is in. It cannot be scrolled from San Andreas to Vice City.
- On the pause-menu map, blips in a different city than the player's are hidden.

---

## Repository layout

```
filterscripts/   vc2sa4samp.pwn, lc2sa4samp.pwn (maps + ColAndreas collision)
models/          vc2sa/, lc2sa/ (dff + txd of both cities)
include/         gl_common.inc (strtok for the filterscripts), vc_lc.inc (zones, offsets, helpers)
scriptfiles/     GPS_VC.dat, GPS_LC.dat (road networks)
maps/            vc.png, lc.png (city maps)
client/          vc_lc.asi source (CMake)
tools/           city_roads.pl, radar2png.pl, bin2hpp.pl
```

## Credits

- **Rockstar Games:** Grand Theft Auto: Vice City and Grand Theft Auto III, the original maps, models, paths and radar art.
- **The GTA:United team:** the conversion of both cities to San Andreas.
- **VC2SA 4 SAMP / LC2SA 4 SAMP:** the SA-MP 0.3.DL conversion of GTA:United 1.1 that the filterscripts and models come from.
- **[ColAndreas](https://github.com/Pottus/ColAndreas)**, **[Streamer](https://github.com/samp-incognito/samp-streamer-plugin)**, **[kthook](https://github.com/kin4stat/kthook)**.
- Corrected offsets, ColAndreas collision, zones, road networks, maps and vc_lc.asi: **noiseXYZ** (neXus, [nexmp.eu](https://nexmp.eu)).

## License

The code and tools (`client/`, `include/vc_lc.inc`, `tools/` and the collision code in the filterscripts) are under the [MIT License](LICENSE), (c) 2026 noiseXYZ.

The map content is **not** covered by it. That includes the models, object placements, paths, zones and radar art, and the road files and map images made from them. It comes from Grand Theft Auto: Vice City and Grand Theft Auto III, (c) Rockstar Games, by way of GTA:United and the VC2SA / LC2SA 4 SAMP conversion, and belongs to its owners.
