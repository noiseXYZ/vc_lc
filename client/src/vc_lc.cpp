// vc_lc.asi - Vice City / Liberty City radar and pause-menu map for GTA:SA 1.0 US.
//
// While the player is inside one of the two maps (vc2sa4samp / lc2sa4samp), the HUD radar
// and the pause-menu map show that city instead of San Andreas: blips, the player arrow,
// gang zones, the waypoint and the zone name under the crosshair all work. Anywhere else
// the game draws its own maps.
//
// The original 4000 x 4000 city map is stretched over SA's 6000 x 6000 map:
//   map = (world - city offset) * 1.5
// The radar then works in map space, so only world positions going in are converted:
// - CRadar::TransformRealWorldPointToRadarSpace (0x583530): every blip.
// - FindPlayerCoors (0x56E400) inside CRadar::DrawBlips (0x588050): the player arrow.
// - CRadar::DrawAreaOnRadar (0x5853D0): gang zones. Zones outside the city are skipped.
// DrawBlips also transforms two points that are already in map space (north marker at
// 0x588157, menu crosshair at 0x588203); those calls skip the conversion.
//
// Menu: CMenuManager::PrintMap (0x575130) switches the conversion on while it runs,
// CRadar::DrawRadarSectionMap (0x586520) draws our tiles, 0x571D90 names the zone under the
// crosshair, and the map click's CRadar::SetCoordBlip (call at 0x5775D2) converts the
// crosshair back to the world (waypoint, OnPlayerClickMap).
//
// HUD: CRadar::DrawMap (0x586B00, called at 0x58A38A) sets the radar origin and range, then
// jumps to CRadar::DrawRadarMap (0x586880). That hook moves the origin into map space and
// scales the range by 1.5, so the radar shows the same area as normal. CRadar::DrawRadarSection
// (0x586110) is redone with our tiles plus a ring around the game's 3 x 3 tiles, because the
// city map is 1.5x denser.
//
// Tiles are made on the first draw (WIC decodes the PNG, 1024 -> 1536 px, 144 rasters of
// 128 px). Any failure is written to vc_lc.log and the game's own maps are drawn instead.

#include <Windows.h>
#include <wincodec.h>

#include <algorithm>
#include <bit>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

#include <kthook/kthook.hpp>

#include "map_data.hpp"

namespace vc_lc {
namespace {
HMODULE self_module = nullptr;

FILE *open_log() {
    char path[MAX_PATH]{};
    GetModuleFileNameA(self_module, path, MAX_PATH);
    char *slash = std::strrchr(path, '\\');
    std::strcpy(slash != nullptr ? slash + 1 : path, "vc_lc.log");
    return std::fopen(path, "w");
}

void log(const char *fmt, ...) {
    static FILE *file = open_log();
    if (file == nullptr) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    std::vfprintf(file, fmt, args);
    va_end(args);
    std::fputc('\n', file);
    std::fflush(file);
}

template <typename Fn>
struct function_traits;
template <typename R, typename... Args>
struct function_traits<R(__cdecl *)(Args...)> {
    using result = R;
};
template <typename R, typename... Args>
struct function_traits<R(__thiscall *)(Args...)> {
    using result = R;
};

// kthook_simple with the detour getting the original function as its first argument
template <typename Fn, auto Detour>
class hook {
    using result_t = typename function_traits<Fn>::result;
    using impl_t   = kthook::kthook_simple<Fn>;

public:
    explicit hook(Fn target) : target_{target} {}

    bool install() {
        impl_ = std::make_unique<impl_t>(
            std::bit_cast<std::uintptr_t>(target_),
            [](const impl_t &hk, auto &&...args) -> result_t {
                if constexpr (std::is_void_v<result_t>) {
                    Detour(hk.get_trampoline(), std::forward<decltype(args)>(args)...);
                } else {
                    return Detour(hk.get_trampoline(), std::forward<decltype(args)>(args)...);
                }
            },
            false);
        if (!impl_->install()) {
            impl_.reset();
            return false;
        }
        return true;
    }

private:
    Fn                      target_;
    std::unique_ptr<impl_t> impl_;
};

struct vector2 {
    float x, y;
};
struct vector3 {
    float x, y, z;
};
struct rect {
    float left, bottom, right, top; // CRect
};
struct rgba {
    std::uint8_t r, g, b, a;
};

struct zone {
    const char *name;
    float       min_x, min_y, max_x, max_y;
};

// same rows as include/vc_lc.inc: first match wins
constexpr zone vc_zones[] = {
    {"Junkyard",                4603.240,  1007.089,  4791.790,  1280.390},
    {"Downtown",                4386.970,  1463.128,  5786.270,  2727.320},
    {"Vice Point",              6163.656,   698.847,  7246.030,  2448.850},
    {"Washington Beach",        5896.030,   119.474,  7246.030,   698.847},
    {"Ocean Beach",             5746.794,  -755.370,  7254.900,   119.474},
    {"Escobar International",   4111.790,  -729.610,  4791.790,  1280.390},
    {"Starfish Island",         5251.794,   231.734,  5895.495,   808.533},
    {"Prawn Island",            5786.270,  1847.605,  6163.656,  2293.470},
    {"Leaf Links",              5786.270,   808.571,  6163.656,  1847.605},
    {"Viceport",                4791.790,  -729.610,  5746.794,   151.262},
    {"Little Havana",           4791.790,   151.262,  5251.794,   808.533},
    {"Little Haiti",            4791.790,   808.533,  5421.711,  1462.660},
};
constexpr zone lc_zones[] = {
    {"Callahan Bridge",        -4732.558, -1158.347, -4284.560, -1108.347},
    {"Callahan Bridge",        -4905.232, -1158.298, -4735.122, -1108.298},
    {"Callahan Point",         -4598.320, -1378.220, -4284.320, -1158.725},
    {"Atlantic Quays",         -4284.120, -1451.550, -3848.120, -1269.930},
    {"Portland Harbor",        -3986.320, -1269.650, -3534.320,  -813.646},
    {"Trenton",                -4284.120, -1269.850, -3986.620,  -942.054},
    {"Chinatown",              -4604.579, -1108.289, -4284.580,  -663.690},
    {"Red Light District",     -4604.622,  -663.616, -4284.620,  -482.616},
    {"Hepburn Heights",        -4604.579,  -482.400, -4284.580,  -278.770},
    {"Saint Mark's",           -4284.100,  -712.324, -3961.100,  -278.324},
    {"Harwood",                -4604.021,  -278.178, -3961.020,   122.676},
    {"Portland Beach",         -3960.630,  -813.467, -3552.400,    -0.372},
    {"Portland View",          -4283.900,  -941.806, -3986.400,  -712.806},
    {"Fort Staunton",          -5110.122,  -611.617, -4735.678,  -261.617},
    {"Aspatria",               -5575.764,  -612.604, -5233.764,   -39.504},
    {"Torrington",             -5150.234, -1872.420, -4772.234, -1259.930},
    {"Bedford Point",          -5574.438, -1872.050, -5150.438, -1204.450},
    {"Newport",                -5149.893, -1259.190, -4734.893,  -612.193},
    {"Belleville Park",        -5471.567, -1203.070, -5150.729,  -613.068},
    {"Liberty Campus",         -5232.732,  -611.622, -5110.732,  -261.622},
    {"Rockford",               -5232.764,  -261.111, -4734.764,    68.889},
    {"Francis Intl. Airport",  -6982.970, -1544.710, -5818.629,  -468.443},
    {"Wichita Gardens",        -6161.835,  -468.074, -5721.041,  -107.274},
    {"Cedar Grove",            -6217.229,  -106.612, -5616.914,   450.058},
    {"Pike Creek",             -6757.570,  -467.966, -6162.306,  -107.244},
    {"Cochrane Dam",           -6744.500,  -106.556, -6217.520,   504.544},
    {"Shoreside Vale",         -5615.444,   -38.887, -5471.287,   167.043},
    {"Shoreside Vale",         -5615.434,  -120.908, -5576.334,   -38.936},
    {"Portland",               -4732.849, -1529.720, -3447.340,   234.115},
    {"Staunton Island",        -5615.479, -1919.970, -4734.480,   167.265},
    {"Shoreside Vale",         -6994.640, -1551.380, -5616.895,  1006.350},
};

constexpr int   grid      = 12;     // SA's map: 12 x 12 tiles of 500 units, -3000..3000
constexpr int   tile_px   = 128;
constexpr float tile_size = 500.0f;
constexpr float map_half  = 3000.0f;
constexpr float map_scale = 1.5f;   // original city map (-2000..2000) on SA's map (-3000..3000)
constexpr float city_half = 2000.0f;

struct city {
    const char         *name;
    float               offset_x, offset_y; // world = original city position + offset
    const zone         *zones;
    std::size_t         zone_count;
    const std::uint8_t *png;
    std::size_t         png_size;
    void               *tiles[grid * grid]; // RwRaster*
    rgba                water;              // the map's corner colour, drawn outside the 12 x 12 tiles
    bool                tried;
    bool                ready;
};

city cities[] = {
    {"Vice City", 6000.0f, 1050.0f, vc_zones, std::size(vc_zones), map_data::vc_png, sizeof(map_data::vc_png), {}, {}, false, false},
    {"Liberty City", -5350.0f, -200.0f, lc_zones, std::size(lc_zones), map_data::lc_png, sizeof(map_data::lc_png), {}, {}, false, false},
};

// gta_sa.exe 1.0 US
constexpr std::uintptr_t print_map_address         = 0x575130; // CMenuManager::PrintMap
constexpr std::uintptr_t draw_map_address          = 0x586B00; // CRadar::DrawMap
constexpr std::uintptr_t draw_map_call_address     = 0x58A38A; // the HUD's call of DrawMap (its only caller)
constexpr std::uintptr_t draw_radar_map_address    = 0x586880; // CRadar::DrawRadarMap
constexpr std::uintptr_t draw_section_address      = 0x586110; // CRadar::DrawRadarSection (HUD tile)
constexpr std::uintptr_t draw_section_map_address  = 0x586520; // CRadar::DrawRadarSectionMap (menu tile)
constexpr std::uintptr_t draw_blips_address        = 0x588050; // CRadar::DrawBlips
constexpr std::uintptr_t draw_area_address         = 0x5853D0; // CRadar::DrawAreaOnRadar
constexpr std::uintptr_t to_radar_space_address    = 0x583530; // CRadar::TransformRealWorldPointToRadarSpace
constexpr std::uintptr_t to_screen_address         = 0x583480; // CRadar::TransformRadarPointToScreenSpace
constexpr std::uintptr_t texture_corners_address   = 0x584D90; // CRadar::GetTextureCorners
constexpr std::uintptr_t clip_poly_address         = 0x585040; // CRadar::ClipRadarPoly
constexpr std::uintptr_t player_coors_address      = 0x56E400; // FindPlayerCoors
constexpr std::uintptr_t player_centre_address     = 0x56E320; // FindPlayerCentreOfWorld_NoInteriorShift
constexpr std::uintptr_t crosshair_zone_address    = 0x571D90; // menu: zone name at the crosshair
constexpr std::uintptr_t waypoint_call_address     = 0x5775D2; // menu: call of CRadar::SetCoordBlip
constexpr std::uintptr_t set_coord_blip_address    = 0x583820; // CRadar::SetCoordBlip
constexpr std::uintptr_t north_call_address        = 0x588157; // DrawBlips: north marker (map space)
constexpr std::uintptr_t crosshair_call_address    = 0x588203; // DrawBlips: menu crosshair (map space)
constexpr std::uintptr_t raster_create_address     = 0x7FB230; // RwRasterCreate
constexpr std::uintptr_t raster_lock_address       = 0x7FB2D0; // RwRasterLock
constexpr std::uintptr_t raster_unlock_address     = 0x7FAEC0; // RwRasterUnlock
constexpr std::uintptr_t set_vertices_address      = 0x727420; // CSprite2d::SetVertices(rect, 4 colours)
constexpr std::uintptr_t set_mask_vertices_address = 0x727890; // CSprite2d::SetMaskVertices(n, xy, uv, colour)
constexpr std::uintptr_t im2d_render_address       = 0x734E90; // RwIm2DRenderPrimitive
constexpr std::uintptr_t sprite_vertices_address   = 0xC80468; // CSprite2d::maVertices
constexpr std::uintptr_t rw_engine_address         = 0xC97B24; // RwEngineInstance
constexpr std::uintptr_t radar_origin_address      = 0xBAA248; // CRadar::vec2DRadarOrigin
constexpr std::uintptr_t radar_range_address       = 0xBA8314; // CRadar::m_radarRange
constexpr std::uintptr_t radar_cos_address         = 0xBA8308; // cachedCos
constexpr std::uintptr_t radar_sin_address         = 0xBA830C; // cachedSin
constexpr std::uintptr_t menu_map_flag_address     = 0xBA67A1; // FrontEndMenuManager.m_bDrawRadarOrMap
constexpr std::uintptr_t grey_radar_flag_address   = 0xA444A4; // DrawRadarSection draws plain grey
constexpr std::uintptr_t version_check_address     = 0x82457C; // 0x94BF on 1.0 US
constexpr std::size_t    render_state_set_offset   = 0x20;     // dOpenDevice.fpRenderStateSet
constexpr std::size_t    raster_stride_offset      = 0x18;

constexpr int raster_texture_8888  = 0x504; // rwRASTERTYPETEXTURE | rwRASTERFORMAT8888
constexpr int raster_lock_write    = 2;
constexpr int state_texture_raster = 1;
constexpr int primitive_tri_fan    = 5;

using player_centre_t     = const vector3 *(__cdecl *)(int player);
using raster_create_t     = void *(__cdecl *)(int width, int height, int depth, int flags);
using raster_lock_t       = std::uint8_t *(__cdecl *)(void *raster, std::uint8_t level, int mode);
using raster_unlock_t     = void *(__cdecl *)(void *raster);
using set_vertices_t      = void(__cdecl *)(const rect &r, const rgba &c0, const rgba &c1, const rgba &c2, const rgba &c3);
using set_mask_vertices_t = void(__cdecl *)(int count, const float *positions, const float *uvs, const rgba &colour);
using im2d_render_t       = int(__cdecl *)(int primitive, void *vertices, int count);
using render_state_t      = int(__cdecl *)(int state, void *value);
using set_coord_blip_t    = int(__cdecl *)(int type, vector3 position, unsigned colour, int display, const char *name);
using to_screen_t         = void(__cdecl *)(vector2 *out, const vector2 *in);
using texture_corners_t   = void(__cdecl *)(int x, int y, vector2 *out);
using clip_poly_t         = int(__cdecl *)(vector2 *out, const vector2 *in);

city *menu_city = nullptr; // PrintMap is drawing this city's map
city *hud_city  = nullptr; // the HUD radar is in this city's map space (until the next DrawMap)
bool  in_blips  = false;   // inside CRadar::DrawBlips
bool  raw_point = false;   // the point being transformed is already in map space
bool  ring_due  = false;   // HUD: draw the outer ring with the next tile

template <typename T>
T &game(std::uintptr_t address) {
    return *reinterpret_cast<T *>(address);
}

city *current_city() {
    return menu_city != nullptr ? menu_city : hud_city;
}

city *city_at(float x, float y) {
    for (city &c : cities) {
        const float ox = x - c.offset_x;
        const float oy = y - c.offset_y;
        if (ox >= -city_half && ox <= city_half && oy >= -city_half && oy <= city_half) {
            return &c;
        }
    }
    return nullptr;
}

city *player_city() {
    const vector3 *p = reinterpret_cast<player_centre_t>(player_centre_address)(0);
    return p != nullptr ? city_at(p->x, p->y) : nullptr;
}

vector2 to_map(const city &c, vector2 world) {
    return {(world.x - c.offset_x) * map_scale, (world.y - c.offset_y) * map_scale};
}

vector2 to_world(const city &c, vector2 map) {
    return {map.x / map_scale + c.offset_x, map.y / map_scale + c.offset_y};
}

// PNG -> size x size BGRA pixels
std::unique_ptr<std::uint8_t[]> decode(const std::uint8_t *png, std::size_t png_size, UINT size) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IWICImagingFactory    *factory   = nullptr;
    IWICStream            *stream    = nullptr;
    IWICBitmapDecoder     *decoder   = nullptr;
    IWICBitmapFrameDecode *frame     = nullptr;
    IWICBitmapScaler      *scaler    = nullptr;
    IWICFormatConverter   *converter = nullptr;

    std::unique_ptr<std::uint8_t[]> pixels(new std::uint8_t[size * size * 4]);
    const bool ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
                    SUCCEEDED(factory->CreateStream(&stream)) &&
                    SUCCEEDED(stream->InitializeFromMemory(const_cast<BYTE *>(png), static_cast<DWORD>(png_size))) &&
                    SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) &&
                    SUCCEEDED(decoder->GetFrame(0, &frame)) && SUCCEEDED(factory->CreateBitmapScaler(&scaler)) &&
                    SUCCEEDED(scaler->Initialize(frame, size, size, WICBitmapInterpolationModeFant)) &&
                    SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
                    SUCCEEDED(converter->Initialize(scaler, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)) &&
                    SUCCEEDED(converter->CopyPixels(nullptr, size * 4, size * size * 4, pixels.get()));

    if (converter) converter->Release();
    if (scaler) scaler->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (stream) stream->Release();
    if (factory) factory->Release();
    return ok ? std::move(pixels) : nullptr;
}

// 144 rasters of 128 px, row 0 = north (like SA's radar00..radar143)
bool make_tiles(city &c) {
    constexpr UINT size = grid * tile_px;

    const auto pixels = decode(c.png, c.png_size, size);
    if (!pixels) {
        log("could not decode the %s map", c.name);
        return false;
    }
    c.water = {pixels[2], pixels[1], pixels[0], 255}; // top-left pixel, BGRA

    const auto raster_create = reinterpret_cast<raster_create_t>(raster_create_address);
    const auto raster_lock   = reinterpret_cast<raster_lock_t>(raster_lock_address);
    const auto raster_unlock = reinterpret_cast<raster_unlock_t>(raster_unlock_address);

    for (int ty = 0; ty < grid; ty++) {
        for (int tx = 0; tx < grid; tx++) {
            void *raster = raster_create(tile_px, tile_px, 32, raster_texture_8888);
            if (raster == nullptr) {
                log("could not create a %s tile", c.name);
                return false;
            }
            c.tiles[tx + ty * grid] = raster;

            std::uint8_t *dst = raster_lock(raster, 0, raster_lock_write);
            if (dst == nullptr) {
                log("could not lock a %s tile", c.name);
                return false;
            }
            const int stride = *reinterpret_cast<int *>(static_cast<std::uint8_t *>(raster) + raster_stride_offset);
            for (int y = 0; y < tile_px; y++) {
                const std::uint8_t *src = pixels.get() + ((ty * tile_px + y) * size + tx * tile_px) * 4;
                std::memcpy(dst + y * stride, src, tile_px * 4);
            }
            raster_unlock(raster);
        }
    }
    log("%s map ready", c.name);
    return true;
}

bool tiles_ready(city &c) {
    if (!c.tried) {
        c.tried = true;
        c.ready = make_tiles(c);
    }
    return c.ready;
}

city *ready_player_city() {
    city *c = player_city();
    return c != nullptr && tiles_ready(*c) ? c : nullptr;
}

void set_texture(void *raster) {
    void *const engine           = game<void *>(rw_engine_address);
    const auto  render_state_set = *reinterpret_cast<render_state_t *>(static_cast<std::uint8_t *>(engine) + render_state_set_offset);
    render_state_set(state_texture_raster, raster);
}

// CRadar::DrawRadarSection with our tile: the tile's map-space corners go to radar space,
// are clipped to the radar shape, and get texture coordinates back from the clipped points.
// Outside the 12 x 12 tiles it is the map's water colour.
void draw_tile(const city &c, int x, int y) {
    const vector2 origin = game<vector2>(radar_origin_address);
    const float   range  = game<float>(radar_range_address);
    const float   cos_r  = game<float>(radar_cos_address);
    const float   sin_r  = game<float>(radar_sin_address);

    vector2 corners[4];
    reinterpret_cast<texture_corners_t>(texture_corners_address)(x, y, corners);

    vector2 radar[4];
    for (int i = 0; i < 4; i++) {
        const float dx = (corners[i].x - origin.x) / range;
        const float dy = (corners[i].y - origin.y) / range;
        radar[i]       = {sin_r * dy + cos_r * dx, cos_r * dy - sin_r * dx};
    }

    vector2   clipped[16];
    const int count = reinterpret_cast<clip_poly_t>(clip_poly_address)(clipped, radar);
    if (count <= 2) {
        return;
    }

    const float left = x * tile_size - map_half;
    const float top  = map_half - y * tile_size;
    vector2     screen[16];
    vector2     uv[16];
    for (int i = 0; i < count; i++) {
        const float wx = (cos_r * clipped[i].x - sin_r * clipped[i].y) * range + origin.x;
        const float wy = (sin_r * clipped[i].x + cos_r * clipped[i].y) * range + origin.y;
        uv[i]          = {(wx - left) / tile_size, (top - wy) / tile_size};
        reinterpret_cast<to_screen_t>(to_screen_address)(&screen[i], &clipped[i]);
    }

    const bool inside = x >= 0 && x < grid && y >= 0 && y < grid;
    const rgba white{255, 255, 255, 255};
    set_texture(inside ? c.tiles[x + y * grid] : nullptr);
    reinterpret_cast<set_mask_vertices_t>(set_mask_vertices_address)(count, &screen[0].x, &uv[0].x, inside ? white : c.water);
    reinterpret_cast<im2d_render_t>(im2d_render_address)(primitive_tri_fan, reinterpret_cast<void *>(sprite_vertices_address), count);
}

// --- hooks ----------------------------------------------------------------------------

using print_map_t = void(__thiscall *)(void *menu);

void print_map(print_map_t orig, void *menu) {
    hud_city  = nullptr; // InitFrontEndMap puts the radar back to world space
    menu_city = ready_player_city();
    orig(menu);
    menu_city = nullptr;
}

using draw_map_t = void(__cdecl *)();

draw_map_t draw_map_next = nullptr; // the call's previous target (DrawMap, or another mod)

// the HUD's call of CRadar::DrawMap (patched call: DrawMap starts with a relative call)
void __cdecl draw_map_call() {
    hud_city = nullptr; // DrawMap sets the world-space origin again
    draw_map_next();
}

using draw_radar_map_t = void(__cdecl *)();

void draw_radar_map(draw_radar_map_t orig) {
    if (!game<bool>(menu_map_flag_address)) {
        if (city *c = ready_player_city(); c != nullptr) {
            game<vector2>(radar_origin_address) = to_map(*c, game<vector2>(radar_origin_address));
            game<float>(radar_range_address) *= map_scale;
            hud_city = c;
            ring_due = true;
        }
    }
    orig();
    ring_due = false;
}

using draw_section_t = void(__cdecl *)(int x, int y);

void draw_section(draw_section_t orig, int x, int y) {
    if (hud_city == nullptr || game<bool>(grey_radar_flag_address)) {
        orig(x, y);
        return;
    }
    if (ring_due) {
        // DrawRadarMap's first tile is (x - 1, y - 1) of its 3 x 3: add the ring around them
        ring_due     = false;
        const int cx = x + 1;
        const int cy = y + 1;
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                if ((std::max)(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy) == 2) {
                    draw_tile(*hud_city, cx + dx, cy + dy);
                }
            }
        }
    }
    draw_tile(*hud_city, x, y);
}

using to_radar_space_t = void(__cdecl *)(vector2 *out, const vector2 *in);

void to_radar_space(to_radar_space_t orig, vector2 *out, const vector2 *in) {
    city *c = current_city();
    if (c == nullptr || in == nullptr || raw_point) {
        orig(out, in);
        return;
    }
    vector2 map = to_map(*c, *in);
    if (menu_city != nullptr && city_at(in->x, in->y) != c) {
        map = {1.0e5f, 1.0e5f}; // menu: not in this city, off-screen
    }
    orig(out, &map);
}

// DrawBlips' calls with points that are already in map space
void __cdecl to_radar_space_raw(vector2 *out, const vector2 *in) {
    raw_point = true;
    reinterpret_cast<to_radar_space_t>(to_radar_space_address)(out, in); // through the hook, unchanged
    raw_point = false;
}

using draw_blips_t = void(__cdecl *)();

void draw_blips(draw_blips_t orig) {
    in_blips = true;
    orig();
    in_blips = false;
}

using player_coors_t = vector3 *(__cdecl *)(vector3 *out, int player);

vector3 *player_coors(player_coors_t orig, vector3 *out, int player) {
    vector3 *p = orig(out, player);
    if (city *c = current_city(); in_blips && c != nullptr && p != nullptr) {
        const vector2 map = to_map(*c, {p->x, p->y}); // the player arrow
        p->x              = map.x;
        p->y              = map.y;
    }
    return p;
}

using draw_area_t = void(__cdecl *)(const rect *area, const rgba *colour, bool in_menu);

void draw_area(draw_area_t orig, const rect *area, const rgba *colour, bool in_menu) {
    city *c = current_city();
    if (c == nullptr || area == nullptr) {
        orig(area, colour, in_menu);
        return;
    }

    const vector2 low  = {(std::min)(area->left, area->right), (std::min)(area->bottom, area->top)};
    const vector2 high = {(std::max)(area->left, area->right), (std::max)(area->bottom, area->top)};
    if (high.x < c->offset_x - city_half || low.x > c->offset_x + city_half || high.y < c->offset_y - city_half ||
        low.y > c->offset_y + city_half) {
        return; // outside this city
    }

    const vector2 a = to_map(*c, {area->left, area->bottom});
    const vector2 b = to_map(*c, {area->right, area->top});
    const rect    moved{a.x, a.y, b.x, b.y};
    orig(&moved, colour, in_menu);
}

using draw_section_map_t = void(__cdecl *)(int x, int y, rect r);

void draw_section_map(draw_section_map_t orig, int x, int y, rect r) {
    if (menu_city == nullptr || x < 0 || x >= grid || y < 0 || y >= grid) {
        orig(x, y, r);
        return;
    }
    set_texture(menu_city->tiles[x + y * grid]);
    const rgba white{255, 255, 255, 255};
    reinterpret_cast<set_vertices_t>(set_vertices_address)(r, white, white, white, white);
    reinterpret_cast<im2d_render_t>(im2d_render_address)(primitive_tri_fan, reinterpret_cast<void *>(sprite_vertices_address), 4);
}

using crosshair_zone_t = const char *(__thiscall *)(void *self, float x, float y);

const char *crosshair_zone(crosshair_zone_t orig, void *self, float x, float y) {
    if (menu_city == nullptr) {
        return orig(self, x, y);
    }
    const vector2 world = to_world(*menu_city, {x, y});
    for (std::size_t i = 0; i < menu_city->zone_count; i++) {
        const zone &z = menu_city->zones[i];
        if (world.x >= z.min_x && world.x <= z.max_x && world.y >= z.min_y && world.y <= z.max_y) {
            return z.name;
        }
    }
    return menu_city->name;
}

set_coord_blip_t waypoint_next = nullptr; // the call's previous target (SetCoordBlip, or another mod)

// the map click's CRadar::SetCoordBlip: crosshair (map space) -> world
int __cdecl waypoint_blip(int type, vector3 position, unsigned colour, int display, const char *name) {
    if (city *c = player_city(); c != nullptr && c->ready) {
        const vector2 world = to_world(*c, {position.x, position.y});
        position.x          = world.x;
        position.y          = world.y;
    }
    return waypoint_next(type, position, colour, display, name);
}

// target of the call (E8 rel32) at `at`, 0 if there is no call
std::uintptr_t call_target(std::uintptr_t at) {
    const auto *code = reinterpret_cast<const std::uint8_t *>(at);
    if (code[0] != 0xE8) {
        return 0;
    }
    std::int32_t rel{};
    std::memcpy(&rel, code + 1, sizeof(rel));
    return at + 5 + rel;
}

bool redirect_call(std::uintptr_t at, const void *target) {
    auto *code = reinterpret_cast<std::uint8_t *>(at);
    DWORD old  = 0;
    if (code[0] != 0xE8 || !VirtualProtect(code, 5, PAGE_EXECUTE_READWRITE, &old)) {
        return false;
    }
    const auto rel = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(target) - (at + 5));
    std::memcpy(code + 1, &rel, sizeof(rel));
    VirtualProtect(code, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), code, 5);
    return true;
}

std::unique_ptr<hook<print_map_t, &print_map>>               print_map_hook;
std::unique_ptr<hook<draw_radar_map_t, &draw_radar_map>>     draw_radar_map_hook;
std::unique_ptr<hook<draw_section_t, &draw_section>>         draw_section_hook;
std::unique_ptr<hook<to_radar_space_t, &to_radar_space>>     to_radar_space_hook;
std::unique_ptr<hook<draw_blips_t, &draw_blips>>             draw_blips_hook;
std::unique_ptr<hook<player_coors_t, &player_coors>>         player_coors_hook;
std::unique_ptr<hook<draw_area_t, &draw_area>>               draw_area_hook;
std::unique_ptr<hook<draw_section_map_t, &draw_section_map>> draw_section_map_hook;
std::unique_ptr<hook<crosshair_zone_t, &crosshair_zone>>     crosshair_zone_hook;

// kthook copies a function's first instructions into its trampoline; a relative branch
// there would be moved wrongly, so only functions whose first 5 bytes branch nowhere are hooked
bool safe_to_hook(std::uintptr_t address) {
    for (std::uintptr_t p = address; p - address < 5;) {
        hde32s             hs{};
        const unsigned int len = hde32_disasm(reinterpret_cast<const void *>(p), &hs);
        const std::uint8_t op  = hs.opcode;
        if (len == 0 || (hs.flags & F_ERROR) != 0 || op == 0xE8 || op == 0xE9 || op == 0xEB || (op & 0xF0) == 0x70 ||
            (op & 0xFC) == 0xE0 || (op == 0x0F && (hs.opcode2 & 0xF0) == 0x80) || (op & 0xFE) == 0xC2 || op == 0xC3 || op == 0xFF) {
            return false;
        }
        p += len;
    }
    return true;
}

template <typename Hook, typename Fn>
bool install_hook(std::unique_ptr<Hook> &h, std::uintptr_t address, const char *what) {
    if (!safe_to_hook(address)) {
        log("%s is already changed by another mod, city maps are off", what);
        return false;
    }
    h = std::make_unique<Hook>(reinterpret_cast<Fn>(address));
    if (!h->install()) {
        log("could not hook %s, city maps are off", what);
        return false;
    }
    return true;
}

bool is_sa_10_us() {
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(reinterpret_cast<const void *>(version_check_address), &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT) {
        return false;
    }
    return game<std::uint32_t>(version_check_address) == 0x94BF;
}

void install() {
    if (!is_sa_10_us()) {
        log("this is not gta_sa.exe 1.0 US, city maps are off");
        return;
    }

    const std::uintptr_t waypoint = call_target(waypoint_call_address);
    const std::uintptr_t draw_map = call_target(draw_map_call_address);
    if (waypoint == 0 || draw_map == 0 || call_target(north_call_address) != to_radar_space_address ||
        call_target(crosshair_call_address) != to_radar_space_address) {
        log("the radar code is changed by another mod, city maps are off");
        return;
    }
    if (waypoint != set_coord_blip_address) {
        log("the map waypoint call was already redirected (0x%X), chaining it", static_cast<unsigned>(waypoint));
    }
    if (draw_map != draw_map_address) {
        log("the radar's DrawMap call was already redirected (0x%X), chaining it", static_cast<unsigned>(draw_map));
    }
    waypoint_next = reinterpret_cast<set_coord_blip_t>(waypoint);
    draw_map_next = reinterpret_cast<draw_map_t>(draw_map);

    // PrintMap and DrawRadarMap last: they switch the city maps on, the rest only acts then
    const bool hooked =
        install_hook<decltype(to_radar_space_hook)::element_type, to_radar_space_t>(to_radar_space_hook, to_radar_space_address, "the radar transform") &&
        install_hook<decltype(player_coors_hook)::element_type, player_coors_t>(player_coors_hook, player_coors_address, "FindPlayerCoors") &&
        install_hook<decltype(draw_blips_hook)::element_type, draw_blips_t>(draw_blips_hook, draw_blips_address, "CRadar::DrawBlips") &&
        install_hook<decltype(draw_area_hook)::element_type, draw_area_t>(draw_area_hook, draw_area_address, "CRadar::DrawAreaOnRadar") &&
        install_hook<decltype(draw_section_hook)::element_type, draw_section_t>(draw_section_hook, draw_section_address, "CRadar::DrawRadarSection") &&
        install_hook<decltype(draw_section_map_hook)::element_type, draw_section_map_t>(draw_section_map_hook, draw_section_map_address, "CRadar::DrawRadarSectionMap") &&
        install_hook<decltype(crosshair_zone_hook)::element_type, crosshair_zone_t>(crosshair_zone_hook, crosshair_zone_address, "the map zone name");
    if (!hooked) {
        return;
    }
    if (!redirect_call(draw_map_call_address, reinterpret_cast<const void *>(&draw_map_call)) ||
        !redirect_call(north_call_address, reinterpret_cast<const void *>(&to_radar_space_raw)) ||
        !redirect_call(crosshair_call_address, reinterpret_cast<const void *>(&to_radar_space_raw)) ||
        !redirect_call(waypoint_call_address, reinterpret_cast<const void *>(&waypoint_blip))) {
        log("could not patch the radar calls, city maps are off");
        return;
    }
    if (install_hook<decltype(draw_radar_map_hook)::element_type, draw_radar_map_t>(draw_radar_map_hook, draw_radar_map_address, "CRadar::DrawRadarMap") &&
        install_hook<decltype(print_map_hook)::element_type, print_map_t>(print_map_hook, print_map_address, "CMenuManager::PrintMap")) {
        log("vc_lc %s: city maps on", VC_LC_VERSION);
    }
}
} // namespace
} // namespace vc_lc

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        vc_lc::self_module = module;
        vc_lc::install();
    }
    return TRUE;
}
