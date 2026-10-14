// Checks the precompiled font and worldmap shaders (see font_shaders.py) by rendering through DXVK.
//
// Build against DXVK native (e.g. the one built by cmake/linux.cmake) and run with any Vulkan driver,
// a software one works too (Mesa lavapipe + Xvfb):
//   clang++ -std=c++20 test_shaders.cpp -I<dxvk>/include/native/directx -I<dxvk>/include/native/windows \
//       $(pkg-config --cflags --libs sdl2) <dxvk-build>/src/d3d9/libdxvk_d3d9.so -o test_shaders
//   xvfb-run ./test_shaders <repo>/src/techniques
#include <SDL2/SDL.h>
#include <d3d9.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

static std::vector<char> readFile(const char *path)
{
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

struct Vertex
{
    float x, y, z, rhw;
    D3DCOLOR color;
    float u, v, w;
};

static IDirect3DTexture9 *makeTexture(IDirect3DDevice9 *dev, UINT w, UINT h, const std::vector<uint32_t> &texels)
{
    IDirect3DTexture9 *tex = nullptr;
    if (FAILED(dev->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr)))
        return nullptr;
    D3DLOCKED_RECT lr;
    tex->LockRect(0, &lr, nullptr, 0);
    for (UINT y = 0; y < h; y++)
        for (UINT x = 0; x < w; x++)
            reinterpret_cast<uint32_t *>(static_cast<char *>(lr.pBits) + y * lr.Pitch)[x] = texels[y * w + x];
    tex->UnlockRect(0);
    return tex;
}

static uint32_t renderAndRead(IDirect3DDevice9 *dev, IDirect3DPixelShader9 *ps, D3DCOLOR vertexColor, float gradientV)
{
    const float s = 64.f;
    Vertex quad[4] = {
        {-0.5f, -0.5f, 0.5f, 1.f, vertexColor, 0.f, 0.f, gradientV},
        {s - 0.5f, -0.5f, 0.5f, 1.f, vertexColor, 1.f, 0.f, gradientV},
        {-0.5f, s - 0.5f, 0.5f, 1.f, vertexColor, 0.f, 1.f, gradientV},
        {s - 0.5f, s - 0.5f, 0.5f, 1.f, vertexColor, 1.f, 1.f, gradientV},
    };
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF000000, 1.f, 0);
    dev->BeginScene();
    dev->SetPixelShader(ps);
    dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE3(0));
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_ZENABLE, FALSE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    for (DWORD i = 0; i < 2; i++)
    {
        dev->SetSamplerState(i, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        dev->SetSamplerState(i, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        dev->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        dev->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    }
    dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex));
    dev->EndScene();

    IDirect3DSurface9 *rt = nullptr, *sys = nullptr;
    dev->GetRenderTarget(0, &rt);
    D3DSURFACE_DESC desc;
    rt->GetDesc(&desc);
    dev->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr);
    dev->GetRenderTargetData(rt, sys);
    D3DLOCKED_RECT lr;
    sys->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    const uint32_t pixel = reinterpret_cast<uint32_t *>(static_cast<char *>(lr.pBits) + 32 * lr.Pitch)[32];
    sys->UnlockRect();
    sys->Release();
    rt->Release();
    return pixel;
}

static bool close(uint32_t a, uint32_t b)
{
    for (int shift = 0; shift < 24; shift += 8) // RGB, the back buffer has no alpha
        if (std::abs(int((a >> shift) & 0xFF) - int((b >> shift) & 0xFF)) > 2)
            return false;
    return true;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::printf("usage: shadertest <techniques dir>\n");
        return 2;
    }
    const std::string dir = argv[1];

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("test", 0, 0, 64, 64, SDL_WINDOW_VULKAN);

    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp = {};
    pp.BackBufferWidth = 64;
    pp.BackBufferHeight = 64;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = reinterpret_cast<HWND>(window);
    pp.Windowed = TRUE;
    IDirect3DDevice9 *dev = nullptr;
    if (FAILED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, reinterpret_cast<HWND>(window),
                                 D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev)))
    {
        std::printf("FAIL: CreateDevice\n");
        return 1;
    }

    int failures = 0;
    auto check = [&](const char *what, bool ok) {
        std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
        failures += ok ? 0 : 1;
    };

    // font bitmap: one solid texel color, gradient: two rows (v = 0 green-ish, v = 1 blue-ish)
    IDirect3DTexture9 *font = makeTexture(dev, 1, 1, {0xFFC08040});
    IDirect3DTexture9 *gradient = makeTexture(dev, 1, 2, {0xFF00FF80, 0xFF8000FF});
    dev->SetTexture(0, font);
    dev->SetTexture(1, gradient);

    IDirect3DPixelShader9 *psFont = nullptr, *psGradient = nullptr;
    auto fontCode = readFile((dir + "/interfaces/font_ps_font.pso").c_str());
    auto gradientCode = readFile((dir + "/interfaces/font_ps_font_gradient.pso").c_str());
    check("create font_ps_font.pso", SUCCEEDED(dev->CreatePixelShader(reinterpret_cast<DWORD *>(fontCode.data()), &psFont)));
    check("create font_ps_font_gradient.pso",
          SUCCEEDED(dev->CreatePixelShader(reinterpret_cast<DWORD *>(gradientCode.data()), &psGradient)));

    IDirect3DVertexShader9 *vsClouds = nullptr;
    auto cloudsCode = readFile((dir + "/worldmap/worldmap_main.vso").c_str());
    check("create worldmap_main.vso",
          SUCCEEDED(dev->CreateVertexShader(reinterpret_cast<DWORD *>(cloudsCode.data()), &vsClouds)));

    // texel 0xC08040 * vertex color 0x808080 (0.5) = 0x604020
    uint32_t p = renderAndRead(dev, psFont, 0xFF808080, 0.f);
    std::printf("  font: 0x%08X\n", p);
    check("font = texture * vertex color", close(p, 0x604020));

    // * gradient row 0 (0x00FF80) = 0x004010 ; row 1 (0x8000FF) = 0x300020
    p = renderAndRead(dev, psGradient, 0xFF808080, 0.f);
    std::printf("  gradient v=0: 0x%08X\n", p);
    check("gradient top row", close(p, 0x004010));
    p = renderAndRead(dev, psGradient, 0xFF808080, 1.f);
    std::printf("  gradient v=1: 0x%08X\n", p);
    check("gradient bottom row", close(p, 0x300020));

    std::printf(failures ? "FAILED (%d)\n" : "ALL PASSED\n", failures);
    return failures ? 1 : 0;
}
