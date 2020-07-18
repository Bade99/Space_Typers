// Space_Typers.cpp : Defines the entry point for the application.
//
#define _CRT_SECURE_NO_WARNINGS //freopen


#include <SDKDDKVer.h> // defines highest available Windows platform, to build for a previous one set the _WIN32_WINNT macro to the platform you want and then inlcude
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include "resource.h"
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <stdio.h>
#include <string>
#include <random>

#include "space_typers.cpp" //INFO: disable compilation of .cpp in properties, that way we get only one translation unit that is win32_space_typers.cpp

#include <windows.h>

#include <Mmreg.h> //Direct Sound
#include <dsound.h> //Direct Sound

//TODO(fran): static linking instead of multithreaded dll?

//Things I learned from this project:

//At the beginnning, when I was trying to make a better determination for dt and wanted to be able to wait on the render from WM_PAINT, I learned that I cant wait on paint, sooner or later someone from the game thread needs to send a message to the main one that cant be asynced (eg. setwindowpos for positionion a word, since it was a win32 static control) -> State and View cant be coupled

void _my_assert(const wchar_t* exp, const wchar_t* file, unsigned int line) {
    wchar_t txt[1024];
    swprintf_s(txt, 1024, L"expression: %s\nfile: %s\nline: %u", exp, file, line);
    MessageBoxW(0,txt,L"Assertion failed!", MB_TOPMOST);//TODO(fran): change to something that does not need windows libs
    //std::terminate(); //TODO(fran): maybe just write to null
}

#define assert(expression) (void)(                                                       \
            (!!(expression)) ||                                                              \
            (_my_assert(_CRT_WIDE(#expression), _CRT_WIDE(__FILE__), (unsigned)(__LINE__)), 0) || \
            (std::terminate(), 0) \
        )

//Main Window Messages

#pragma region Defer
template <typename F>
struct Defer {
    Defer(F f) : f(f) {}
    ~Defer() { f(); }
    F f;
};

template <typename F>
Defer<F> makeDefer(F f) {
    return Defer<F>(f);
};

#define __defer( line ) defer_ ## line
#define _defer( line ) __defer( line )

struct defer_dummy { };
template<typename F>
Defer<F> operator+(defer_dummy, F&& f)
{
    return makeDefer<F>(std::forward<F>(f));
}

#define defer auto _defer( __LINE__ ) = defer_dummy( ) + [ & ]( )
#pragma endregion


#define MAX_LOADSTRING 100

// Global Variables:
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

typedef uint8_t u8; //prepping for jai
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef wchar_t utf16;

struct win32_framebuffer {
    void* bytes{ 0 };
    BITMAPINFO nfo;
    i32 width;
    i32 height;
    i32 bytes_per_pixel;
    i32 pitch;
};

struct sound_output {
    int samples_per_sec; //change to sample_rate
    int volume;
    u32 runningsampleindex;
    int bytes_per_sample;
    int secondary_buf_sz;
    int latency_sample_count;
};

#define rc_width(r) (r.right >= r.left ? r.right - r.left : r.left - r.right )

#define rc_height(r) (r.bottom >= r.top ? r.bottom - r.top : r.top - r.bottom )

#define Bytes(n) (n)
#define Kilobytes(n) ((n)*1024)
#define Megabytes(n) (Kilobytes(n)*1024)
#define Gigabytes(n) (Megabytes(n)*1024LL)
#define Terabytes(n) (Gigabytes(n)*1024LL)

void ResizeFramebuffer(win32_framebuffer* buf, int width, int height) {

    if (buf->bytes) VirtualFree(buf->bytes, 0, MEM_RELEASE);

    buf->nfo.bmiHeader.biSize = sizeof(buf->nfo.bmiHeader);
    buf->nfo.bmiHeader.biWidth = width;
    buf->nfo.bmiHeader.biHeight = -height; //top down DIB, origin at upper left corner
    buf->nfo.bmiHeader.biPlanes = 1;
    buf->nfo.bmiHeader.biBitCount = 32;
    buf->nfo.bmiHeader.biCompression = BI_RGB;
    buf->nfo.bmiHeader.biSizeImage = 0;
    buf->nfo.bmiHeader.biXPelsPerMeter = 0;
    buf->nfo.bmiHeader.biYPelsPerMeter = 0;
    buf->nfo.bmiHeader.biClrUsed = 0;
    buf->nfo.bmiHeader.biClrImportant = 0;

    buf->height = height;
    buf->width = width;
    buf->bytes_per_pixel = buf->nfo.bmiHeader.biBitCount / 8;
    buf->pitch = buf->width * buf->bytes_per_pixel;
    buf->bytes = VirtualAlloc(0, width * height * buf->bytes_per_pixel, MEM_COMMIT, PAGE_READWRITE);
}

void win32_render_to_screen(HDC dc, RECT r, win32_framebuffer* buf) {
    StretchDIBits(dc, r.left, r.top, rc_width(r), rc_height(r), 0, 0, buf->width, buf->height, buf->bytes, &buf->nfo, DIB_RGB_COLORS, SRCCOPY);
}

constexpr const utf16* words[] = { L"canción",L"pequeño",L"año",L"hola",L"especial",L"duda",L"belleza",L"felicidad",L"tristeza",L"libertad",L"hermandad",L"durabilidad" };

using my_engine = std::default_random_engine; // tipo de engine //INFO: me permite cambiarlo facil en el futuro
my_engine re{};

//inline void SpawnWord(game_state* gs) {
//    if (gs->words.size() < ARRAYSIZE(words) - 1) {
//        bool valid = false;
//        for (; !valid;) {
//            using my_distribution = std::uniform_int_distribution<>; // tipo de distribution 
//            my_distribution range{ 0,ARRAYSIZE(words) - 1 }; // distribution que mapea a 0..SIZE
//            //auto dice = [&range, &re]() { return range(re); }; // generator
//            const utf16* word = words[range(re)]; // es un valor [0:SIZE]
//            if (auto it = std::find_if(gs->words.begin(), gs->words.end(), [word](Word it) { return wcscmp(it.txt, word); }); it == gs->words.end()) {
//                valid = true;
//
//                my_distribution rc_range{ 0 + 20, gs->world.height - 20 };
//
//                Word w;
//                w.pos = { (float)gs->world.width,(float)rc_range(re) };
//                w.speed = { gs->word_speed,0 };
//                wcsncpy_s(w.txt, word, sizeof(w.txt) / sizeof(*w.txt));
//                w.txt[sizeof(w.txt) / sizeof(*w.txt) - 1] = 0; //TODO(fran): this is ugly
//                gs->words.push_back(w);
//            }
//        }
//    }
//}

//Compares only from the beginning and stops when it finds a null character, does not check whether both strings contain the null character there
bool my_strcmp(const utf16* s1, const utf16* s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2) return false;
        s1++;
        s2++;
    }
    return true;
}

//void Tick(game_state* gs) {
//    gs->accumulated_time += gs->dt;
//    if (gs->accumulated_time > gs->next_word_time) {
//        gs->accumulated_time = 0;
//        printf("pre-spawn\n");
//        printf("%d words\n",(int)gs->words.size());
//        SpawnWord(gs); //PROBLEM'S HERE y en otros lugares tmb
//        printf("post-spawn\n");
//        printf("%d words\n",(int)gs->words.size());
//    }
//    //printf("pre-changepos\n");
//    for (auto it = gs->words.begin(); it != gs->words.end();) { //TODO(fran): measure, probably two passes is faster
//        it->pos = { it->pos.x - gs->dt * it->speed.x, it->pos.y };//TODO(fran): operator overloading
//        if (it->pos.x <= 0) {
//            printf("ERASE\n");
//            it = gs->words.erase(it);
//        }
//        else it++;
//    }
//    //printf("post-changepos\n");
//
//    if (gs->new_char) {
//        gs->current_word += gs->new_char;
//        //wprintf(L"%ls\n", gs->current_word.c_str());
//    }
//
//    //this should probably also go inside if key_released
//    {
//        bool clear = true;
//        for (const auto& w : gs->words) {
//            if (my_strcmp(gs->current_word.c_str(), w.txt)) { clear = false; break; }
//        }
//        if (clear) gs->current_word.clear();
//    }
//
//    //Word completed
//    //printf("pre-completed\n");
//    if (auto it = std::find_if(gs->words.begin(), gs->words.end(), [w = gs->current_word.c_str() ](Word it) { return wcscmp(it.txt, w); }); it != gs->words.end())
//    {
//        gs->words.erase(it);
//        gs->current_word.clear();
//        gs->word_speed += .1f;
//        printf("GOT ONE\n");
//    }
//    //printf("post-completed\n");
//
//}



#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter )
typedef DIRECT_SOUND_CREATE(direct_sound_create);

LPDIRECTSOUNDBUFFER secondary_buf;

void DirectSound_Init(HWND wnd, u32 samples_per_sec, u32 buf_sz) {
    //TODO(fran): move to XAudio2
    HMODULE DirectSoundLib = LoadLibraryA("dsound.dll"); //We still want the program to work even if the user doesnt have direct sound
    if (DirectSoundLib) {
        direct_sound_create* DirectSound_Create = (direct_sound_create*)GetProcAddress(DirectSoundLib, "DirectSoundCreate");
        LPDIRECTSOUND ds;
        if (DirectSound_Create && SUCCEEDED(DirectSound_Create(0, &ds,0))) {
            ds->SetCooperativeLevel(wnd, DSSCL_PRIORITY);
            LPDIRECTSOUNDBUFFER primary_buf;
            DSBUFFERDESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.dwSize = sizeof(desc);
            desc.dwFlags = DSBCAPS_PRIMARYBUFFER; //TODO(fran): check other flags

            WAVEFORMATEX format;
            format.wFormatTag = WAVE_FORMAT_PCM;//TODO(fran): there are other formats
            format.nChannels = 2;
            format.nSamplesPerSec = samples_per_sec;
            format.wBitsPerSample = 16;
            format.nBlockAlign = (format.nChannels * format.wBitsPerSample) / 8;
            format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
            format.cbSize = 0;
            if (SUCCEEDED(ds->CreateSoundBuffer(&desc, &primary_buf, 0))) {
                if (SUCCEEDED(primary_buf->SetFormat(&format))) {
                }
                else { assert(0); }//TODO(fran): LOG
            }
            else { assert(0); }//TODO(fran): LOG

            DSBUFFERDESC secondary_desc;
            ZeroMemory(&secondary_desc, sizeof(secondary_desc));
            secondary_desc.dwSize = sizeof(secondary_desc);
            secondary_desc.dwFlags = 0; //TODO(fran): check other flags
            secondary_desc.lpwfxFormat = &format;
            secondary_desc.dwBufferBytes = buf_sz;

            if (SUCCEEDED(ds->CreateSoundBuffer(&secondary_desc, &secondary_buf, 0))) {
            }
            else { assert(0); }//TODO(fran): LOG

        }
        else {}//TODO(fran): LOG
    }
}

void FillSoundBuffer(sound_output* dest_buf, DWORD byte_to_lock, DWORD bytes_to_write, game_soundbuffer* source_buf) {
    void* reg1;
    void* reg2;
    DWORD reg1_sz, reg2_sz;
    HRESULT h = secondary_buf->Lock(byte_to_lock, bytes_to_write, &reg1, &reg1_sz, &reg2, &reg2_sz, 0);
    if (SUCCEEDED(h)) {
        DWORD reg1samplecount = reg1_sz / dest_buf->bytes_per_sample;
        i16* source = source_buf->samples;
        i16* dest = (i16*)reg1;
        for (DWORD sample_index = 0; sample_index < reg1samplecount; sample_index++) {
            *dest++ = *source++;
            *dest++ = *source++;
            dest_buf->runningsampleindex++;
        }
        DWORD reg2samplecount = reg2_sz / dest_buf->bytes_per_sample;
        dest = (i16*)reg2;
        for (DWORD sample_index = 0; sample_index < reg2samplecount; sample_index++) {
            *dest++ = *source++;
            *dest++ = *source++;
            dest_buf->runningsampleindex++;
        }

        secondary_buf->Unlock(reg1, reg1_sz, reg2, reg2_sz);

    }
    //else assert(0); //TODO(fran): there are bugs, this gets called sometimes
}

void win32_ClearSoundBuffer(sound_output* sound_out) {
    void* reg1;
    void* reg2;
    DWORD reg1_sz, reg2_sz;
    if (SUCCEEDED(secondary_buf->Lock(0, sound_out->secondary_buf_sz, &reg1, &reg1_sz, &reg2, &reg2_sz, 0))) {
        u8* dest_sample = (u8*)reg1;
        for (DWORD byte_index = 0; byte_index < reg1_sz; byte_index++) {
            *dest_sample++ = 0;
        }
        dest_sample = (u8*)reg2;
        for (DWORD byte_index = 0; byte_index < reg2_sz; byte_index++) {
            *dest_sample++ = 0;
        }
        secondary_buf->Unlock(reg1, reg1_sz, reg2, reg2_sz);
    }
}

void win32_process_digital_button(game_button_state* new_state, bool is_down) {
    assert(new_state->ended_down != is_down);
    new_state->ended_down = is_down;
    new_state->half_transition_count++;
}

void win32_DEBUG_draw_vertical(win32_framebuffer* frame_backbuffer, int x, int top, int bottom, DWORD color) {
    u8* pixel = (u8*)frame_backbuffer->bytes + x* frame_backbuffer->bytes_per_pixel + top*frame_backbuffer->pitch;
    for (int y = top; y < bottom; y++) {
        *(u32*)pixel = color;
        pixel += frame_backbuffer->pitch;
    }
}


u32 win32_get_refresh_rate_hz(HWND wnd) {
    HMONITOR mon = MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST); //TODO(fran): should it directly receive the hmonitor?
    if (mon) {
        MONITORINFOEX nfo;
        nfo.cbSize = sizeof(nfo);
        if (GetMonitorInfo(mon, &nfo)) {
            DEVMODE devmode;
            devmode.dmSize = sizeof(devmode);
            devmode.dmDriverExtra = 0;
            if (EnumDisplaySettings(nfo.szDevice, ENUM_CURRENT_SETTINGS, &devmode)) {
                return devmode.dmDisplayFrequency;
            }
        }
    }
    return 60;//TODO(fran): what to do?
}

//#include <chrono>
//#include <thread>
#include <timeapi.h> //timeBeginPeriod timeEndPeriod
#pragma comment(lib,"Winmm.lib") //timeBeginPeriod timeEndPeriod

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_SPACETYPERS, szWindowClass, MAX_LOADSTRING);

    WNDCLASSEXW wcex;
    
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SPACETYPERS));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = 0;// MAKEINTRESOURCEW(IDC_SPACETYPERS);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassExW(&wcex);

    // Perform application initialization:

    HWND hwnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    assert(hwnd);

    ShowWindow(hwnd, nCmdShow);//TODO(fran): remove
    UpdateWindow(hwnd);

//#ifdef _DEBUG
    AllocConsole();
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
//#endif

    //HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SPACETYPERS));

    win32_framebuffer frame_backbuffer;

    ResizeFramebuffer(&frame_backbuffer, 1920, 1080);

    sound_output sound_out;
    sound_out.samples_per_sec = 48000; //change to sample_rate
    sound_out.volume = 500;
    sound_out.runningsampleindex = 0;
    sound_out.bytes_per_sample = sizeof(i16) * 2;
    sound_out.secondary_buf_sz = sound_out.samples_per_sec * sound_out.bytes_per_sample;
    sound_out.latency_sample_count = (sound_out.samples_per_sec / 50); //TODO(fran): this should divide by the game update hz

    f32 audio_latency_sec;
    DWORD audio_latency_bytes;

    i16* samples = (i16*)VirtualAlloc(0, sound_out.secondary_buf_sz, MEM_COMMIT, PAGE_READWRITE); //TODO(fran): por ahora para evitar la extra complejidad creamos un buffer nuevo, que no tiene los problemas de ring buffer de dsound y luego lo copiamos, pequeño costo en performance q luego se puede arreglar

    DirectSound_Init(hwnd, sound_out.samples_per_sec, sound_out.secondary_buf_sz);
    win32_ClearSoundBuffer(&sound_out);
    secondary_buf->Play(0, 0, DSBPLAY_LOOPING);

#if _DEBUG
    struct DEBUG_sound_cursor { DWORD play, write; };
    DEBUG_sound_cursor DEBUG_last_play_cur[40] = {0};
    int DEBUG_last_play_cur_index=0;
#endif

    //game_state gs;
    //gs.accumulated_time = 0;
    //gs.dt = 0.01f;
    //gs.current_word = L"";
    //gs.word_speed = 5.f;
    //gs.next_word_time = 2.f;
    //gs.world.height = frame_backbuffer.height;
    //gs.world.width = frame_backbuffer.width;

    game_input old_input{0};
    game_input new_input{0};

    game_memory game_mem;
#if _DEBUG
    LPVOID base_address = (LPVOID)Terabytes(1); //Fixed address for ease of debugging
#else
    LPVOID base_address = 0;
#endif
    game_mem.permanent_storage_sz = Megabytes(1);
    game_mem.transient_storage_sz = Megabytes(1);
    u32 total_sz = game_mem.permanent_storage_sz + game_mem.transient_storage_sz;
    game_mem.permanent_storage = VirtualAlloc(base_address, total_sz, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    game_mem.transient_storage = (u8*)game_mem.permanent_storage + game_mem.permanent_storage_sz;
    
    assert(game_mem.permanent_storage && samples && frame_backbuffer.bytes);

    MSG msg{0};

    LARGE_INTEGER pc_freq;
    QueryPerformanceFrequency(&pc_freq); //per sec

    LARGE_INTEGER last_counter;
    QueryPerformanceCounter(&last_counter);

    int monitor_refresh_hz = 60;

    bool sound_is_valid = false;
    DWORD last_play_cursor = 0;
    DWORD last_write_cursor = 0;

    bool run = true;
    //New message loop, does not stop when there are no more messages
    while (run) {
        monitor_refresh_hz = win32_get_refresh_rate_hz(hwnd);//TODO(fran): find a way to detect refresh rate changes
        ZeroMemory(&new_input.controller, sizeof(new_input.controller));
        for (int i = 0; i < ARRAYSIZE(new_input.controller.buttons); i++)
            new_input.controller.buttons[i].ended_down = old_input.controller.buttons[i].ended_down;

        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) { //NOTE IMPORTANT: if you put hwnd in the second parameter you are missing lots of messages, eg WM_QUIT and msgs related to changing the keyboard eg. alt+shift
            if(msg.message==WM_QUIT) run = false; //TODO(fran): should I leave this in wm_destroy so I dont have to pay for this branching?

            switch (msg.message) {
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                u32 vkcode = (u32)msg.wParam;
                bool was_down = (msg.lParam & (1 << 30)) != 0;
                bool is_down = (msg.lParam & (1 << 31)) == 0;
                if (was_down != is_down) {
                    if (vkcode == 'W' ||vkcode == VK_UP) { //TODO(fran): rebindable keys
                        win32_process_digital_button(&new_input.controller.up, is_down);
                    }
                    else if (vkcode == 'A' || vkcode == VK_LEFT) {
                        win32_process_digital_button(&new_input.controller.left, is_down);
                    }
                    else if (vkcode == 'S' || vkcode == VK_DOWN) {
                        win32_process_digital_button(&new_input.controller.down, is_down);
                    }
                    else if (vkcode == 'D' || vkcode == VK_RIGHT) {
                        win32_process_digital_button(&new_input.controller.right, is_down);
                    }
                    else if (vkcode == VK_SPACE ||vkcode==VK_RETURN) {
                        win32_process_digital_button(&new_input.controller.enter, is_down);
                    }
                    else if (vkcode == VK_ESCAPE) {
                        win32_process_digital_button(&new_input.controller.back, is_down);
                    }
                }
                break;
            }
            case WM_CHAR:
            {
                //TODO(fran): surrogate pairs, alt+shift locks down the program, add support for japanese and the like
                new_input.controller.new_char = static_cast<utf16>(msg.wParam);
                //TODO(fran): this is good for now, but characters do get overriden when two are typed too close in time
                //utf16 a[3]{ 0,L'\n',0 }; a[0] = c;
                //OutputDebugStringW(a);
                break;
            }
            }
            //INFO: I still want to process every msg since I leave windows to take care of WM_CHAR (I know TranslateMessage emits some WM_CHAR but I dont know if there are others that do too)
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        //if (GetAsyncKeyState('W') & 0x8000) sound_out.hz = 520;
        //else if (GetAsyncKeyState('S') & 0x8000) sound_out.hz = 128;
        //else sound_out.hz = 256;

        //Tick(&gs);
        
        DWORD byte_to_lock=0, target_cursor=0, bytes_to_write=0;
        if (sound_is_valid) {
            sound_is_valid = true;
            byte_to_lock = (sound_out.runningsampleindex * sound_out.bytes_per_sample) % sound_out.secondary_buf_sz;
            target_cursor = (last_play_cursor + sound_out.latency_sample_count * sound_out.bytes_per_sample) % sound_out.secondary_buf_sz; //I'm pretty sure this is wrong, it looks to me like you're overriding the play cursor
            bytes_to_write=0;
            //if (byte_to_lock == target_cursor) {
            //    bytes_to_write = 0;
            //}
            /*else*/ if (byte_to_lock > target_cursor) {
                bytes_to_write = sound_out.secondary_buf_sz - byte_to_lock;
                bytes_to_write += target_cursor;
            }
            else {
                bytes_to_write = target_cursor - byte_to_lock;
            }
        }

        game_framebuffer game_frame_buf; //INFO: Construís el objeto genérico a partir del tuyo específico
        game_frame_buf.bytes = frame_backbuffer.bytes;
        game_frame_buf.bytes_per_pixel = frame_backbuffer.bytes_per_pixel;
        game_frame_buf.height = frame_backbuffer.height;
        game_frame_buf.width = frame_backbuffer.width;

        game_soundbuffer game_sound_buf;
        game_sound_buf.samples = samples;
        game_sound_buf.samples_per_sec = sound_out.samples_per_sec;
        game_sound_buf.sample_count_to_output = bytes_to_write/sound_out.bytes_per_sample; //TODO(fran): por ahora siempre pedimos 1/30 de segundo de audio cada vez

        game_update_and_render(&game_mem, &game_frame_buf,&game_sound_buf,&new_input);

        //FillSoundBuffer(&sound_out, bytetolock, bytes_to_write);
        if (sound_is_valid) {
            
            DWORD play_cur, write_cur; 
            secondary_buf->GetCurrentPosition(&play_cur, &write_cur);
            DWORD unwrapped_write_cursor = write_cur;
            if (unwrapped_write_cursor < play_cur) //Remember that we have a circular buffer, TODO(fran): wont this will be wrong if the write cursor happened to lag behind the play cursor? can this happen?
                unwrapped_write_cursor += sound_out.secondary_buf_sz;
            audio_latency_bytes = unwrapped_write_cursor - play_cur;
            audio_latency_sec = ((f32)audio_latency_bytes / (f32)sound_out.bytes_per_sample) / (f32)sound_out.samples_per_sec;
            printf("audio latency: %f sec\n", audio_latency_sec);
            
            FillSoundBuffer(&sound_out, byte_to_lock, bytes_to_write,&game_sound_buf);

        }

        old_input = new_input;//TODO(fran): put this somewhere nice

        //IDEA: que el programa de tipeo tenga modo para aprender a tipear teclas tipicas en programación: {} () & % etc
        //IDEA: teclado invertido (q->p, p->q, g->h, ...), así finalmente me la juego intentando entender como funciona el teclado, tiene pinta q es un quilombo igual, distintos formatos de teclado me pueden matar

        LARGE_INTEGER end_counter;
        QueryPerformanceCounter(&end_counter);
        f32 dt_per_sec = (f32)(end_counter.QuadPart - last_counter.QuadPart) / (f32)pc_freq.QuadPart;
        {
            int game_update_hz = monitor_refresh_hz / 2;
            f32 target_sec_per_frame = 1.f/game_update_hz;
            if (dt_per_sec < target_sec_per_frame) {
                UINT desired_scheduler_ms = 1;
                bool granular_sleep = timeBeginPeriod(desired_scheduler_ms) == TIMERR_NOERROR;//TODO(fran): spinlocking doesnt look so bad now
                while (dt_per_sec < target_sec_per_frame) {
                    if(granular_sleep)
                        Sleep((DWORD)((target_sec_per_frame - dt_per_sec) * 1000.f));
                    //std::this_thread::sleep_for(std::chrono::microseconds((u64)((target_sec_per_frame - dt_per_sec) * 1000000.f)));
                    QueryPerformanceCounter(&end_counter);
                    dt_per_sec = (f32)(end_counter.QuadPart - last_counter.QuadPart) / (f32)pc_freq.QuadPart;
                }
                timeEndPeriod(desired_scheduler_ms);
            } else { /*TODO(fran): LOG missed a frame*/ }
            //printf("game update hz: %d\ntarget sec per frame: %f\ndt per sec: %f\n", game_update_hz, target_sec_per_frame,dt_per_sec);
        }
        //QueryPerformanceCounter(&end_counter);
        //dt_per_sec = (f32)(end_counter.QuadPart - last_counter.QuadPart) / (f32)pc_freq.QuadPart;
        printf("%f ms ; fps %f\n", dt_per_sec * 1000.f,1.f/dt_per_sec); //ms
        last_counter = end_counter;

#if _DEBUG
        [&](){
            int top = 0;
            int bottom = frame_backbuffer.height;

            int pad_x = 16;
            f32 c = (f32)(frame_backbuffer.width-2*pad_x) / (f32)sound_out.secondary_buf_sz;
            for (int i = 0; i < DEBUG_last_play_cur_index; i++) {
                i32 x = (i32) (pad_x + c * (f32)DEBUG_last_play_cur[i].play);
                win32_DEBUG_draw_vertical(&frame_backbuffer, x, top, bottom,0xFFFFFFFF);
                x = (i32) (pad_x + c * (f32)DEBUG_last_play_cur[i].write);
                win32_DEBUG_draw_vertical(&frame_backbuffer, x, top, bottom, 0x00FF0000);
            }
        }();
#endif

        //INFO IMPORTANT: Casey said we should wait first and then display the frame, and the time to display counts for the next frame's time. Pretty nice and it makes sense
        RECT r; GetClientRect(hwnd, &r); //TODO(fran): we render the whole screen, not just what rcpaint wants
        HDC dc = GetDC(hwnd);
        win32_render_to_screen(dc, r,&frame_backbuffer);
        ReleaseDC(hwnd,dc);


        if (DWORD play, write; secondary_buf->GetCurrentPosition(&play, &write) == DS_OK) {
            last_play_cursor = play;
            last_write_cursor = write;
            if (!sound_is_valid) {
                sound_out.runningsampleindex = write / sound_out.bytes_per_sample;
                sound_is_valid = true;
            }
#if _DEBUG
            DEBUG_last_play_cur[DEBUG_last_play_cur_index++] = { play,write };
            if (DEBUG_last_play_cur_index >= ARRAYSIZE(DEBUG_last_play_cur)) DEBUG_last_play_cur_index = 0;
#endif
        }
        else sound_is_valid = false;

    }

    return (int) msg.wParam;
}

inline double GetPCFrequency() {
    LARGE_INTEGER li;
    assert(QueryPerformanceFrequency(&li));
    return double(li.QuadPart); //seconds
}

inline __int64 StartCounter() {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
}

// Get time passed since CounterStart, in the same unit of time as PCFreq
inline double GetCounter(__int64 CounterStart, double PCFreq)
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return double(li.QuadPart - CounterStart) / PCFreq;
}

//DWORD WINAPI Game(HWND hwnd) {
//    double freq = GetPCFrequency();
//    __int64 counter;
//    counter = StartCounter(); //Probably start and get counter are annoying and confusing for this case, and I should just retrieve current time and compare against last one
//    Game_State gs;
//    gs.accumulated_time = 0;
//    gs.window = hwnd;
//    gs.dt = 0.01;
//    gs.current_word = "";
//    gs.key_released = '\0';
//    gs.word_speed = 5.f;
//    gs.next_word_time = 2.f; //TODO(fran): maybe all this should get called after a WM_PAINT
//    while (run_game) {
//        gs.dt = GetCounter(counter, freq);
//        counter = StartCounter();
//        Tick(gs);
//        gs.key_released = 0;
//        if (KEY_RELEASED) {
//            gs.key_released = KEY_RELEASED;
//            KEY_RELEASED = 0; //TODO(fran): atomic swap
//        }
//        { //Render
//            InvalidateRect(gs.window, NULL, TRUE);
//        }
//        //printf("%f\n", gs.dt);
//    }
//    return 0;
//}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    //static HANDLE worker_thread_handle;
    switch (message)
    {
    //case WM_CREATE:
    //{
    //    //worker_thread_handle=CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Game, hwnd, 0, NULL);
    //    break;
    //}
    //case WM_COMMAND:
    //    {
    //        int wmId = LOWORD(wparam);
    //        // Parse the menu selections:
    //        switch (wmId)
    //        {
    //        //case IDM_ABOUT:
    //        //    DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, About);
    //        //    break;
    //        //case IDM_EXIT:
    //        //    DestroyWindow(hwnd);
    //        //    break;
    //        default:
    //            return DefWindowProc(hwnd, message, wparam, lparam);
    //        }
    //        break;
    //    }
    //case ST_DESTROYWINDOW:
    //{
    //    DestroyWindow((HWND)wParam);
    //    break;
    //}
    //case ST_CREATE_STATIC_WINDOW:
    //{
    //    SIZE txt_sz;
    //    GetTextExtentPoint32A(GetDC(hwnd), (const char*)wParam, strlen((const char*)wParam), &txt_sz); //TODO(fran): hardcoded HDC for text font

    //    return (LRESULT)CreateWindowA("Static", (LPCSTR)wParam, WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE | SS_CENTER
    //        , HIWORD(lParam), LOWORD(lParam), txt_sz.cx, txt_sz.cy, hwnd, NULL, NULL, NULL); //TODO(fran): no funca xq estamos en otro thread, necesito pedir al otro thread q 
    //}
    //case WM_SYSKEYDOWN:
    //{
    //    break;
    //}
    //case WM_SYSKEYUP:
    //{
    //    break;
    //}
    //case WM_KEYDOWN:
    //{
    //    break;
    //}
    //case WM_KEYUP:
    //{
    //    u32 VKCode = wparam;
    //    break;
    //}
    case WM_ERASEBKGND:
    {
        HDC dc = (HDC)wparam;
        RECT r; GetClientRect(hwnd, &r);
        FillRect(dc, &r, (HBRUSH)GetStockObject(DKGRAY_BRUSH));
        return 1;
    }
    //case WM_CTLCOLORSTATIC:
    //{
    //    HDC dc = (HDC)wparam;
    //    SetTextColor(dc, RGB(255,255,255));

    //    LOGBRUSH lb;
    //    HBRUSH br = (HBRUSH)GetStockObject(DKGRAY_BRUSH);
    //    GetObject(br, sizeof(LOGBRUSH), &lb);
    //    SetBkColor(dc, lb.lbColor);
    //    
    //    return (INT_PTR)br;
    //}
    //case WM_KEYUP:
    //{
    //    char name[1024];
    //    UINT scanCode = lparam & 0x00FF0000;
    //    int result = GetKeyNameTextA(scanCode, name, 1024);
    //    if (result > 0 && name[1] == 0) {
    //        char c = (char)std::tolower(name[0]);
    //        printf("%c\n",c);
    //        KEY_RELEASED = c;
    //    }
    //    break;
    //}
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hwnd, message, wparam, lparam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

u32 safe_u64_to_u32(u64 n) {
    assert(n <= 0xFFFFFFFF); //TODO(fran): u32 max and min
    return (u32)n;
}

std::pair<void*, u32> platform_read_entire_file(const char* filename){
    void* res_mem=0;
    u32 res_mem_sz = 0;
    HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (hFile != INVALID_HANDLE_VALUE) {
        defer{ CloseHandle(hFile); };
        if (LARGE_INTEGER sz; GetFileSizeEx(hFile, &sz)) {
            u32 sz32 = safe_u64_to_u32(sz.QuadPart);
            void* mem = VirtualAlloc(0, sz32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);//TOOD(fran): READONLY?
            if (mem) {
                if (DWORD bytes_read; ReadFile(hFile, mem, sz32, &bytes_read, 0) && sz32==bytes_read) {
                    //SUCCESS
                    res_mem = mem;
                    res_mem_sz = sz32;
                }
                else {
                    platform_free_file_memory(mem);
                }
            }
        }
    }
    return { res_mem,res_mem_sz };
}
bool platform_write_entire_file(const char* filename, void* memory, u32 mem_sz){
    bool res = false;
    HANDLE hFile = CreateFileA(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (hFile != INVALID_HANDLE_VALUE) {
        defer{ CloseHandle(hFile); };
        if (DWORD bytes_written; WriteFile(hFile, memory, mem_sz, &bytes_written, 0)) {
            //SUCCESS
            res = mem_sz == bytes_written;
        }
        else {
            //TODO(fran): log
        }
    }
    return res;
}
void platform_free_file_memory(void* memory){
    if (memory) VirtualFree(memory, 0, MEM_RELEASE);
}