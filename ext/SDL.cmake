set(BUILD_SHARED_LIBS_save BUILD_SHARED_LIBS)
set(BUILD_SHARED_LIBS 0)
set(SDL_SHARED_save SDL_SHARED)
set(SDL_SHARED 0)
set(SDL_STATIC_save SDL_STATIC)
set(SDL_STATIC 1)
set(SDL_ASSEMBLY_save SDL_ASSEMBLY)
set(SDL_ASSEMBLY 0)
set(SDL_RENDER_D3D_save SDL_RENDER_D3D)
set(SDL_RENDER_D3D 0)
set(SDL_RENDER_D3D11_save SDL_RENDER_D3D11)
set(SDL_RENDER_D3D11 0)
set(SDL_RENDER_D3D12_save SDL_RENDER_D3D12)
set(SDL_RENDER_D3D12 0)
set(SDL_OPENGL_save SDL_OPENGL)
set(SDL_OPENGL 1)
set(SDL_OPENGLES_save SDL_OPENGLES)
set(SDL_OPENGLES 0)
set(SDL_GPU_save SDL_GPU)
set(SDL_GPU 0)
set(SDL_STATIC_PIC_save SDL_STATIC_PIC)
set(SDL_STATIC_PIC ON)
FetchContent_Declare(
    sdl3
    GIT_REPOSITORY  https://github.com/libsdl-org/SDL.git
    GIT_TAG main
)
FetchContent_MakeAvailable(sdl3)
add_subdirectory(sdl3 sdl3)
set(SDL_STATIC_PIC SDL_STATIC_PIC_save)
unset(SDL_STATIC_PIC_save)
set(SDL_GPU SDL_GPU_save)
unset(SDL_GPU_save)
set(SDL_OPENGLES SDL_OPENGLES_save)
unset(SDL_OPENGLES_save)
set(SDL_OPENGL SDL_OPENGL_save)
unset(SDL_OPENGL_save)
set(SDL_RENDER_D3D12 SDL_RENDER_D3D12_save)
unset(SDL_RENDER_D3D12_save)
set(SDL_RENDER_D3D11 SDL_RENDER_D3D11_save)
unset(SDL_RENDER_D3D11_save)
set(SDL_RENDER_D3D SDL_RENDER_D3D_save)
unset(SDL_RENDER_D3D_save)
set(SDL_ASSEMBLY SDL_ASSEMBLY_save)
unset(SDL_ASSEMBLY_save)
set(SDL_STATIC SDL_STATIC_save)
unset(SDL_STATIC_save)
set(SDL_SHARED SDL_SHARED_save)
unset(SDL_SHARED_save)
set(BUILD_SHARED_LIBS BUILD_SHARED_LIBS_save)
unset(BUILD_SHARED_LIBS_save)
